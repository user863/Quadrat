#include "PluginEditor.h"
#include "Presets.h"
#include "MidiText.h"

// palette order, grouped
struct PaletteEntry { qd::Kind kind; bool heading; const char* label; };

// name or tags
static bool matches (const PaletteEntry& e, const juce::String& term)
{
    return term.isEmpty() || juce::String (e.label).containsIgnoreCase (term)
        || juce::String (qd::searchTerms (e.kind)).containsIgnoreCase (term);
}

static const std::vector<PaletteEntry>& paletteEntries()
{
    static std::vector<PaletteEntry> list;

    if (list.empty())
    {
        // build order
        struct Section { const char* group; const char* title; };

        const Section groups[] =
        {
            { "source", "sources"    }, { "mi",   "voices"     },
            { "shape",  "shaping"    }, { "mod",  "modulation" },
            { "time",   "timing"     }, { "gen",  "generative" },
            { "out",    "output"     }
        };

        for (const auto& section : groups)
        {
            const char* grp = section.group;
            list.push_back ({ qd::Kind::osc, true, section.title });

            for (int k = 0; k < qd::numKinds(); ++k)
            {
                const auto& sp = qd::spec (static_cast<qd::Kind> (k));
                if (juce::String (sp.group) == grp)
                    list.push_back ({ static_cast<qd::Kind> (k), false, sp.name });
            }
        }
    }

    return list;
}

QuadratEditor::QuadratEditor (QuadratProcessor& p)
    : AudioProcessorEditor (&p), proc (p)
{
    setLookAndFeel (&lnf);
    setWantsKeyboardFocus (true);

    if (! welcomeSeen()) { welcoming = true; tourCard = TourCard::welcome; }

    addAndMakeVisible (clearCap);
    addAndMakeVisible (rollCap);
    addAndMakeVisible (startCap);
    addAndMakeVisible (stopCap);
    addAndMakeVisible (search);

    startCap.setCapColour (paint::green);

    search.setTextToShowWhenEmpty ("search", paint::inkSoft);
    search.setColour (juce::TextEditor::backgroundColourId, paint::relief.brighter (0.03f));
    search.setColour (juce::TextEditor::textColourId, paint::ink);
    search.setColour (juce::TextEditor::outlineColourId, paint::engrave);
    search.setColour (juce::TextEditor::focusedOutlineColourId, paint::ink);
    search.setColour (juce::TextEditor::highlightColourId, paint::inkSoft.withAlpha (0.3f));
    search.setJustification (juce::Justification::centredLeft);
    search.onTextChange = [this] { filterPalette(); palScroll = 0.0f; repaint(); };
    search.onEscapeKey = [this] { search.clear(); filterPalette(); grabKeyboardFocus(); repaint(); };

    filterPalette();

    clearCap.onClick = [this]
    {
        pushUndo();
        const juce::ScopedLock sl (proc.patchLock);
        proc.patch.clear();
        selection.clear();
        repaint();
    };

    // random patch, always plays
    rollCap.onClick = [this]
    {
        pushUndo();

        std::string name;

        {
            const juce::ScopedLock sl (proc.patchLock);
            name = presets::randomPatch (proc.patch,
                                      (unsigned int) juce::Random::getSystemRandom().nextInt (100000));
        }

        // show its name
        proc.setTitle (juce::String::fromUTF8 (name.c_str()));
        selection.clear();
        framedProgram = -1;                 // frame it
        repaint();
    };

    // resume, no reload
    startCap.onClick = [this]
    {
        proc.setMuted (false);
        stopCap.setToggleState (false, juce::dontSendNotification);
        repaint();
    };

    // stop = reset everything
    stopCap.onClick = [this] { proc.setMuted (! proc.isMuted()); repaint(); };

    setResizable (true, true);
    // down to 75%: fits a 150%-scaled laptop
    setResizeLimits ((int) (layout::W * 0.75f), (int) (layout::H * 0.75f), (int) layout::W * 2, (int) layout::H * 2);
    getConstrainer()->setFixedAspectRatio (layout::W / layout::H);
    setSize ((int) layout::W, (int) layout::H);
    startTimerHz (20);
}

QuadratEditor::~QuadratEditor() { setLookAndFeel (nullptr); }

void QuadratEditor::pushUndo()
{
    const juce::ScopedLock sl (proc.patchLock);
    undoStack.push_back ({ proc.patch.blocks(), proc.patch.cables(), proc.patch.notes(), proc.patchTitle() });
    redoStack.clear();

    if (undoStack.size() > 60) undoStack.erase (undoStack.begin());
}

void QuadratEditor::restore (const Snapshot& s)
{
    const juce::ScopedLock sl (proc.patchLock);
    proc.patch.blocks() = s.blocks;
    proc.patch.cables() = s.cables;
    proc.patch.notes() = s.notes;
    proc.patch.rebuild();
    if (s.title.isNotEmpty()) proc.setTitle (s.title);
    selection.clear();
    repaint();
}

void QuadratEditor::undo()
{
    if (undoStack.empty()) return;

    {
        const juce::ScopedLock sl (proc.patchLock);
        redoStack.push_back ({ proc.patch.blocks(), proc.patch.cables(), proc.patch.notes(), proc.patchTitle() });
    }

    const auto s = undoStack.back();
    undoStack.pop_back();
    restore (s);
}

void QuadratEditor::redo()
{
    if (redoStack.empty()) return;

    {
        const juce::ScopedLock sl (proc.patchLock);
        undoStack.push_back ({ proc.patch.blocks(), proc.patch.cables(), proc.patch.notes(), proc.patchTitle() });
    }

    const auto s = redoStack.back();
    redoStack.pop_back();
    restore (s);
}

// header flash, few secs
void QuadratEditor::say (const juce::String& text)
{
    notice = text;
    noticeUntil = juce::Time::getMillisecondCounterHiRes() + 2500.0;
    repaint();
}

void QuadratEditor::timerCallback()
{
    if (notice.isNotEmpty() && juce::Time::getMillisecondCounterHiRes() > noticeUntil)
        notice.clear();

    stopCap.setToggleState (proc.isMuted(), juce::dontSendNotification);

    {
        // copy, unlock, then draw
        const juce::ScopedLock sl (proc.patchLock);
        viewBlocks = proc.patch.blocks();
        viewCables = proc.patch.cables();
        viewTouching = proc.patch.touching();
        inputLive = proc.patch.hasInput();
        viewNotes = proc.patch.notes();
        viewMeters.resize (viewBlocks.size());
        viewSteps.assign (viewBlocks.size(), -1);
        viewPhase.assign (viewBlocks.size(), 0.0f);

        for (std::size_t i = 0; i < viewBlocks.size(); ++i)
        {
            if (viewBlocks[i].kind == qd::Kind::seq || viewBlocks[i].kind == qd::Kind::midi)
                viewSteps[i] = proc.patch.playhead (viewBlocks[i].id);

            if (viewBlocks[i].kind == qd::Kind::clock || viewBlocks[i].kind == qd::Kind::midi)
                viewPhase[i] = proc.patch.phase (viewBlocks[i].id);

            if (viewBlocks[i].kind == qd::Kind::ball)
            {
                auto& st = balls[viewBlocks[i].id];
                proc.patch.ballAt (viewBlocks[i].id, st.x, st.y, st.floorLit, st.wallLit);

                st.trail.push_back ({ st.x, st.y });
                while (st.trail.size() > 10) st.trail.erase (st.trail.begin());
            }
        }

        viewScopes.resize (viewBlocks.size());

        for (std::size_t i = 0; i < viewBlocks.size(); ++i)
            if (const float* sc = proc.patch.scope (viewBlocks[i].id))
                std::copy (sc, sc + qd::Patch::kScope, viewScopes[i].begin());

        // lamp = output only
        faceReadings.assign (viewBlocks.size(), 0.0f);

        for (std::size_t i = 0; i < viewBlocks.size(); ++i)
        {
            const auto& v = viewBlocks[i];
            viewMeters[i] = proc.patch.meter (v.id);

            if (v.kind == qd::Kind::comp || v.kind == qd::Kind::limit)
                faceReadings[i] = proc.patch.gainReduction (v.id);
            else if (v.kind == qd::Kind::meter)
                faceReadings[i] = proc.patch.needle (v.id);
        }

        // draw modulated knobs
        for (const auto& c : viewCables)
        {
            if (! c.toKnob) continue;

            for (auto& v : viewBlocks)
                if (v.id == c.to && c.toPort >= 0 && c.toPort < qd::kMaxKnobs)
                    v.knob[(std::size_t) c.toPort] = proc.patch.liveKnob (c.to, c.toPort);
        }
    }

    // frame new patch once
    if (framedProgram != proc.getCurrentProgram())
    {
        framedProgram = proc.getCurrentProgram();
        zoomToFit();
    }

    // tab rotates, short way
    for (const auto& b : viewBlocks)
    {
        if (qd::spec (b.kind).numKnobs <= 0) continue;

        bool wired = false;

        for (const auto& c : viewCables)
            if (c.toKnob && c.to == b.id) { wired = true; break; }

        const float target = tabRotation (b, wired);
        const auto it = tabAnim.find (b.id);

        if (it == tabAnim.end()) { tabAnim[b.id] = target; continue; }

        const float twoPi = juce::MathConstants<float>::twoPi;
        const float d = std::fmod (target - it->second + juce::MathConstants<float>::pi * 3.0f, twoPi)
                          - juce::MathConstants<float>::pi;

        it->second = std::abs (d) > 0.004f ? it->second + d * 0.35f : target;
    }

    updateTour();
    dimChildren();
    repaint();
}

// components sit above paint(): fade them with the overlay
void QuadratEditor::dimChildren()
{
    const bool veiled = welcoming || touring;
    const float searchAlpha = showAbout ? 0.06f : veiled ? 0.45f : 1.0f;
    const float capAlpha = showAbout ? 0.06f : 1.0f;

    search.setAlpha (searchAlpha);
    search.setInterceptsMouseClicks (! showAbout && ! welcoming, ! showAbout && ! welcoming);

    for (auto* cap : { &clearCap, &rollCap, &startCap, &stopCap })
    {
        cap->setAlpha (capAlpha);
        cap->setInterceptsMouseClicks (! showAbout, false);
    }
}

// ── geometry ─────────────────────────────────────────────────────────────────
float QuadratEditor::cellSize() const { return layout::cell * zoom * (getWidth() / layout::W); }

// snap grid = 1/4 square
float QuadratEditor::subSize() const { return cellSize() / (float) qd::kSub; }

juce::Rectangle<float> QuadratEditor::paletteArea() const
{
    const float s = getWidth() / layout::W;
    return { layout::M * s, layout::headerH * s, layout::paletteW * s,
             (layout::H - layout::headerH - layout::M) * s };
}

juce::Rectangle<float> QuadratEditor::fieldArea() const
{
    const float s = getWidth() / layout::W;
    const float x = (layout::M + layout::paletteW + 14.0f) * s;
    return { x, layout::headerH * s, getWidth() - x - layout::M * s,
             (layout::H - layout::headerH - layout::M) * s };
}

juce::Point<float> QuadratEditor::cellToScreen (int cx, int cy) const
{
    const auto f = fieldArea();
    const float c = subSize();
    return { f.getX() + cx * c - panX, f.getY() + cy * c - panY };
}

juce::Point<int> QuadratEditor::screenToCell (juce::Point<float> p) const
{
    const auto f = fieldArea();
    const float c = subSize();
    return { (int) std::floor ((p.x - f.getX() + panX) / c),
             (int) std::floor ((p.y - f.getY() + panY) / c) };
}

juce::Rectangle<float> QuadratEditor::tileRect (const qd::Block& b) const
{
    const auto o = cellToScreen (b.x, b.y);
    const float c = cellSize();
    return juce::Rectangle<float> (o.x, o.y, c * b.cells(), c * b.rows()).reduced (subSize() * 0.16f);
}

juce::Point<float> QuadratEditor::portPos (const qd::Block& b, bool isInput, int index) const
{
    const auto r = tileRect (b);
    const auto& sp = qd::spec (b.kind);
    const int count = isInput ? sp.numIn : sp.numOut;
    const float y = r.getY() + r.getHeight() * (index + 1.0f) / (count + 1.0f);
    return { isInput ? r.getX() : r.getRight(), y };
}

// 3-per-row control grid
juce::Rectangle<float> QuadratEditor::controlRect (const qd::Block& b, juce::Rectangle<float> r, int index) const
{
    const auto& sp = qd::spec (b.kind);
    const int n = sp.numKnobs;
    if (n <= 0 || index < 0 || index >= n) return {};

    const int perRow = juce::jlimit (1, n, sp.cells);
    const int rows = (n + perRow - 1) / perRow;

    const float cell = r.getHeight() / juce::jmax (1, sp.rows);
    const float titleH = cell * 0.25f;
    const auto strip = optionStrip (r, b);
    const float bottom = strip.isEmpty() ? cell * 0.09f : r.getBottom() - strip.getY() + cell * 0.05f;

    auto area = r.withTrimmedTop (titleH)
                 .withTrimmedBottom (bottom)
                 .reduced (cell * 0.09f, cell * 0.03f);

    // face first, controls below
    if (b.kind == qd::Kind::meter)
        area = area.withTrimmedTop (area.getHeight() * 0.62f);

    if (hasScope (b.kind))
        area = area.withTrimmedTop (area.getHeight()
                                      * (b.kind == qd::Kind::midi ? 0.66f
                                         : b.kind == qd::Kind::input ? 0.6f
                                         : b.kind == qd::Kind::clock || b.kind == qd::Kind::ball ? 0.56f : 0.42f));

    // faders left, knobs right
    if (b.kind == qd::Kind::seq)
        area = area.removeFromRight (area.getWidth() * 0.22f).reduced (0.0f, area.getHeight() * 0.12f);

    // cap row height
    const float maxRow = cell * 0.78f;
    const float ch = juce::jmin (maxRow, area.getHeight() / rows);
    const float top = area.getY() + (area.getHeight() - ch * rows) * 0.5f;

    const int row = index / perRow, col = index % perRow;
    const int inRow = juce::jmin (perRow, n - row * perRow);
    const float cw = area.getWidth() / inRow;

    return { area.getX() + cw * col, top + ch * row, cw, ch };
}

int QuadratEditor::controlAt (const qd::Block& b, juce::Rectangle<float> r, juce::Point<float> p) const
{
    const auto& sp = qd::spec (b.kind);

    for (int i = 0; i < sp.numKnobs; ++i)
        if (controlRect (b, r, i).contains (p)) return i;

    return -1;
}

// seq: 8 columns
juce::Rectangle<float> QuadratEditor::seqArea (juce::Rectangle<float> r, const qd::Block& b) const
{
    const auto& sp = qd::spec (b.kind);
    const float unit = r.getHeight() / juce::jmax (1, sp.rows);

    return r.withTrimmedTop (unit * 0.26f)
            .withTrimmedBottom (unit * 0.1f)
            .reduced (unit * 0.08f, unit * 0.03f)
            .withTrimmedRight (r.getWidth() * 0.24f);
}

juce::Rectangle<float> QuadratEditor::seqCell (juce::Rectangle<float> r, const qd::Block& b,
                                               int step, int lane) const
{
    const auto a = seqArea (r, b);
    if (a.isEmpty() || step < 0 || step >= qd::kSteps) return {};

    const float w = a.getWidth() / qd::kSteps;
    const auto col = juce::Rectangle<float> (a.getX() + w * step, a.getY(), w * 0.86f, a.getHeight());

    const float switchH = col.getHeight() * 0.17f;
    const float laneH = (col.getHeight() - switchH) * 0.5f;

    if (lane == 2) return col.withTop (col.getBottom() - switchH);

    return { col.getX(), col.getY() + laneH * lane, col.getWidth(), laneH * 0.94f };
}

int QuadratEditor::stepAt (const qd::Block& b, juce::Point<float> p, int& lane) const
{
    lane = -1;
    if (b.kind != qd::Kind::seq) return -1;

    const auto r = tileRect (b);

    for (int i = 0; i < qd::kSteps; ++i)
        for (int l = 0; l < 3; ++l)
            if (seqCell (r, b, i, l).contains (p)) { lane = l; return i; }

    return -1;
}

// drawer tab rect
juce::Rectangle<float> QuadratEditor::tabRect (juce::Rectangle<float> r, const qd::Block& b) const
{
    const auto& sp = qd::spec (b.kind);
    if (sp.numKnobs <= 0) return {};

    const float cell = r.getHeight() / juce::jmax (1, sp.rows);
    const float w = cell * 0.2f;

    // strip centre line
    const float stripH = cell * 0.22f, inset = cell * 0.07f;
    const float cy = r.getBottom() - inset * 0.6f - stripH * 0.5f;
    const float cx = r.getRight() - cell * 0.15f;

    return { cx - w * 0.5f, cy - w * 0.5f, w, w };
}

// left shut, down open
float QuadratEditor::tabRotation (const qd::Block& b, bool) const
{
    return b.drawer ? juce::MathConstants<float>::halfPi : juce::MathConstants<float>::pi;
}

juce::Rectangle<float> QuadratEditor::drawerTab (const qd::Block& b) const
{
    return tabRect (tileRect (b), b);
}

// drawer under the block
juce::Rectangle<float> QuadratEditor::drawerRect (const qd::Block& b) const
{
    const auto& sp = qd::spec (b.kind);
    if (! b.drawer || sp.numKnobs <= 0) return {};

    const auto r = tileRect (b);
    const float cell = r.getHeight() / juce::jmax (1, sp.rows);
    const int perRow = juce::jlimit (1, sp.numKnobs, sp.cells);
    const int rows = (sp.numKnobs + perRow - 1) / perRow;

    return { r.getX(), r.getBottom() + cell * 0.06f,
             r.getWidth(), cell * 0.30f * rows + cell * 0.16f };
}

juce::Rectangle<float> QuadratEditor::drawerCell (const qd::Block& b, int knob) const
{
    const auto d = drawerRect (b);
    const auto& sp = qd::spec (b.kind);
    if (d.isEmpty() || knob < 0 || knob >= sp.numKnobs) return {};

    const float cell = tileRect (b).getHeight() / juce::jmax (1, sp.rows);
    const int perRow = juce::jlimit (1, sp.numKnobs, sp.cells);
    const auto inner = d.reduced (cell * 0.06f, cell * 0.08f);

    const int row = knob / perRow, col = knob % perRow;
    const int inRow = juce::jmin (perRow, sp.numKnobs - row * perRow);
    const float cw = inner.getWidth() / inRow;

    return { inner.getX() + cw * col, inner.getY() + cell * 0.30f * row, cw, cell * 0.30f };
}

// jack centre + radius
juce::Point<float> QuadratEditor::jackAt (const qd::Block& b, int knob, float& radius) const
{
    const auto c = drawerCell (b, knob);
    radius = c.getHeight() * 0.26f;
    return { c.getX() + c.getHeight() * 0.46f, c.getCentreY() };
}

// jack, or tile corner
juce::Point<float> QuadratEditor::knobTarget (const qd::Block& b, int knob) const
{
    if (! drawerCell (b, knob).isEmpty())
    {
        float jr = 0.0f;
        return jackAt (b, knob, jr);
    }

    const auto r = tileRect (b);
    const auto& sp = qd::spec (b.kind);

    if (sp.numKnobs > 0)
    {
        const float cell = r.getHeight() / juce::jmax (1, sp.rows);
        return { r.getRight() - cell * 0.26f, r.getBottom() };
    }

    return controlRect (b, r, knob).getCentre();
}

juce::Point<float> QuadratEditor::dragOffsetFor (const qd::Block& b) const
{
    if (drag == Drag::moveBlock && b.id == dragBlock)
        return mousePos - dragOffset - tileRect (b).getPosition();

    return {};
}

int QuadratEditor::drawerJackAt (const qd::Block& b, juce::Point<float> p) const
{
    if (drawerRect (b).isEmpty()) return -1;

    for (int i = 0; i < qd::spec (b.kind).numKnobs; ++i)
        if (drawerCell (b, i).contains (p)) return i;

    return -1;
}

int QuadratEditor::cableToKnob (int blockId, int knob) const
{
    for (std::size_t i = 0; i < viewCables.size(); ++i)
        if (viewCables[i].toKnob && viewCables[i].to == blockId && viewCables[i].toPort == knob)
            return (int) i;

    return -1;
}

void QuadratEditor::setDrawer (int id, bool open)
{
    if (auto* b = proc.patch.find (id)) b->drawer = open;

    for (auto& v : viewBlocks)                 // open under the cursor
        if (v.id == id) v.drawer = open;
}

void QuadratEditor::drawDrawer (juce::Graphics& g, const qd::Block& b)
{
    auto d = drawerRect (b);
    if (d.isEmpty()) return;

    const auto& sp = qd::spec (b.kind);
    const auto shift = dragOffsetFor (b);
    d += shift;

    const float cell = tileRect (b).getHeight() / juce::jmax (1, sp.rows);
    const float rad = cell * 0.05f;

    g.setColour (juce::Colours::black.withAlpha (0.13f));
    g.fillRoundedRectangle (d.translated (0.0f, cell * 0.03f), rad);
    g.setColour (paint::plateHi);
    g.fillRoundedRectangle (d, rad);
    g.setColour (paint::engrave.withAlpha (0.7f));
    g.drawRoundedRectangle (d, rad, 1.0f);

    for (int i = 0; i < sp.numKnobs; ++i)
    {
        const auto c = drawerCell (b, i) + shift;
        if (c.isEmpty()) continue;

        float jr = 0.0f;
        const auto jack = jackAt (b, i, jr) + shift;
        const int wired = cableToKnob (b.id, i);

        auto col = colourFor (qd::Sig::cv);

        if (wired >= 0)
            if (const auto* src = viewBlock (viewCables[(std::size_t) wired].from))
                col = colourFor (qd::portKind (src->kind, false, viewCables[(std::size_t) wired].fromPort));

        // jack socket
        const auto ring = juce::Rectangle<float> (jack.x - jr, jack.y - jr, jr * 2.0f, jr * 2.0f);

        g.setColour (juce::Colours::black.withAlpha (0.18f));
        g.fillEllipse (ring.translated (0.0f, jr * 0.12f));
        g.setColour (paint::chrome);
        g.fillEllipse (ring);
        g.setColour (paint::chromeLo);
        g.fillEllipse (ring.reduced (jr * 0.16f).translated (0.0f, jr * 0.10f));
        g.setColour (paint::ink);
        g.drawEllipse (ring, jr * 0.22f);

        const auto throat = ring.reduced (jr * 0.46f);
        g.setColour (paint::screen);
        g.fillEllipse (throat);

        if (wired >= 0)
        {
            g.setColour (col);
            g.fillEllipse (throat.reduced (jr * 0.06f));
            g.setColour (paint::ink);
            g.drawEllipse (throat.reduced (jr * 0.06f), jr * 0.14f);
            g.setColour (juce::Colours::white.withAlpha (0.55f));
            g.fillEllipse (throat.getX() + throat.getWidth() * 0.18f,
                           throat.getY() + throat.getHeight() * 0.14f,
                           throat.getWidth() * 0.26f, throat.getHeight() * 0.26f);
        }
        else
        {
            g.setColour (juce::Colours::white.withAlpha (0.35f));
            g.fillEllipse (ring.getX() + ring.getWidth() * 0.22f,
                           ring.getY() + ring.getHeight() * 0.12f,
                           ring.getWidth() * 0.22f, ring.getHeight() * 0.16f);
        }

        // depth ring
        {
            const float rr = jr * 1.5f;
            const float lw = juce::jmax (1.4f, jr * 0.2f);
            const float from = juce::MathConstants<float>::pi * 1.25f;
            const float span = juce::MathConstants<float>::pi * 1.5f;
            const float d = juce::jlimit (0.0f, 1.0f, b.depth[(std::size_t) i]);

            juce::Path track;
            track.addCentredArc (jack.x, jack.y, rr, rr, 0.0f, from, from + span, true);
            g.setColour (paint::engrave.withAlpha (0.35f));
            g.strokePath (track, juce::PathStrokeType (lw, juce::PathStrokeType::curved,
                                                       juce::PathStrokeType::rounded));

            if (d > 0.001f)
            {
                juce::Path amount;
                amount.addCentredArc (jack.x, jack.y, rr, rr, 0.0f, from, from + span * d, true);
                g.setColour (wired >= 0 ? col : paint::inkSoft.withAlpha (0.55f));
                g.strokePath (amount, juce::PathStrokeType (lw, juce::PathStrokeType::curved,
                                                            juce::PathStrokeType::rounded));
            }
        }

        g.setColour (wired >= 0 ? paint::ink : paint::inkSoft);
        drawLabel (g, sp.knobs[i].name,
                     jack.x + jr * 1.95f, c.getCentreY() + c.getHeight() * 0.18f,
                     juce::jmax (6.0f, cell * 0.115f), 0.0f, juce::Justification::left,
                     c.getRight() - (jack.x + jr * 1.95f));
    }
}

juce::Rectangle<float> QuadratEditor::optionRect (const qd::Block& b) const
{
    return optionStrip (tileRect (b), b);
}

// all options inline
juce::Rectangle<float> QuadratEditor::optionStrip (juce::Rectangle<float> r, const qd::Block& b) const
{
    const auto& sp = qd::spec (b.kind);
    if (sp.options == nullptr || *sp.options == 0) return {};

    // timbre: display only
    if (b.kind == qd::Kind::timbre) return {};

    const float cell = r.getHeight() / juce::jmax (1, sp.rows);
    const float h = cell * 0.22f;
    const float inset = cell * 0.07f;

    // corner is the tab's
    const float corner = sp.numKnobs > 0 ? cell * 0.26f : inset;

    return r.withTrimmedLeft (inset).withTrimmedRight (corner)
            .removeFromBottom (h + inset * 0.6f).withTrimmedBottom (inset * 0.6f);
}

// filter as you type
void QuadratEditor::filterPalette()
{
    const auto term = search.getText().trim().toLowerCase();
    const auto& all = paletteEntries();

    visibleItems.clear();

    for (int i = 0; i < (int) all.size(); ++i)
    {
        const auto& e = all[(std::size_t) i];

        if (e.heading)
        {
            bool any = false;

            for (int k = i + 1; k < (int) all.size() && ! all[(std::size_t) k].heading; ++k)
                if (matches (all[(std::size_t) k], term))
                    any = true;

            if (any) visibleItems.push_back (i);
        }
        else if (matches (e, term))
        {
            visibleItems.push_back (i);
        }
    }
}

float QuadratEditor::paletteContentHeight() const
{
    const float s = getWidth() / layout::W;
    float h = 0.0f;

    for (const int i : visibleItems)
        h += paletteEntries()[(std::size_t) i].heading ? 26.0f * s : 76.0f * s;

    return h;
}

juce::Rectangle<float> QuadratEditor::scrollTrack() const
{
    const auto a = paletteArea();
    const float s = getWidth() / layout::W;
    return { a.getRight() - 5.0f * s, a.getY() + 40.0f * s, 4.0f * s, a.getHeight() - 44.0f * s };
}

juce::Rectangle<float> QuadratEditor::scrollThumb() const
{
    const auto t = scrollTrack();
    const float content = paletteContentHeight();

    if (content <= t.getHeight()) return {};

    const float frac = t.getHeight() / content;
    const float h = juce::jmax (18.0f, t.getHeight() * frac);
    const float y = t.getY() + (t.getHeight() - h) * (palScroll / juce::jmax (1.0f, content - t.getHeight()));
    return { t.getX(), y, t.getWidth(), h };
}

void QuadratEditor::scrollPalette (float delta)
{
    const float room = juce::jmax (0.0f, paletteContentHeight() - scrollTrack().getHeight());
    palScroll = juce::jlimit (0.0f, room, palScroll + delta);
    repaint();
}

juce::Rectangle<float> QuadratEditor::paletteItemRect (int index) const
{
    const auto a = paletteArea();
    const float s = getWidth() / layout::W;
    float y = a.getY() + 40.0f * s - palScroll;   // below search

    for (const int i : visibleItems)
    {
        const float h = paletteEntries()[(std::size_t) i].heading ? 26.0f * s : 76.0f * s;

        if (i == index) return { a.getX(), y, a.getWidth(), h };

        y += h;
    }

    return {};
}

int QuadratEditor::paletteIndexOf (qd::Kind k) const
{
    const auto& all = paletteEntries();

    for (int i = 0; i < (int) all.size(); ++i)
        if (! all[(std::size_t) i].heading && all[(std::size_t) i].kind == k) return i;

    return -1;
}

int QuadratEditor::paletteItemAt (juce::Point<float> p) const
{
    if (! paletteArea().contains (p)) return -1;

    for (const int i : visibleItems)
        if (! paletteEntries()[(std::size_t) i].heading && paletteItemRect (i).contains (p))
            return i;

    return -1;
}

// fader from cursor y
void QuadratEditor::setStep (qd::Block& b, int step, int lane, float y)
{
    if (step < 0 || step >= qd::kSteps || lane < 0 || lane > 1) return;

    const auto tile = tileRect (b);
    const auto cell = seqCell (tile, b, step, lane);
    if (cell.isEmpty()) return;

    const float unit = tile.getHeight() / juce::jmax (1, qd::spec (b.kind).rows);
    const float capH = juce::jmax (3.0f, cell.getHeight() * 0.17f);
    const float travel = juce::jmax (1.0f, cell.getHeight() - capH - unit * 0.04f);
    const float v = juce::jlimit (0.0f, 1.0f,
                                  (cell.getBottom() - unit * 0.02f - capH * 0.5f - y) / travel);

    (lane == 0 ? b.steps : b.steps2)[(std::size_t) step] = v;
}

// face reading
float QuadratEditor::faceReading (int id) const
{
    for (std::size_t i = 0; i < viewBlocks.size() && i < faceReadings.size(); ++i)
        if (viewBlocks[i].id == id) return faceReadings[i];

    return 0.0f;
}

const QuadratEditor::BallView& QuadratEditor::ballState (int id) const
{
    static const BallView none;
    const auto it = balls.find (id);
    return it != balls.end() ? it->second : none;
}

float QuadratEditor::clockPhase (int id) const
{
    for (std::size_t i = 0; i < viewBlocks.size() && i < viewPhase.size(); ++i)
        if (viewBlocks[i].id == id) return viewPhase[i];

    return 0.0f;
}

// fit patch on open
void QuadratEditor::zoomToFit()
{
    if (viewBlocks.empty()) return;

    int x0 = viewBlocks.front().x, y0 = viewBlocks.front().y, x1 = x0, y1 = y0;

    for (const auto& b : viewBlocks)
    {
        x0 = juce::jmin (x0, b.x);
        y0 = juce::jmin (y0, b.y);
        x1 = juce::jmax (x1, b.x + b.subW());
        y1 = juce::jmax (y1, b.y + b.subH() + (b.drawer ? qd::kSub : 0));
    }

    zoomToFitBounds (x0, y0, x1, y1);
}

// fit arbitrary bounds
void QuadratEditor::zoomToFitBounds (int x0, int y0, int x1, int y1)
{
    x1 = juce::jmax (x1, x0 + 1);
    y1 = juce::jmax (y1, y0 + 1);

    const auto f = fieldArea();
    const float sub = (float) (x1 - x0), subY = (float) (y1 - y0);
    const float unit = layout::cell / (float) qd::kSub * (getWidth() / layout::W);
    const float margin = 0.9f;

    zoom = juce::jlimit (0.3f, 1.6f, juce::jmin (f.getWidth() / (sub * unit),
                                                  f.getHeight() / (subY * unit)) * margin);

    // centre, capped margin
    const float px = subSize();
    const float slackX = juce::jmin (40.0f, (f.getWidth() - sub * px) * 0.5f);
    const float slackY = juce::jmin (40.0f, (f.getHeight() - subY * px) * 0.5f);

    panX = juce::jmax (-40.0f, x0 * px - juce::jmax (8.0f, slackX));
    panY = juce::jmax (-40.0f, y0 * px - juce::jmax (8.0f, slackY));
    repaint();
}

// notes zoom with field
juce::Rectangle<float> QuadratEditor::noteRect (const qd::Note& t) const
{
    const auto o = cellToScreen (t.x, t.y);
    const float h = subSize() * 1.1f;
    const juce::Font f { juce::FontOptions (h * 0.8f) };
    const auto text = juce::String::fromUTF8 (t.text.c_str());
    const float w = juce::jmax (h * 1.2f,
                                juce::GlyphArrangement::getStringWidth (f, text) + h * 0.5f);

    return { o.x, o.y, w, h };
}

int QuadratEditor::noteAt (juce::Point<float> p) const
{
    const auto& notes = viewNotes;

    for (int i = (int) notes.size() - 1; i >= 0; --i)
        if (noteRect (notes[(std::size_t) i]).contains (p)) return i;

    return -1;
}

void QuadratEditor::drawNotes (juce::Graphics& g)
{
    for (std::size_t i = 0; i < viewNotes.size(); ++i)
    {
        const auto& t = viewNotes[i];
        const auto r = noteRect (t);
        const bool editing = (int) i == editingNote;
        auto text = juce::String::fromUTF8 (t.text.c_str());

        if (editing)
        {
            g.setColour (paint::plateHi.withAlpha (0.75f));
            g.fillRect (r);
            g.setColour (paint::ink.withAlpha (0.5f));
            g.drawRect (r, 1.0f);

            // blinking caret
            if ((int) (juce::Time::getMillisecondCounterHiRes() / 500.0) % 2 == 0) text += "|";
        }

        g.setColour (paint::ink.withAlpha (editing ? 0.95f : 0.62f));
        drawLabel (g, text, r.getX() + r.getHeight() * 0.2f,
                     r.getBottom() - r.getHeight() * 0.24f, r.getHeight() * 0.72f,
                     r.getHeight() * 0.02f, juce::Justification::left, r.getWidth());
    }
}

// empty note = delete
void QuadratEditor::commitNote()
{
    if (editingNote < 0) return;

    const juce::ScopedLock sl (proc.patchLock);
    auto& notes = proc.patch.notes();

    if (editingNote < (int) notes.size() && notes[(std::size_t) editingNote].text.empty())
        notes.erase (notes.begin() + editingNote);

    editingNote = -1;
    repaint();
}

int QuadratEditor::playingStep (int id) const
{
    for (std::size_t i = 0; i < viewBlocks.size() && i < viewSteps.size(); ++i)
        if (viewBlocks[i].id == id) return viewSteps[i];

    return -1;
}

const qd::Block* QuadratEditor::viewBlock (int id) const
{
    for (const auto& b : viewBlocks) if (b.id == id) return &b;
    return nullptr;
}

const float* QuadratEditor::scopeFor (int id) const
{
    for (std::size_t i = 0; i < viewBlocks.size(); ++i)
        if (viewBlocks[i].id == id && i < viewScopes.size())
            return viewScopes[i].data();

    return nullptr;
}

// ── painting ─────────────────────────────────────────────────────────────────
void QuadratEditor::paint (juce::Graphics& g)
{
    const float s = getWidth() / layout::W;
    const auto u = [s] (float v) { return v * s; };

    g.setGradientFill (juce::ColourGradient (paint::plateHi, 0.0f, 0.0f,
                                             paint::plateLo, 0.0f, (float) getHeight(), false));
    g.fillAll();

    g.setColour (showAbout ? paint::sigAudio : paint::ink);
    drawLabel (g, "quadrat", u (layout::M), u (38), u (17), u (1.2f), juce::Justification::left);


    // no patch name
    if (notice.isNotEmpty())
    {
        g.setColour (paint::inkSoft);
        drawLabel (g, notice, presetsRect().getRight() + u (24), u (37), u (9.5f), 0.0f,
                     juce::Justification::left, u (420));
    }

    // presets menu: current patch
    {
        const auto pr = presetsRect();
        const bool hot = pr.contains (getMouseXYRelative().toFloat()) && ! touring;
        g.setColour (hot ? paint::ink : paint::inkSoft);
        drawLabel (g, headerTitle(), pr.getX(), u (37), u (10.5f), u (0.3f), juce::Justification::left, pr.getWidth());
        const float tx = pr.getRight() - u (7), ty = u (33);
        juce::Path tri;
        tri.addTriangle (tx, ty, tx + u (7), ty, tx + u (3.5f), ty + u (4.5f));
        g.fillPath (tri);
    }

    // palette shelf
    {
        // 10 px gaps: header, search, shelf
        const auto a = paletteArea();
        const juce::Rectangle<float> shelf (u (16), a.getY() + u (34),
                                            fieldArea().getX() - u (32), a.getHeight() - u (34));
        g.setColour (paint::shelf);
        g.fillRoundedRectangle (shelf, u (5));
        g.setColour (paint::engrave.withAlpha (0.35f));
        g.drawRoundedRectangle (shelf, u (5), 1.0f);
    }

    drawPalette (g);
    drawField (g);

    // a .mid hovering: outline where it lands
    if (fileHover.x >= 0.0f)
    {
        juce::Rectangle<float> target;
        const auto cell = screenToCell (fileHover);

        for (const auto& b : viewBlocks)
            if (b.kind == qd::Kind::midi && b.covers (cell.x, cell.y)) target = tileRect (b);

        if (target.isEmpty()) target = fieldArea().reduced (u (4));
        g.setColour (paint::sigPitch);
        g.drawRoundedRectangle (target, u (4), u (2.5f));
    }

    if (touring || welcoming) drawHints (g);

    // drag ghost, snapped
    if (drag == Drag::fromPalette)
    {
        const auto& sp = qd::spec (dragKind);
        const float c = cellSize();

        qd::Block sample;
        sample.kind = dragKind;

        for (int k = 0; k < qd::kMaxKnobs; ++k)
            sample.knob[(std::size_t) k] = sp.knobs[k].name ? sp.knobs[k].def : 0.5f;

        juce::Rectangle<float> ghost (mousePos.x - dragOffset.x, mousePos.y - dragOffset.y,
                                      c * sp.cells, c * sp.rows);
        const float sub = subSize();
        bool free = true;

        if (fieldArea().contains (mousePos))
        {
            const auto cellXY = snapToTarget (dragKind, screenToCell (ghost.getPosition()));
            const int gx = juce::jmax (0, cellXY.x), gy = juce::jmax (0, cellXY.y);
            const auto o = cellToScreen (gx, gy);
            ghost = juce::Rectangle<float> (o.x, o.y, c * sp.cells, c * sp.rows).reduced (sub * 0.16f);

            const juce::ScopedLock sl (proc.patchLock);
            free = proc.patch.isFree (gx, gy, sp.cells, sp.rows);
        }

        g.setOpacity (0.85f);
        drawTile (g, sample, ghost, 0.0f, false);
        g.setOpacity (1.0f);

        g.setColour (free ? paint::sigAudio : paint::sigGate);
        g.drawRect (ghost, juce::jmax (1.5f, c * 0.025f));
    }

    drawTooltip (g);
    if (touring || welcoming) drawCard (g);
    drawAbout (g);
}

juce::Rectangle<float> QuadratEditor::wordmarkRect() const
{
    const float s = getWidth() / layout::W;
    return { layout::M * s, 20.0f * s, 110.0f * s, 26.0f * s };
}

// credits (MIT/OFL notices)
void QuadratEditor::drawAbout (juce::Graphics& g)
{
    if (! showAbout) return;

    const float s = getWidth() / layout::W;
    const auto u = [s] (float v) { return v * s; };

    g.setColour (paint::plateLo.withAlpha (0.94f));
    g.fillAll();

    auto panel = aboutPanel();
    g.setColour (paint::plateHi);
    g.fillRoundedRectangle (panel, u (3));
    g.setColour (paint::inkSoft.withAlpha (0.4f));
    g.drawRoundedRectangle (panel, u (3), u (1));

    auto body = panel.reduced (u (26), u (22));

    static const char* lines[] =
    {
        "QUADRAT",
        "Quadrat is an open-source modular synthesizer, with a fun,",
        "easy-to-understand interface that enables people to try out modular",
        "synthesis without having to develop a deep technical understanding of",
        "the underlying mechanisms. Enjoy!",
        "Free software under the GNU GPL v3, with no warranty; see LICENSE.",
        "",
        "SOURCES",
        "macro, timbre, reson, mallet, xmod, scatter and drum are built on DSP",
        "code from the eurorack repository https://github.com/pichenettes/eurorack,",
        "by Emilie Gillet. Only code from STM32F projects is used, released",
        "under the MIT licence; see full text in third_party/mi/LICENSE-MIT.",
        "",
        "EVERYTHING ELSE",
        "The oscillators, filters, envelopes, sequencers, delay, reverb, the",
        "patch graph and this panel are written for Quadrat. Two files in",
        "the vendored library are third-party: a CRC table by Gary S. Brown,",
        "free of restriction, and MurmurHash3 by Austin Appleby, public domain.",
        "",
        "Built with JUCE. Type is Inter, by Rasmus Andersson, under the SIL",
        "Open Font License; the full text is in Resources/Inter-OFL.txt.",
        "",
        "Click anywhere to go back."
    };

    float y = body.getY() + u (14);

    for (const char* line : lines)
    {
        bool heading = line[0] != 0;

        for (const char* p = line; *p != 0; ++p)
            if (*p >= 'a' && *p <= 'z') { heading = false; break; }
        g.setColour (heading ? paint::sigAudio : paint::ink);
        drawLabel (g, line, body.getX(), y, u (heading ? 8.5f : 9.5f),
                     heading ? u (0.8f) : 0.0f, juce::Justification::left, body.getWidth());
        y += u (heading ? 19 : 15);
    }

    drawAboutLinks (g);
}

juce::Rectangle<float> QuadratEditor::aboutPanel() const
{
    const float s = getWidth() / layout::W;
    return { (layout::W * 0.5f - 250.0f) * s, 76.0f * s, 500.0f * s, 500.0f * s };
}

void QuadratEditor::drawPalette (juce::Graphics& g)
{
    const float s = getWidth() / layout::W;
    const auto& list = paletteEntries();

    juce::Graphics::ScopedSaveState save (g);
    const auto clip = paletteArea().withTrimmedTop (38.0f * s);
    g.reduceClipRegion (clip.toNearestInt());

    for (const int i : visibleItems)
    {
        const auto r = paletteItemRect (i);
        const auto& e = list[(std::size_t) i];

        if (e.heading)
        {
            g.setColour (paint::inkSoft);
            drawLabel (g, e.label, r.getX(), r.getBottom() - 4.0f * s, 8.0f * s, 1.4f * s,
                         juce::Justification::left, r.getWidth());
            continue;
        }

        // tile at true size
        qd::Block sample;
        sample.kind = e.kind;
        const auto& sp = qd::spec (e.kind);

        for (int kx = 0; kx < qd::kMaxKnobs; ++kx)
            sample.knob[(std::size_t) kx] = sp.knobs[kx].name ? sp.knobs[kx].def : 0.5f;

        // scale to column
        // name above tile
        const float nameH = 13.0f * s;
        const float gap = 6.0f * s;
        const float avail = r.getWidth() - 10.0f * s;
        const float unit = juce::jmin ((r.getHeight() - nameH - gap) / juce::jmax (1, sp.rows),
                                       avail / 4.0f);

        g.setColour (paint::ink);
        drawLabel (g, sp.name, r.getX() + 4.0f * s, r.getY() + nameH * 0.85f,
                     10.5f * s, 0.4f * s, juce::Justification::left, r.getWidth());

        const auto tile = juce::Rectangle<float> (r.getX() + 4.0f * s, r.getY() + nameH + gap * 0.3f,
                                                  unit * sp.cells, unit * sp.rows);

        drawTile (g, sample, tile, 0.0f, false);
    }

    const auto thumb = scrollThumb();

    if (! thumb.isEmpty())
    {
        g.setColour (paint::engrave.withAlpha (0.35f));
        g.fillRoundedRectangle (scrollTrack(), scrollTrack().getWidth() * 0.35f);
        g.setColour (paint::inkSoft);
        g.fillRoundedRectangle (thumb, thumb.getWidth() * 0.35f);
    }
}

// tooltip, on top
void QuadratEditor::drawTooltip (juce::Graphics& g)
{
    if (hoverKind < 0) return;

    const juce::String text (qd::describe ((qd::Kind) hoverKind));
    if (text.isEmpty()) return;

    const float s = getWidth() / layout::W;
    const float w = 250.0f * s, pad = 9.0f * s;
    const juce::Font f (juce::FontOptions (10.5f * s));

    juce::GlyphArrangement ga;
    ga.addJustifiedText (f, text, 0.0f, 0.0f, w - pad * 2.0f, juce::Justification::left);
    const float h = ga.getBoundingBox (0, -1, true).getHeight() + pad * 2.0f;

    float x = hoverAt.x + 14.0f * s, y = hoverAt.y + 16.0f * s;
    if (x + w > getWidth()) x = getWidth() - w - 4.0f * s;
    if (y + h > getHeight()) y = hoverAt.y - h - 8.0f * s;

    const juce::Rectangle<float> box (x, y, w, h);

    g.setColour (juce::Colours::black.withAlpha (0.16f));
    g.fillRect (box.translated (0.0f, 2.0f * s));
    g.setColour (paint::capTop);
    g.fillRect (box);
    g.setColour (paint::engrave);
    g.drawRect (box, 1.0f);

    g.setColour (paint::ink);
    juce::GlyphArrangement out;
    out.addJustifiedText (f, text, box.getX() + pad, box.getY() + pad + 9.0f * s,
                          w - pad * 2.0f, juce::Justification::left);
    out.draw (g);
}

void QuadratEditor::drawField (juce::Graphics& g)
{
    const auto f = fieldArea();

    g.setColour (paint::field);
    g.fillRect (f);
    g.setColour (paint::engrave);
    g.drawRect (f, 1.0f);

    juce::Graphics::ScopedSaveState save (g);
    g.reduceClipRegion (f.toNearestInt());

    // grid + subgrid
    const float c = cellSize(), sub = subSize();

    if (sub > 6.0f)
    {
        g.setColour (paint::grid.withAlpha (0.45f));

        for (float x = f.getX() - std::fmod (panX, sub); x < f.getRight(); x += sub)
            g.fillRect (x, f.getY(), 1.0f, f.getHeight());

        for (float y = f.getY() - std::fmod (panY, sub); y < f.getBottom(); y += sub)
            g.fillRect (f.getX(), y, f.getWidth(), 1.0f);
    }

    g.setColour (paint::grid);

    for (float x = f.getX() - std::fmod (panX, c); x < f.getRight(); x += c)
        g.fillRect (x, f.getY(), 1.0f, f.getHeight());

    for (float y = f.getY() - std::fmod (panY, c); y < f.getBottom(); y += c)
        g.fillRect (f.getX(), y, f.getWidth(), 1.0f);

    drawNotes (g);

    for (std::size_t i = 0; i < viewBlocks.size(); ++i)
        if (viewBlocks[i].id != dragBlock || drag != Drag::moveBlock)
            drawTile (g, viewBlocks[i], tileRect (viewBlocks[i]),
                      i < viewMeters.size() ? viewMeters[i] : 0.0f, isSelected (viewBlocks[i].id));

    for (const auto& b : viewBlocks)
        drawDrawer (g, b);

    drawCables (g);

    // dragged block on top
    if (drag == Drag::moveBlock)
        if (const auto* b = viewBlock (dragBlock))
            drawTile (g, *b, tileRect (*b).withPosition (mousePos - dragOffset), 0.0f, true);

    if (drag == Drag::box)
    {
        const juce::Rectangle<float> box (juce::Point<float> (juce::jmin (boxFrom.x, mousePos.x),
                                                              juce::jmin (boxFrom.y, mousePos.y)),
                                          juce::Point<float> (juce::jmax (boxFrom.x, mousePos.x),
                                                              juce::jmax (boxFrom.y, mousePos.y)));
        g.setColour (paint::ink.withAlpha (0.08f));
        g.fillRect (box);
        g.setColour (paint::ink.withAlpha (0.5f));
        g.drawRect (box, 1.0f);
    }

    if (drag == Drag::cable && cableFrom >= 0)
    {
        if (const auto* b = viewBlock (cableFrom))
        {
            const auto from = portPos (*b, false, cableFromPort);
            juce::Path p;
            p.startNewSubPath (from);
            p.cubicTo (from.x + 40.0f, from.y, mousePos.x - 40.0f, mousePos.y, mousePos.x, mousePos.y);
            g.setColour (colourFor (qd::portKind (b->kind, false, cableFromPort)));
            g.strokePath (p, juce::PathStrokeType (2.2f));
        }
    }
}

bool QuadratEditor::hasScope (qd::Kind k)
{
    return k == qd::Kind::osc || k == qd::Kind::timbre || k == qd::Kind::ball
        || k == qd::Kind::macro || k == qd::Kind::noise
        || k == qd::Kind::comp || k == qd::Kind::limit
        || k == qd::Kind::clock || k == qd::Kind::filter || k == qd::Kind::midi
        || k == qd::Kind::input;
}

// clock face, ticking hand
void QuadratEditor::drawClockFace (juce::Graphics& g, juce::Rectangle<float> r,
                                   float phase01, float width01, int marks, float tempo)
{
    g.setColour (paint::screen);
    g.fillRect (r);

    const float rad = juce::jmin (r.getWidth(), r.getHeight()) * 0.46f;
    const auto c = r.getCentre();
    const float turn = juce::MathConstants<float>::twoPi;

    marks = juce::jlimit (1, 96, marks);
    const int tick = juce::jlimit (0, marks - 1, (int) (juce::jlimit (0.0f, 1.0f, phase01) * marks));
    const float step = 1.0f / (float) marks;
    const bool dense = marks > 24;

    // pulse arc
    juce::Path wedge;
    wedge.startNewSubPath (c);
    wedge.addPieSegment (c.x - rad, c.y - rad, rad * 2.0f, rad * 2.0f,
                         turn * tick * step,
                         turn * (tick + juce::jlimit (0.05f, 1.0f, width01)) * step, 0.0f);
    g.setColour (paint::sigGate.withAlpha (0.3f));
    g.fillPath (wedge);

    g.setColour (paint::screenLn);
    g.drawEllipse (c.x - rad, c.y - rad, rad * 2.0f, rad * 2.0f, 1.0f);

    // beat ticks longer
    const int perBeat = marks % 4 == 0 ? marks / 4 : marks;

    for (int i = 0; i < marks; ++i)
    {
        const float a = turn * i * step;
        const bool beat = (i % juce::jmax (1, perBeat)) == 0;
        const float inner = rad * (beat ? 0.7f : (dense ? 0.88f : 0.82f));

        g.setColour (beat ? paint::screenLn.brighter (0.8f)
                          : paint::screenLn.brighter (dense ? 0.1f : 0.35f));
        g.drawLine (c.x + std::sin (a) * inner, c.y - std::cos (a) * inner,
                    c.x + std::sin (a) * rad * 0.97f, c.y - std::cos (a) * rad * 0.97f,
                    beat ? 1.4f : 1.0f);
    }

    if (tempo > 0.0f)
    {
        g.setColour (paint::screenLn.brighter (0.6f));
        drawLabel (g, juce::String (juce::roundToInt (tempo)), r.getX(),
                     r.getBottom() - r.getHeight() * 0.06f,
                     juce::jmax (6.0f, r.getHeight() * 0.13f), 0.0f,
                     juce::Justification::centred, r.getWidth());
    }

    const float a = turn * tick * step;
    const bool high = (phase01 - tick * step) < width01 * step;

    g.setColour (high ? paint::sigGate : paint::sigGate.withAlpha (0.55f));
    g.drawLine (c.x, c.y, c.x + std::sin (a) * rad * 0.8f, c.y - std::cos (a) * rad * 0.8f,
                juce::jmax (1.5f, rad * 0.08f));
    g.fillEllipse (c.x - rad * 0.1f, c.y - rad * 0.1f, rad * 0.2f, rad * 0.2f);
}

// filter response curve
void QuadratEditor::drawResponse (juce::Graphics& g, juce::Rectangle<float> r, const qd::Block& b)
{
    g.setColour (paint::screen);
    g.fillRect (r);

    const float cutoff = juce::jlimit (0.0f, 1.0f, b.knob[0]);
    const float res = juce::jlimit (0.0f, 1.0f, b.knob[1]);
    const int type = juce::jlimit (0, 5, b.option);
    const float q = 0.5f + res * 9.0f;

    g.setColour (paint::screenLn);

    for (int i = 1; i < 5; ++i)
        g.drawLine (r.getX() + r.getWidth() * i * 0.2f, r.getY(),
                    r.getX() + r.getWidth() * i * 0.2f, r.getBottom(), 1.0f);

    g.drawLine (r.getX(), r.getCentreY(), r.getRight(), r.getCentreY(), 1.0f);

    juce::Path p;

    for (int i = 0; i <= 96; ++i)
    {
        const float t = i / 96.0f;
        const float w = std::pow (2.0f, (t - cutoff) * 10.0f);      // octaves from cutoff
        float mag;

        switch (type)
        {
            case 2:  mag = 1.0f / std::sqrt (1.0f + w * w * w * w); break;              // svf lp, 2-pole
            case 3:  mag = (w / q) / std::sqrt (std::pow (1.0f - w * w, 2.0f)
                                                + std::pow (w / q, 2.0f)); break;       // band
            case 4:  mag = w * w / std::sqrt (std::pow (1.0f - w * w, 2.0f)
                                              + std::pow (w / q, 2.0f)); break;         // high
            case 5:  mag = 1.0f / std::sqrt (std::pow (1.0f - w * w, 2.0f)
                                             + std::pow (w / q, 2.0f)); break;          // sallen
            default: mag = 1.0f / std::sqrt (1.0f + std::pow (w, 8.0f));                // ladder, 4-pole
                     mag *= 1.0f + res * 6.0f / (1.0f + std::pow ((w - 1.0f) * 8.0f, 2.0f));
                     break;
        }

        if (type == 2 || type == 3 || type == 4)
            mag *= 1.0f + res * 5.0f / (1.0f + std::pow ((w - 1.0f) * 8.0f, 2.0f));

        // +12 to -36 dB
        const float db = 20.0f * std::log10 (juce::jmax (1.0e-4f, mag));
        const float y = r.getY() + r.getHeight() * juce::jlimit (0.0f, 1.0f, (12.0f - db) / 48.0f);
        const float x = r.getX() + r.getWidth() * t;

        if (i == 0) p.startNewSubPath (x, y); else p.lineTo (x, y);
    }

    g.setColour (paint::sigAudio);
    g.strokePath (p, juce::PathStrokeType (1.4f));

    const float cx = r.getX() + r.getWidth() * cutoff;
    g.setColour (paint::sigCv.withAlpha (0.55f));
    g.drawLine (cx, r.getY(), cx, r.getBottom(), 1.0f);
}

// ball arena
void QuadratEditor::drawArena (juce::Graphics& g, juce::Rectangle<float> r, const qd::Block& b)
{
    g.setColour (paint::screen);
    g.fillRect (r);

    const float side = juce::jmin (r.getWidth(), r.getHeight()) - 2.0f;
    const auto box = juce::Rectangle<float> (r.getCentreX() - side * 0.5f,
                                             r.getCentreY() - side * 0.5f, side, side);

    const auto st = ballState (b.id);
    const float rad = juce::jmax (1.5f, side * 0.07f);

    auto place = [&box, rad] (float x, float y)
    {
        return juce::Point<float> (box.getX() + rad + (x * 0.5f + 0.5f) * (box.getWidth() - rad * 2.0f),
                                   box.getBottom() - rad - (y * 0.5f + 0.5f) * (box.getHeight() - rad * 2.0f));
    };

    g.setColour (paint::screenLn);
    g.drawRect (box, 1.0f);

    // lit on impact
    const float lit = juce::jlimit (0.0f, 1.0f, st.floorLit);
    g.setColour (paint::screenLn.brighter (0.3f).interpolatedWith (paint::sigGate, lit));
    g.fillRect (box.getX(), box.getBottom() - 1.5f, box.getWidth(), 1.5f);

    const float wlit = juce::jlimit (0.0f, 1.0f, st.wallLit);
    g.setColour (paint::screenLn.interpolatedWith (paint::sigGate, wlit));
    g.fillRect (box.getX(), box.getY(), 1.5f, box.getHeight());
    g.fillRect (box.getRight() - 1.5f, box.getY(), 1.5f, box.getHeight());

    for (int i = 0; i < (int) st.trail.size(); ++i)     // trail
    {
        const auto p = place (st.trail[(std::size_t) i].x, st.trail[(std::size_t) i].y);
        const float a = 0.06f + 0.18f * i / (float) juce::jmax (1, (int) st.trail.size());
        g.setColour (paint::sigCv.withAlpha (a));
        g.fillEllipse (p.x - rad * 0.5f, p.y - rad * 0.5f, rad, rad);
    }

    // squash on contact
    const auto p = place (st.x, st.y);
    const float squash = juce::jlimit (0.0f, 1.0f, st.floorLit) * 0.45f;

    g.setColour (paint::sigCv);
    g.fillEllipse (p.x - rad * (1.0f + squash), p.y - rad * (1.0f - squash * 0.8f),
                   rad * 2.0f * (1.0f + squash), rad * 2.0f * (1.0f - squash * 0.8f));
}

// model name readout
void QuadratEditor::drawModelName (juce::Graphics& g, juce::Rectangle<float> r, const qd::Block& b)
{
    const auto& sp = qd::spec (b.kind);
    const int count = juce::jmax (1, qd::optionCount (sp.options));
    const int idx = juce::jlimit (0, count - 1,
                                  (int) std::lround (juce::jlimit (0.0f, 1.0f, b.knob[0]) * (count - 1)));

    g.setColour (paint::screen);
    g.fillRect (r);

    // model ticks
    const float lane = r.getHeight() * 0.16f;

    for (int i = 0; i < count; ++i)
    {
        const float x = r.getX() + r.getWidth() * (0.04f + 0.92f * i / (float) (count - 1));
        g.setColour (i == idx ? paint::sigAudio : paint::screenLn.brighter (0.25f));
        g.fillRect (x, r.getBottom() - lane, juce::jmax (1.0f, r.getWidth() * 0.004f),
                    lane * (i == idx ? 0.9f : 0.5f));
    }

    // centred in box
    g.setColour (paint::sigAudio);
    drawLabel (g, qd::optionName (sp.options, idx), r.getX(),
                 r.getCentreY() + r.getHeight() * 0.12f,
                 juce::jmax (7.0f, r.getHeight() * 0.4f), r.getHeight() * 0.03f,
                 juce::Justification::centred, r.getWidth());

    g.setColour (paint::screenLn.brighter (0.4f));
    drawLabel (g, juce::String (idx + 1) + "/" + juce::String (count),
                 r.getX() + r.getWidth() * 0.03f, r.getY() + r.getHeight() * 0.26f,
                 juce::jmax (6.0f, r.getHeight() * 0.2f), 0.0f, juce::Justification::left,
                 r.getWidth() * 0.3f);
}

// VU meter
void QuadratEditor::drawMeterFace (juce::Graphics& g, juce::Rectangle<float> r, float value)
{
    g.setColour (paint::capTop);
    g.fillRect (r);
    g.setColour (paint::engrave.withAlpha (0.6f));
    g.drawRect (r, 1.0f);

    juce::Graphics::ScopedSaveState keep (g);
    g.reduceClipRegion (r.toNearestInt());

    const juce::Point<float> pivot (r.getCentreX(), r.getBottom() + r.getHeight() * 0.42f);
    const float rad = r.getHeight() * 1.15f;
    const float span = 0.62f;                     // ± radians

    for (int i = 0; i <= 10; ++i)
    {
        const float t = i / 10.0f;
        const float a = (t * 2.0f - 1.0f) * span;
        const float inner = rad * (i % 5 == 0 ? 0.84f : 0.9f);

        g.setColour (t > 0.8f ? paint::red : paint::ink.withAlpha (i % 5 == 0 ? 0.8f : 0.45f));
        g.drawLine (pivot.x + std::sin (a) * inner, pivot.y - std::cos (a) * inner,
                    pivot.x + std::sin (a) * rad * 0.96f, pivot.y - std::cos (a) * rad * 0.96f,
                    i % 5 == 0 ? 1.4f : 0.9f);
    }

    // clip needle to face
    const float a = (juce::jlimit (0.0f, 1.0f, value) * 2.0f - 1.0f) * span;
    g.setColour (paint::ink);
    g.drawLine (pivot.x + std::sin (a) * rad * 0.5f, pivot.y - std::cos (a) * rad * 0.5f,
                pivot.x + std::sin (a) * rad * 0.9f, pivot.y - std::cos (a) * rad * 0.9f,
                juce::jmax (1.2f, r.getHeight() * 0.04f));
}

// GR trace, hangs down
void QuadratEditor::drawReduction (juce::Graphics& g, juce::Rectangle<float> r,
                                   const float* data, float now)
{
    g.setColour (paint::screen);
    g.fillRect (r);

    const auto lane = r.withTrimmedRight (r.getWidth() * 0.14f);
    const auto strip = r.withTrimmedLeft (r.getWidth() * 0.88f).reduced (r.getWidth() * 0.012f,
                                                                        r.getHeight() * 0.06f);

    g.setColour (paint::screenLn);

    for (int i = 1; i < 4; ++i)              // quarter ticks
        g.fillRect (lane.getX(), lane.getY() + lane.getHeight() * i * 0.25f, lane.getWidth(), 1.0f);

    if (data != nullptr)
    {
        juce::Path p;
        p.startNewSubPath (lane.getX(), lane.getY());
        float deepest = 0.0f;

        for (int i = 0; i < qd::Patch::kScope; ++i)
        {
            const float v = juce::jlimit (0.0f, 1.0f, data[i]);
            const float x = lane.getX() + lane.getWidth() * i / (float) (qd::Patch::kScope - 1);
            deepest = juce::jmax (deepest, v);
            p.lineTo (x, lane.getY() + lane.getHeight() * v);
        }

        p.lineTo (lane.getRight(), lane.getY());
        p.closeSubPath();

        // idle = faint
        g.setColour (paint::sigGate.withAlpha (0.32f * deepest));
        g.fillPath (p);
        g.setColour (paint::sigGate.withAlpha (0.18f + 0.8f * deepest));
        g.strokePath (p, juce::PathStrokeType (1.2f));
    }

    g.setColour (paint::screenLn);
    g.fillRect (strip);
    g.setColour (paint::sigGate);
    g.fillRect (strip.withHeight (strip.getHeight() * juce::jlimit (0.0f, 1.0f, now)));
}

// osc scope
void QuadratEditor::drawScope (juce::Graphics& g, juce::Rectangle<float> r,
                               const float* data, juce::Colour col)
{
    g.setColour (paint::screen);
    g.fillRect (r);
    g.setColour (paint::screenLn);
    g.fillRect (r.getX(), r.getCentreY(), r.getWidth(), 1.0f);

    if (data == nullptr) return;

    juce::Path wave;
    const float half = r.getHeight() * 0.42f;

    for (int i = 0; i < qd::Patch::kScope; ++i)
    {
        const float x = r.getX() + r.getWidth() * i / (float) (qd::Patch::kScope - 1);
        const float y = r.getCentreY() - juce::jlimit (-1.2f, 1.2f, data[i]) * half;

        if (i == 0) wave.startNewSubPath (x, y);
        else        wave.lineTo (x, y);
    }

    g.setColour (col);
    g.strokePath (wave, juce::PathStrokeType (juce::jmax (1.0f, r.getHeight() * 0.05f)));
}

juce::Colour QuadratEditor::colourFor (qd::Sig s)
{
    switch (s)
    {
        case qd::Sig::pitch: return paint::sigPitch;
        case qd::Sig::gate:  return paint::sigGate;
        case qd::Sig::cv:    return paint::sigCv;
        default:             return paint::sigAudio;
    }
}

void QuadratEditor::drawOptionStrip (juce::Graphics& g, juce::Rectangle<float> strip, const qd::Block& b)
{
    const auto& sp = qd::spec (b.kind);
    const int count = juce::jmax (1, qd::optionCount (sp.options));
    float fs = strip.getHeight() * 0.55f;

    if (count <= 6)
    {
        const float w = strip.getWidth() / count;

        // shrink to fit
        for (int guard = 0; guard < 8 && fs > 1.0f; ++guard)
        {
            const juce::Font f { juce::FontOptions (fs) };
            float widest = 0.0f;

            for (int i = 0; i < count; ++i)
                widest = juce::jmax (widest, juce::GlyphArrangement::getStringWidth (
                                                 f, juce::String::fromUTF8 (
                                                        qd::optionName (sp.options, i).c_str())));

            const float room = w - strip.getHeight() * 0.3f;
            if (widest <= room) break;
            fs *= juce::jmax (0.7f, room / juce::jmax (1.0f, widest));
        }

        for (int i = 0; i < count; ++i)
        {
            const auto box = juce::Rectangle<float> (strip.getX() + i * w, strip.getY(),
                                                     w - 1.5f, strip.getHeight());
            const bool on = (i == b.option);

            g.setColour (on ? paint::ink : paint::field);
            g.fillRect (box);
            g.setColour (paint::engrave);
            g.drawRect (box, strip.getHeight() * 0.05f);
            g.setColour (on ? paint::plate : paint::inkSoft);
            drawLabel (g, qd::optionName (sp.options, i), box.getX(),
                         box.getCentreY() + fs * 0.36f, fs, 0.0f,
                         juce::Justification::centred, box.getWidth());
        }

        return;
    }

    // >6: dropdown
    g.setColour (paint::field);
    g.fillRect (strip);
    g.setColour (paint::engrave);
    g.drawRect (strip, 0.8f);
    g.setColour (paint::ink);
    drawLabel (g, qd::optionName (sp.options, b.option), strip.getX() + strip.getHeight() * 0.4f,
                 strip.getCentreY() + fs * 0.36f, fs, 0.0f,
                 juce::Justification::left, strip.getWidth() - 18.0f);

    juce::Path tri;
    const float cx = strip.getRight() - strip.getHeight() * 0.45f, cy = strip.getCentreY();
    const float ts = strip.getHeight() * 0.18f;
    tri.addTriangle (cx - ts, cy - ts * 0.6f, cx + ts, cy - ts * 0.6f, cx, cy + ts * 0.7f);
    g.setColour (paint::inkSoft);
    g.fillPath (tri);
}

void QuadratEditor::drawTile (juce::Graphics& g, const qd::Block& b,
                              juce::Rectangle<float> r, float meterValue, bool selected)
{
    const auto& sp = qd::spec (b.kind);
    // scale off one cell
    const float unit = r.getHeight() / juce::jmax (1, sp.rows);

    g.setColour (juce::Colours::black.withAlpha (0.12f));
    g.fillRect (r.translated (0.0f, unit * 0.03f));
    g.setColour (selected ? paint::tile.brighter (0.06f) : paint::tile);
    g.fillRect (r);
    g.setColour (selected ? paint::ink : paint::tileEdge);
    g.drawRect (r, unit * (selected ? 0.022f : 0.013f));

    g.setColour (paint::ink);
    drawLabel (g, sp.name, r.getX() + unit * 0.09f, r.getY() + unit * 0.21f,
                 unit * 0.15f, 0.0f, juce::Justification::left, r.getWidth());

    if (blockPresets::count (b.kind) > 0)
    {
        // name -> presets
        const juce::Font f { juce::FontOptions (unit * 0.15f) };
        const float w = juce::GlyphArrangement::getStringWidth (f, sp.name);
        juce::Path tri;
        const float cx = r.getX() + unit * 0.09f + w + unit * 0.08f;
        const float cy = r.getY() + unit * 0.17f;
        const float ts = unit * 0.035f;
        tri.addTriangle (cx - ts, cy - ts * 0.7f, cx + ts, cy - ts * 0.7f, cx, cy + ts * 0.8f);
        g.setColour (paint::inkSoft);
        g.fillPath (tri);
    }

    if (const auto strip = optionStrip (r, b); ! strip.isEmpty())
        drawOptionStrip (g, strip, b);

    // drawer tab
    if (const auto tab = tabRect (r, b); ! tab.isEmpty())
    {
        int wired = 0;

        for (const auto& c : viewCables)
            if (c.toKnob && c.to == b.id) ++wired;

        const auto col = paint::inkSoft.withAlpha (wired > 0 ? 0.85f : 0.55f);
        const float ts = tab.getWidth() * (wired > 0 ? 0.34f : 0.26f);
        const auto c0 = tab.getCentre();

        // tab rotation
        float theta = tabRotation (b, wired > 0);

        if (const auto it = tabAnim.find (b.id); it != tabAnim.end()) theta = it->second;

        const juce::Point<float> dir { std::cos (theta), std::sin (theta) };
        const juce::Point<float> perp { -dir.y, dir.x };

        juce::Path tri;
        tri.addTriangle (c0 + dir * ts,
                         c0 - dir * (ts * 0.55f) + perp * (ts * 0.85f),
                         c0 - dir * (ts * 0.55f) - perp * (ts * 0.85f));

        g.setColour (col);
        g.fillPath (tri);
    }

    if (hasScope (b.kind))
    {
        const auto face = faceArea (r, b);

        if (b.kind == qd::Kind::comp || b.kind == qd::Kind::limit)
            drawReduction (g, face, scopeFor (b.id), faceReading (b.id));
        else if (b.kind == qd::Kind::clock)
            drawClockFace (g, face, clockPhase (b.id), b.knob[0], qd::clockDivisions (b.option),
                           (float) proc.tapTempo());
        else if (b.kind == qd::Kind::filter)
            drawResponse (g, face, b);
        else if (b.kind == qd::Kind::timbre)
            drawModelName (g, face, b);
        else if (b.kind == qd::Kind::ball)
            drawArena (g, face, b);
        else if (b.kind == qd::Kind::midi)
            drawPhrase (g, face, b);
        else if (b.kind == qd::Kind::input)
            drawInput (g, face, b, meterValue);
        else
            drawScope (g, face, scopeFor (b.id), colourFor (qd::portKind (b.kind, false, 0)));
    }

    for (int i = 0; i < sp.numKnobs; ++i)
    {
        const auto cell = controlRect (b, r, i);
        if (cell.isEmpty()) continue;

        // tint = mod source
        juce::Colour mark = paint::index;
        bool moved = false;

        for (const auto& cbl : viewCables)
        {
            if (! cbl.toKnob || cbl.to != b.id || cbl.toPort != i) continue;

            if (const auto* src = viewBlock (cbl.from); src != nullptr && b.depth[(std::size_t) i] > 0.001f)
            {
                mark = colourFor (qd::portKind (src->kind, false, cbl.fromPort));
                moved = true;
            }
        }

        const auto& cs = sp.knobs[i];
        const float v = juce::jlimit (0.0f, 1.0f, b.knob[(std::size_t) i]);
        const float labelH = cell.getHeight() * 0.3f;
        const auto face = cell.withTrimmedBottom (labelH);

        switch (cs.type)
        {
            case qd::Ctl::slider:
            {
                const float y = face.getCentreY();
                const float x0 = face.getX() + unit * 0.03f, x1 = face.getRight() - unit * 0.03f;
                engraveLine (g, x0, y, x1 - x0, unit * 0.014f);
                const float hx = x0 + (x1 - x0) * v;
                const float hw = unit * 0.035f, hh = unit * 0.16f;
                g.setColour (moved ? mark.darker (0.35f) : paint::chromeLo);
                g.fillRect (hx - hw, y - hh * 0.5f, hw * 2.0f, hh);
                g.setColour (moved ? mark : paint::chrome);
                g.fillRect (hx - hw, y - hh * 0.5f, hw * 2.0f, hh * 0.55f);
                break;
            }

            case qd::Ctl::toggle:
            {
                const float d = juce::jmin (face.getHeight(), face.getWidth()) * 0.62f;
                const juce::Rectangle<float> box (face.getCentreX() - d * 0.5f,
                                                  face.getCentreY() - d * 0.5f, d, d);
                const bool on = v > 0.5f;
                g.setColour (on ? paint::red : paint::capTop);
                g.fillRect (box);
                g.setColour (moved ? mark : paint::engrave);
                g.drawRect (box, moved ? juce::jmax (1.5f, unit * 0.02f) : 1.0f);
                break;
            }

            case qd::Ctl::select:
            {
                const auto box = face.reduced (face.getWidth() * 0.08f, face.getHeight() * 0.2f);
                g.setColour (paint::field);
                g.fillRect (box);
                g.setColour (moved ? mark : paint::engrave);
                g.drawRect (box, moved ? juce::jmax (1.5f, unit * 0.02f) : 1.0f);
                const int count = juce::jmax (1, qd::optionCount (cs.opts));
                const int idx = juce::jlimit (0, count - 1, (int) std::round (v * (count - 1)));
                g.setColour (paint::ink);
                drawLabel (g, qd::optionName (cs.opts, idx), box.getX(), box.getCentreY() + unit * 0.045f,
                             juce::jmax (6.0f, unit * 0.12f), 0.0f, juce::Justification::centred, box.getWidth());
                break;
            }

            default:
            {
                const float kr = juce::jmin (face.getHeight() * 0.40f, face.getWidth() * 0.32f);
                drawKnob (g, face.getCentreX(), face.getCentreY(), kr, v, kr > 9.0f, mark);
                break;
            }
        }

        if (cs.name != nullptr && labelH > 5.0f)
        {
            g.setColour (paint::inkSoft);
            drawLabel (g, cs.name, cell.getX(), cell.getBottom() - labelH * 0.28f,
                         unit * 0.105f, 0.0f,
                         juce::Justification::centred, cell.getWidth());
        }
    }

    if (b.kind == qd::Kind::meter)
    {
        const auto strip2 = optionStrip (r, b);
        auto face = r.withTrimmedTop (juce::jmin (22.0f, unit * 0.26f))
                     .withTrimmedBottom (r.getBottom() - strip2.getY() + 6.0f)
                     .reduced (8.0f, 2.0f);
        face = face.withTrimmedBottom (face.getHeight() * 0.4f);
        drawMeterFace (g, face, faceReading (b.id));
    }

    if (b.kind == qd::Kind::seq)
    {
        const int playing = playingStep (b.id);

        for (int i = 0; i < qd::kSteps; ++i)
        {
            const auto mode = b.stepMode[(std::size_t) i];
            const bool on = mode != qd::Block::Step::off;

            for (int lane = 0; lane < 2; ++lane)
            {
                const auto cellR = seqCell (r, b, i, lane);
                if (cellR.isEmpty()) continue;

                const float v = juce::jlimit (0.0f, 1.0f, lane == 0 ? b.steps[(std::size_t) i]
                                                                    : b.steps2[(std::size_t) i]);

                // slider slot
                g.setColour (paint::field.darker (i == playing ? 0.02f : 0.12f));
                g.fillRect (cellR);
                g.setColour (paint::engrave.withAlpha (0.5f));
                g.fillRect (cellR.getCentreX() - juce::jmax (1.0f, unit * 0.008f), cellR.getY() + unit * 0.02f,
                            juce::jmax (2.0f, unit * 0.016f), cellR.getHeight() - unit * 0.04f);

                // slider cap
                const float capH = juce::jmax (3.0f, cellR.getHeight() * 0.17f);
                const float travel = cellR.getHeight() - capH - unit * 0.04f;
                const float cy = cellR.getBottom() - unit * 0.02f - capH - travel * v;

                g.setColour (on ? paint::chromeLo : paint::chromeLo.withAlpha (0.4f));
                g.fillRect (cellR.getX() + cellR.getWidth() * 0.1f, cy,
                            cellR.getWidth() * 0.8f, capH);
                g.setColour (on ? (lane == 0 ? paint::sigPitch : paint::sigCv)
                                : paint::engrave.withAlpha (0.5f));
                g.fillRect (cellR.getX() + cellR.getWidth() * 0.1f, cy,
                            cellR.getWidth() * 0.8f, capH * 0.42f);
            }

            const auto sw = seqCell (r, b, i, 2);

            if (! sw.isEmpty())
            {
                // step glyphs
                const auto box = sw.reduced (sw.getWidth() * 0.08f, sw.getHeight() * 0.16f);
                g.setColour (mode == qd::Block::Step::off ? paint::field.darker (0.06f) : paint::ink);
                g.fillRect (box);

                const auto in = box.reduced (box.getWidth() * 0.26f, box.getHeight() * 0.3f);
                const float lw = juce::jmax (1.2f, unit * 0.022f);
                g.setColour (mode == qd::Block::Step::off ? paint::engrave : paint::plateHi);

                if (mode == qd::Block::Step::rise)
                    g.drawLine (in.getX(), in.getBottom(), in.getRight(), in.getY(), lw);
                else if (mode == qd::Block::Step::fall)
                    g.drawLine (in.getX(), in.getY(), in.getRight(), in.getBottom(), lw);
                else
                    g.drawLine (in.getX(), in.getCentreY(), in.getRight(), in.getCentreY(), lw);
            }

            if (i == playing)
            {
                const auto col = seqCell (r, b, i, 0);
                g.setColour (paint::red.withAlpha (0.85f));
                g.fillRect (col.getX(), col.getY() - unit * 0.045f, col.getWidth(), unit * 0.025f);
            }
        }
    }

    const float pr = unit * 0.055f;
    const float labelSize = unit * 0.095f;
    const bool roomForNames = unit > 46.0f;

    // names sit outside, inside when a cable or a neighbour is there
    const bool onField = viewBlock (b.id) != nullptr;
    auto nameInside = [&] (bool isInput, int port, float y)
    {
        if (! onField) return true;

        for (const auto& c : viewCables)
            if (isInput ? (c.to == b.id && ! c.toKnob && c.toPort == port)
                        : (c.from == b.id && c.fromPort == port))
                return true;

        const float edge = isInput ? r.getX() : r.getRight();
        for (const auto& t : viewBlocks)
        {
            if (t.id == b.id) continue;
            const auto tr = tileRect (t);
            if (std::abs ((isInput ? tr.getRight() : tr.getX()) - edge) < unit * 0.3f
                && y > tr.getY() && y < tr.getBottom())
                return true;
        }

        return false;
    };

    // highlight valid inputs
    const bool hunting = (drag == Drag::cable && cableFrom >= 0);
    qd::Sig dragKindSig = qd::Sig::audio;

    if (hunting)
        if (const auto* src = viewBlock (cableFrom))
            dragKindSig = qd::portKind (src->kind, false, cableFromPort);

    for (int i = 0; i < sp.numIn; ++i)
    {
        const float y = r.getY() + r.getHeight() * (i + 1.0f) / (sp.numIn + 1.0f);
        const auto kind = qd::portKind (b.kind, true, i);
        const auto col = colourFor (kind);

        if (hunting)
        {
            const bool agrees = (kind == dragKindSig);
            const float halo = pr * (agrees ? 2.2f : 1.6f);
            g.setColour (colourFor (dragKindSig).withAlpha (agrees ? 0.45f : 0.18f));
            g.fillEllipse (r.getX() - halo, y - halo, halo * 2.0f, halo * 2.0f);
        }

        g.setColour (paint::field);
        g.fillEllipse (r.getX() - pr, y - pr, pr * 2.0f, pr * 2.0f);
        g.setColour (col);
        g.drawEllipse (r.getX() - pr, y - pr, pr * 2.0f, pr * 2.0f, unit * 0.022f);

        if (roomForNames)
        {
            g.setColour (col.withAlpha (0.85f));
            if (nameInside (true, i, y))
                drawLabel (g, qd::portName (b.kind, true, i), r.getX() + pr * 1.6f,
                             y + labelSize * 0.36f, labelSize, 0.0f,
                             juce::Justification::left, r.getWidth() * 0.4f);
            else
                drawLabel (g, qd::portName (b.kind, true, i), r.getX() - pr * 1.6f - unit * 0.8f,
                             y + labelSize * 0.36f, labelSize, 0.0f,
                             juce::Justification::right, unit * 0.8f);
        }
    }

    for (int i = 0; i < sp.numOut; ++i)
    {
        const float y = r.getY() + r.getHeight() * (i + 1.0f) / (sp.numOut + 1.0f);
        const auto col = colourFor (qd::portKind (b.kind, false, i));

        g.setColour (col);
        g.fillEllipse (r.getRight() - pr, y - pr, pr * 2.0f, pr * 2.0f);

        if (roomForNames)
        {
            g.setColour (col.withAlpha (0.85f));
            if (nameInside (false, i, y))
                drawLabel (g, qd::portName (b.kind, false, i), r.getRight() - pr * 1.6f - r.getWidth() * 0.4f,
                             y + labelSize * 0.36f, labelSize, 0.0f,
                             juce::Justification::right, r.getWidth() * 0.4f);
            else
                drawLabel (g, qd::portName (b.kind, false, i), r.getRight() + pr * 1.6f,
                             y + labelSize * 0.36f, labelSize, 0.0f,
                             juce::Justification::left, unit * 0.8f);
        }
    }

    // output lamp
    const float m = juce::jlimit (0.0f, 1.0f, meterValue);
    const float lr = unit * 0.06f;
    const juce::Point<float> lamp (r.getRight() - lr * 2.2f, r.getY() + lr * 2.2f);

    g.setColour (paint::engrave.withAlpha (0.45f));
    g.fillEllipse (lamp.x - lr, lamp.y - lr, lr * 2.0f, lr * 2.0f);
    g.setColour (paint::red.withAlpha (juce::jlimit (0.12f, 1.0f, m * 2.2f)));
    g.fillEllipse (lamp.x - lr * 0.78f, lamp.y - lr * 0.78f, lr * 1.56f, lr * 1.56f);
}

void QuadratEditor::drawCables (juce::Graphics& g)
{
    // adjacency links
    for (const auto& t : viewTouching)
    {
        if (drag == Drag::moveBlock && (t.from == dragBlock || t.to == dragBlock)) continue;

        const auto* a = viewBlock (t.from);
        const auto* b = viewBlock (t.to);
        if (a == nullptr || b == nullptr) continue;

        const auto from = portPos (*a, false, t.fromPort);
        const auto to = portPos (*b, true, t.toPort);
        const float reach = juce::jmax (cellSize() * 0.12f, std::abs (to.y - from.y) * 0.35f);

        juce::Path p;
        p.startNewSubPath (from);
        p.cubicTo (from.x + reach, from.y, to.x - reach, to.y, to.x, to.y);

        const float w = juce::jlimit (1.4f, 2.2f, cellSize() * 0.03f);
        g.setColour (juce::Colours::black.withAlpha (0.16f));
        g.strokePath (p, juce::PathStrokeType (w + 0.8f), juce::AffineTransform::translation (0.0f, 1.5f));
        g.setColour (colourFor (qd::portKind (a->kind, false, t.fromPort)).withAlpha (0.92f));
        g.strokePath (p, juce::PathStrokeType (w));
    }

    for (const auto& cbl : viewCables)
    {
        const auto* a = viewBlock (cbl.from);
        const auto* b = viewBlock (cbl.to);
        if (a == nullptr || b == nullptr) continue;

        auto from = portPos (*a, false, cbl.fromPort);
        auto to = cbl.toKnob ? knobTarget (*b, cbl.toPort)
                             : portPos (*b, true, cbl.toPort);

        if (drag == Drag::moveBlock)
        {
            const auto shift = mousePos - dragOffset - tileRect (*a).getPosition();
            if (a->id == dragBlock) from += shift;
            const auto shiftB = mousePos - dragOffset - tileRect (*b).getPosition();
            if (b->id == dragBlock) to += shiftB;
        }

        // sag with length
        const float sag = juce::jmin (46.0f, from.getDistanceFrom (to) * 0.22f);

        juce::Path p;
        p.startNewSubPath (from);
        p.cubicTo (from.x + 30.0f, from.y + sag, to.x - 30.0f, to.y + sag, to.x, to.y);

        g.setColour (juce::Colours::black.withAlpha (0.16f));
        g.strokePath (p, juce::PathStrokeType (3.0f), juce::AffineTransform::translation (0.0f, 2.0f));
        g.setColour (colourFor (qd::portKind (a->kind, false, cbl.fromPort)).withAlpha (0.92f));
        g.strokePath (p, juce::PathStrokeType (2.2f));
    }
}

void QuadratEditor::resized()
{
    const float s = getWidth() / layout::W;
    const auto px = [s] (float v) { return juce::roundToInt (v * s); };

    // stop flush right
    const float capR = 11.0f, w = 52.0f * s, step = 56.0f * s;
    const float stopX = fieldArea().getRight() - capR - w * 0.5f;
    stopCap.setBounds (juce::roundToInt (stopX), px (10), juce::roundToInt (w), px (44));
    startCap.setBounds (juce::roundToInt (stopX - step), px (10), juce::roundToInt (w), px (44));
    rollCap.setBounds (juce::roundToInt (stopX - step * 2.0f), px (10), juce::roundToInt (w), px (44));
    clearCap.setBounds (juce::roundToInt (stopX - step * 3.0f), px (10), juce::roundToInt (w), px (44));

    const auto a = paletteArea();
    // above the shelf, same width
    // 16 px either side, like the shelf
    search.setBounds (px (16), (int) a.getY(), juce::roundToInt (fieldArea().getX() - 32.0f * s), px (24));
    search.applyFontToAllText (juce::Font (juce::FontOptions (px (11))));
}

// ── interaction ──────────────────────────────────────────────────────────────
void QuadratEditor::mouseMove (const juce::MouseEvent& e)
{
    mousePos = e.position;

    int kind = -1, id = -1;

    if (const int item = paletteItemAt (mousePos); item >= 0)
    {
        kind = (int) paletteEntries()[(std::size_t) item].kind;
    }
    else if (fieldArea().contains (mousePos))
    {
        const auto cell = screenToCell (mousePos);

        for (const auto& b : viewBlocks)
            if (b.covers (cell.x, cell.y)) { kind = (int) b.kind; id = b.id; break; }
    }

    if (kind != hoverKind || id != hoverBlock)
    {
        hoverKind = kind;
        hoverBlock = id;
        hoverAt = mousePos;
        repaint();
    }
}

void QuadratEditor::mouseDown (const juce::MouseEvent& e)
{
    grabKeyboardFocus();
    mousePos = e.position;
    dragOffset = { cellSize() * 0.5f, cellSize() * 0.5f };

    if (showAbout)                       // click closes credits
    {
        showAbout = false;
        dimChildren();

        if (tourLink().contains (mousePos)) toggleTour();
        else if (githubLink().contains (mousePos))
            juce::URL ("https://github.com/user863/Quadrat").launchInDefaultBrowser();

        repaint();
        return;
    }

    if ((touring || welcoming) && handleTourClick (mousePos)) return;

    if (presetsRect().contains (mousePos) && ! touring) { showPresetMenu(); return; }

    if (wordmarkRect().contains (mousePos))
    {
        showAbout = true;
        dimChildren();
        repaint();
        return;
    }

    if (const auto thumb = scrollThumb(); ! thumb.isEmpty() && thumb.expanded (4.0f).contains (mousePos))
    {
        draggingScroll = true;
        scrollGrab = mousePos.y - thumb.getY();
        return;
    }

    if (paletteArea().contains (mousePos))
    {
        const int i = paletteItemAt (mousePos);

        if (i >= 0)
        {
            pushUndo();
            drag = Drag::fromPalette;
            dragKind = paletteEntries()[(std::size_t) i].kind;

            const auto& sp = qd::spec (dragKind);
            dragOffset = { cellSize() * sp.cells * 0.5f, cellSize() * sp.rows * 0.5f };
            repaint();
        }

        return;
    }

    if (! fieldArea().contains (mousePos)) return;

    const juce::ScopedLock sl (proc.patchLock);
    const float grab = juce::jmax (6.0f, cellSize() * 0.09f);

    // ports first
    for (const auto& b : proc.patch.blocks())
    {
        const auto& sp = qd::spec (b.kind);

        for (int i = 0; i < sp.numOut; ++i)
            if (portPos (b, false, i).getDistanceFrom (mousePos) <= grab)
            {
                pushUndo();
                drag = Drag::cable; cableFrom = b.id; cableFromPort = i;
                return;
            }

        for (int i = 0; i < sp.numIn; ++i)
            if (portPos (b, true, i).getDistanceFrom (mousePos) <= grab)
            {
                // grab cable off input
                for (const auto& c : proc.patch.cables())
                    if (c.to == b.id && c.toPort == i)
                    {
                        pushUndo();
                        drag = Drag::cable; cableFrom = c.from; cableFromPort = c.fromPort;
                        proc.patch.disconnect (b.id, i);
                        proc.patch.rebuild();
                        return;
                    }

                return;
            }
    }

    // notes: add / move / edit
    if (const int t = noteAt (mousePos); t >= 0)
    {
        if (e.getNumberOfClicks() >= 2) { editingNote = t; repaint(); return; }

        commitNote();
        pushUndo();
        draggingNote = t;
        drag = Drag::note;
        dragOffset = mousePos - noteRect (viewNotes[(std::size_t) t]).getPosition();
        return;
    }

    if (editingNote >= 0) commitNote();

    // tap tempo
    for (const auto& b : proc.patch.blocks())
    {
        if (b.kind != qd::Kind::clock) continue;

        const auto r = tileRect (b);
        const float unit = r.getHeight() / juce::jmax (1, qd::spec (b.kind).rows);
        const auto face = r.withTrimmedTop (unit * 0.26f).withTrimmedBottom (unit * 0.9f);

        if (! face.contains (mousePos)) continue;

        const double now = juce::Time::getMillisecondCounterHiRes();

        if (now - lastTap > 2000.0) taps.clear();
        taps.push_back (now);
        lastTap = now;

        while (taps.size() > 4) taps.erase (taps.begin());

        if (taps.size() >= 2)
        {
            const double span = (taps.back() - taps.front()) / (double) (taps.size() - 1);
            if (span > 120.0) proc.setTapTempo (60000.0 / span);
        }

        repaint();
        return;
    }

    // tab, then drawer
    for (auto& b : proc.patch.blocks())
    {
        if (const auto tab = drawerTab (b); ! tab.isEmpty()
                                            && tab.expanded (grab * 0.35f).contains (mousePos))
        {
            setDrawer (b.id, ! b.drawer);
            repaint();
            return;
        }

        if (drawerRect (b).isEmpty() || ! drawerRect (b).contains (mousePos)) continue;

        if (const int k = drawerJackAt (b, mousePos); k >= 0)
        {
            float jr = 0.0f;
            const auto centre = jackAt (b, k, jr);
            const float dist = centre.getDistanceFrom (mousePos);

            // depth ring drag
            if (dist > jr * 1.1f && dist < jr * 1.9f)
            {
                pushUndo();
                drag = Drag::depth; dragBlock = b.id; dragKnob = k;
                dragStartValue = b.depth[(std::size_t) k];

                // ring is a host param too
                dragLink = proc.linkKnob (b.id, qd::kMaxKnobs + k);
                if (auto* lp = proc.linkParam (dragLink)) lp->beginChangeGesture();
                proc.linkSend (dragLink, dragStartValue);
                return;
            }

            if (dist <= jr * 1.1f)
                for (const auto& c : proc.patch.cables())
                    if (c.toKnob && c.to == b.id && c.toPort == k)
                    {
                        // grab cable off jack
                        pushUndo();
                        drag = Drag::cable; cableFrom = c.from; cableFromPort = c.fromPort;
                        proc.patch.disconnect (b.id, k, true);
                        repaint();
                        return;
                    }
        }

        return;                       // drawer eats click
    }

    const auto cell = screenToCell (mousePos);

    if (auto* b = proc.patch.at (cell.x, cell.y))
    {
        if (e.mods.isShiftDown())
        {
            if (! selection.insert (b->id).second) selection.erase (b->id);
            repaint();
            return;
        }

        if (! isSelected (b->id)) { selection.clear(); selection.insert (b->id); }

        if (const int k = controlAt (*b, tileRect (*b), mousePos); k >= 0)
        {
            if (e.mods.isShiftDown())          // shift: unplug
            {
                pushUndo();
                proc.patch.disconnect (b->id, k, true);
                proc.patch.rebuild();
                repaint();
                return;
            }

            const auto& cs = qd::spec (b->kind).knobs[k];
            auto& v = b->knob[(std::size_t) k];

            if (cs.type == qd::Ctl::toggle)
            {
                v = v > 0.5f ? 0.0f : 1.0f;       // toggle on click
                return;
            }

            if (cs.type == qd::Ctl::select)
            {
                const int count = juce::jmax (1, qd::optionCount (cs.opts));
                const int idx = juce::jlimit (0, count - 1, (int) std::round (v * (count - 1)));
                v = count > 1 ? (float) ((idx + 1) % count) / (count - 1) : 0.0f;
                return;
            }

            pushUndo();
            drag = Drag::knob; dragBlock = b->id; dragKnob = k;
            dragStartValue = v;

            if (cs.type == qd::Ctl::slider)       // jump to click
            {
                const auto cell = controlRect (*b, tileRect (*b), k);
                v = juce::jlimit (0.0f, 1.0f, (mousePos.x - cell.getX()) / juce::jmax (1.0f, cell.getWidth()));
                dragStartValue = v;
            }

            // tell the host: "last tweaked"
            dragLink = proc.linkKnob (b->id, k);
            if (auto* lp = proc.linkParam (dragLink)) lp->beginChangeGesture();
            proc.linkSend (dragLink, v);
            return;
        }

        int lane = -1;

        if (const int st = stepAt (*b, mousePos, lane); st >= 0)
        {
            pushUndo();

            if (lane == 2)      // note / rest / tie
            {
                const int next = ((int) b->stepMode[(std::size_t) st] + 1) % 3;
                b->stepMode[(std::size_t) st] = (qd::Block::Step) next;
                repaint();
                return;
            }

            drag = Drag::step; dragBlock = b->id; dragStep = st; dragLane = lane;
            setStep (*b, st, lane, mousePos.y);
            return;
        }

        const auto& sp = qd::spec (b->kind);

        // midi name menu
        if (b->kind == qd::Kind::midi)
        {
            const auto r = tileRect (*b);
            const float cell = r.getHeight() / juce::jmax (1, sp.rows);
            const auto nameBox = juce::Rectangle<float> (r.getX(), r.getY(), r.getWidth() * 0.5f, cell * 0.28f);

            // screen: file browser
            if (faceArea (r, *b).contains (mousePos))
            {
                chooseMidiFile (b->id);
                return;
            }

            if (nameBox.contains (mousePos))
            {
                juce::PopupMenu menu;
                menu.addItem (3, "load MIDI file...");
               #if JUCE_MAC
                menu.addItem (1, "paste notes from Logic");
               #endif
                menu.addItem (2, "clear notes", ! b->phrase.empty());

                const int id = b->id;
                menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this)
                                        .withTargetScreenArea ({ (int) localPointToGlobal (nameBox.getPosition()).x,
                                                                 (int) localPointToGlobal (nameBox.getPosition()).y,
                                                                 (int) nameBox.getWidth(), (int) nameBox.getHeight() }),
                                    [this, id] (int choice)
                                    {
                                        if (choice == 1) pasteNotes (id);
                                        if (choice == 3) chooseMidiFile (id);

                                        if (choice == 2)
                                        {
                                            pushUndo();
                                            const juce::ScopedLock sl (proc.patchLock);
                                            if (auto* blk = proc.patch.find (id)) blk->phrase.clear();
                                        }

                                        repaint();
                                    });
                return;
            }
        }

        // preset menu
        if (const int np = blockPresets::count (b->kind); np > 0)
        {
            const auto r = tileRect (*b);
            const float cell = r.getHeight() / juce::jmax (1, sp.rows);
            const auto nameBox = juce::Rectangle<float> (r.getX(), r.getY(),
                                                         r.getWidth() * 0.5f, cell * 0.28f);

            if (nameBox.contains (mousePos))
            {
                // grouped by heading
                juce::PopupMenu menu;
                juce::String heading;

                for (int i = 0; i < np; ++i)
                {
                    const juce::String grp (blockPresets::group (b->kind, i));

                    if (grp.isNotEmpty() && grp != heading)
                    {
                        menu.addSectionHeader (grp);
                        heading = grp;
                    }

                    menu.addItem (i + 1, blockPresets::name (b->kind, i));
                }

                const int id = b->id;
                menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this)
                                        .withTargetScreenArea ({ (int) localPointToGlobal (nameBox.getPosition()).x,
                                                                 (int) localPointToGlobal (nameBox.getPosition()).y,
                                                                 (int) nameBox.getWidth(), (int) nameBox.getHeight() }),
                                    [this, id] (int choice)
                                    {
                                        if (choice <= 0) return;
                                        pushUndo();
                                        const juce::ScopedLock sl (proc.patchLock);
                                        if (auto* blk = proc.patch.find (id))
                                            blockPresets::apply (blk->kind, choice - 1, *blk);
                                        repaint();
                                    });
                return;
            }
        }

        if (const auto strip = optionStrip (tileRect (*b), *b);
            ! strip.isEmpty() && strip.contains (mousePos))
        {
            const int count = juce::jmax (1, qd::optionCount (sp.options));

            if (count <= 6)
            {
                pushUndo();
                b->option = juce::jlimit (0, count - 1,
                                          (int) ((mousePos.x - strip.getX()) / (strip.getWidth() / count)));
            }
            else
            {
                juce::PopupMenu menu;
                for (int i = 0; i < count; ++i)
                    menu.addItem (i + 1, juce::String::fromUTF8 (qd::optionName (sp.options, i).c_str()),
                                  true, i == b->option);

                const int id = b->id;
                menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this)
                                        .withTargetScreenArea ({ (int) localPointToGlobal (strip.getPosition()).x,
                                                                 (int) localPointToGlobal (strip.getPosition()).y,
                                                                 (int) strip.getWidth(), (int) strip.getHeight() }),
                                    [this, id] (int choice)
                                    {
                                        if (choice <= 0) return;
                                        const juce::ScopedLock sl (proc.patchLock);
                                        if (auto* blk = proc.patch.find (id)) blk->option = choice - 1;
                                        repaint();
                                    });
            }

            return;
        }

        pushUndo();
        drag = Drag::moveBlock;
        dragBlock = b->id;
        dragOffset = mousePos - tileRect (*b).getPosition();
        return;
    }

    if (e.getNumberOfClicks() >= 2)          // double-click: new note
    {
        pushUndo();
        const auto at = screenToCell (mousePos);
        qd::Note t;
        t.x = juce::jmax (0, at.x);
        t.y = juce::jmax (0, at.y);
        proc.patch.notes().push_back (t);
        viewNotes = proc.patch.notes();
        editingNote = (int) proc.patch.notes().size() - 1;
        grabKeyboardFocus();
        repaint();
        return;
    }

    // alt pans, else box
    if (e.mods.isAltDown() || e.mods.isMiddleButtonDown())
    {
        drag = Drag::pan;
        return;
    }

    if (! e.mods.isShiftDown()) selection.clear();

    drag = Drag::box;
    boxFrom = mousePos;
    repaint();
}

void QuadratEditor::mouseDrag (const juce::MouseEvent& e)
{
    const auto delta = e.position - mousePos;
    mousePos = e.position;

    if (draggingScroll)
    {
        const auto t = scrollTrack();
        const auto th = scrollThumb();
        const float room = juce::jmax (1.0f, t.getHeight() - th.getHeight());
        const float content = juce::jmax (0.0f, paletteContentHeight() - t.getHeight());
        palScroll = juce::jlimit (0.0f, content,
                                  (mousePos.y - scrollGrab - t.getY()) / room * content);
        repaint();
        return;
    }

    const juce::ScopedLock sl (proc.patchLock);

    switch (drag)
    {
        case Drag::knob:
            if (auto* b = proc.patch.find (dragBlock))
            {
                const auto& cs = qd::spec (b->kind).knobs[dragKnob];

                if (cs.type == qd::Ctl::slider)
                {
                    const auto cell = controlRect (*b, tileRect (*b), dragKnob);
                    b->knob[(std::size_t) dragKnob] =
                        juce::jlimit (0.0f, 1.0f, (mousePos.x - cell.getX()) / juce::jmax (1.0f, cell.getWidth()));
                }
                else
                {
                    const float fine = e.mods.isShiftDown() ? 4.0f : 1.0f;
                    b->knob[(std::size_t) dragKnob] =
                        juce::jlimit (0.0f, 1.0f,
                                      dragStartValue - e.getDistanceFromDragStartY() / (170.0f * fine));
                }

                proc.linkSend (dragLink, b->knob[(std::size_t) dragKnob]);
            }
            break;

        case Drag::step:
            // sweep across faders
            if (auto* b = proc.patch.find (dragBlock))
            {
                int lane = dragLane;
                const int st = stepAt (*b, mousePos, lane);

                setStep (*b, st >= 0 ? st : dragStep, dragLane, mousePos.y);
            }
            break;

        case Drag::depth:
            if (auto* b = proc.patch.find (dragBlock); b != nullptr && dragKnob >= 0)
            {
                const float fine = e.mods.isShiftDown() ? 4.0f : 1.0f;
                b->depth[(std::size_t) dragKnob] =
                    juce::jlimit (0.0f, 1.0f, dragStartValue - e.getDistanceFromDragStartY() / (170.0f * fine));
                proc.linkSend (dragLink, b->depth[(std::size_t) dragKnob]);
            }
            break;

        case Drag::note:
            if (draggingNote >= 0 && draggingNote < (int) proc.patch.notes().size())
            {
                const auto at = screenToCell (mousePos - dragOffset);
                auto& t = proc.patch.notes()[(std::size_t) draggingNote];
                t.x = juce::jmax (0, at.x);
                t.y = juce::jmax (0, at.y);
                viewNotes = proc.patch.notes();
            }
            break;

        case Drag::pan:
            panX = juce::jmax (-40.0f, panX - delta.x);
            panY = juce::jmax (-40.0f, panY - delta.y);
            break;

        case Drag::box:
        case Drag::fromPalette:
            break;                      // drawn live

        default: break;
    }

    repaint();
}

void QuadratEditor::mouseUp (const juce::MouseEvent&)
{
    if (draggingScroll) { draggingScroll = false; return; }

    const juce::ScopedLock sl (proc.patchLock);

    if (drag == Drag::fromPalette && fieldArea().contains (mousePos))
    {
        const auto cell = snapToTarget (dragKind, screenToCell (mousePos - dragOffset));

        if (proc.patch.add (dragKind, juce::jmax (0, cell.x), juce::jmax (0, cell.y)) >= 0)
            proc.patch.rebuild();
    }
    else if (drag == Drag::moveBlock)
    {
        if (paletteArea().contains (mousePos))
        {
            // back to palette = delete
            proc.patch.remove (dragBlock);
            selection.erase (dragBlock);
        }
        else
        {
            const auto* moving = proc.patch.find (dragBlock);
            const auto cell = snapToTarget (moving != nullptr ? moving->kind : qd::Kind::count,
                                          screenToCell (mousePos - dragOffset
                                                          + juce::Point<float> (subSize() * 0.5f, subSize() * 0.5f)));
            proc.patch.move (dragBlock, juce::jmax (0, cell.x), juce::jmax (0, cell.y));
        }

        proc.patch.rebuild();
    }
    else if (drag == Drag::cable && cableFrom >= 0)
    {
        const float grab = juce::jmax (9.0f, cellSize() * 0.12f);
        bool landed = false;

        for (const auto& b : proc.patch.blocks())
        {
            const auto& sp = qd::spec (b.kind);

            for (int i = 0; i < sp.numIn && ! landed; ++i)
                if (portPos (b, true, i).getDistanceFrom (mousePos) <= grab)
                {
                    proc.patch.connect (cableFrom, cableFromPort, b.id, i);
                    landed = true;
                }
        }

        // drawer jacks
        if (! landed)
            for (const auto& b : proc.patch.blocks())
                if (const int k = drawerJackAt (b, mousePos); k >= 0)
                {
                    proc.patch.connect (cableFrom, cableFromPort, b.id, k, true);
                    landed = true;
                    break;
                }

        // fallback: the knob
        if (! landed)
            for (const auto& b : proc.patch.blocks())
                if (const int c = controlAt (b, tileRect (b), mousePos); c >= 0)
                {
                    proc.patch.connect (cableFrom, cableFromPort, b.id, c, true);
                    landed = true;
                    break;
                }

        if (landed) proc.patch.rebuild();
    }
    else if (drag == Drag::box)
    {
        const juce::Rectangle<float> box (juce::Point<float> (juce::jmin (boxFrom.x, mousePos.x),
                                                              juce::jmin (boxFrom.y, mousePos.y)),
                                          juce::Point<float> (juce::jmax (boxFrom.x, mousePos.x),
                                                              juce::jmax (boxFrom.y, mousePos.y)));

        for (const auto& b : viewBlocks)
            if (tileRect (b).intersects (box)) selection.insert (b.id);
    }
    else if (drag == Drag::note)
    {
        // no rebuild
    }
    else if (drag == Drag::knob || drag == Drag::step)
    {
        // no rebuild
    }

    if (auto* lp = proc.linkParam (dragLink)) lp->endChangeGesture();
    dragLink = -1;

    drag = Drag::none;
    dragBlock = dragKnob = dragStep = draggingNote = -1;
    cableFrom = -1;
    repaint();
}

void QuadratEditor::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w)
{
    if (paletteArea().contains (e.position))
    {
        scrollPalette (-w.deltaY * 260.0f);
        return;
    }

    if (! fieldArea().contains (e.position)) return;

    mousePos = e.position;

    // scroll pans, cmd zooms
    if (! (e.mods.isCommandDown() || e.mods.isCtrlDown()))
    {
        panX = juce::jmax (-40.0f, panX - w.deltaX * 320.0f);
        panY = juce::jmax (-40.0f, panY - w.deltaY * 320.0f);
        repaint();
        return;
    }

    const auto f = fieldArea();
    const float before = cellSize();
    const float wx = (mousePos.x - f.getX() + panX) / before;
    const float wy = (mousePos.y - f.getY() + panY) / before;

    zoom = juce::jlimit (0.45f, 2.2f, zoom * std::pow (2.0f, w.deltaY * 1.4f));

    const float after = cellSize();
    panX = juce::jmax (-40.0f, wx * after - (mousePos.x - f.getX()));
    panY = juce::jmax (-40.0f, wy * after - (mousePos.y - f.getY()));
    repaint();
}

void QuadratEditor::mouseMagnify (const juce::MouseEvent& e, float factor)
{
    mousePos = e.position;
    const auto f = fieldArea();
    const float before = cellSize();
    const float wx = (mousePos.x - f.getX() + panX) / before;
    const float wy = (mousePos.y - f.getY() + panY) / before;

    zoom = juce::jlimit (0.45f, 2.2f, zoom * factor);

    const float after = cellSize();
    panX = juce::jmax (-40.0f, wx * after - (mousePos.x - f.getX()));
    panY = juce::jmax (-40.0f, wy * after - (mousePos.y - f.getY()));
    repaint();
}

// copy blocks + inner cables
void QuadratEditor::copySelection (bool cut)
{
    if (selection.empty()) return;

    const juce::ScopedLock sl (proc.patchLock);

    clipboard.blocks.clear();
    clipboard.cables.clear();

    for (const auto& b : proc.patch.blocks())
        if (isSelected (b.id)) clipboard.blocks.push_back (b);

    for (const auto& c : proc.patch.cables())
        if (isSelected (c.from) && isSelected (c.to)) clipboard.cables.push_back (c);

    // tag system clipboard
    {
        juce::StringArray names;
        for (const auto& b : clipboard.blocks) names.add (qd::spec (b.kind).name);

        juce::SystemClipboard::copyTextToClipboard ("quadrat: " + juce::String (names.size())
                                                    + (names.size() == 1 ? " block (" : " blocks (")
                                                    + names.joinIntoString (", ") + ")");
    }

    if (! cut) return;

    pushUndo();

    for (const int id : selection) proc.patch.remove (id);

    selection.clear();
    repaint();
}

// paste, find room
void QuadratEditor::pasteAt (juce::Point<int> cell)
{
    if (clipboard.blocks.empty()) return;

    const juce::ScopedLock sl (proc.patchLock);

    int minX = clipboard.blocks.front().x, minY = clipboard.blocks.front().y;

    for (const auto& b : clipboard.blocks)
    {
        minX = juce::jmin (minX, b.x);
        minY = juce::jmin (minY, b.y);
    }

    const int wantX = juce::jmax (0, cell.x), wantY = juce::jmax (0, cell.y);
    int dx = wantX - minX, dy = wantY - minY;
    bool room = false;

    for (int tries = 0; tries < 64 && ! room; ++tries)
    {
        room = true;

        for (const auto& b : clipboard.blocks)
            if (! proc.patch.isFree (b.x + dx, b.y + dy, b.cells(), b.rows()))
                { room = false; break; }

        if (! room) dy += 1;                 // next row down
    }

    if (! room) return;

    pushUndo();

    std::map<int, int> idMap;
    selection.clear();

    for (const auto& b : clipboard.blocks)
    {
        const int id = proc.patch.add (b.kind, b.x + dx, b.y + dy);
        if (id < 0) continue;

        idMap[b.id] = id;
        selection.insert (id);

        if (auto* n = proc.patch.find (id))
        {
            const int x = n->x, y = n->y, nid = n->id;
            *n = b;                          // knobs, steps, option, drawer
            n->x = x; n->y = y; n->id = nid;
        }
    }

    for (const auto& c : clipboard.cables)
    {
        const auto f = idMap.find (c.from), t = idMap.find (c.to);

        if (f != idMap.end() && t != idMap.end())
            proc.patch.connect (f->second, c.fromPort, t->second, c.toPort, c.toKnob);
    }

    repaint();
}

bool QuadratEditor::keyPressed (const juce::KeyPress& k)
{
    // note editing eats keys
    if (editingNote >= 0)
    {
        const juce::ScopedLock sl (proc.patchLock);
        auto& notes = proc.patch.notes();

        if (editingNote >= (int) notes.size()) { editingNote = -1; return false; }

        auto& text = notes[(std::size_t) editingNote].text;

        if (k == juce::KeyPress::returnKey || k == juce::KeyPress::escapeKey)
        {
            commitNote();
            return true;
        }

        if (k == juce::KeyPress::backspaceKey || k == juce::KeyPress::deleteKey)
        {
            if (! text.empty()) text.pop_back();
            viewNotes = notes;
            repaint();
            return true;
        }

        if (const auto ch = k.getTextCharacter(); ch >= 32 && ch < 127 && text.size() < 40)
        {
            text += (char) ch;
            viewNotes = notes;
            repaint();
            return true;
        }

        return true;                      // swallow
    }

    if ((touring || welcoming) && handleTourKey (k)) return true;

    if ((k == juce::KeyPress::deleteKey || k == juce::KeyPress::backspaceKey) && ! selection.empty())
    {
        const juce::ScopedLock sl (proc.patchLock);

        pushUndo();

        for (const int id : selection)
            proc.patch.remove (id);

        proc.patch.rebuild();
        selection.clear();
        repaint();
        return true;
    }

    if (k == juce::KeyPress ('z', juce::ModifierKeys::commandModifier, 0))
    {
        undo();
        return true;
    }

    if (k == juce::KeyPress ('z', juce::ModifierKeys::commandModifier
                                   | juce::ModifierKeys::shiftModifier, 0))
    {
        redo();
        return true;
    }

    if (k == juce::KeyPress ('a', juce::ModifierKeys::commandModifier, 0))
    {
        for (const auto& b : viewBlocks) selection.insert (b.id);
        repaint();
        return true;
    }

    // patch as text code
    if (k == juce::KeyPress ('c', juce::ModifierKeys::commandModifier
                                   | juce::ModifierKeys::shiftModifier, 0))
    {
        juce::SystemClipboard::copyTextToClipboard (proc.patchCode());
        say ("patch copied");
        return true;
    }

    if (k == juce::KeyPress ('v', juce::ModifierKeys::commandModifier
                                   | juce::ModifierKeys::shiftModifier, 0))
    {
        pushUndo();
        const auto text = juce::SystemClipboard::getTextFromClipboard();
        say (proc.loadPatchCode (text) ? "patch pasted" : "no patch on the clipboard");
        selection.clear();
        framedProgram = -1;                 // frame whatever arrived
        repaint();
        return true;
    }

    if (k == juce::KeyPress ('c', juce::ModifierKeys::commandModifier, 0))
    {
        copySelection (false);
        return true;
    }

    if (k == juce::KeyPress ('x', juce::ModifierKeys::commandModifier, 0))
    {
        copySelection (true);
        return true;
    }

    if (k == juce::KeyPress ('v', juce::ModifierKeys::commandModifier, 0))
    {
        // Logic notes or blocks
        if (pasteNotes (-1))
            return true;

        // at cursor, else origin
        const auto at = fieldArea().contains (mousePos)
                          ? screenToCell (mousePos)
                          : juce::Point<int> (0, 0);
        pasteAt (at);
        return true;
    }

    if (k == juce::KeyPress ('d', juce::ModifierKeys::commandModifier, 0))
    {
        const auto keep = clipboard;         // keep clipboard
        copySelection (false);

        if (! clipboard.blocks.empty())
        {
            int minX = clipboard.blocks.front().x, minY = clipboard.blocks.front().y;

            for (const auto& b : clipboard.blocks)
            {
                minX = juce::jmin (minX, b.x);
                minY = juce::jmin (minY, b.y);
            }

            pasteAt ({ minX, minY + 1 });
        }

        clipboard = keep;
        return true;
    }

    if (k == juce::KeyPress ('0', juce::ModifierKeys::commandModifier, 0))
    {
        zoom = 1.0f; panX = panY = 0.0f;
        repaint();
        return true;
    }

    return false;
}

// ── midi block ──
static juce::String noteName (int pitch)
{
    static const char* names[] = { "C", "C\xe2\x99\xaf", "D", "D\xe2\x99\xaf", "E", "F",
                                   "F\xe2\x99\xaf", "G", "G\xe2\x99\xaf", "A", "A\xe2\x99\xaf", "B" };
    return juce::String::fromUTF8 (names[pitch % 12]) + juce::String (pitch / 12 - 2);   // C3 = 60
}

// mini piano roll
void QuadratEditor::drawPhrase (juce::Graphics& g, juce::Rectangle<float> r, const qd::Block& b)
{
    g.setColour (paint::screen);
    g.fillRect (r);

    const juce::Colour dim { 0xff8d8f8b };
    const float text = juce::jmax (6.0f, r.getHeight() * 0.13f);

    if (b.phrase.empty())
    {
        g.setColour (dim);
        drawLabel (g, "click to load a MIDI file", r.getX(), r.getCentreY() - text * 0.35f, text, 0.0f,
                     juce::Justification::centred, r.getWidth());
       #if JUCE_MAC
        const char* second = "or drop one here, or \xe2\x8c\x98V Logic notes";
       #else
        const char* second = "or drop one here";
       #endif
        drawLabel (g, juce::String::fromUTF8 (second), r.getX(), r.getCentreY() + text * 1.0f,
                     text, 0.0f, juce::Justification::centred, r.getWidth());
        return;
    }

    int lo = 127, hi = 0;
    for (const auto& n : b.phrase) { lo = juce::jmin (lo, n.pitch); hi = juce::jmax (hi, n.pitch); }

    // min one octave
    while (hi - lo < 11) { if (hi < 127) ++hi; if (hi - lo < 11 && lo > 0) --lo; }

    const float keys = text * 2.4f;
    const auto roll = r.reduced (r.getHeight() * 0.07f).withTrimmedLeft (keys);
    const float loop = (float) juce::jmax (qd::kPpq, b.loopTicks);
    const float rowH = roll.getHeight() / (float) (hi - lo + 1);

    // bars, beats if room
    const float bar = (float) (qd::kPpq * 4);
    const bool beats = roll.getWidth() / (loop / (float) qd::kPpq) > 7.0f;

    for (float t = 0.0f; t <= loop + 1.0f; t += (float) qd::kPpq)
    {
        const bool isBar = std::fmod (t, bar) < 1.0f;
        if (! isBar && ! beats) continue;

        g.setColour (paint::screenLn.brighter (isBar ? 0.5f : 0.0f));
        g.fillRect (roll.getX() + roll.getWidth() * t / loop, roll.getY(), 1.0f, roll.getHeight());
    }

    const int playing = playingStep (b.id);

    for (int k = 0; k < (int) b.phrase.size(); ++k)
    {
        const auto& n = b.phrase[(std::size_t) k];
        const float x = roll.getX() + roll.getWidth() * (float) n.start / loop;
        const float w = juce::jmax (1.5f, roll.getWidth() * (float) n.length / loop - 0.6f);
        const float y = roll.getBottom() - rowH * (float) (n.pitch - lo + 1);

        g.setColour (k == playing ? paint::index : paint::index.withAlpha (0.5f));
        g.fillRect (x, y + rowH * 0.1f, w, juce::jmax (1.2f, rowH * 0.8f));
    }

    const float phase = clockPhase (b.id);

    if (phase > 0.0f)
    {
        g.setColour (paint::sigGate);
        g.fillRect (roll.getX() + roll.getWidth() * phase - 0.6f, roll.getY(), 1.2f, roll.getHeight());
    }

    g.setColour (dim);
    drawLabel (g, noteName (hi), r.getX() + 2.0f, roll.getY() + text * 0.8f, text * 0.85f, 0.0f,
                 juce::Justification::left, keys);
    drawLabel (g, noteName (lo), r.getX() + 2.0f, roll.getBottom(), text * 0.85f, 0.0f,
                 juce::Justification::left, keys);
}

bool QuadratEditor::pasteNotes (int targetId)
{
    std::vector<qd::MidiNote> phrase;
    int loop = 0;

    if (! midiText::fromLogic (juce::SystemClipboard::getTextFromClipboard().toStdString(), phrase, loop))
    {
        if (targetId >= 0) say ("no Logic notes on the clipboard");
        return false;
    }

    return placePhrase (phrase, loop, targetId, mousePos);
}

// into target, a selected midi block, or a new one at `at`
bool QuadratEditor::placePhrase (std::vector<qd::MidiNote> phrase, int loop, int targetId, juce::Point<float> at)
{
    pushUndo();
    int id = targetId;

    {
        const juce::ScopedLock sl (proc.patchLock);
        auto& p = proc.patch;

        if (id >= 0 && p.find (id) == nullptr) id = -1;

        if (id < 0)
            for (const int s : selection)
                if (const auto* b = p.find (s); b != nullptr && b->kind == qd::Kind::midi) { id = s; break; }

        if (id < 0)
        {
            const auto from = fieldArea().contains (at) ? screenToCell (at)
                                                        : screenToCell (fieldArea().getTopLeft() + juce::Point<float> (30.0f, 30.0f));
            const auto& sp = qd::spec (qd::Kind::midi);

            for (int dy = 0; dy < 40 * qd::kSub && id < 0; dy += qd::kSub)
                for (int dx = 0; dx < 40 * qd::kSub && id < 0; dx += qd::kSub)
                {
                    const int x = juce::jmax (0, from.x + dx), y = juce::jmax (0, from.y + dy);
                    if (p.isFree (x, y, sp.cells, sp.rows)) id = p.add (qd::Kind::midi, x, y);
                }

            p.rebuild();
        }

        if (auto* b = p.find (id))
        {
            b->phrase = std::move (phrase);
            b->loopTicks = loop;
        }
    }

    if (id < 0)
    {
        undoStack.pop_back();
        say ("no room for a midi block");
        return true;
    }

    selection = { id };
    repaint();
    return true;
}

// ── MIDI files ──
bool QuadratEditor::loadMidiFile (const juce::File& f, int targetId, juce::Point<float> at)
{
    juce::FileInputStream in (f);
    juce::MidiFile mf;

    if (! in.openedOk() || ! mf.readFrom (in))
    {
        say ("couldn't read " + f.getFileName());
        return false;
    }

    const int tpq = mf.getTimeFormat();
    if (tpq <= 0) { say ("timecode-based MIDI files aren't supported"); return false; }

    // bar length from the first time signature
    int beats = 4;
    juce::MidiMessageSequence sigs;
    mf.findAllTimeSigEvents (sigs);

    if (sigs.getNumEvents() > 0)
    {
        int num = 4, den = 4;
        sigs.getEventPointer (0)->message.getTimeSignatureInfo (num, den);
        beats = juce::jmax (1, juce::roundToInt (num * 4.0 / juce::jmax (1, den)));
    }

    const double scale = (double) qd::kPpq / tpq;
    std::vector<qd::MidiNote> notes;

    for (int t = 0; t < mf.getNumTracks(); ++t)
    {
        juce::MidiMessageSequence seq (*mf.getTrack (t));
        seq.updateMatchedPairs();

        for (int e = 0; e < seq.getNumEvents(); ++e)
        {
            const auto& m = seq.getEventPointer (e)->message;
            if (! m.isNoteOn()) continue;

            const double on = m.getTimeStamp();
            double off = seq.getTimeOfMatchingKeyUp (e);
            if (off <= on) off = on + tpq / 4.0;              // unmatched: a 16th

            qd::MidiNote n;
            n.start = juce::roundToInt (on * scale);
            n.length = juce::jmax (1, juce::roundToInt ((off - on) * scale));
            n.pitch = m.getNoteNumber();
            n.velocity = juce::jlimit (1, 127, (int) m.getVelocity());
            notes.push_back (n);
        }
    }

    std::vector<qd::MidiNote> phrase;
    int loop = 0;

    if (! midiText::fromNotes (notes, beats, phrase, loop))
    {
        say ("no notes in " + f.getFileName());
        return false;
    }

    return placePhrase (phrase, loop, targetId, at);
}

void QuadratEditor::chooseMidiFile (int targetId)
{
    chooser = std::make_unique<juce::FileChooser> ("Load a MIDI file", juce::File(), "*.mid;*.midi");
    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                          [this, targetId] (const juce::FileChooser& fc)
                          {
                              const auto f = fc.getResult();
                              if (f.existsAsFile()) loadMidiFile (f, targetId, {});
                          });
}

static bool isMidiFile (const juce::String& path)
{
    return path.endsWithIgnoreCase (".mid") || path.endsWithIgnoreCase (".midi");
}

bool QuadratEditor::isInterestedInFileDrag (const juce::StringArray& files)
{
    for (const auto& f : files) if (isMidiFile (f)) return true;
    return false;
}

void QuadratEditor::filesDropped (const juce::StringArray& files, int x, int y)
{
    fileHover = { -1.0f, -1.0f };
    const juce::Point<float> at ((float) x, (float) y);

    // onto a midi block fills it
    int target = -1;
    const auto cell = screenToCell (at);

    for (const auto& b : viewBlocks)
        if (b.kind == qd::Kind::midi && b.covers (cell.x, cell.y)) target = b.id;

    for (const auto& f : files)
        if (isMidiFile (f)) { loadMidiFile (juce::File (f), target, at); break; }

    repaint();
}

// ── header preset menu ──
// screen area of blocks with a face
juce::Rectangle<float> QuadratEditor::faceArea (juce::Rectangle<float> r, const qd::Block& b) const
{
    const float unit = r.getHeight() / juce::jmax (1, qd::spec (b.kind).rows);
    const auto strip = optionStrip (r, b);
    const float bottom = strip.isEmpty() ? unit * 0.09f : r.getBottom() - strip.getY() + unit * 0.05f;
    auto area = r.withTrimmedTop (unit * 0.25f).withTrimmedBottom (bottom).reduced (unit * 0.09f, unit * 0.03f);
    return area.removeFromTop (area.getHeight()
                                 * (b.kind == qd::Kind::midi ? 0.62f
                                    : b.kind == qd::Kind::input ? 0.56f
                                    : b.kind == qd::Kind::clock || b.kind == qd::Kind::ball ? 0.52f : 0.38f));
}

// tracked label width, unscaled
static float labelWidth (const juce::String& text, float size, float tracking)
{
    const juce::Font f { juce::FontOptions { size } };
    float w = -tracking;
    for (int i = 0; i < text.length(); ++i)
        w += juce::GlyphArrangement::getStringWidth (f, text.substring (i, i + 1)) + tracking;
    return juce::jmax (0.0f, w);
}

// patch name, no slot number
juce::String QuadratEditor::headerTitle() const
{
    auto t = proc.patchTitle();
    if (t.length() > 3 && juce::CharacterFunctions::isDigit (t[0]) && juce::CharacterFunctions::isDigit (t[1]) && t[2] == ' ')
        t = t.substring (3);
    return t;
}

// right of the logo
juce::Rectangle<float> QuadratEditor::presetsRect() const
{
    const float s = getWidth() / layout::W;
    const float x = layout::M + labelWidth ("quadrat", 17.0f, 1.2f) + 28.0f;
    const float w = labelWidth (headerTitle(), 10.5f, 0.3f) + 8.0f + 7.0f;
    return { x * s, 24.0f * s, w * s, 20.0f * s };
}

void QuadratEditor::showPresetMenu()
{
    juce::PopupMenu menu;
    for (int i = 0; i < presets::count(); ++i)
        menu.addItem (i + 1, presets::name (i), true, i == proc.getCurrentProgram());

    const auto r = presetsRect();
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this)
                            .withTargetScreenArea (localAreaToGlobal (r.toNearestInt())),
                        [this] (int choice)
                        {
                            if (choice <= 0 || touring) return;
                            pushUndo();
                            proc.setCurrentProgram (choice - 1);
                            framedProgram = -1;            // frame it
                            repaint();
                        });
}

// ── in block ──
// auto-zoom scope
void QuadratEditor::drawInput (juce::Graphics& g, juce::Rectangle<float> r, const qd::Block& b, float level)
{
    g.setColour (paint::screen);
    g.fillRect (r);
    g.setColour (paint::screenLn);
    g.fillRect (r.getX(), r.getCentreY(), r.getWidth(), 1.0f);

    const juce::Colour dim { 0xff8d8f8b };
    const float text = juce::jmax (6.0f, r.getHeight() * 0.26f);
    const float db = level > 1.0e-5f ? juce::Decibels::gainToDecibels (level) : -100.0f;

    // placeholder text
    if (b.id <= 0 || ! inputLive)
    {
        g.setColour (dim);
        drawLabel (g, b.id <= 0 ? "audio in" : "no input", r.getX(), r.getCentreY() + text * 0.35f,
                     text, 0.0f, juce::Justification::centred, r.getWidth());
        return;
    }

    const float* data = scopeFor (b.id);
    float peak = 0.0f;

    if (data != nullptr)
        for (int i = 0; i < qd::Patch::kScope; ++i) peak = juce::jmax (peak, std::abs (data[i]));

    auto& zoom = inputZoom[b.id];
    zoom = juce::jmax (peak, zoom * 0.94f);            // ~1 s decay

    const auto wave = r.withTrimmedBottom (r.getHeight() * 0.16f);

    if (data != nullptr && zoom > 1.0e-4f)             // -80 dB floor
    {
        const float scale = 0.9f / zoom;
        const float half = wave.getHeight() * 0.46f;
        juce::Path path;

        for (int i = 0; i < qd::Patch::kScope; ++i)
        {
            const float x = wave.getX() + wave.getWidth() * i / (float) (qd::Patch::kScope - 1);
            const float y = wave.getCentreY() - juce::jlimit (-1.0f, 1.0f, data[i] * scale) * half;

            if (i == 0) path.startNewSubPath (x, y);
            else        path.lineTo (x, y);
        }

        g.setColour (paint::sigAudio.brighter (0.4f));
        g.strokePath (path, juce::PathStrokeType (juce::jmax (1.0f, r.getHeight() * 0.04f)));
    }

    // level bar
    const float barH = juce::jmax (2.0f, r.getHeight() * 0.1f);
    const auto track = juce::Rectangle<float> (r.getX() + 2.0f, r.getBottom() - barH - 2.0f,
                                               r.getWidth() - 4.0f, barH);
    const float fill = juce::jlimit (0.0f, 1.0f, (db + 60.0f) / 60.0f);

    g.setColour (paint::screenLn);
    g.fillRect (track);
    g.setColour (db > -1.0f ? paint::sigGate : db > -6.0f ? paint::sigCv : paint::sigAudio.brighter (0.3f));
    g.fillRect (track.withWidth (track.getWidth() * fill));

    // top left
    g.setColour (dim);
    drawLabel (g, db < -80.0f ? juce::String ("silent")
                                : juce::String (juce::roundToInt (db)) + " dB",
                 r.getX() + 3.0f, r.getY() + text * 1.0f, text * 0.85f, 0.0f,
                 juce::Justification::left, r.getWidth());
}

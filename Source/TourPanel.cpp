// tour overlay + card
#include "PluginEditor.h"
#include "Presets.h"

namespace
{
    enum Action { back = 1, skip, next, exitTour, carryOn, keep, goBack, stay, welcomeStart, welcomeSkip, dontShow };

    // card type sizes
    constexpr float cardW = 450.0f, cardPad = 17.0f;
    constexpr float headSize = 10.5f, mainSize = 16.0f, bigSize = 20.0f, whySize = 13.0f, buttonSize = 11.5f;

    const juce::Colour lamp { 0xfff3f4f1 };        // ring/dot, not a signal
    const juce::Colour veilInk { 0xff121312 };

    double nowSeconds() { return juce::Time::getMillisecondCounterHiRes() * 0.001; }

    juce::String utf8 (const char* s) { return juce::String::fromUTF8 (s); }

    juce::Font face (float size) { return juce::Font (juce::FontOptions (size)); }

    // wrap text, return height
    float drawWrapped (juce::Graphics* g, const juce::String& text, float size,
                   float x, float y, float width)
    {
        const auto f = face (size);
        juce::GlyphArrangement ga;
        ga.addJustifiedText (f, text, x, y + f.getAscent(), width, juce::Justification::left);

        if (g != nullptr) ga.draw (*g);

        const auto box = ga.getBoundingBox (0, -1, true);
        return box.isEmpty() ? f.getHeight() : box.getBottom() - y + f.getDescent() * 0.5f;
    }

    float ease (float u) { u = juce::jlimit (0.0f, 1.0f, u); return u * u * (3.0f - 2.0f * u); }

    juce::Point<float> bezier (juce::Point<float> a, juce::Point<float> b, juce::Point<float> c,
                               juce::Point<float> d, float t)
    {
        const float m = 1.0f - t;
        return a * (m * m * m) + b * (3.0f * m * m * t) + c * (3.0f * m * t * t) + d * (t * t * t);
    }
}

// ── start / stop ──
void QuadratEditor::startTour()
{
    if (touring) return;

    commitNote();
    showAbout = false;

    // stash user's patch + history
    {
        const juce::ScopedLock sl (proc.patchLock);
        savedPatch = { proc.patch.blocks(), proc.patch.cables(), proc.patch.notes(), proc.patchTitle() };
        proc.patch.clear();
    }

    savedUndo = undoStack;  undoStack.clear();
    savedRedo = redoStack;  redoStack.clear();

    proc.setTitle ({});
    proc.setMuted (false);
    stopCap.setToggleState (false, juce::dontSendNotification);

    selection.clear();
    search.clear();
    filterPalette();
    palScroll = 0.0f;

    touring = true;
    tourCard = TourCard::step;
    withTour ([this] (const tour::Context& c) { guide.start (c); return 0; });

    frameTour();
    onStepStart();
    repaint();
}

void QuadratEditor::finishTour (bool keepIt)
{
    commitNote();

    if (confettiAt > 0.0) { confettiAt = 0.0; startTimerHz (20); }

    touring = false;
    tourCard = TourCard::step;
    stepDoneAt = 0.0;

    if (keepIt)
    {
        // one undo = old patch
        undoStack = savedUndo;
        undoStack.push_back (savedPatch);
        redoStack.clear();

        if (proc.patchTitle() == juce::String (presets::name (proc.getCurrentProgram())))
            proc.setTitle ("first patch");
    }
    else
    {
        restore (savedPatch);
        undoStack = savedUndo;
        redoStack = savedRedo;
        framedProgram = -1;                 // reframe
    }

    savedUndo.clear();
    savedRedo.clear();
    selection.clear();
    repaint();
}

void QuadratEditor::frameTour()
{
    zoomToFitBounds (0, 0, tour::kFrameW, tour::kFrameH);
}

// ── stepping ──
void QuadratEditor::updateTour()
{
    // 60 fps while confetti
    if (confettiAt > 0.0 && nowSeconds() - confettiAt > 3.2)
    {
        confettiAt = 0.0;
        startTimerHz (20);
    }

    if (! touring || tourCard != TourCard::step) return;

    // green flash, then advance
    if (stepDoneAt > 0.0)
    {
        if (nowSeconds() - stepDoneAt > 0.8) advanceTour();
        return;
    }

    const auto event = withTour ([this] (const tour::Context& c) { return guide.update (c); });

    if (event == tour::Runner::Event::completed)
    {
        {
            const juce::ScopedLock sl (proc.patchLock);
            guide.settle (proc.patch);
        }

        stepDoneAt = nowSeconds();
    }
    else if (event == tour::Runner::Event::steppedBack)
    {
        onStepStart();
        say ("Tutorial: back to step " + juce::String (guide.at() + 1));
    }
}

void QuadratEditor::onStepStart()
{
    stepStartedAt = nowSeconds();
    stepDoneAt = 0.0;

    const auto& st = guide.step();
    if (st.aim == tour::Aim::place) showPaletteItem (st.kind);
}

void QuadratEditor::advanceTour()
{
    stepDoneAt = 0.0;

    if (guide.last())
    {
        tourCard = TourCard::finished;
        frameTour();
        celebrate();
    }
    else if (guide.endsChapter())
    {
        tourCard = TourCard::chapter;
    }
    else
    {
        withTour ([this] (const tour::Context& c) { guide.next (c); return 0; });
        onStepStart();
    }

    repaint();
}

void QuadratEditor::onTourButton (int action)
{
    switch (action)
    {
        case back:
            withTour ([this] (const tour::Context& c) { guide.back (c); return 0; });
            onStepStart();
            break;

        case skip:
            pushUndo();
            {
                const juce::ScopedLock sl (proc.patchLock);
                guide.skip (proc.patch);
            }
            advanceTour();
            break;

        case next:                          // revisited step
            if (guide.last()) { tourCard = TourCard::finished; celebrate(); break; }
            withTour ([this] (const tour::Context& c) { guide.next (c); return 0; });
            onStepStart();
            break;

        case carryOn:
            tourCard = TourCard::step;
            withTour ([this] (const tour::Context& c) { guide.next (c); return 0; });
            onStepStart();
            break;

        case exitTour: tourCard = TourCard::leave; break;
        case stay:     tourCard = TourCard::step; break;
        case welcomeStart: welcoming = false; if (dontShowAgain) markWelcomeSeen(); startTour(); break;
        case welcomeSkip:  welcoming = false; if (dontShowAgain) markWelcomeSeen(); tourCard = TourCard::step; break;
        case dontShow:     dontShowAgain = ! dontShowAgain; break;
        case keep:     finishTour (true); break;
        case goBack:   finishTour (false); break;
        default: break;
    }

    repaint();
}

bool QuadratEditor::handleTourClick (juce::Point<float> p)
{
    const auto lay = cardLayout (hints());

    for (const auto& b : lay.buttons)
        if (b.r.contains (p)) { onTourButton (b.action); return true; }

    // modal cards eat clicks
    return tourCard != TourCard::step || lay.card.contains (p);
}

bool QuadratEditor::pressTourButton (const juce::String& label)
{
    if (! touring && ! welcoming) return false;

    for (const auto& b : cardLayout (hints()).buttons)
        if (b.label.equalsIgnoreCase (label)) { onTourButton (b.action); return true; }

    return false;
}

bool QuadratEditor::handleTourKey (const juce::KeyPress& k)
{
    if (welcoming)
    {
        if (k == juce::KeyPress::escapeKey) { onTourButton (welcomeSkip); return true; }
        if (k == juce::KeyPress::returnKey) { onTourButton (welcomeStart); return true; }
        return false;
    }

    if (k == juce::KeyPress::escapeKey)
    {
        onTourButton (tourCard == TourCard::leave ? stay : exitTour);
        return true;
    }

    if (k == juce::KeyPress::returnKey && tourCard == TourCard::chapter)
    {
        onTourButton (carryOn);
        return true;
    }

    return false;
}

// snap near the outline
juce::Point<int> QuadratEditor::snapToTarget (qd::Kind kind, juce::Point<int> cell) const
{
    if (! touring || tourCard != TourCard::step || stepDoneAt > 0.0) return cell;

    const auto& st = guide.step();

    if ((st.aim == tour::Aim::place || st.aim == tour::Aim::move) && st.kind == kind
        && std::abs (cell.x - st.x) <= 3 && std::abs (cell.y - st.y) <= 3)
        return { st.x, st.y };

    return cell;
}

void QuadratEditor::showPaletteItem (qd::Kind kind)
{
    const int index = paletteIndexOf (kind);
    if (index < 0) return;

    if (std::find (visibleItems.begin(), visibleItems.end(), index) == visibleItems.end())
    {
        search.clear();
        filterPalette();
        palScroll = 0.0f;
    }

    const float s = getWidth() / layout::W;
    const auto r = paletteItemRect (index);
    const auto a = paletteArea();
    const float top = a.getY() + 40.0f * s;

    if (r.getY() < top || r.getBottom() > a.getBottom())
        scrollPalette (r.getY() - top - 26.0f * s);     // keep heading visible
}

const qd::Block* QuadratEditor::firstOfKind (qd::Kind k) const
{
    for (const auto& b : viewBlocks) if (b.kind == k) return &b;
    return nullptr;
}

juce::String QuadratEditor::tourLabel() const
{
    if (tourCard == TourCard::finished) return utf8 ("Tutorial \xc2\xb7 done");

    return utf8 ("Tutorial \xc2\xb7 ") + tour::chapterName (guide.step().chapter);
}

// ── hints ──
QuadratEditor::Hints QuadratEditor::hints() const
{
    Hints m;
    if (! touring || tourCard != TourCard::step) return m;

    using Aim = tour::Aim;
    using Move = Hints::Move;

    const auto& st = guide.step();
    const float c = cellSize();

    auto spotAround = [] (juce::Point<float> p, float r)
    {
        return juce::Rectangle<float> (p.x - r, p.y - r, r * 2.0f, r * 2.0f);
    };

    switch (st.aim)
    {
        case Aim::place:
        case Aim::move:
        {
            const auto& sp = qd::spec (st.kind);
            const auto o = cellToScreen (st.x, st.y);
            m.outline = juce::Rectangle<float> (o.x, o.y, c * sp.cells, c * sp.rows).reduced (subSize() * 0.16f);
            m.carry = m.outline;
            m.move = Move::drag;
            m.to = m.outline.getCentre();

            // already placed: drag it
            if (const auto* b = firstOfKind (st.kind))
            {
                const auto t = tileRect (*b);
                m.spots.push_back (t);
                m.from = t.getCentre();
                m.outlineText = utf8 (st.aim == Aim::move ? "move " : "put ") + sp.name + " here";
            }
            else
            {
                const auto item = paletteItemRect (paletteIndexOf (st.kind));
                if (! item.isEmpty()) m.spots.push_back (item.reduced (4.0f, 2.0f));
                m.from = item.getCentre();
                m.outlineText = utf8 ("drop ") + sp.name + " here";
            }
            break;
        }

        case Aim::knob:
            if (const auto* b = firstOfKind (st.kind))
            {
                const auto r = controlRect (*b, tileRect (*b), st.index);
                m.spots.push_back (r);
                m.move = st.id == tour::Id::turnTune ? Move::up : Move::updown;   // tune goes up
                m.from = m.to = r.getCentre();
            }
            break;

        case Aim::strip:
            if (const auto* b = firstOfKind (st.kind))
            {
                const auto strip = optionRect (*b);
                const int count = juce::jmax (1, qd::optionCount (qd::spec (b->kind).options));
                const float w = strip.getWidth() / count;
                m.spots.push_back (strip);
                m.move = Move::click;
                m.from = m.to = { strip.getX() + w * (st.index + 0.5f), strip.getCentreY() };
            }
            break;

        case Aim::cable:
        {
            const auto* a = firstOfKind (st.kind);
            const auto* b = firstOfKind (st.toKind);
            if (a == nullptr || b == nullptr) break;

            m.from = portPos (*a, false, st.index);

            if (! st.toKnob)
                m.to = portPos (*b, true, st.toIndex);
            else if (b->drawer)
            {
                float jr = 0.0f;
                m.to = jackAt (*b, st.toIndex, jr);
            }
            else
                m.to = controlRect (*b, tileRect (*b), st.toIndex).getCentre();

            m.spots.push_back (spotAround (m.from, c * 0.17f));
            m.spots.push_back (spotAround (m.to, c * 0.17f));
            m.move = Move::cable;
            m.colour = colourFor (qd::portKind (a->kind, false, st.index));
            break;
        }

        case Aim::faders:
            if (const auto* b = firstOfKind (st.kind))
            {
                const auto t = tileRect (*b);
                m.spots.push_back (seqArea (t, *b).expanded (2.0f));
                m.move = Move::updown;
                m.from = m.to = seqCell (t, *b, 2, 0).getCentre();
                m.also = seqCell (t, *b, 5, 2).getCentre();
            }
            break;

        case Aim::tab:
            if (const auto* b = firstOfKind (st.kind))
            {
                const auto t = drawerTab (*b);
                m.spots.push_back (t.expanded (t.getWidth() * 0.9f));
                m.move = Move::click;
                m.from = m.to = t.getCentre();
                m.lens = true;
            }
            break;

        case Aim::ring:
            if (const auto* b = firstOfKind (st.kind); b != nullptr && b->drawer)
            {
                float jr = 0.0f;
                const auto jack = jackAt (*b, st.index, jr);
                m.spots.push_back (spotAround (jack, jr * 2.1f));
                m.move = Move::updown;
                m.from = { jack.x + jr * 1.5f, jack.y };
                m.to = jack;
                m.lens = true;
            }
            break;

        case Aim::name:
            if (const auto* b = firstOfKind (st.kind))
            {
                const auto t = tileRect (*b);
                const float unit = t.getHeight() / juce::jmax (1, qd::spec (b->kind).rows);
                const juce::Rectangle<float> box (t.getX(), t.getY(), t.getWidth() * 0.5f, unit * 0.28f);
                m.spots.push_back (box);
                m.move = Move::click;
                m.from = m.to = { box.getX() + box.getWidth() * 0.3f, box.getCentreY() };
            }
            break;

        case Aim::cell:
            if (editingNote < 0)
            {
                const auto o = cellToScreen (st.x, st.y);
                m.outline = juce::Rectangle<float> (o.x, o.y, c * 2.0f, c * 0.5f);
                m.outlineText = "double-click";
                m.move = Move::doubleClick;
                m.from = m.to = { m.outline.getX() + m.outline.getHeight() * 0.5f, m.outline.getCentreY() };
            }
            break;

        case Aim::button:
        {
            const CapButton* cap = &rollCap;

            if (st.button == tour::Button::stop)
                cap = guide.pausedOnce() || proc.isMuted() ? &startCap : &stopCap;

            const auto r = cap->getBounds().toFloat();
            const juce::Point<float> centre { r.getCentreX(), r.getY() + 14.0f };
            m.spots.push_back (spotAround (centre, 17.0f));
            m.move = Move::click;
            m.from = m.to = centre;
            break;
        }

        case Aim::none:
        default: break;
    }

    // clip to the field
    const auto f = fieldArea();
    m.outline = m.outline.getIntersection (f);

    for (auto& spot : m.spots)
        if (f.contains (spot.getCentre())) spot = spot.getIntersection (f);

    return m;
}

// ── the card ─────────────────────────────────────────────────────────────────
QuadratEditor::CardLayout QuadratEditor::cardLayout (const Hints& m) const
{
    CardLayout lay;
    if (! touring && ! welcoming) return lay;

    const float s = getWidth() / layout::W;
    const auto f = fieldArea();
    const float pad = cardPad * s, w = cardW * s, inner = w - pad * 2.0f;

    const auto& st = guide.step();
    const int total = (int) tour::steps().size();

    struct Btn { const char* label; int action; bool primary; bool right; };
    std::vector<Btn> btns;
    lay.mainSize = mainSize * s;

    switch (tourCard)
    {
        case TourCard::step:
            lay.head = juce::String (st.chapter + 1) + utf8 (" \xc2\xb7 ") + tour::chapterName (st.chapter);
            lay.counter = stepDoneAt > 0.0 ? juce::String ("done")
                                           : juce::String (guide.at() + 1) + " / " + juce::String (total);
            lay.main = utf8 (st.task);
            lay.why = utf8 (st.why);
            if (guide.at() > 0) btns.push_back ({ "back", back, false, false });
            btns.push_back ({ guide.reviewing() ? "next" : "skip", guide.reviewing() ? next : skip, false, false });
            btns.push_back ({ "exit", exitTour, false, true });
            break;

        case TourCard::chapter:
            lay.head = "chapter " + juce::String (st.chapter + 1) + " of "
                       + juce::String (tour::chapterCount()) + utf8 (" \xc2\xb7 done");
            lay.main = tour::chapterName (st.chapter);
            lay.mainSize = bigSize * s;
            lay.why = utf8 (tour::chapterSummary (st.chapter));
            btns.push_back ({ "exit", exitTour, false, false });
            btns.push_back ({ "continue", carryOn, true, true });
            break;

        case TourCard::leave:
            lay.head = "leave the tutorial";
            lay.main = "Keep what you built, or go back to the previous patch?";
            lay.why = "Going back puts your patch and its undo history back exactly as they were.";
            btns.push_back ({ "stay", stay, false, false });
            btns.push_back ({ "go back to the previous patch", goBack, false, true });
            btns.push_back ({ "keep this patch", keep, true, true });
            break;

        case TourCard::welcome:
            lay.head = "Tutorial";
            lay.main = "Welcome to Quadrat!";
            lay.mainSize = bigSize * s;
            lay.why = "Quadrat is a modular synth built to make it easy to make your first modular piece! "
                      "The tutorial walks you through your first patch, one click at a time. "
                      "It takes about ten minutes, and you can go back whenever you want.";
            btns.push_back ({ "skip", welcomeSkip, false, false });
            btns.push_back ({ "start the tutorial (recommended)", welcomeStart, true, true });
            break;

        case TourCard::finished:
            lay.head = "Yay!";
            lay.main = "You built your first patch :)";
            lay.mainSize = bigSize * s;
            lay.why = "Do you want to keep working on it or go back to the previous patch?";
            btns.push_back ({ "go back to the previous patch", goBack, false, true });
            btns.push_back ({ "keep this patch", keep, true, true });
            break;
    }

    const float headH = headSize * 1.2f * s;
    const float dotsH = tourCard == TourCard::step ? 15.0f * s : 5.0f * s;
    const float mainH = drawWrapped (nullptr, lay.main, lay.mainSize, 0.0f, 0.0f, inner);
    const float whyH = drawWrapped (nullptr, lay.why, whySize * s, 0.0f, 0.0f, inner);
    const float btnH = 26.0f * s;
    const float checkH = tourCard == TourCard::welcome ? 30.0f * s : 0.0f;     // checkbox row
    const float h = pad + headH + dotsH + 7.0f * s + mainH + 8.0f * s + whyH + 14.0f * s + checkH + btnH + pad;

    // dodge the hints
    juce::Rectangle<float> card;

    if (tourCard == TourCard::step)
    {
        const float gap = 12.0f * s;
        const juce::Rectangle<float> places[] = {
            { f.getRight() - w - gap, f.getBottom() - h - gap, w, h },
            { f.getRight() - w - gap, f.getY() + gap, w, h },
            { f.getX() + gap, f.getBottom() - h - gap, w, h },
            { f.getX() + gap, f.getY() + gap, w, h } };

        card = places[0];

        for (const auto& place : places)
        {
            bool clear = ! place.intersects (m.outline.expanded (8.0f * s));

            for (const auto& spot : m.spots)
                clear = clear && ! place.intersects (spot.expanded (8.0f * s));

            if (m.move == Hints::Move::drag || m.move == Hints::Move::cable)
                clear = clear && ! place.contains (m.from) && ! place.contains (m.to);

            if (clear) { card = place; break; }
        }
    }
    else
    {
        card = juce::Rectangle<float> (w, h).withCentre (f.getCentre());
    }

    lay.card = card;
    lay.headY = card.getY() + pad + headSize * 0.85f * s;
    lay.dotsY = lay.headY + 10.0f * s;
    lay.textY = card.getY() + pad + headH + dotsH + 7.0f * s;
    lay.whyY = lay.textY + mainH + 8.0f * s;

    // left: back/skip, right: go
    const float by = card.getBottom() - pad - btnH;
    float left = card.getX() + pad, right = card.getRight() - pad;

    for (auto it = btns.rbegin(); it != btns.rend(); ++it)
    {
        if (! it->right) continue;
        const float bw = juce::GlyphArrangement::getStringWidth (face (buttonSize * s), it->label) + 22.0f * s;
        lay.buttons.push_back ({ { right - bw, by, bw, btnH }, it->label, it->action, it->primary });
        right -= bw + 6.0f * s;
    }

    for (const auto& b : btns)
    {
        if (b.right) continue;
        const float bw = juce::GlyphArrangement::getStringWidth (face (buttonSize * s), b.label) + 22.0f * s;
        lay.buttons.push_back ({ { left, by, bw, btnH }, b.label, b.action, b.primary });
        left += bw + 6.0f * s;
    }

    if (checkH > 0.0f)
    {
        const juce::String label ("Don't show this again");
        const float bw = juce::GlyphArrangement::getStringWidth (face (buttonSize * s), label) + 24.0f * s;
        lay.buttons.push_back ({ { card.getX() + pad, by - checkH, bw, 20.0f * s }, label, dontShow, false, true });
    }

    return lay;
}

// ── drawing ──────────────────────────────────────────────────────────────────
void QuadratEditor::drawHints (juce::Graphics& g)
{
    const float s = getWidth() / layout::W;
    const auto m = hints();
    const double t = nowSeconds() - stepStartedAt;
    const bool done = stepDoneAt > 0.0;

    // veil with holes
    const juce::Rectangle<float> veiled (0.0f, layout::headerH * s, (float) getWidth(), getHeight() - layout::headerH * s);

    // squares get round rings
    auto corner = [s] (juce::Rectangle<float> r)
    {
        return std::abs (r.getWidth() - r.getHeight()) < 0.5f ? r.getWidth() * 0.5f : 5.0f * s;
    };

    juce::Path veil;
    veil.addRectangle (veiled);

    // holes only in veil
    if (tourCard == TourCard::step)
    {
        for (const auto& spot : m.spots)
            if (veiled.contains (spot.getCentre()))
                veil.addRoundedRectangle (spot.expanded (3.0f * s), corner (spot.expanded (3.0f * s)));

        if (! m.outline.isEmpty()) veil.addRoundedRectangle (m.outline.expanded (2.0f * s), 4.0f * s);
    }

    veil.setUsingNonZeroWinding (false);
    g.setColour (veilInk.withAlpha (tourCard == TourCard::step ? 0.36f : 0.5f));
    g.fillPath (veil);

    if (tourCard != TourCard::step) return;

    const float pulse = 0.5f + 0.5f * std::sin ((float) t * juce::MathConstants<float>::twoPi / 1.4f);
    const auto ring = done ? paint::green : lamp;

    for (const auto& spot : m.spots)
    {
        const auto r = spot.expanded (3.0f * s + (done ? 1.0f : pulse * 2.0f) * s);
        g.setColour (juce::Colours::black.withAlpha (0.28f));
        g.drawRoundedRectangle (r.expanded (1.6f * s), corner (r.expanded (1.6f * s)), 1.0f * s);
        g.setColour (ring.withAlpha (done ? 1.0f : 0.75f + 0.25f * pulse));
        g.drawRoundedRectangle (r, corner (r), 2.2f * s);
    }

    // dashed drop outline
    if (! m.outline.isEmpty() && ! done)
    {
        g.setColour (juce::Colours::white.withAlpha (0.2f));
        g.fillRoundedRectangle (m.outline, 3.0f * s);

        juce::Path box, dashed;
        box.addRoundedRectangle (m.outline, 3.0f * s);
        const float dash[] = { 6.0f * s, 4.0f * s };
        juce::PathStrokeType (1.6f * s).createDashedStroke (dashed, box, dash, 2);
        g.setColour (paint::ink.withAlpha (0.8f));
        g.fillPath (dashed);

        g.setColour (paint::ink.withAlpha (0.85f));
        drawLabel (g, m.outlineText, m.outline.getX(), m.outline.getCentreY() + 3.5f * s,
                     juce::jmin (10.0f * s, m.outline.getHeight() * 0.4f), 0.4f * s,
                     juce::Justification::centred, m.outline.getWidth());
    }

    if (done) return;

    // hide while dragging
    if (drag == Drag::none) drawGesture (g, m, t, 1.0f);

    // magnifier lens
    if (m.lens)
    {
        const auto f = fieldArea();
        const float radius = 58.0f * s, k = 2.8f;
        // lens down-right if room
        auto centre = m.to + juce::Point<float> (120.0f * s, 105.0f * s);
        if (centre.x + radius > f.getRight() - 8.0f * s) centre.x = m.to.x - 120.0f * s;
        if (centre.y + radius > f.getBottom() - 8.0f * s) centre.y = m.to.y - 105.0f * s;
        centre.x = juce::jlimit (f.getX() + radius + 8.0f * s, f.getRight() - radius - 8.0f * s, centre.x);
        centre.y = juce::jlimit (f.getY() + radius + 8.0f * s, f.getBottom() - radius - 8.0f * s, centre.y);

        const auto lens = juce::Rectangle<float> (radius * 2.0f, radius * 2.0f).withCentre (centre);

        g.setColour (juce::Colours::black.withAlpha (0.25f));
        g.fillEllipse (lens.translated (0.0f, 3.0f * s).expanded (2.0f * s));

        {
            juce::Graphics::ScopedSaveState keep (g);
            juce::Path clip;
            clip.addEllipse (lens);
            g.reduceClipRegion (clip);
            g.setColour (paint::field);
            g.fillEllipse (lens);
            g.addTransform (juce::AffineTransform::translation (-m.to.x, -m.to.y)
                              .scaled (k).translated (centre.x, centre.y));
            drawField (g);
            drawGesture (g, m, t, k);
        }

        g.setColour (paint::chrome);
        g.drawEllipse (lens, 3.0f * s);
        g.setColour (paint::ink);
        g.drawEllipse (lens.expanded (1.5f * s), 1.0f * s);

        const auto dir = (m.to - centre) / juce::jmax (1.0f, m.to.getDistanceFrom (centre));
        g.setColour (lamp);
        g.drawLine ({ centre + dir * (radius + 2.0f * s), m.to - dir * (10.0f * s) }, 1.6f * s);
    }
}

void QuadratEditor::drawGesture (juce::Graphics& g, const Hints& m, double t, float k)
{
    using Move = Hints::Move;
    const float s = getWidth() / layout::W / k;
    const float rd = 6.5f * s;

    auto dot = [&] (juce::Point<float> p, bool pressed)
    {
        const float r = pressed ? rd * 0.82f : rd;
        g.setColour (juce::Colours::black.withAlpha (0.3f));
        g.fillEllipse (p.x - r, p.y - r + 2.0f * s, r * 2.0f, r * 2.0f);
        g.setColour (lamp);
        g.fillEllipse (p.x - r, p.y - r, r * 2.0f, r * 2.0f);
        g.setColour (paint::ink);
        g.drawEllipse (p.x - r, p.y - r, r * 2.0f, r * 2.0f, 1.4f * s);
    };

    auto ripple = [&] (juce::Point<float> p, float phase)       // phase 0…1
    {
        if (phase < 0.0f || phase > 1.0f) return;
        const float r = rd * (1.0f + 2.2f * phase);
        g.setColour (lamp.withAlpha ((1.0f - phase) * 0.95f));
        g.drawEllipse (p.x - r, p.y - r, r * 2.0f, r * 2.0f, 2.0f * s);
        g.setColour (paint::ink.withAlpha ((1.0f - phase) * 0.4f));
        g.drawEllipse (p.x - r - 1.5f * s, p.y - r - 1.5f * s, r * 2.0f + 3.0f * s, r * 2.0f + 3.0f * s, 0.8f * s);
    };

    const float tf = (float) t;

    switch (m.move)
    {
        case Move::drag:
        case Move::cable:
        {
            const float period = 2.4f, p = std::fmod (tf, period);
            if (p > 2.05f) break;

            const float u = p < 0.35f ? 0.0f : p > 1.75f ? 1.0f : ease ((p - 0.35f) / 1.4f);
            const bool held = p < 1.75f;

            // same sag as cables
            const float sag = juce::jmin (46.0f, m.from.getDistanceFrom (m.to) * 0.22f);
            const juce::Point<float> c1 { m.from.x + 30.0f, m.from.y + sag }, c2 { m.to.x - 30.0f, m.to.y + sag };

            auto at = [&] (float v)
            {
                return m.move == Move::cable ? bezier (m.from, c1, c2, m.to, v)
                                             : m.from + (m.to - m.from) * v;
            };

            if (m.move == Move::cable)
            {
                juce::Path whole, dashed, drawn;
                whole.startNewSubPath (m.from);
                whole.cubicTo (c1, c2, m.to);
                const float dash[] = { 5.0f * s, 4.0f * s };
                juce::PathStrokeType (1.6f * s).createDashedStroke (dashed, whole, dash, 2);
                g.setColour (m.colour.withAlpha (0.55f));
                g.fillPath (dashed);

                drawn.startNewSubPath (m.from);
                for (int i = 1; i <= 24; ++i) drawn.lineTo (at (u * (float) i / 24.0f));
                g.setColour (m.colour.withAlpha (0.95f));
                g.strokePath (drawn, juce::PathStrokeType (2.4f * s));
            }
            else if (p >= 0.35f)
            {
                // ghost, field only
                juce::Graphics::ScopedSaveState keep (g);
                g.reduceClipRegion (fieldArea().toNearestInt());
                const auto ghost = m.carry.withCentre (at (u));
                g.setColour (juce::Colours::white.withAlpha (0.22f));
                g.fillRoundedRectangle (ghost, 3.0f * s);

                juce::Path box, dashed;
                box.addRoundedRectangle (ghost, 3.0f * s);
                const float dash[] = { 4.0f * s, 3.0f * s };
                juce::PathStrokeType (1.2f * s).createDashedStroke (dashed, box, dash, 2);
                g.setColour (lamp.withAlpha (0.9f));
                g.fillPath (dashed);
            }

            dot (at (u), held);
            break;
        }

        case Move::updown:
        {
            const float y = -std::sin (tf * juce::MathConstants<float>::twoPi / 1.6f) * 12.0f * s;
            const auto p = m.from + juce::Point<float> (0.0f, y);

            // up/down chevrons
            for (const float dir : { -1.0f, 1.0f })
            {
                juce::Path tri;
                const float cy = m.from.y + dir * 22.0f * s, w = 4.5f * s;
                tri.addTriangle (m.from.x - w, cy - dir * w * 0.6f, m.from.x + w, cy - dir * w * 0.6f,
                                 m.from.x, cy + dir * w * 0.8f);
                g.setColour (paint::ink.withAlpha (0.4f));
                g.fillPath (tri, juce::AffineTransform::translation (0.0f, 1.0f * s));
                g.setColour (lamp);
                g.fillPath (tri);
            }

            dot (p, true);

            if (m.also != juce::Point<float>())
            {
                ripple (m.also, std::fmod (tf, 1.2f) / 0.9f);
                dot (m.also, false);
            }
            break;
        }

        case Move::up:
        {
            // up only
            const float u = ease (std::fmod (tf, 1.3f) / 1.0f);
            const auto p = m.from + juce::Point<float> (0.0f, (10.0f - 26.0f * u) * s);

            juce::Path tri;
            const float cy = m.from.y - 26.0f * s, w = 5.0f * s;
            tri.addTriangle (m.from.x - w, cy + w * 0.6f, m.from.x + w, cy + w * 0.6f, m.from.x, cy - w * 0.8f);
            g.setColour (paint::ink.withAlpha (0.4f));
            g.fillPath (tri, juce::AffineTransform::translation (0.0f, 1.0f * s));
            g.setColour (lamp);
            g.fillPath (tri);

            dot (p, true);
            break;
        }

        case Move::click:
            ripple (m.from, std::fmod (tf, 1.2f) / 0.9f);

            // dot would hide it
            if (! m.lens) dot (m.from, std::fmod (tf, 1.2f) < 0.15f);
            break;

        case Move::doubleClick:
        {
            const float p = std::fmod (tf, 1.6f);
            ripple (m.from, p / 0.8f);
            ripple (m.from, (p - 0.22f) / 0.8f);
            dot (m.from, p < 0.1f || (p > 0.22f && p < 0.32f));
            break;
        }

        case Move::none:
        default: break;
    }
}

void QuadratEditor::drawCard (juce::Graphics& g)
{
    const float s = getWidth() / layout::W;
    const auto lay = cardLayout (hints());
    const auto card = lay.card;
    if (card.isEmpty()) return;

    const float pad = cardPad * s, inner = card.getWidth() - pad * 2.0f;
    const float x = card.getX() + pad;

    if (tourCard == TourCard::finished && confettiAt > 0.0) drawConfetti (g, card);

    g.setColour (juce::Colours::black.withAlpha (0.18f));
    g.fillRoundedRectangle (card.translated (0.0f, 4.0f * s).expanded (1.0f * s), 8.0f * s);
    g.setColour (juce::Colours::black.withAlpha (0.12f));
    g.fillRoundedRectangle (card.translated (0.0f, 1.5f * s), 7.0f * s);
    g.setGradientFill (juce::ColourGradient (paint::capTop, 0.0f, card.getY(),
                                             paint::plateHi, 0.0f, card.getBottom(), false));
    g.fillRoundedRectangle (card, 7.0f * s);
    g.setColour (paint::engrave.withAlpha (0.8f));
    g.drawRoundedRectangle (card, 7.0f * s, 1.0f);

    g.setColour (paint::inkSoft);
    drawLabel (g, lay.head, x, lay.headY, headSize * s, 0.5f * s, juce::Justification::left, inner);

    if (lay.counter.isNotEmpty())
    {
        const bool done = stepDoneAt > 0.0;
        g.setColour (done ? paint::green : paint::inkSoft);
        drawLabel (g, lay.counter, x, lay.headY, headSize * s, 0.5f * s, juce::Justification::right, inner, done);
    }

    // progress dots
    if (tourCard == TourCard::step)
    {
        const auto& all = tour::steps();
        const float d = 5.0f * s, gap = 3.5f * s, chapterGap = 9.0f * s;
        float cx = x;

        for (int i = 0; i < (int) all.size(); ++i)
        {
            if (i > 0 && all[(std::size_t) i].chapter != all[(std::size_t) i - 1].chapter) cx += chapterGap - gap;

            const auto r = juce::Rectangle<float> (cx, lay.dotsY - d * 0.5f, d, d);
            const bool here = i == guide.at();

            if (i < guide.at() || (here && stepDoneAt > 0.0))
            {
                g.setColour (paint::ink.withAlpha (0.75f));
                g.fillEllipse (r);
            }
            else if (here)
            {
                g.setColour (paint::ink);
                g.drawEllipse (r.expanded (0.8f * s), 1.3f * s);
            }
            else
            {
                g.setColour (paint::engrave.withAlpha (0.55f));
                g.fillEllipse (r);
            }

            cx += d + gap;
        }
    }

    g.setColour (paint::ink);
    drawWrapped (&g, lay.main, lay.mainSize, x, lay.textY, inner);
    g.setColour (paint::inkSoft);
    drawWrapped (&g, lay.why, whySize * s, x, lay.whyY, inner);

    for (const auto& b : lay.buttons)
    {
        const float r = 4.0f * s;

        if (b.checkbox)            // box + label
        {
            const auto box = juce::Rectangle<float> (b.r.getX(), b.r.getCentreY() - 6.5f * s, 13.0f * s, 13.0f * s);
            g.setColour (dontShowAgain ? paint::ink : paint::capTop);
            g.fillRoundedRectangle (box, 2.5f * s);
            g.setColour (paint::engrave);
            g.drawRoundedRectangle (box, 2.5f * s, 1.0f);

            if (dontShowAgain)
            {
                juce::Path tick;
                tick.startNewSubPath (box.getX() + box.getWidth() * 0.22f, box.getCentreY());
                tick.lineTo (box.getX() + box.getWidth() * 0.43f, box.getBottom() - box.getHeight() * 0.25f);
                tick.lineTo (box.getRight() - box.getWidth() * 0.2f, box.getY() + box.getHeight() * 0.25f);
                g.setColour (paint::plateHi);
                g.strokePath (tick, juce::PathStrokeType (1.6f * s));
            }

            g.setColour (paint::inkSoft);
            drawLabel (g, b.label, box.getRight() + 7.0f * s, b.r.getCentreY() + buttonSize * 0.36f * s,
                       buttonSize * s, 0.0f, juce::Justification::left, b.r.getWidth());
            continue;
        }

        if (b.primary)
        {
            g.setColour (paint::ink);
            g.fillRoundedRectangle (b.r, r);
            g.setColour (paint::plateHi);
        }
        else
        {
            g.setColour (paint::capTop);
            g.fillRoundedRectangle (b.r, r);
            g.setColour (paint::engrave);
            g.drawRoundedRectangle (b.r, r, 1.0f);
            g.setColour (paint::ink);
        }

        drawLabel (g, b.label, b.r.getX(), b.r.getCentreY() + buttonSize * 0.36f * s, buttonSize * s, 0.0f,
                     juce::Justification::centred, b.r.getWidth());
    }
}

// ── the end ──────────────────────────────────────────────────────────────────
void QuadratEditor::celebrate()
{
    confettiAt = nowSeconds();
    startTimerHz (60);
}

// confetti: signal-colour squares
void QuadratEditor::drawConfetti (juce::Graphics& g, juce::Rectangle<float> card)
{
    const float s = getWidth() / layout::W;
    const float t = (float) (nowSeconds() - confettiAt);
    if (t < 0.0f || t > 3.2f) return;

    const juce::Colour colours[] = { paint::sigAudio, paint::sigPitch, paint::sigGate, paint::sigCv };

    // ripples
    for (int i = 0; i < 4; ++i)
    {
        const float u = (t - (float) i * 0.14f) / 1.1f;
        if (u < 0.0f || u > 1.0f) continue;

        const float grow = ease (u) * 70.0f * s;
        g.setColour (colours[i].withAlpha ((1.0f - u) * 0.8f));
        g.drawRoundedRectangle (card.expanded (grow), 7.0f * s + grow * 0.2f, 2.5f * s * (1.0f - u * 0.6f));
    }

    juce::Random rng (0x51ad);
    const float gravity = 1100.0f * s;

    for (int i = 0; i < 84; ++i)
    {
        const float delay = rng.nextFloat() * 0.3f;
        const float x0 = card.getX() + card.getWidth() * (0.08f + rng.nextFloat() * 0.84f);
        const float vx = (x0 - card.getCentreX()) * (1.2f + rng.nextFloat() * 1.6f) + (rng.nextFloat() - 0.5f) * 180.0f * s;
        const float vy = -(420.0f + rng.nextFloat() * 380.0f) * s;
        const float size = (4.0f + rng.nextFloat() * 6.0f) * s;
        const float spin = (rng.nextFloat() - 0.5f) * 14.0f;
        const float phase0 = rng.nextFloat() * juce::MathConstants<float>::twoPi;
        const int which = rng.nextInt (9);          // 1/9 white
        const float life = t - delay;

        if (life <= 0.0f) continue;

        // drag, not freefall
        const float drag = 1.0f - std::exp (-1.6f * life);
        const float x = x0 + vx / 1.6f * drag;
        const float y = card.getY() + 6.0f * s + vy / 1.6f * drag + 0.5f * gravity * life * life * 0.55f;

        const float fade = juce::jlimit (0.0f, 1.0f, (3.0f - t) / 0.7f);
        const auto colour = which < 8 ? colours[which % 4] : lamp;

        // fake 3D tumble
        const float turn = std::cos (phase0 + life * spin * 0.7f);
        juce::Path sq;
        sq.addRectangle (-size * 0.5f, -size * 0.5f * std::abs (turn), size, juce::jmax (0.8f * s, size * std::abs (turn)));
        sq.applyTransform (juce::AffineTransform::rotation (phase0 + life * spin).translated (x, y));

        g.setColour (juce::Colours::black.withAlpha (0.18f * fade));
        g.fillPath (sq, juce::AffineTransform::translation (0.0f, 1.5f * s));
        g.setColour (colour.withAlpha (fade));
        g.fillPath (sq);
    }
}

// ── tour link ──
void QuadratEditor::toggleTour()
{
    if (! touring) startTour();
    else tourCard = tourCard == TourCard::leave ? TourCard::step : TourCard::leave;
    repaint();
}

// bottom row of the about page
juce::Rectangle<float> QuadratEditor::tourLink() const
{
    const float s = getWidth() / layout::W;
    const auto p = aboutPanel().reduced (26.0f * s, 22.0f * s);
    const auto label = touring ? juce::String ("Leave the Tutorial") : juce::String ("Start the Tutorial");
    const float w = juce::GlyphArrangement::getStringWidth (face (11.0f * s), label) + 4.0f * s;
    return { p.getRight() - w, p.getBottom() - 17.0f * s, w, 17.0f * s };
}

juce::Rectangle<float> QuadratEditor::githubLink() const
{
    const float s = getWidth() / layout::W;
    const auto p = aboutPanel().reduced (26.0f * s, 22.0f * s);
    const float w = juce::GlyphArrangement::getStringWidth (face (11.0f * s), "github.com/user863/Quadrat") + 4.0f * s;
    return { p.getX(), p.getBottom() - 17.0f * s, w, 17.0f * s };
}

void QuadratEditor::drawAboutLinks (juce::Graphics& g)
{
    const float s = getWidth() / layout::W;
    const auto mouse = getMouseXYRelative().toFloat();

    auto link = [&] (juce::Rectangle<float> r, const juce::String& text, juce::Justification just)
    {
        const bool hot = r.contains (mouse);
        g.setColour (hot ? paint::ink : paint::sigPitch);
        drawLabel (g, text, r.getX(), r.getY() + 12.5f * s, 11.0f * s, 0.2f * s, just, r.getWidth());
        g.fillRect (r.getX() + 1.0f * s, r.getY() + 15.0f * s, r.getWidth() - 3.0f * s, 1.0f);
    };

    link (githubLink(), "github.com/user863/Quadrat", juce::Justification::left);
    link (tourLink(), touring ? "Leave the Tutorial" : "Start the Tutorial", juce::Justification::right);
}

// ── first run ────────────────────────────────────────────────────────────────
juce::File QuadratEditor::settingsOverride;

static juce::File settingsFile()
{
    if (QuadratEditor::settingsOverride != juce::File()) return QuadratEditor::settingsOverride;

    juce::PropertiesFile::Options o;
    o.applicationName = "Quadrat";
    o.filenameSuffix = "settings";
    o.osxLibrarySubFolder = "Application Support";
    return o.getDefaultFile();
}

static juce::PropertiesFile::Options settingsOptions()
{
    juce::PropertiesFile::Options o;
    o.storageFormat = juce::PropertiesFile::storeAsXML;
    return o;
}

bool QuadratEditor::welcomeSeen()
{
    juce::PropertiesFile p (settingsFile(), settingsOptions());
    return p.getBoolValue ("welcomed", false);
}

void QuadratEditor::markWelcomeSeen()
{
    settingsFile().getParentDirectory().createDirectory();
    juce::PropertiesFile p (settingsFile(), settingsOptions());
    p.setValue ("welcomed", true);
    p.saveIfNeeded();
}

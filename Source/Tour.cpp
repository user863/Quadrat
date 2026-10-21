#include "Tour.h"
#include "Presets.h"
#include <cmath>

namespace tour
{

using K = qd::Kind;
constexpr int S = qd::kSub;

// tour layout
const std::vector<Step>& steps()
{
    static const std::vector<Step> all = {
        // ── 1 · first sound ──
        { Id::placeOsc, 0, "Drag osc from the list to the outlined square.",
          "An oscillator starts to make sound as soon as it's on the board, now we need to connect it!",
          Aim::place, K::osc, 4 * S, 0 },
        { Id::placeOut, 0, "Drag the out block so it touches the right side of the osc.",
          "Blocks that touch are connected. Now you should hear it :)",
          Aim::place, K::out, 7 * S, 0 },
        { Id::turnTune, 0, "Turn tune up: drag the knob upwards, go past the middle.",
          "This is how you adjust any of the knobs here!",
          Aim::knob, K::osc, 0, 0, 0 },
        { Id::pickSine, 0, "Click on sine on the bottom switch.",
          "In the oscillator you pick between waveforms, in other synths you can switch sounds.",
          Aim::strip, K::osc, 0, 0, 3 },

        // ── 2 · a melody ──
        { Id::placeClock, 1, "Drag the clock onto its outline.",
          "A clock gives the time everything ticks by, right now, it's set to 1/8s",
          Aim::place, K::clock, 0, 4 * S },
        { Id::placeSeq, 1, "Drag seq so it touches the clock.",
          "Now the clock gives the seq its timing!",
          Aim::place, K::seq, 2 * S, 4 * S },
        { Id::cablePitch, 1, "Draw a cable from seq's blue pitch output to osc's blue pitch input.",
          "Blue means pitch. The colours tell you what usually connects, but everything works.",
          Aim::cable, K::seq, 0, 0, 0, K::osc, 0 },
        { Id::editSteps, 1, "Drag a few of the faders, then click the switch under one step.",
          "The switch makes a step a note, a rest, or a tie that slides into the one before.",
          Aim::faders, K::seq },

        // ── 3 · shaped notes ──
        { Id::moveOut, 2, "Drag out to its new outline, away from the osc.",
          "Now, there's no connection. We could use cables, but first let's add something",
          Aim::move, K::out, 10 * S, 0 },
        { Id::placeLevel, 2, "Drop level into the gap.",
          "Now osc, level and out touch in a row: a chain, left to right.",
          Aim::place, K::level, 7 * S, 0 },
        { Id::placeEnv, 2, "Drag env onto its outline.",
          "An envelope turns each note into a shape: how it starts, holds and fades.",
          Aim::place, K::env, 7 * S, 4 * S },
        { Id::cableGate, 2, "Draw a cable from seq's red gate output to env's gate input.",
          "Red means gate: on or off.",
          Aim::cable, K::seq, 0, 0, 1, K::env, 0 },
        { Id::cableEnv, 2, "Draw a cable from env's out to level's cv input. Then try dec and rel.",
          "Yellow is modulation. The envelope now opens the level for every note.",
          Aim::cable, K::env, 0, 0, 0, K::level, 1 },

        // ── 4 · movement ──
        { Id::placeLfo, 3, "Drag lfo onto its outline.",
          "An LFO is a low-frequency oscillator. We can use it to change different parameters in the other blocks.",
          Aim::place, K::lfo, 11 * S, 4 * S },
        { Id::openDrawer, 3, "Open the osc's drawer: click the small triangle at its bottom right.",
          "Every knob has an input of its own.",
          Aim::tab, K::osc },
        { Id::cableFine, 3, "Draw a cable from the LFO's out to the fine jack in the drawer.",
          "Now, the fine knob sways by itself, controlled by the LFO",
          Aim::cable, K::lfo, 0, 0, 0, K::osc, 1, true },
        { Id::setDepth, 3, "Drag the ring around the fine jack up or down.",
          "The ring is how much of the LFO reaches the knob: here, how wide the vibrato is.",
          Aim::ring, K::osc, 0, 0, 1 },

        // ── 5 · extras ──
        { Id::pickPreset, 4, "Click the name seq and choose a pattern from its list.",
          "Many blocks have settings you can pick by name. Try acid line.",
          Aim::name, K::seq },
        { Id::stopStart, 4, "Press stop, then start.",
          "Stop silences everything at once.",
          Aim::button, K::count, 0, 0, 0, K::count, 0, false, Button::stop },
        { Id::writeNote, 4, "Double-click the outlined square, type a note, and press return.",
          "Write on the field itself. Notes are saved with the patch.",
          Aim::cell, K::count, 1 * S, 1 * S },
        { Id::pressRoll, 4, "Press roll.",
          "This generates a random patch, sometimes you get something pretty cool!",
          Aim::button, K::count, 0, 0, 0, K::count, 0, false, Button::roll },
        { Id::undoRoll, 4, "Press " QD_CMD " and Z to bring your patch back.",
          "Undo takes back anything, even a whole new patch.",
          Aim::none } };

    return all;
}

int chapterCount() { return 5; }

const char* chapterName (int c)
{
    static const char* names[] = { "first sound", "a melody", "shaping notes",
                                   "movement", "Misc" };
    return names[c >= 0 && c < 5 ? c : 0];
}

const char* chapterSummary (int c)
{
    static const char* lines[] = {
        "Blocks make sound, you can hear them when connected to an output.",
        "A clock keeps time, a sequencer plays notes, and cables connect everything.",
        "Blocks in a row form a chain, and an envelope gives every note a shape.",
        "Any knob can be moved by a cable, and the ring sets by how much.",
        "Presets, stop and start, notes, roll and undo: that's it!" };
    return lines[c >= 0 && c < 5 ? c : 0];
}

// ── checks ──
static const qd::Block* first (const qd::Patch& p, K k)
{
    for (const auto& b : p.blocks()) if (b.kind == k) return &b;
    return nullptr;
}

static const qd::Block* firstIn (const std::vector<qd::Block>& v, K k)
{
    for (const auto& b : v) if (b.kind == k) return &b;
    return nullptr;
}

// cable or touching?
static bool feeds (const qd::Patch& p, const qd::Block* a, const qd::Block* b)
{
    if (a == nullptr || b == nullptr) return false;

    for (const auto& c : p.cables())
        if (! c.toKnob && c.from == a->id && c.to == b->id && c.toPort == 0) return true;

    return a->x + a->subW() == b->x && b->y < a->y + a->subH() && a->y < b->y + b->subH();
}

static bool cabled (const qd::Patch& p, K from, int fromPort, K to, int toPort, bool toKnob)
{
    const auto* a = first (p, from);
    const auto* b = first (p, to);
    if (a == nullptr || b == nullptr) return false;

    for (const auto& c : p.cables())
        if (c.from == a->id && c.fromPort == fromPort && c.to == b->id
            && c.toPort == toPort && c.toKnob == toKnob) return true;

    return false;
}

// exact seq preset?
static bool onAPreset (const qd::Block& seq)
{
    for (int i = 0; i < blockPresets::count (K::seq); ++i)
    {
        qd::Block probe = seq;
        blockPresets::apply (K::seq, i, probe);

        bool same = probe.stepMode == seq.stepMode;

        for (int s = 0; s < qd::kSteps && same; ++s)
            same = std::fabs (probe.steps[(std::size_t) s] - seq.steps[(std::size_t) s]) < 1.0e-4f;

        if (same) return true;
    }

    return false;
}

bool Runner::complete (const Step& st, const Context& c) const
{
    const auto& p = c.patch;
    const auto* osc = first (p, K::osc);
    const auto* seq = first (p, K::seq);
    const auto* out = first (p, K::out);
    const auto* was = firstIn (baseline, st.kind);

    switch (st.id)
    {
        case Id::placeOsc:   return osc != nullptr;
        case Id::placeOut:   return feeds (p, osc, out);
        case Id::turnTune:   return osc != nullptr && was != nullptr
                                    && osc->knob[0] - was->knob[0] > 0.1f;          // up a fifth-ish
        case Id::pickSine:   return osc != nullptr && osc->option == 3;
        case Id::placeClock: return first (p, K::clock) != nullptr;
        case Id::placeSeq:   return feeds (p, first (p, K::clock), seq);
        case Id::cablePitch: return cabled (p, K::seq, 0, K::osc, 0, false);

        case Id::editSteps:
        {
            if (seq == nullptr || was == nullptr) return false;
            bool moved = false, switched = false;

            for (int s = 0; s < qd::kSteps; ++s)
            {
                moved = moved || std::fabs (seq->steps[(std::size_t) s] - was->steps[(std::size_t) s]) > 0.03f;
                switched = switched || seq->stepMode[(std::size_t) s] != was->stepMode[(std::size_t) s];
            }

            return moved && switched;
        }

        case Id::moveOut:    return out != nullptr && osc != nullptr && ! feeds (p, osc, out);
        case Id::placeLevel: { const auto* lv = first (p, K::level);
                               return feeds (p, osc, lv) && feeds (p, lv, out); }
        case Id::placeEnv:   return first (p, K::env) != nullptr;
        case Id::cableGate:  return cabled (p, K::seq, 1, K::env, 0, false);
        case Id::cableEnv:   return cabled (p, K::env, 0, K::level, 1, false);
        case Id::placeLfo:   return first (p, K::lfo) != nullptr;
        case Id::openDrawer: return osc != nullptr && osc->drawer;
        case Id::cableFine:  return cabled (p, K::lfo, 0, K::osc, 1, true);
        case Id::setDepth:   return osc != nullptr && was != nullptr
                                    && std::fabs (osc->depth[1] - was->depth[1]) > 0.05f;

        case Id::pickPreset:
        {
            if (seq == nullptr || was == nullptr || ! onAPreset (*seq)) return false;
            return seq->stepMode != was->stepMode || seq->steps != was->steps;
        }

        case Id::writeNote:
            return ! c.editingNote && p.notes().size() > notesBefore
                   && ! p.notes().back().text.empty();

        case Id::stopStart:  return sawMuted && ! c.muted;
        case Id::pressRoll:  return ! c.title.empty() && c.title != titleBefore;

        case Id::undoRoll:
        {
            if (p.blocks().size() != prerollPatch.size()) return false;

            for (std::size_t i = 0; i < prerollPatch.size(); ++i)
                if (p.blocks()[i].kind != prerollPatch[i].kind || p.blocks()[i].x != prerollPatch[i].x
                    || p.blocks()[i].y != prerollPatch[i].y) return false;

            return true;
        }

        default: return false;
    }
}

// regressions send us back
bool Runner::intact (const Step& st, const qd::Patch& p) const
{
    switch (st.aim)
    {
        case Aim::place: return first (p, st.kind) != nullptr;
        case Aim::cable: return cabled (p, st.kind, st.index, st.toKind, st.toIndex, st.toKnob);
        default:         return true;
    }
}

// ── runner ──
void Runner::start (const Context& c)
{
    current = 0;
    revisiting = false;
    prerollPatch.clear();
    begin (c);
}

void Runner::begin (const Context& c)
{
    baseline = c.patch.blocks();
    notesBefore = c.patch.notes().size();
    titleBefore = c.title;
    sawMuted = c.muted;

    if (step().id == Id::pressRoll) prerollPatch = c.patch.blocks();
}

bool Runner::endsChapter() const
{
    return last() || steps()[(std::size_t) current + 1].chapter != step().chapter;
}

Runner::Event Runner::update (const Context& c)
{
    // post-roll: skip checks
    if (current < (int) Id::pressRoll)
        for (int i = 0; i < current; ++i)
            if (! intact (steps()[(std::size_t) i], c.patch))
            {
                current = i;
                revisiting = false;
                begin (c);
                return Event::steppedBack;
            }

    if (c.muted) sawMuted = true;

    if (revisiting) return Event::none;

    return complete (step(), c) ? Event::completed : Event::none;
}

void Runner::next (const Context& c)
{
    revisiting = false;
    if (! last()) ++current;
    begin (c);
}

// review, no auto-advance
void Runner::back (const Context& c)
{
    if (current > 0) --current;
    revisiting = true;
    begin (c);
}

void Runner::skip (qd::Patch& p)
{
    const auto& st = step();

    auto place = [&p] (K k, int x, int y)
    {
        if (first (p, k) == nullptr) p.add (k, x, y);
    };

    switch (st.id)
    {
        case Id::placeOsc: case Id::placeOut: case Id::placeClock: case Id::placeSeq:
        case Id::placeEnv: case Id::placeLfo:
            place (st.kind, st.x, st.y);
            break;

        case Id::placeLevel:
            if (const auto* o = first (p, K::out); o != nullptr && o->x < 10 * S)
                p.move (o->id, 10 * S, 0);
            place (K::level, st.x, st.y);
            break;

        case Id::moveOut:
            if (const auto* o = first (p, K::out)) p.move (o->id, st.x, st.y);
            break;

        case Id::turnTune:          // octave up
            for (auto& b : p.blocks()) if (b.kind == K::osc) b.knob[0] = std::max (b.knob[0], 0.75f);
            break;

        case Id::pickSine:
            for (auto& b : p.blocks()) if (b.kind == K::osc) b.option = 3;
            break;

        case Id::openDrawer:
            for (auto& b : p.blocks()) if (b.kind == K::osc) b.drawer = true;
            break;

        case Id::cablePitch: case Id::cableGate: case Id::cableEnv: case Id::cableFine:
        {
            const auto* a = first (p, st.kind);
            const auto* b = first (p, st.toKind);
            if (a != nullptr && b != nullptr) p.connect (a->id, st.index, b->id, st.toIndex, st.toKnob);
            break;
        }

        default: break;             // nothing to do
    }

    settle (p);
    p.rebuild();
}

// clock -> 1/8
void Runner::settle (qd::Patch& p) const
{
    if (step().id == Id::placeClock)
        for (auto& b : p.blocks())
            if (b.kind == K::clock && b.option == 0) b.option = 3;
}

} // namespace tour

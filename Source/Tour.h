// tour steps + checks
#pragma once
#include "Blocks.h"
#include <string>
#include <vector>

namespace tour
{

enum class Id
{
    placeOsc, placeOut, turnTune, pickSine,                    // 1 · first sound
    placeClock, placeSeq, cablePitch, editSteps,                // 2 · a melody
    moveOut, placeLevel, placeEnv, cableGate, cableEnv,         // 3 · shaped notes
    placeLfo, openDrawer, cableFine, setDepth,                 // 4 · movement
    pickPreset, stopStart, writeNote, pressRoll, undoRoll,      // 5 · extras
    count
};

// what a step targets
enum class Aim
{
    none,        // the card alone
    place,       // palette -> outline
    move,        // block -> outline
    knob,        // a control
    strip,       // a block's switch strip
    cable,       // port -> port/jack
    faders,      // seq faders
    tab,         // a block's drawer triangle
    ring,        // depth ring
    name,        // block name
    cell,        // empty cell
    button       // header button
};

enum class Button { none, stop, start, roll };

struct Step
{
    Id id;
    int chapter;                        // 0-based
    const char* task;                 // the ask
    const char* why;                    // the why
    Aim aim;

    qd::Kind kind = qd::Kind::count;    // target block
    int x = 0, y = 0;                   // target cell
    int index = 0;                      // knob / port / step
    qd::Kind toKind = qd::Kind::count;  // cable dest block
    int toIndex = 0;                    // dest input/knob
    bool toKnob = false;                // drawer jack?
    Button button = Button::none;
};

const std::vector<Step>& steps();
int chapterCount();
const char* chapterName (int chapter);
const char* chapterSummary (int chapter);

// frame size, sub-cells
constexpr int kFrameW = 56, kFrameH = 28;

// plugin state for checks
struct Context
{
    const qd::Patch& patch;
    bool muted = false;
    std::string title;                  // rolled name or empty
    bool editingNote = false;
};

class Runner
{
public:
    // fresh field
    void start (const Context&);

    int at() const { return current; }
    const Step& step() const { return steps()[(std::size_t) current]; }
    bool last() const { return current == (int) steps().size() - 1; }
    bool endsChapter() const;
    bool reviewing() const { return revisiting; }

    enum class Event { none, completed, steppedBack };

    // poll; may step back
    Event update (const Context&);

    void next (const Context&);
    void back (const Context&);

    // skip = auto-build
    void skip (qd::Patch&);

    // post-step setup
    void settle (qd::Patch&) const;

    bool pausedOnce() const { return sawMuted; }   // stop seen?

private:
    bool complete (const Step&, const Context&) const;
    bool intact (const Step&, const qd::Patch&) const;
    void begin (const Context&);

    int current = 0;
    bool revisiting = false;
    bool sawMuted = false;

    // step baseline
    std::vector<qd::Block> baseline;
    std::size_t notesBefore = 0;
    std::string titleBefore;
    std::vector<qd::Block> prerollPatch;   // pre-roll patch
};

} // namespace tour

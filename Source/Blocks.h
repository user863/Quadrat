// patch + DSP, no JUCE
#pragma once
#include <vector>
#include <array>
#include <string>
#include <cstdint>
#include <cmath>
#include <memory>
#include <algorithm>

namespace qd
{

constexpr int kMaxIn = 4, kMaxOut = 4, kMaxKnobs = 12, kSteps = 8;

// quarter-square grid
constexpr int kSub = 4;
constexpr int kMacros = 16;      // host macros

enum class Ctl : unsigned char { knob = 0, slider, toggle, select };

// port kinds (colour only)
enum class Sig : unsigned char
{
    audio = 0,   // green, ±1
    pitch,       // blue, semis, 0 = C3
    gate,        // red, 0/1
    cv           // yellow, ±1
};

enum class Kind
{
    key = 0, osc, noise,                        // sources
    filter, level, damage, mix, lpg, ring,      // shape
    env, lfo, rand, slew, quant,                // modulation
    clock, seq, delay, div, logic, sswitch,     // time
    turing, euclid, chaos,                      // generative
    ctrl,                                       // host macros
    macro, drum, reson, scatter, mallet, xmod,  // vendored voices and processors
    pluck, reverb, out,                         // voices and output
    comp, limit,                                // dynamics
    meter, timbre, ball,                        // append-only
    midi, input,
    count
};

// pre-cut session ids
Kind kindFromLegacy (int oldIndex);

struct KnobSpec
{
    const char* name = nullptr;
    float def = 0.5f;
    Ctl type = Ctl::knob;
    const char* opts = nullptr;    // for Ctl::select
};

struct BlockSpec
{
    Kind kind;
    const char* name;
    const char* group;            // palette section
    int cells;                    // grid columns it occupies
    int rows = 1;                 // height in squares
    int numIn, numOut, numKnobs;
    const char* in[kMaxIn];
    const char* out[kMaxOut];
    KnobSpec knobs[kMaxKnobs];
    const char* options;          // "a|b|c", or empty
};

const BlockSpec& spec (Kind);
const char* searchTerms (Kind);     // search tags
int numKinds();
int optionCount (const char* options);
const char* describe (Kind);          // tooltip text
Sig  portKind (Kind, bool isInput, int index);
const char* portName (Kind, bool isInput, int index);
std::string optionName (const char* options, int index);
int clockDivisions (int option);       // ticks per bar

// ticks, 960/quarter
constexpr int kPpq = 960;

// shortcut key, as shown on screen
#if defined (__APPLE__)
 #define QD_CMD "command"
#else
 #define QD_CMD "Ctrl"
#endif

struct MidiNote
{
    int start = 0, length = kPpq / 4;
    int pitch = 60;                  // MIDI note, 60 = C3
    int velocity = 100;              // 1…127
};

// knobs are 0..1
struct Block
{
    Block() { knob.fill (0.5f); }

    Kind kind = Kind::osc;
    int  x = 0, y = 0;                       // top-left corner, in sub-cells
    int  id = 0;
    std::array<float, kMaxKnobs> knob;
    int  option = 0;

    // knob-input drawer open?
    bool drawer = false;

    // per-jack attenuators
    std::array<float, kMaxKnobs> depth { { 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1 } };
    // pitch lane, cv lane
    enum class Step : unsigned char { rise = 0, off, fall };

    std::array<float, kSteps> steps { { 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f } };
    std::array<float, kSteps> steps2 { { 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f } };
    std::array<Step, kSteps> stepMode { {} };

    // midi block phrase
    std::vector<MidiNote> phrase;
    int loopTicks = kPpq * 4;

    int cells() const { return spec (kind).cells; }
    int rows() const { return spec (kind).rows; }
    int subW() const { return cells() * kSub; }      // its size in sub-cells
    int subH() const { return rows() * kSub; }

    bool covers (int cx, int cy) const
    {
        return cx >= x && cx < x + subW() && cy >= y && cy < y + subH();
    }
};

struct Cable
{
    int from = -1, fromPort = 0;   // block id

    // toKnob: hits a knob
    int  to = -1, toPort = 0;
    bool toKnob = false;
};

// label tape
struct Note
{
    int x = 0, y = 0;            // sub-cells
    std::string text;
};

class Patch
{
public:
    Patch();
    ~Patch();                                      // Runtime is pimpl

    // ── editing ─────────────────────────────────────────────────────────────
    int  add (Kind, int x, int y);                 // id, or -1 if taken
    void remove (int id);
    bool move (int id, int x, int y);
    Block* find (int id);
    const Block* find (int id) const;
    Block* at (int cx, int cy);
    const std::vector<Block>& blocks() const { return items; }
    std::vector<Block>& blocks() { return items; }

    void connect (int fromId, int fromPort, int toId, int toPort, bool toKnob = false);
    void disconnect (int toId, int toPort, bool toKnob = false);   // one cable per destination
    void removeCablesFor (int id);
    const std::vector<Cable>& cables() const { return wires; }

    // implicit adjacency wires
    std::vector<Cable> touching() const;
    std::vector<Cable>& cables() { return wires; }

    // text labels
    std::vector<Note>& notes() { return written; }
    const std::vector<Note>& notes() const { return written; }

    void clear();
    bool isFree (int x, int y, int cells, int rows, int ignoreId = -1) const;  // size in squares

    // ── audio ───────────────────────────────────────────────────────────────
    void prepare (double sampleRate, int maxBlockSize);
    void setTransport (double bpm, double ppq, bool playing);
    void setMacro (int index, float value);        // 0…15, from the host
    float macro (int index) const;

    void noteOn (int note, float velocity);
    void noteOff (int note);
    void pitchBend (float semis);                  // key block, ±2 by default
    void sustain (bool down);                      // CC64
    void allNotesOff();
    void panic();          // notes off + reset
    void process (float* outL, float* outR, int numSamples);

    // sidechain audio, copied
    void setInput (const float* left, const float* right, int numSamples);
    bool hasInput() const { return inputLen > 0; }   // got audio this block?

    void rebuild();
    bool needsRebuild = true;

    float meter (int id) const;      // output peak
    float needle (int id) const;     // damped needle
    float liveKnob (int id, int knob) const;   // knob incl. modulation
    float gainReduction (int id) const;  // 0..1
    int   playhead (int id) const;   // seq step
    float phase (int id) const;      // clock phase
    void  ballAt (int id, float& x, float& y, float& floorLit, float& wallLit) const;

    static constexpr int kScope = 96;
    const float* scope (int id) const;   // scope ring

private:
    struct Runtime;
    void processBlock (Block&, Runtime&, int n);
    const float* input (const Block&, int port, int n);
    bool  connected (int id, int port) const;
    int   slot (int id) const;

    std::vector<Block> items;
    std::vector<Cable> wires;
    std::vector<Note> written;
    std::vector<int> order;                        // ids, in processing order
    std::vector<Cable> edges;                      // cables + adjacency
    std::vector<int> indexOfId;                    // id -> slot
    // state, keyed by id
    std::vector<std::unique_ptr<Runtime>> rt;
    std::vector<float> silence, sumBuf;

    double sr = 44100.0, bpm = 120.0, ppq = 0.0;
    bool transportPlaying = false;
    int nextId = 1, maxBlock = 512;

    // mono, last-note prio
    std::vector<int> held;
    std::vector<int> latched;                      // released under sustain
    bool sustainDown = false;
    float keyBend = 0.0f;
    std::array<float, kMacros> macros {};
    float keyPitch = 0.0f, keyGate = 0.0f, keyVel = 1.0f;
    std::vector<float> inputL, inputR;           // sidechain in
    int inputLen = 0;
};

} // namespace qd

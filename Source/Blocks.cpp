#include "Blocks.h"
#include "MIBlocks.h"
#include <cstring>

namespace qd
{

// ── specs ──
static constexpr BlockSpec kSpecs[] =
{
    // w h in out ctl
    { Kind::key,    "key",    "source", 2, 1, 0, 2, 1, {}, { "pitch", "gate" },
      { { "glide", 0.0f } }, "" },
    { Kind::osc,    "osc",    "source", 3, 2, 2, 1, 3, { "pitch", "fm" }, { "out" },
      { { "tune", 0.5f }, { "fine", 0.5f }, { "shape", 0.5f } }, "saw|pulse|tri|sine" },
    { Kind::noise,  "noise",  "source", 2, 1, 0, 1, 1, {}, { "out" },
      { { "tone", 0.5f, Ctl::slider } }, "" },

    { Kind::filter, "filter", "shape", 3, 2, 2, 1, 3, { "in", "cv" }, { "out" },
      { { "cutoff", 0.62f }, { "res", 0.2f }, { "cv", 0.5f } },
      "lp ladder|lp acid|lp 2-pole|bp 2-pole|hp 2-pole|lp sallen" },
    { Kind::level,  "level",  "shape", 3, 2, 2, 1, 4, { "in", "cv" }, { "out" },
      { { "gain", 0.5f }, { "cv", 1.0f }, { "offset", 0.5f }, { "invert", 0.0f, Ctl::toggle } },
      "\u00b112 dB|\u00b124 dB|\u00b148 dB" },
    { Kind::damage, "damage", "shape", 3, 1, 1, 1, 2, { "in" }, { "out" },
      { { "amount", 0.3f }, { "shape", 0.5f } }, "soft|fold|crush" },
    { Kind::mix,    "mix",    "shape", 2, 1, 3, 1, 3, { "a", "b", "c" }, { "out" },
      { { "a", 0.7f, Ctl::slider }, { "b", 0.7f, Ctl::slider }, { "c", 0.0f, Ctl::slider } }, "" },
    { Kind::lpg,    "lpg",    "shape", 2, 2, 2, 1, 2, { "in", "ping" }, { "out" },
      { { "decay", 0.35f }, { "level", 0.8f } }, "lpg|vca|filter" },
    { Kind::ring,   "ring",   "shape", 2, 1, 2, 1, 1, { "a", "b" }, { "out" },
      { { "mix", 1.0f, Ctl::slider } }, "" },
    { Kind::env,    "env",    "mod", 3, 2, 1, 2, 5, { "gate" }, { "out", "eoc" },
      { { "att", 0.08f }, { "dec", 0.35f }, { "sus", 0.6f }, { "rel", 0.35f }, { "curve", 0.5f } },
      "once|cycle" },
    { Kind::lfo,    "lfo",    "mod", 3, 1, 1, 1, 2, { "reset" }, { "out" },
      { { "rate", 0.4f }, { "depth", 1.0f } }, "sine|tri|saw|square|rand" },
    { Kind::rand,   "rand",   "mod", 2, 1, 1, 1, 2, { "clock" }, { "out" },
      { { "range", 0.6f }, { "slew", 0.0f } }, "" },
    { Kind::slew,   "slew",   "mod", 2, 1, 1, 1, 2, { "in" }, { "out" },
      { { "rise", 0.25f }, { "fall", 0.25f } }, "" },
    { Kind::quant,  "quant",  "mod", 3, 1, 1, 1, 2, { "pitch" }, { "out" },
      { { "shift", 0.5f }, { "range", 0.5f } }, "chrom|major|minor|penta|dorian|whole" },

    { Kind::clock,  "clock",  "time", 2, 2, 0, 1, 1, {}, { "out" },
      { { "width", 0.5f, Ctl::slider } },
      "1/1|1/2|1/4|1/8|1/8T|1/16|1/16T|1/32|1/32T|1/64|1/64T" },
    { Kind::seq,    "seq",    "time", 4, 3, 1, 3, 2, { "clock" }, { "pitch", "gate", "cv" },
      { { "range", 0.5f }, { "glide", 0.25f } }, "" },
    { Kind::delay,  "delay",  "time", 3, 1, 1, 1, 3, { "in" }, { "out" },
      { { "feedback", 0.35f }, { "mix", 0.3f, Ctl::slider }, { "tone", 0.7f } },
      "1/16|1/8|1/8.|1/4|1/4." },
    { Kind::div,    "div",    "time", 2, 1, 1, 2, 1, { "clock" }, { "out", "and" },
      { { "n", 0.25f } }, "divide|multiply" },
    { Kind::logic,  "logic",  "time", 2, 1, 2, 1, 1, { "a", "b" }, { "out" },
      { { "level", 1.0f, Ctl::slider } }, "and|or|xor|nor" },
    { Kind::sswitch, "switch", "time", 2, 1, 3, 1, 1, { "clock", "a", "b" }, { "out" },
      { { "glide", 0.0f, Ctl::slider } }, "" },

    { Kind::turing, "turing", "gen", 3, 1, 1, 2, 2, { "clock" }, { "cv", "gate" },
      { { "mutate", 0.15f }, { "range", 0.6f } }, "2|3|4|6|8|12|16" },
    { Kind::euclid, "euclid", "gen", 3, 1, 1, 2, 3, { "clock" }, { "out", "off" },
      { { "fills", 0.3f }, { "rotate", 0.0f }, { "chance", 1.0f } }, "8|12|16|32" },
    { Kind::chaos,  "chaos",  "gen", 2, 1, 0, 2, 2, {}, { "x", "y" },
      { { "rate", 0.4f }, { "range", 0.6f } }, "lorenz|rossler" },

    { Kind::ctrl,   "ctrl",   "mod", 2, 1, 0, 4, 1, {}, { "a", "b", "c", "d" },
      { { "bank", 0.0f, Ctl::select, "1|2|3|4" } }, "" },

    { Kind::macro,  "macro",  "mi", 4, 2, 3, 2, 6, { "pitch", "trig", "fm" }, { "out", "aux" },
      { { "harm", 0.4f }, { "timbre", 0.5f }, { "morph", 0.5f },
        { "decay", 0.5f, Ctl::slider }, { "colour", 0.5f, Ctl::slider }, { "fm", 0.0f } },
      // voice registration order
      "va vcf|phase dist|six op|six op 2|six op 3|wave terrain|string machine|chiptune|"
      "virtual analog|waveshape|fm|grain|additive|wavetable|chord|speech|"
      "swarm|noise|particle|string|modal|bass drum|snare|hi-hat" },
    { Kind::drum,   "drum",   "mi", 4, 2, 1, 1, 4, { "trig" }, { "out" },
      { { "tone", 0.5f }, { "decay", 0.4f }, { "snap", 0.5f }, { "punch", 0.5f, Ctl::slider } },
      "bass|snare|hat|fm drum" },
    { Kind::reson,  "reson",  "mi", 3, 2, 4, 2, 5, { "in", "pitch", "strum", "pos" }, { "odd", "even" },
      { { "struct", 0.35f }, { "bright", 0.5f }, { "damp", 0.55f },
        { "pos", 0.3f, Ctl::slider }, { "voices", 0.0f, Ctl::select, "1|2|4" } },
      "modal|sympathetic|string|fm|quantized|reverb" },
    { Kind::scatter, "scatter", "mi", 4, 2, 4, 2, 6, { "left", "right", "pitch", "freeze" }, { "l", "r" },
      { { "pos", 0.4f }, { "size", 0.5f }, { "dens", 0.45f }, { "texture", 0.5f },
        { "blend", 0.6f, Ctl::slider }, { "hold", 0.0f, Ctl::toggle } },
      "granular|stretch|delay|spectral" },
    { Kind::mallet, "mallet", "mi", 4, 2, 3, 2, 6, { "in", "pitch", "gate" }, { "l", "r" },
      { { "blow", 0.3f }, { "strike", 0.5f }, { "geom", 0.5f }, { "bright", 0.55f },
        { "damp", 0.55f, Ctl::slider }, { "space", 0.3f, Ctl::slider } }, "modal|string|drum" },
    { Kind::xmod,   "xmod",   "mi", 3, 2, 2, 2, 5, { "a", "b" }, { "l", "r" },
      { { "algo", 0.3f }, { "timbre", 0.5f }, { "drive a", 0.7f }, { "drive b", 0.7f },
        { "level", 0.6f, Ctl::slider } }, "xfade|fold|ring|digital" },

    { Kind::pluck,  "pluck",  "out", 3, 1, 2, 1, 3, { "in", "pitch" }, { "out" },
      { { "decay", 0.7f }, { "damp", 0.5f }, { "mix", 1.0f, Ctl::slider } }, "string|bell" },
    { Kind::reverb, "reverb", "out", 2, 1, 1, 1, 2, { "in" }, { "out" },
      { { "size", 0.5f }, { "mix", 0.3f, Ctl::slider } }, "" },
    { Kind::out,    "out",    "out", 2, 1, 2, 0, 2, { "left", "right" }, {},
      { { "level", 0.8f, Ctl::slider }, { "pan", 0.5f } }, "" },

    // appended: ids are saved
    { Kind::comp,   "comp",   "shape", 4, 2, 2, 1, 5, { "in", "key" }, { "out" },
      { { "thresh", 0.72f }, { "ratio", 0.35f }, { "attack", 0.3f }, { "release", 0.45f },
        { "makeup", 0.0f, Ctl::slider } }, "" },

    { Kind::limit,  "limit",  "shape", 3, 2, 1, 1, 3, { "in" }, { "out" },
      { { "ceiling", 0.92f }, { "release", 0.35f }, { "gain", 0.0f, Ctl::slider } }, "" },

    { Kind::meter,  "meter",  "out", 2, 2, 1, 1, 2, { "in" }, { "thru" },
      { { "range", 0.5f }, { "damping", 0.4f, Ctl::slider } }, "peak|rms" },

    // model picked by knob
    { Kind::timbre, "timbre", "mi", 3, 2, 2, 1, 4, { "pitch", "trig" }, { "out" },
      { { "model", 0.0f }, { "tune", 0.5f }, { "timbre", 0.5f }, { "colour", 0.5f } },
      "csaw|morph|saw square|sine triangle|buzz|square sub|saw sub|square sync|saw sync|"
      "triple saw|triple square|triple tri|triple sine|triple ring|saw swarm|saw comb|toy|"
      "filter lp|filter pk|filter bp|filter hp|vosim|vowel|vowel fof|harmonics|"
      "fm|feedback fm|chaotic fm|plucked|bowed|blown|fluted|struck bell|struck drum|"
      "kick|cymbal|snare|wavetables|wave map|wave line|wave paraphonic|filtered noise|"
      "twin peaks|clocked noise|granular|particle noise|digital mod|question mark" }
,

    // ball in a box
    { Kind::ball,   "ball",   "gen", 2, 3, 1, 4, 4, { "kick" }, { "x", "y", "floor", "wall" },
      { { "throw", 0.7f }, { "drift", 0.35f }, { "gravity", 0.4f }, { "bounce", 0.8f } },
      "walls|wrap" },

    // pasted notes, clocked
    { Kind::midi,   "midi",   "time", 4, 2, 2, 3, 1, { "clock", "reset" }, { "pitch", "gate", "vel" },
      { { "transpose", 0.5f } },
      "1/16|1/8|1/4" },

    // sidechain audio in
    { Kind::input,  "in",     "source", 2, 1, 0, 2, 1, {}, { "left", "right" },
      { { "gain", 0.6f, Ctl::slider } }, "" }
};

// old id -> new kind
Kind kindFromLegacy (int old)
{
    static const Kind was[] = {
        Kind::key, Kind::osc, Kind::noise, Kind::filter,
        Kind::level,                       // vca
        Kind::damage,                      // drive
        Kind::mix, Kind::lpg, Kind::ring,
        Kind::damage,                      // crush
        Kind::env, Kind::lfo, Kind::rand,
        Kind::level,                       // att
        Kind::slew,
        Kind::env,                         // func -> env cycle
        Kind::quant, Kind::clock, Kind::seq, Kind::delay, Kind::div, Kind::logic,
        Kind::sswitch, Kind::turing, Kind::euclid, Kind::chaos, Kind::ctrl,
        Kind::count,                       // eq, retired
        Kind::meter,
        Kind::macro,
        Kind::timbre,
        Kind::drum, Kind::reson, Kind::scatter, Kind::mallet, Kind::xmod,
        Kind::pluck, Kind::reverb, Kind::out,
        Kind::level,                       // gain
        Kind::comp, Kind::limit };

    const int n = (int) (sizeof (was) / sizeof (was[0]));
    return (old >= 0 && old < n) ? was[old] : Kind::count;
}

// table must match Kind
constexpr bool specsMatchKinds()
{
    for (int i = 0; i < (int) Kind::count; ++i)
        if (kSpecs[i].kind != (Kind) i) return false;

    return true;
}

static_assert (sizeof (kSpecs) / sizeof (kSpecs[0]) == (std::size_t) Kind::count,
               "every Kind needs a BlockSpec");
static_assert (specsMatchKinds(), "kSpecs must be written in Kind order");

const BlockSpec& spec (Kind k)
{
    const int i = static_cast<int> (k);
    return kSpecs[(i >= 0 && i < static_cast<int> (Kind::count)) ? i : 0];
}

int numKinds() { return static_cast<int> (Kind::count); }

const char* searchTerms (Kind k)
{
    switch (k)
    {
        case Kind::key:     return "midi keyboard notes input pitch gate velocity";
        case Kind::osc:     return "oscillator vco tone waveform saw square pulse triangle sine";
        case Kind::noise:   return "white pink random hiss source";
        case Kind::filter:  return "vcf lowpass highpass bandpass resonance ladder moog 303 cutoff";
        case Kind::level:   return "vca amp amplifier gain volume attenuator attenuverter invert";
        case Kind::damage:  return "distortion overdrive drive saturation fold wavefolder bitcrush crusher lofi";
        case Kind::mix:     return "mixer sum merge combine";
        case Kind::lpg:     return "low pass gate filter vca vactrol pluck";
        case Kind::ring:    return "ring modulator am rm metallic";
        case Kind::env:     return "envelope adsr eg contour attack decay";
        case Kind::lfo:     return "modulation low frequency wobble vibrato tremolo";
        case Kind::rand:    return "sample hold random stepped";
        case Kind::slew:    return "glide portamento lag smooth";
        case Kind::quant:   return "quantizer scale notes key";
        case Kind::clock:   return "tempo bpm sync pulse timing metronome";
        case Kind::seq:     return "sequencer steps pattern melody";
        case Kind::delay:   return "echo tape feedback";
        case Kind::div:     return "clock divider multiplier ratchet";
        case Kind::logic:   return "and or xor nor boolean gate";
        case Kind::sswitch: return "switch sequential selector router";
        case Kind::turing:  return "random loop shift register generative melody";
        case Kind::euclid:  return "euclidean rhythm trigger drums pattern";
        case Kind::chaos:   return "lorenz rossler attractor random";
        case Kind::ctrl:    return "macro automation host parameter knob";
        case Kind::macro:   return "voice synth engine fm wavetable speech chord";
        case Kind::drum:    return "kick snare hihat hat percussion";
        case Kind::reson:   return "resonator string modal physical sympathetic";
        case Kind::scatter: return "granular texture freeze grains cloud";
        case Kind::mallet:  return "physical model struck bowed blown";
        case Kind::xmod:    return "cross modulation vocoder crossfade fold";
        case Kind::pluck:   return "karplus strong string physical";
        case Kind::reverb:  return "space room hall ambience";
        case Kind::out:     return "output master speaker main";
        case Kind::comp:    return "compressor dynamics sidechain ducking";
        case Kind::limit:   return "limiter dynamics ceiling loudness";
        case Kind::meter:   return "vu level peak rms monitor";
        case Kind::timbre:  return "oscillator models voice macro";
        case Kind::ball:    return "bouncing physics random gate";
        case Kind::midi:    return "notes piano roll player phrase logic paste";
        case Kind::input:   return "audio in microphone mic sidechain line external";
        default:        return "";
    }
}

// ── per-block state ──────────────────────────────────────────────────────────
struct Patch::Runtime
{
    std::array<std::vector<float>, kMaxOut> out;

    // post-mod knobs, for UI
    std::array<float, kMaxKnobs> live { {} };

    double phase = 0.0;
    float ic1 = 0.0f, ic2 = 0.0f;          // filter
    float envVal = 0.0f; int stage = 0;    // envelope
    float sampled = 0.0f, slewed = 0.0f;   // random
    float det = 0.0f;                      // compressor / limiter detector

    // ball state, -1..1
    float bx = 0.0f, by = 1.0f, bvx = 0.4f, bvy = 0.0f;
    float restFor = 0.0f, floorLit = 0.0f, wallLit = 0.0f;
    float grNow = 0.0f;                    // GR 0..1
    float tone = 0.0f, pink = 0.0f;        // noise / delay tone
    float glided = 0.0f;
    bool  prevGate = false;
    int   step = 0;
    float gateTimer = 0.0f;

    std::vector<float> delayBuf;
    int delayIdx = 0;

    // reverb: 4 diffusers + 8-line FDN
    std::array<std::vector<float>, 4> diff;
    std::array<int, 4> diffPos {};
    std::array<std::vector<float>, 8> line;
    std::array<float, 8> lineLp {};
    std::array<int, 8> linePos {};
    double lfo = 0.0;

    float meter = 0.0f;
    std::array<float, Patch::kScope> scope {};
    std::uint32_t seed = 22222u;

    std::array<float, 4> ladder { { 0.0f, 0.0f, 0.0f, 0.0f } };   // four-pole
    std::array<float, 4> ladderZ { { 0.0f, 0.0f, 0.0f, 0.0f } };
    std::uint32_t shift = 0xA5A5u;      // turing machine register
    int pulseCount = 0;                 // div / euclid / switch
    float funcVal = 0.0f; int funcStage = 0;
    double cx = 0.1, cy = 0.0, cz = 0.0;   // chaos state
    float crushHold = 0.0f; double crushPhase = 0.0;
    int resonLen = 100;

    // midi playback state
    int midiStep = -1;                     // -1 = not started
    double midiPos = 0.0, midiRate = 0.0;
    int sinceTick = 0, midiNote = -1, retrig = 0;
    float midiPitch = 0.0f, midiVel = 0.0f;
    bool prevReset = false;
    std::array<std::array<float, 5>, 5> eqCoef {};
    std::array<std::array<float, 2>, 5> eqZ {};
    std::unique_ptr<mi::Unitary> unit;      // vendored engine

    float rnd()
    {
        seed = seed * 1664525u + 1013904223u;
        return seed / 2147483648.0f - 1.0f;      // −1 … 1
    }
};

// ── helpers ──────────────────────────────────────────────────────────────────
static int imin (int a, int b) { return a < b ? a : b; }

// MI unit, or -1
static int vendoredUnit (Kind k)
{
    switch (k)
    {
        case Kind::macro:   return (int) mi::Unit::macro;
        case Kind::reson:   return (int) mi::Unit::reson;
        case Kind::scatter: return (int) mi::Unit::scatter;
        case Kind::mallet:  return (int) mi::Unit::mallet;
        case Kind::xmod:    return (int) mi::Unit::xmod;
        case Kind::drum:    return (int) mi::Unit::drum;
        case Kind::timbre:  return (int) mi::Unit::timbre;
        default:            return -1;
    }
}
static float clampf (float v, float a, float b) { return v < a ? a : (v > b ? b : v); }
static float lerp (float a, float b, float t) { return a + (b - a) * t; }

// PolyBLEP
// reverb geometry @ 48k, ~prime
static constexpr int kDiffLen[4] = { 142, 107, 379, 277 };
static constexpr int kLineLen[8] = { 1117, 1277, 1433, 1601, 1753, 1933, 2089, 2269 };
static constexpr float kWetGain = 2.0f;      // calibrated by test
static constexpr float kWetCeil = 0.7f;      // wet limiter threshold
static constexpr double kLineRate[8] = { 0.23, 0.31, 0.41, 0.53, 0.67, 0.79, 0.94, 1.09 };

static constexpr float kBusGain = 0.5f;      // master headroom

// soft ceiling, NaN-proof
static float softClip (float x)
{
    constexpr float t = 0.85f;
    if (x >  t) return  t + std::tanh ((x - t) / (1.0f - t)) * (1.0f - t);
    if (x < -t) return -t + std::tanh ((x + t) / (1.0f - t)) * (1.0f - t);
    return x;
}

static float polyBlep (double t, double dt)
{
    if (t < dt)          { const double x = t / dt;        return (float) (x + x - x * x - 1.0); }
    if (t > 1.0 - dt)    { const double x = (t - 1.0) / dt; return (float) (x * x + x + x + 1.0); }
    return 0.0f;
}

static double divisionBeats (const char* name)
{
    const std::string s (name);
    if (s == "1/1")  return 4.0;
    if (s == "1/2")  return 2.0;
    if (s == "1/4")  return 1.0;
    if (s == "1/4.") return 1.5;
    if (s == "1/8")  return 0.5;
    if (s == "1/8.") return 0.75;
    if (s == "1/8T") return 1.0 / 3.0;
    if (s == "1/16") return 0.25;
    if (s == "1/16T") return 1.0 / 6.0;
    if (s == "1/32") return 0.125;
    if (s == "1/32T") return 1.0 / 12.0;
    if (s == "1/64") return 0.0625;
    if (s == "1/64T") return 1.0 / 24.0;
    return 0.5;
}

// ticks per bar
int clockDivisions (int option)
{
    const auto& sp = spec (Kind::clock);
    std::string all (sp.options), part;
    int n = 0;

    for (char ch : all + "|")
    {
        if (ch == '|')
        {
            if (n++ == option) return std::max (1, (int) std::lround (4.0 / divisionBeats (part.c_str())));
            part.clear();
        }
        else part += ch;
    }

    return 4;
}

static std::string optionAt (const char* options, int index)
{
    std::string all (options ? options : ""), part;
    int n = 0;

    for (std::size_t i = 0; i <= all.size(); ++i)
    {
        if (i == all.size() || all[i] == '|')
        {
            if (n == index) return part;
            part.clear();
            ++n;
        }
        else part += all[i];
    }

    return {};
}

int optionCount (const char* options)
{
    if (options == nullptr || *options == 0) return 0;
    int n = 1;
    for (const char* p = options; *p; ++p) if (*p == '|') ++n;
    return n;
}

// ── patch ────────────────────────────────────────────────────────────────────
Patch::Patch()
{
    silence.assign (2048, 0.0f);
    sumBuf.assign (2048, 0.0f);
    held.reserve (128);                // no audio-thread allocs
    latched.reserve (128);
}
Patch::~Patch() = default;

bool Patch::isFree (int x, int y, int cells, int rows, int ignoreId) const
{
    for (const auto& b : items)
    {
        if (b.id == ignoreId) continue;
        if (x < b.x + b.subW() && b.x < x + cells * kSub
            && y < b.y + b.subH() && b.y < y + rows * kSub) return false;
    }
    return true;
}

int Patch::add (Kind k, int x, int y)
{
    if (! isFree (x, y, spec (k).cells, spec (k).rows)) return -1;

    Block b;
    b.kind = k; b.x = x; b.y = y; b.id = nextId++;

    for (int i = 0; i < kMaxKnobs; ++i)
        b.knob[(std::size_t) i] = spec (k).knobs[i].name ? spec (k).knobs[i].def : 0.5f;

    for (int i = 0; i < kSteps; ++i)
        b.steps[(std::size_t) i] = 0.5f + 0.06f * i;

    items.push_back (b);
    rebuild();
    return b.id;
}

void Patch::remove (int id)
{
    removeCablesFor (id);
    items.erase (std::remove_if (items.begin(), items.end(),
                                 [id] (const Block& b) { return b.id == id; }), items.end());
    rebuild();
}

bool Patch::move (int id, int x, int y)
{
    auto* b = find (id);
    if (b == nullptr || ! isFree (x, y, b->cells(), b->rows(), id)) return false;
    b->x = x; b->y = y;
    rebuild();
    return true;
}

Block* Patch::find (int id)
{
    for (auto& b : items) if (b.id == id) return &b;
    return nullptr;
}

const Block* Patch::find (int id) const
{
    for (const auto& b : items) if (b.id == id) return &b;
    return nullptr;
}

Block* Patch::at (int cx, int cy)
{
    for (auto& b : items) if (b.covers (cx, cy)) return &b;
    return nullptr;
}

void Patch::connect (int fromId, int fromPort, int toId, int toPort, bool toKnob)
{
    if (fromId == toId) return;
    disconnect (toId, toPort, toKnob);         // one cable per destination
    wires.push_back ({ fromId, fromPort, toId, toPort, toKnob });
    rebuild();
}

void Patch::disconnect (int toId, int toPort, bool toKnob)
{
    wires.erase (std::remove_if (wires.begin(), wires.end(),
                                 [=] (const Cable& c)
                                 { return c.to == toId && c.toPort == toPort && c.toKnob == toKnob; }),
                 wires.end());
    rebuild();
}

void Patch::setMacro (int i, float v)
{
    if (i >= 0 && i < kMacros) macros[(std::size_t) i] = v;
}

float Patch::macro (int i) const
{
    return (i >= 0 && i < kMacros) ? macros[(std::size_t) i] : 0.0f;
}

void Patch::removeCablesFor (int id)
{
    wires.erase (std::remove_if (wires.begin(), wires.end(),
                                 [=] (const Cable& c) { return c.from == id || c.to == id; }),
                 wires.end());
    rebuild();
}

void Patch::clear()
{
    items.clear(); wires.clear(); written.clear(); order.clear(); rt.clear();
    rebuild();
}

void Patch::prepare (double sampleRate, int maxBlockSize)
{
    const double rate = sampleRate > 0.0 ? sampleRate : 44100.0;

    // new rate: drop buffers
    if (rate != sr) rt.clear();

    sr = rate;
    maxBlock = std::max (32, maxBlockSize);
    silence.assign ((std::size_t) maxBlock, 0.0f);
    sumBuf.assign ((std::size_t) maxBlock, 0.0f);
    inputL.assign ((std::size_t) maxBlock, 0.0f);
    inputR.assign ((std::size_t) maxBlock, 0.0f);
    inputLen = 0;
    rebuild();
}

void Patch::setInput (const float* left, const float* right, int n)
{
    inputLen = left == nullptr ? 0 : std::min (n, (int) inputL.size());

    if (inputLen == 0) return;

    std::copy (left, left + inputLen, inputL.begin());
    std::copy (right != nullptr ? right : left, (right != nullptr ? right : left) + inputLen, inputR.begin());
}

void Patch::setTransport (double b, double p, bool playing)
{
    bpm = b > 1.0 ? b : 120.0;

    // host clock, else free-run
    if (playing) ppq = p;

    transportPlaying = playing;
}

void Patch::noteOn (int note, float velocity)
{
    held.erase (std::remove (held.begin(), held.end(), note), held.end());
    latched.erase (std::remove (latched.begin(), latched.end(), note), latched.end());
    if (held.size() < 128) held.push_back (note);
    keyVel = velocity;
}

void Patch::noteOff (int note)
{
    // pedal down: keep it sounding
    if (sustainDown)
    {
        if (std::find (held.begin(), held.end(), note) != held.end()
            && std::find (latched.begin(), latched.end(), note) == latched.end()
            && latched.size() < 128)
            latched.push_back (note);
        return;
    }

    held.erase (std::remove (held.begin(), held.end(), note), held.end());
}

void Patch::pitchBend (float semis) { keyBend = semis; }

void Patch::sustain (bool down)
{
    sustainDown = down;
    if (down) return;

    for (const int note : latched)
        held.erase (std::remove (held.begin(), held.end(), note), held.end());
    latched.clear();
}

void Patch::allNotesOff() { held.clear(); latched.clear(); sustainDown = false; }

// hard reset to silence
void Patch::panic()
{
    held.clear();
    latched.clear();
    sustainDown = false;
    keyBend = 0.0f;
    keyGate = 0.0f;

    for (auto& up : rt)
    {
        if (up == nullptr) continue;
        auto& r = *up;
        r.envVal = 0.0f; r.stage = 0; r.funcVal = 0.0f; r.funcStage = 0;
        r.ic1 = r.ic2 = 0.0f; r.tone = 0.0f; r.slewed = 0.0f; r.meter = 0.0f;
        r.ladder.fill (0.0f); r.ladderZ.fill (0.0f);
        r.lineLp.fill (0.0f);
        std::fill (r.delayBuf.begin(), r.delayBuf.end(), 0.0f);
        for (auto& d : r.diff) std::fill (d.begin(), d.end(), 0.0f);
        for (auto& l : r.line) std::fill (l.begin(), l.end(), 0.0f);
    }
}

const float* Patch::scope (int id) const
{
    for (std::size_t i = 0; i < items.size(); ++i)
        if (items[i].id == id && i < rt.size())
            return rt[i]->scope.data();

    return nullptr;
}

float Patch::needle (int id) const
{
    for (std::size_t i = 0; i < items.size(); ++i)
        if (items[i].id == id && i < rt.size())
            return rt[i]->slewed;

    return 0.0f;
}

// post-mod knob
float Patch::liveKnob (int id, int knob) const
{
    if (knob < 0 || knob >= kMaxKnobs) return 0.0f;

    for (std::size_t i = 0; i < items.size(); ++i)
        if (items[i].id == id)
            return i < rt.size() && rt[i] != nullptr ? rt[i]->live[(std::size_t) knob]
                                                     : items[i].knob[(std::size_t) knob];

    return 0.0f;
}

// cycle phase 0..1
float Patch::phase (int id) const
{
    for (std::size_t i = 0; i < items.size(); ++i)
        if (items[i].id == id && i < rt.size() && rt[i] != nullptr)
            return (float) rt[i]->phase;

    return 0.0f;
}

void Patch::ballAt (int id, float& x, float& y, float& floorLit, float& wallLit) const
{
    for (std::size_t i = 0; i < items.size(); ++i)
        if (items[i].id == id && i < rt.size() && rt[i] != nullptr)
        {
            x = rt[i]->bx; y = rt[i]->by;
            floorLit = rt[i]->floorLit; wallLit = rt[i]->wallLit;
            return;
        }
}

int Patch::playhead (int id) const
{
    for (std::size_t i = 0; i < items.size(); ++i)
        if (items[i].id == id && i < rt.size() && rt[i] != nullptr)
            return rt[i]->step;

    return -1;
}

float Patch::gainReduction (int id) const
{
    for (std::size_t i = 0; i < items.size(); ++i)
        if (items[i].id == id && i < rt.size() && rt[i] != nullptr)
            return rt[i]->grNow;

    return 0.0f;
}

float Patch::meter (int id) const
{
    for (std::size_t i = 0; i < items.size(); ++i)
        if (items[i].id == id && i < rt.size())
            return rt[i]->meter;

    return 0.0f;
}

int Patch::slot (int id) const
{
    return (id >= 0 && id < (int) indexOfId.size()) ? indexOfId[(std::size_t) id] : -1;
}

bool Patch::connected (int id, int port) const
{
    for (const auto& e : edges)
        if (! e.toKnob && e.to == id && e.toPort == port) return true;

    return false;
}

// delay buffer size
static std::size_t delayLine (Kind k, double sr)
{
    switch (k)
    {
        case Kind::delay: return (std::size_t) (sr * 4.0) + 4;
        case Kind::pluck: return (std::size_t) (sr * 0.25) + 4;
        default:          return 0;
    }
}

std::vector<Cable> Patch::touching() const
{
    std::vector<Cable> found;

    for (const auto& a : items)
    {
        if (spec (a.kind).numOut == 0) continue;

        for (const auto& b : items)
        {
            if (b.id == a.id || spec (b.kind).numIn == 0) continue;
            if (a.x + a.subW() != b.x) continue;                         // edge to edge
            if (b.y >= a.y + a.subH() || a.y >= b.y + b.subH()) continue;   // and overlapping

            bool taken = false;
            for (const auto& w : wires)
                if (! w.toKnob && w.to == b.id && w.toPort == 0) { taken = true; break; }

            if (! taken)
                found.push_back ({ a.id, 0, b.id, 0 });
        }
    }

    return found;
}

void Patch::rebuild()
{
    // keep state across rebuild
    const auto previous = indexOfId;
    std::vector<std::unique_ptr<Runtime>> fresh (items.size());

    for (std::size_t i = 0; i < items.size(); ++i)
    {
        const int id = items[i].id;
        const int was = (id >= 0 && id < (int) previous.size()) ? previous[(std::size_t) id] : -1;

        if (was >= 0 && was < (int) rt.size() && rt[(std::size_t) was] != nullptr)
            fresh[i] = std::move (rt[(std::size_t) was]);
    }

    rt = std::move (fresh);
    indexOfId.assign ((std::size_t) nextId + 1, -1);

    for (std::size_t i = 0; i < items.size(); ++i)
    {
        indexOfId[(std::size_t) items[i].id] = (int) i;

        const bool isNew = (rt[i] == nullptr);
        if (isNew) rt[i].reset (new Runtime());

        for (int o = 0; o < kMaxOut; ++o)
            if (rt[i]->out[(std::size_t) o].size() != (std::size_t) maxBlock)
                rt[i]->out[(std::size_t) o].assign ((std::size_t) maxBlock, 0.0f);

        // buffers only where needed
        const std::size_t want = delayLine (items[i].kind, sr);

        if (rt[i]->delayBuf.size() != want)
        {
            rt[i]->delayBuf.assign (want, 0.0f);
            rt[i]->delayBuf.shrink_to_fit();
        }

        if (rt[i]->unit == nullptr)
            if (const int u = vendoredUnit (items[i].kind); u >= 0)
            {
                rt[i]->unit.reset (new mi::Unitary());
                rt[i]->unit->init ((mi::Unit) u, sr);
            }

        if (items[i].kind == Kind::reverb && rt[i]->line[0].empty())
        {
            const double k = sr / 48000.0;

            for (int d = 0; d < 4; ++d)
                rt[i]->diff[(std::size_t) d].assign ((std::size_t) (kDiffLen[d] * k) + 2, 0.0f);

            for (int l = 0; l < 8; ++l)      // + mod headroom
                rt[i]->line[(std::size_t) l].assign ((std::size_t) (kLineLen[l] * k) + 64, 0.0f);
        }
    }

    edges = wires;

    for (const auto& t : touching())
        edges.push_back (t);

    // topo sort; loops last
    order.clear();
    std::vector<int> degree (items.size(), 0);

    for (const auto& e : edges)
    {
        const int t = slot (e.to), f = slot (e.from);
        if (t >= 0 && f >= 0) ++degree[(std::size_t) t];
    }

    std::vector<int> queue;
    for (std::size_t i = 0; i < items.size(); ++i)
        if (degree[i] == 0) queue.push_back ((int) i);

    std::vector<bool> done (items.size(), false);

    while (! queue.empty())
    {
        const int i = queue.front();
        queue.erase (queue.begin());
        done[(std::size_t) i] = true;
        order.push_back (items[(std::size_t) i].id);

        for (const auto& e : edges)
        {
            if (e.from != items[(std::size_t) i].id) continue;
            const int t = slot (e.to);
            if (t < 0 || done[(std::size_t) t]) continue;
            if (--degree[(std::size_t) t] == 0) queue.push_back (t);
        }
    }

    for (std::size_t i = 0; i < items.size(); ++i)
        if (! done[i]) order.push_back (items[i].id);

    needsRebuild = false;
}

const float* Patch::input (const Block& b, int port, int n)
{
    const float* single = nullptr;
    int count = 0;

    for (const auto& e : edges)
    {
        if (e.toKnob || e.to != b.id || e.toPort != port) continue;
        const int f = slot (e.from);
        if (f < 0) continue;

        const float* src = rt[(std::size_t) f]->out[(std::size_t) std::min (e.fromPort, kMaxOut - 1)].data();

        if (++count == 1) single = src;
        else
        {
            if (count == 2) std::memcpy (sumBuf.data(), single, sizeof (float) * (std::size_t) n);
            for (int i = 0; i < n; ++i) sumBuf[(std::size_t) i] += src[i];
        }
    }

    if (count == 0) return silence.data();
    return count == 1 ? single : sumBuf.data();
}

void Patch::process (float* outL, float* outR, int n)
{
    // never alloc here
    if (needsRebuild)
    {
        std::fill (outL, outL + n, 0.0f);
        std::fill (outR, outR + n, 0.0f);
        return;
    }

    std::fill (outL, outL + n, 0.0f);
    std::fill (outR, outR + n, 0.0f);

    // keyboard, per block
    if (! held.empty())
    {
        keyPitch = (float) (held.back() - 60);
        keyGate = 1.0f;
    }
    else keyGate = 0.0f;

    for (const int id : order)
    {
        const int i = slot (id);
        if (i < 0) continue;

        auto& b = items[(std::size_t) i];
        auto& r = *rt[(std::size_t) i];

        processBlock (b, r, n);

        // peak hold, decaying
        float peak = 0.0f;
        const auto& o = r.out[0];
        for (int k = 0; k < n; ++k) peak = std::max (peak, std::abs (o[(std::size_t) k]));
        r.meter = std::max (peak, r.meter * 0.86f);

        // strided scope grab
        if (b.kind != Kind::comp && b.kind != Kind::limit)
        {
            const int stride = std::max (1, n / kScope);

            for (int k = 0; k < kScope; ++k)
                r.scope[(std::size_t) k] = o[(std::size_t) std::min (n - 1, k * stride)];
        }

        if (b.kind == Kind::out)
        {
            const float lvl = b.knob[0] * b.knob[0] * 1.6f;
            const float pan = b.knob[1];
            const float gl = std::cos (pan * 1.5707963f), gr = std::sin (pan * 1.5707963f);
            const float* l = input (b, 0, n);
            const float* rr = connected (b.id, 1) ? input (b, 1, n) : l;

            for (int k = 0; k < n; ++k)
            {
                outL[k] += l[k] * lvl * gl * 1.4142f;
                outR[k] += rr[k] * lvl * gr * 1.4142f;
            }
        }
    }

    // -6 dB headroom, then soft ceiling
    for (int k = 0; k < n; ++k)
    {
        outL[k] = softClip (outL[k] * kBusGain);
        outR[k] = softClip (outR[k] * kBusGain);
    }

    ppq += (n / sr) * (bpm / 60.0);
}

std::string optionName (const char* options, int index) { return optionAt (options, index); }

const char* portName (Kind k, bool isInput, int i)
{
    const auto& sp = spec (k);
    if (isInput)  return (i >= 0 && i < sp.numIn)  ? sp.in[i]  : "";
    return (i >= 0 && i < sp.numOut) ? sp.out[i] : "";
}

// port kind overrides
Sig portKind (Kind k, bool isInput, int i)
{
    const std::string n (portName (k, isInput, i));

    if (n == "gate" || n == "trig" || n == "clock" || n == "strum" || n == "reset"
        || n == "eoc" || n == "freeze" || n == "ping" || n == "and" || n == "off")
        return Sig::gate;

    if (n == "pitch") return Sig::pitch;
    if (n == "cv" || n == "fm" || n == "pos") return Sig::cv;

    switch (k)
    {
        case Kind::key:     return isInput ? Sig::audio : (i == 0 ? Sig::pitch : Sig::gate);
        case Kind::seq:     return isInput ? Sig::gate
                                          : (i == 0 ? Sig::pitch : (i == 1 ? Sig::gate : Sig::cv));
        case Kind::turing:  return isInput ? Sig::gate  : (i == 0 ? Sig::pitch : Sig::gate);
        case Kind::quant:   return Sig::pitch;
        case Kind::chaos:   return Sig::cv;
        case Kind::ball:    return isInput ? Sig::gate : (i < 2 ? Sig::cv : Sig::gate);
        case Kind::midi:    return isInput ? Sig::gate
                                          : (i == 0 ? Sig::pitch : (i == 1 ? Sig::gate : Sig::cv));
        case Kind::env:     return isInput ? Sig::gate : (i == 0 ? Sig::cv : Sig::gate);
        case Kind::lfo:     return isInput ? Sig::gate : Sig::cv;
        case Kind::rand:    return isInput ? Sig::gate : Sig::cv;
        case Kind::slew:    return Sig::cv;
        case Kind::clock:   return Sig::gate;
        case Kind::div:     return Sig::gate;
        case Kind::logic:   return Sig::gate;
        case Kind::euclid:  return Sig::gate;
        case Kind::sswitch: return isInput && i == 0 ? Sig::gate : Sig::cv;
        default:            return Sig::audio;
    }
}

const char* describe (Kind k)
{
    switch (k)
    {
        case Kind::key:    return "Turns played midi notes into pitch and gate.";
        case Kind::osc:    return "An oscillator with 4 shapes.";
        case Kind::noise:  return "White noise can be used to excite a filter, a resonator or a low-pass gate.";
        case Kind::filter: return "Six filters: Moog-style ladder, squelchier 303 variant, state-variable low, band and high, and Sallen-Key.";
        case Kind::level:  return "Gain.";
        case Kind::damage: return "Soft clipping, a wavefolder, or bits.";
        case Kind::mix:    return "Mix.";
        case Kind::lpg:    return "A low-pass gate: filter and level open together.";
        case Kind::ring:   return "Ring modulation: multiplies two signals into their sum and difference.";
        case Kind::env:    return "A gate in, a shape out: attack, decay, sustain and release.";
        case Kind::lfo:    return "Slow modulation in five shapes.";
        case Kind::rand:   return "Sample and hold: on every clock it takes a new random voltage. Slew rounds the steps into curves.";
        case Kind::slew:   return "Separate rise and fall times. Glide for pitch, used to turn steps into slopes.";
        case Kind::quant:  return "Forces a wandering voltage onto the notes of a scale.";
        case Kind::clock:  return "Gives everything its timing. You can have multiple clocks per patch.";
        case Kind::seq:    return "Eight-step sequencer. The switch under a step makes it a note, a rest, or a tie that slides into the one before.";
        case Kind::delay:  return "Tempo-synced echo with tone in the feedback path.";
        case Kind::div:    return "Divides or multiplies a clock.";
        case Kind::logic:  return "And, or, exclusive-or and nor on two gates.";
        case Kind::sswitch:return "Routes one of two inputs to the output, changing over on every clock.";
        case Kind::turing: return "A looping shift register of random voltages. At zero it repeats forever, at full it never repeats, in between it drifts.";
        case Kind::euclid: return "Spreads a number of hits as evenly as possible across the steps. Rotate turns the pattern, chance thins it out.";
        case Kind::input:  return "Use input for mics, sidechain from other instruments, etc. In FL Studio, load Quadrat as an effect to use this.";
        case Kind::midi:
           #if defined (__APPLE__)
            return "A phrase of notes, played on a clock. Click to load a MIDI file, or drop one onto it; from Logic you can also copy notes in the piano roll and press command-V here.";
           #else
            return "A phrase of notes, played on a clock. Click to load a MIDI file, or drop one onto it. Each clock pulse moves it on by one step of its switch; reset starts it from the top.";
           #endif
        case Kind::ball:   return "A ball thrown in a box.";
        case Kind::chaos:  return "A Lorenz or Rossler attractor as two voltages. Deterministic, and never quite repeating.";
        case Kind::timbre: return "Forty-eight oscillator models on one knob, from plain waves through vowels and plucked strings to clocked noise.";
        case Kind::macro:  return "A complete synthesis voice with sixteen engines, from virtual analogue through wavetables and speech to noise and drums.";
        case Kind::drum:   return "Analogue drum voices: bass, snare, hat and an FM drum.";
        case Kind::reson:  return "Bank of resonators: modal bars, sympathetic strings, a plucked string and an FM voice.";
        case Kind::scatter:return "Granular texture processor.";
        case Kind::mallet: return "Physical models struck and blown: modal bars, a string, a drum head.";
        case Kind::xmod:   return "Cross-modulates two signals: crossfade, wavefolding, ring modulation and a vocoder among them.";
        case Kind::ctrl:   return "Controls for the host to automate.";
        case Kind::pluck:  return "A short delay fed back through a damper. Hit it with a click it rings like a string.";
        case Kind::reverb: return "A smooth feedback-delay reverb.";
        case Kind::out:    return "Patch output. Can be configured to stereo.";
        case Kind::comp:   return "Feed-forward compressor.";
        case Kind::meter:  return "Visual representation of the level.";
        case Kind::limit:  return "Limits the output.";
        default:           return "";
    }
}

// GR history
static void pushHistory (std::array<float, Patch::kScope>& ring, float v)
{
    std::memmove (ring.data(), ring.data() + 1, (Patch::kScope - 1) * sizeof (float));
    ring[(std::size_t) (Patch::kScope - 1)] = v;
}

void Patch::processBlock (Block& b, Runtime& r, int n)
{
    const auto& sp = spec (b.kind);
    float* o0 = r.out[0].data();
    float* o1 = r.out[1].data();
    float* o2 = r.out[2].data();          // 3+ output blocks
    float* o3 = r.out[3].data();
    const double ppqStep = (bpm / 60.0) / sr;

    // knob + cable, block rate
    float kv[kMaxKnobs];

    for (int i = 0; i < kMaxKnobs; ++i)
        kv[i] = b.knob[(std::size_t) i];

    for (const auto& e : edges)
    {
        if (! e.toKnob || e.to != b.id || e.toPort < 0 || e.toPort >= kMaxKnobs) continue;

        const int f = slot (e.from);
        if (f < 0) continue;

        const int port = std::min (e.fromPort, kMaxOut - 1);
        kv[e.toPort] = clampf (kv[e.toPort] + rt[(std::size_t) f]->out[(std::size_t) port][0]
                                              * b.depth[(std::size_t) e.toPort],
                               0.0f, 1.0f);
    }

    for (int i = 0; i < kMaxKnobs; ++i)
        r.live[(std::size_t) i] = kv[i];

    switch (b.kind)
    {
        case Kind::key:
        {
            // one-pole glide
            const float coef = kv[0] < 0.001f ? 1.0f
                             : (float) (1.0 - std::exp (-1.0 / (sr * (0.002 + kv[0] * 0.5))));

            for (int i = 0; i < n; ++i)
            {
                r.glided += (keyPitch - r.glided) * coef;
                o0[i] = r.glided + keyBend;
                o1[i] = keyGate * keyVel;
            }
            break;
        }

        case Kind::osc:
        {
            const float* pitch = input (b, 0, n);
            const float* fm = input (b, 1, n);
            const float tune = std::round ((kv[0] * 2.0f - 1.0f) * 24.0f);
            const float fine = (kv[1] * 2.0f - 1.0f);
            const float width = clampf (0.08f + kv[2] * 0.84f, 0.05f, 0.95f);
            const int shape = b.option;

            for (int i = 0; i < n; ++i)
            {
                const float semis = pitch[i] + tune + fine + fm[i] * 12.0f;
                const double f = 261.6255653 * std::pow (2.0, semis / 12.0);
                const double dt = clampf ((float) (f / sr), 0.0f, 0.45f);

                r.phase += dt;
                if (r.phase >= 1.0) r.phase -= 1.0;

                float v = 0.0f;

                if (shape == 0)            // saw
                    v = (float) (2.0 * r.phase - 1.0) - polyBlep (r.phase, dt);
                else if (shape == 1)       // pulse
                {
                    v = r.phase < width ? 1.0f : -1.0f;
                    v += polyBlep (r.phase, dt);
                    double t2 = r.phase - width; if (t2 < 0.0) t2 += 1.0;
                    v -= polyBlep (t2, dt);
                }
                else if (shape == 2)       // triangle
                    v = (float) (4.0 * std::abs (r.phase - 0.5) - 1.0);
                else                       // sine
                    v = (float) std::sin (6.283185307 * r.phase);

                o0[i] = v * 0.8f;
            }
            break;
        }

        case Kind::noise:
        {
            const float bright = kv[0];

            for (int i = 0; i < n; ++i)
            {
                const float w = r.rnd();
                r.pink += (w - r.pink) * 0.035f;           // a dark, rolled-off noise
                o0[i] = lerp (r.pink * 3.2f, w, bright) * 0.7f;
            }
            break;
        }

        case Kind::filter:
        {
            const float* in = input (b, 0, n);
            const float* cv = input (b, 1, n);
            const float amt = (kv[2] * 2.0f - 1.0f) * 60.0f;
            const float base = 20.0f * std::pow (2.0f, kv[0] * 10.0f);
            const float res = clampf (kv[1], 0.0f, 0.97f);
            const float k = 2.0f - 2.0f * res;

            // blew up? reset
            if (! std::isfinite (r.ic1 + r.ic2 + r.ladderZ[0] + r.ladderZ[1]))
                r.ic1 = r.ic2 = r.ladderZ[0] = r.ladderZ[1] = 0.0f;

            for (int i = 0; i < n; ++i)
            {
                const float fc = clampf (base * std::pow (2.0f, cv[i] * amt / 12.0f), 20.0f, (float) (sr * 0.45));

                if (b.option <= 1)
                {
                    // 4-pole ladder; 1 = 303
                    const float acid = b.option == 1 ? 1.0f : 0.0f;
                    const float wc = clampf (2.0f * 3.14159265f * fc / (float) sr, 0.0f, 1.2f);
                    const float gg = wc / (1.0f + wc);
                    const float fbAmt = res * (4.0f - acid * 0.6f);
                    float x = in[i] * (1.0f + acid * 2.4f) - fbAmt * r.ladder[3];
                    x = std::tanh (x);

                    for (int st = 0; st < 4; ++st)
                    {
                        const float lp = r.ladder[(std::size_t) st]
                                       + gg * (x - r.ladder[(std::size_t) st]);
                        r.ladder[(std::size_t) st] = lp + (acid > 0.0f ? 0.0f : 1.0e-20f);
                        x = lp;
                    }

                    o0[i] = x * (1.0f + res * 1.4f);
                }
                else if (b.option == 5)
                {
                    // Sallen-Key 2-pole
                    // saturate the feedback
                    const float g = std::tan (3.14159265f * fc / (float) sr);
                    const float a1 = 1.0f / (1.0f + g);
                    const float fb = (1.0f + res * 1.85f) * std::tanh (r.ladderZ[0]);
                    const float hp = (in[i] - fb - r.ladderZ[1]) * a1;
                    r.ladderZ[0] = clampf (r.ladderZ[0] + g * hp, -32.0f, 32.0f);
                    r.ladderZ[1] = clampf (r.ladderZ[1] + g * r.ladderZ[0], -32.0f, 32.0f);
                    o0[i] = std::tanh (r.ladderZ[1]);
                }
                else
                {
                    const float g = std::tan (3.14159265f * fc / (float) sr);
                    const float a1 = 1.0f / (1.0f + g * (g + k));
                    const float a2 = g * a1, a3 = g * a2;

                    const float v3 = in[i] - r.ic2;
                    const float v1 = a1 * r.ic1 + a2 * v3;
                    const float v2 = r.ic2 + a2 * r.ic1 + a3 * v3;
                    r.ic1 = clampf (2.0f * v1 - r.ic1, -32.0f, 32.0f);
                    r.ic2 = clampf (2.0f * v2 - r.ic2, -32.0f, 32.0f);

                    o0[i] = b.option == 2 ? v2 : (b.option == 3 ? v1 : in[i] - k * v1 - v2);
                }
            }
            break;
        }

        case Kind::level:
        {
            // dB gain, cv, offset
            const float* in = input (b, 0, n);
            const float* cv = input (b, 1, n);
            const bool cvOn = connected (b.id, 1);

            const float range = b.option == 0 ? 12.0f : (b.option == 1 ? 24.0f : 48.0f);
            const float lvl = kv[0] <= 0.005f ? 0.0f
                                              : std::pow (10.0f, (kv[0] * 2.0f - 1.0f) * range / 20.0f);
            const float amount = kv[1];
            const float off = kv[2] * 2.0f - 1.0f;
            const float sign = kv[3] > 0.5f ? -1.0f : 1.0f;

            for (int i = 0; i < n; ++i)
            {
                const float f = cvOn ? (1.0f - amount) + amount * clampf (cv[i], 0.0f, 2.0f) : 1.0f;
                o0[i] = in[i] * lvl * f * sign + off;
            }

            break;
        }

        case Kind::damage:
        {
            // clip / fold / crush
            const float* in = input (b, 0, n);

            if (b.option < 2)
            {
                const float gain = 1.0f + kv[0] * kv[0] * 24.0f;
                const float bias = (kv[1] * 2.0f - 1.0f) * 0.5f;
                const float norm = 1.0f / std::tanh (gain * 0.7f + 0.001f);

                for (int i = 0; i < n; ++i)
                {
                    const float x = in[i] * gain + bias;
                    o0[i] = b.option == 0 ? std::tanh (x) * norm * 0.7f
                                          : std::sin (x * 1.5707963f) * 0.7f;   // fold
                }
            }
            else
            {
                const float levels = std::pow (2.0f, 1.0f + (1.0f - kv[0]) * 14.0f);
                const double step = std::pow (2.0, -1.0 + (1.0 - kv[1]) * 9.0);   // hold, in samples

                for (int i = 0; i < n; ++i)
                {
                    r.crushPhase += 1.0;

                    if (r.crushPhase >= step)
                    {
                        r.crushPhase -= step;
                        r.crushHold = std::round (in[i] * levels) / levels;
                    }

                    o0[i] = r.crushHold;
                }
            }

            break;
        }

        case Kind::mix:
        {
            const float* a = input (b, 0, n);
            const float* c = input (b, 1, n);
            const float* d = input (b, 2, n);

            for (int i = 0; i < n; ++i)
                o0[i] = a[i] * kv[0] + c[i] * kv[1] + d[i] * kv[2];

            break;
        }

        case Kind::env:
        {
            // ADSR, or cycling AD
            const float* gate = input (b, 0, n);
            const bool cycle = b.option == 1;

            const float atkT = 0.001f + kv[0] * kv[0] * 3.0f;
            const float decT = 0.005f + kv[1] * kv[1] * 5.0f;
            const float relT = 0.005f + kv[3] * kv[3] * 5.0f;
            const float sus = cycle ? clampf (kv[2], 0.02f, 1.0f) : kv[2];
            const float atk = 1.0f - std::exp (-1.0f / (float) (sr * atkT));
            const float dec = 1.0f - std::exp (-1.0f / (float) (sr * decT));
            const float rel = 1.0f - std::exp (-1.0f / (float) (sr * relT));
            const float up = 1.0f / (float) (sr * atkT);
            const float dn = 1.0f / (float) (sr * decT);
            const float shape = std::pow (2.0f, (kv[4] - 0.5f) * 2.4f);

            for (int i = 0; i < n; ++i)
            {
                const bool on = gate[i] > 0.5f;
                bool ended = false;

                if (cycle)
                {
                    if (on && ! r.prevGate) { r.envVal = 0.0f; r.stage = 1; }
                    if (r.stage == 0) r.stage = 1;

                    if (r.stage == 1)
                    {
                        r.envVal += up * sus;
                        if (r.envVal >= sus) { r.envVal = sus; r.stage = 2; }
                    }
                    else
                    {
                        r.envVal -= dn * sus;
                        if (r.envVal <= 0.0f) { r.envVal = 0.0f; r.stage = 1; ended = true; }
                    }
                }
                else
                {
                    if (on && ! r.prevGate) r.stage = 1;
                    if (! on && r.prevGate) r.stage = 3;

                    if (r.stage == 1)
                    {
                        r.envVal += (1.06f - r.envVal) * atk;
                        if (r.envVal >= 1.0f) { r.envVal = 1.0f; r.stage = 2; }
                    }
                    else if (r.stage == 2) r.envVal += (sus - r.envVal) * dec;
                    else if (r.stage == 3)
                    {
                        r.envVal += (0.0f - r.envVal) * rel;

                        // denormal guard
                        if (r.envVal < 1.0e-4f) { r.envVal = 0.0f; r.stage = 0; ended = true; }
                    }
                }

                r.prevGate = on;
                o0[i] = std::pow (clampf (r.envVal, 0.0f, 1.0f), shape);
                o1[i] = ended ? 1.0f : 0.0f;
            }

            break;
        }

        case Kind::lfo:
        {
            const float* reset = input (b, 0, n);
            const bool resetOn = connected (b.id, 0);
            const double rate = 0.02 * std::pow (2.0, kv[0] * 10.0);
            const double dt = rate / sr;
            const float depth = kv[1];

            for (int i = 0; i < n; ++i)
            {
                if (resetOn)
                {
                    const bool hi = reset[i] > 0.5f;
                    if (hi && ! r.prevGate) r.phase = 0.0;
                    r.prevGate = hi;
                }

                r.phase += dt;
                if (r.phase >= 1.0) { r.phase -= 1.0; r.sampled = r.rnd(); }

                float v = 0.0f;
                switch (b.option)
                {
                    case 0: v = (float) std::sin (6.283185307 * r.phase); break;
                    case 1: v = (float) (4.0 * std::abs (r.phase - 0.5) - 1.0); break;
                    case 2: v = (float) (2.0 * r.phase - 1.0); break;
                    case 3: v = r.phase < 0.5 ? 1.0f : -1.0f; break;
                    default: v = r.sampled; break;
                }

                o0[i] = v * depth;
            }
            break;
        }

        case Kind::rand:
        {
            const float* clk = input (b, 0, n);
            const float range = kv[0];
            const float slewT = 0.0005f + kv[1] * kv[1] * 0.5f;
            const float co = 1.0f - std::exp (-1.0f / (float) (sr * slewT));

            for (int i = 0; i < n; ++i)
            {
                const bool hi = clk[i] > 0.5f;
                if (hi && ! r.prevGate) r.sampled = r.rnd() * range;
                r.prevGate = hi;
                r.slewed += (r.sampled - r.slewed) * co;
                o0[i] = r.slewed;
            }
            break;
        }

        case Kind::clock:
        {
            const double beats = divisionBeats (optionAt (sp.options, b.option).c_str());
            const float width = clampf (kv[0], 0.05f, 0.95f);

            for (int i = 0; i < n; ++i)
            {
                const double p = (ppq + ppqStep * i) / beats;
                const double frac = p - std::floor (p);
                o0[i] = frac < width ? 1.0f : 0.0f;

                // face = one bar
                const double bar = (ppq + ppqStep * i) * 0.25;
                r.phase = bar - std::floor (bar);
            }
            break;
        }

        case Kind::seq:
        {
            const float* clk = input (b, 0, n);
            const float range = kv[0];

            // glide on ties only
            const float slide = 0.002f + kv[1] * kv[1] * 0.25f;
            const float coef = (float) (1.0 - std::exp (-1.0 / (sr * slide)));

            for (int i = 0; i < n; ++i)
            {
                const bool hi = clk[i] > 0.5f;

                if (hi && ! r.prevGate)
                {
                    // rests still tick
                    r.step = (r.step + 1) % kSteps;
                }

                r.prevGate = hi;

                const auto s = (std::size_t) r.step;
                const auto mode = b.stepMode[s];
                const float semis = std::round ((b.steps[s] * 2.0f - 1.0f) * range * 24.0f);

                if (mode != Block::Step::off)
                {
                    r.glided += (semis - r.glided) * (mode == Block::Step::fall ? coef : 1.0f);
                    r.sampled = b.steps2[s] * 2.0f - 1.0f;
                }

                o0[i] = r.glided;
                o1[i] = mode == Block::Step::off ? 0.0f
                      : (mode == Block::Step::fall ? 1.0f : (hi ? 1.0f : 0.0f));
                o2[i] = r.sampled;
            }
            break;
        }

        case Kind::delay:
        {
            const float* in = input (b, 0, n);
            const double beats = divisionBeats (optionAt (sp.options, b.option).c_str());
            const double timeSec = std::min (3.9, beats * 60.0 / bpm);
            const int len = std::max (8, (int) (timeSec * sr));
            const float fb = kv[0] * 0.95f, mix = kv[1];
            const float tone = 0.02f + kv[2] * 0.9f;
            auto& buf = r.delayBuf;

            for (int i = 0; i < n; ++i)
            {
                const int readIdx = (r.delayIdx - len + (int) buf.size()) % (int) buf.size();
                const float d = buf[(std::size_t) readIdx];
                r.tone += (d - r.tone) * tone;

                buf[(std::size_t) r.delayIdx] = in[i] + r.tone * fb;
                r.delayIdx = (r.delayIdx + 1) % (int) buf.size();

                o0[i] = lerp (in[i], r.tone, mix);
            }
            break;
        }

        case Kind::reverb:
        {
            // FDN: diffuse, 8 lines, Hadamard
            const float* in = input (b, 0, n);
            const float size = clampf (kv[0], 0.0f, 1.0f);
            const float rt60 = 0.7f * std::pow (11.4f, size);      // 0.7..8 s
            const float mix = clampf (kv[1], 0.0f, 1.0f);
            const float bright = 0.55f - 0.3f * size;              // big = darker
            const double k = sr / 48000.0;

            float fb[8], len[8], gsum = 0.0f;

            for (int l = 0; l < 8; ++l)
            {
                len[l] = (float) (kLineLen[l] * k);
                fb[l] = std::pow (10.0f, -3.0f * len[l] / (float) (sr * rt60));
                gsum += fb[l];
            }

            // wet level ~flat vs decay
            const float g = gsum / 8.0f;
            const float inGain = std::sqrt (std::max (0.0f, 1.0f - g * g));
            const double lfoStep = 1.0 / sr;
            const float atk = 1.0f - std::exp (-1.0f / (float) (sr * 0.002));
            const float rls = 1.0f - std::exp (-1.0f / (float) (sr * 0.35));
            const float depth = (float) (14.0 * k);
            constexpr float h8 = 0.35355339f;                      // 1/sqrt(8)

            for (int i = 0; i < n; ++i)
            {
                // input diffusion
                float x = in[i];

                for (int d = 0; d < 4; ++d)
                {
                    auto& buf = r.diff[(std::size_t) d];
                    auto& pos = r.diffPos[(std::size_t) d];
                    const float y = buf[(std::size_t) pos];
                    const float w = x + 0.62f * y;
                    buf[(std::size_t) pos] = w;
                    x = y - 0.62f * w;
                    pos = (pos + 1) % (int) buf.size();
                }

                r.lfo += lfoStep;
                if (r.lfo >= 1000.0) r.lfo -= 1000.0;     // all rates x0.01 Hz: wraps clean

                // read, wobbled, damped
                float v[8];

                for (int l = 0; l < 8; ++l)
                {
                    const auto& buf = r.line[(std::size_t) l];
                    const int sz = (int) buf.size();
                    // own rate per line: 0.23..1.1 Hz
                    const float mod = depth * (float) std::sin (6.283185307 * r.lfo * kLineRate[l] + l * 0.785);
                    float rp = (float) r.linePos[(std::size_t) l] - (len[l] + mod);
                    while (rp < 0.0f) rp += (float) sz;
                    const int i0 = (int) rp % sz;
                    const int i1 = (i0 + 1) % sz;
                    const float fr = rp - std::floor (rp);
                    const float y = buf[(std::size_t) i0] + (buf[(std::size_t) i1] - buf[(std::size_t) i0]) * fr;
                    r.lineLp[(std::size_t) l] += (y - r.lineLp[(std::size_t) l]) * bright;
                    v[l] = r.lineLp[(std::size_t) l];
                }

                float wet = 0.0f;
                for (int l = 0; l < 8; ++l) wet += (l & 1) ? -v[l] : v[l];

                // Hadamard 8, in place
                for (int h = 1; h < 8; h <<= 1)
                    for (int a = 0; a < 8; a += h << 1)
                        for (int c = a; c < a + h; ++c)
                        {
                            const float p0 = v[c], p1 = v[c + h];
                            v[c] = p0 + p1;
                            v[c + h] = p0 - p1;
                        }

                const float feed = x * inGain;

                for (int l = 0; l < 8; ++l)
                {
                    auto& buf = r.line[(std::size_t) l];
                    auto& wp = r.linePos[(std::size_t) l];
                    buf[(std::size_t) wp] = feed + fb[l] * v[l] * h8;
                    wp = (wp + 1) % (int) buf.size();
                }

                // smooth limiter: dips, never clips
                wet *= h8 * kWetGain;
                const float a = std::abs (wet);
                r.det += (a - r.det) * (a > r.det ? atk : rls);
                if (r.det > kWetCeil) wet *= kWetCeil / r.det;
                o0[i] = lerp (in[i], wet, mix);
            }
            break;
        }

        case Kind::lpg:
        {
            // filter + vca, self-decay
            const float* in = input (b, 0, n);
            const float* ping = input (b, 1, n);
            const bool pinged = connected (b.id, 1);
            const float decT = 0.01f + kv[0] * kv[0] * 3.0f;
            const float fall = 1.0f - std::exp (-1.0f / (float) (sr * decT));

            for (int i = 0; i < n; ++i)
            {
                if (pinged)
                {
                    const bool hi = ping[i] > 0.5f;
                    if (hi && ! r.prevGate) r.envVal = 1.0f;
                    r.prevGate = hi;
                    r.envVal -= r.envVal * fall;
                }
                else r.envVal = 1.0f;

                const float e = r.envVal * kv[1];
                const float fc = clampf (60.0f + e * e * 7000.0f, 20.0f, (float) (sr * 0.45));
                const float g = std::tan (3.14159265f * fc / (float) sr);
                const float a1 = 1.0f / (1.0f + g * (g + 1.4f));
                const float v3 = in[i] - r.ic2;
                const float v1 = a1 * r.ic1 + g * a1 * v3;
                const float v2 = r.ic2 + g * a1 * r.ic1 + g * g * a1 * v3;
                r.ic1 = 2.0f * v1 - r.ic1;
                r.ic2 = 2.0f * v2 - r.ic2;

                o0[i] = b.option == 1 ? in[i] * e            // plain vca
                      : b.option == 2 ? v2                   // plain filter
                                      : v2 * e;              // both, the lpg
            }
            break;
        }

        case Kind::ring:
        {
            const float* a = input (b, 0, n);
            const float* c = input (b, 1, n);

            for (int i = 0; i < n; ++i)
                o0[i] = lerp (a[i], a[i] * c[i] * 1.6f, kv[0]);

            break;
        }

        case Kind::slew:
        {
            const float* in = input (b, 0, n);
            const float riseT = 0.0002f + kv[0] * kv[0] * 2.0f;
            const float fallT = 0.0002f + kv[1] * kv[1] * 2.0f;
            const float up = 1.0f - std::exp (-1.0f / (float) (sr * riseT));
            const float dn = 1.0f - std::exp (-1.0f / (float) (sr * fallT));

            for (int i = 0; i < n; ++i)
            {
                r.slewed += (in[i] - r.slewed) * (in[i] > r.slewed ? up : dn);
                o0[i] = r.slewed;
            }
            break;
        }

        case Kind::quant:
        {
            // snap to scale
            static const int scales[6][7] = {
                { 0, 1, 2, 3, 4, 5, 6 },          // chromatic handled below
                { 0, 2, 4, 5, 7, 9, 11 },         // major
                { 0, 2, 3, 5, 7, 8, 10 },         // minor
                { 0, 3, 5, 7, 10, -1, -1 },       // pentatonic
                { 0, 2, 3, 5, 7, 9, 10 },         // dorian
                { 0, 2, 4, 6, 8, 10, -1 } };      // whole tone

            const float* in = input (b, 0, n);
            const float shift = std::round ((kv[0] * 2.0f - 1.0f) * 24.0f);
            const float range = 0.2f + kv[1] * 2.0f;
            const int sel = imin (5, std::max (0, b.option));

            for (int i = 0; i < n; ++i)
            {
                const float want = in[i] * range + shift;

                if (sel == 0) { o0[i] = std::round (want); continue; }

                const int oct = (int) std::floor (want / 12.0f);
                const float within = want - oct * 12.0f;
                float best = 0.0f, bestD = 1.0e9f;

                for (int k = 0; k < 7; ++k)
                {
                    const int d = scales[sel][k];
                    if (d < 0) continue;
                    const float dist = std::abs (within - d);
                    if (dist < bestD) { bestD = dist; best = (float) d; }
                }

                o0[i] = oct * 12.0f + best;
            }
            break;
        }

        case Kind::div:
        {
            const float* clk = input (b, 0, n);
            const int nDiv = 1 + (int) std::round (kv[0] * 15.0f);

            for (int i = 0; i < n; ++i)
            {
                const bool hi = clk[i] > 0.5f;

                if (hi && ! r.prevGate) ++r.pulseCount;
                r.prevGate = hi;

                if (b.option == 0)
                {
                    const bool on = (r.pulseCount % nDiv) == 0;
                    o0[i] = (on && hi) ? 1.0f : 0.0f;
                }
                else
                {
                    // ratchet
                    r.phase = hi ? r.phase + (double) nDiv / (sr * 0.12) : 0.0;
                    o0[i] = (hi && std::fmod (r.phase, 1.0) < 0.5) ? 1.0f : 0.0f;
                }

                o1[i] = hi ? 1.0f : 0.0f;
            }
            break;
        }

        case Kind::logic:
        {
            const float* a = input (b, 0, n);
            const float* c = input (b, 1, n);
            const float lvl = kv[0];

            for (int i = 0; i < n; ++i)
            {
                const bool x = a[i] > 0.5f, y = c[i] > 0.5f;
                bool out = false;

                switch (b.option)
                {
                    case 0: out = x && y; break;
                    case 1: out = x || y; break;
                    case 2: out = x != y; break;
                    default: out = ! (x || y); break;
                }

                o0[i] = out ? lvl : 0.0f;
            }
            break;
        }

        case Kind::sswitch:
        {
            const float* clk = input (b, 0, n);
            const float* a = input (b, 1, n);
            const float* c = input (b, 2, n);
            const float glide = 0.0002f + kv[0] * kv[0] * 0.2f;
            const float co = 1.0f - std::exp (-1.0f / (float) (sr * glide));

            for (int i = 0; i < n; ++i)
            {
                const bool hi = clk[i] > 0.5f;
                if (hi && ! r.prevGate) ++r.pulseCount;
                r.prevGate = hi;

                const float want = (r.pulseCount % 2) == 0 ? a[i] : c[i];
                r.slewed += (want - r.slewed) * co;
                o0[i] = r.slewed;
            }
            break;
        }

        case Kind::turing:
        {
            // shift register, flip odds
            const float* clk = input (b, 0, n);
            const int lengths[7] = { 2, 3, 4, 6, 8, 12, 16 };
            const int len = lengths[imin (6, std::max (0, b.option))];
            const float mutate = kv[0];
            const float range = kv[1];

            for (int i = 0; i < n; ++i)
            {
                const bool hi = clk[i] > 0.5f;

                if (hi && ! r.prevGate)
                {
                    const bool wrapped = (r.shift >> (len - 1)) & 1u;
                    bool feed = wrapped;

                    if ((r.rnd() * 0.5f + 0.5f) < mutate) feed = ! feed;

                    r.shift = ((r.shift << 1) | (feed ? 1u : 0u)) & ((1u << len) - 1u);

                    // low byte -> cv
                    r.sampled = ((r.shift & 0xFFu) / 255.0f * 2.0f - 1.0f) * range * 24.0f;
                }

                r.prevGate = hi;
                o0[i] = r.sampled;
                o1[i] = (hi && (r.shift & 1u)) ? 1.0f : 0.0f;
            }
            break;
        }

        case Kind::euclid:
        {
            // Bjorklund-ish spread
            const int lengths[4] = { 8, 12, 16, 32 };
            const int len = lengths[imin (3, std::max (0, b.option))];
            const int fills = imin (len, std::max (0, (int) std::round (kv[0] * len)));
            const int rot = (int) std::round (kv[1] * (len - 1));
            const float chance = kv[2];
            const float* clk = input (b, 0, n);

            for (int i = 0; i < n; ++i)
            {
                const bool hi = clk[i] > 0.5f;

                if (hi && ! r.prevGate)
                {
                    r.pulseCount = (r.pulseCount + 1) % len;
                    const int k = (r.pulseCount + rot) % len;
                    const bool on = fills > 0 && ((k * fills) % len) < fills;
                    r.sampled = (on && (r.rnd() * 0.5f + 0.5f) <= chance) ? 1.0f : 0.0f;
                }

                r.prevGate = hi;
                o0[i] = (hi && r.sampled > 0.5f) ? 1.0f : 0.0f;
                o1[i] = (hi && r.sampled <= 0.5f) ? 1.0f : 0.0f;
            }
            break;
        }

        case Kind::chaos:
        {
            // strange attractor
            const double dt = (0.0001 + kv[0] * kv[0] * 0.02) * (48000.0 / sr);
            const float range = kv[1] * 24.0f;

            for (int i = 0; i < n; ++i)
            {
                if (b.option == 0)
                {
                    const double dx = 10.0 * (r.cy - r.cx);
                    const double dy = r.cx * (28.0 - r.cz) - r.cy;
                    const double dz = r.cx * r.cy - (8.0 / 3.0) * r.cz;
                    r.cx += dx * dt; r.cy += dy * dt; r.cz += dz * dt;
                    o0[i] = (float) (r.cx / 20.0) * range;
                    o1[i] = (float) (r.cy / 26.0) * range;
                }
                else
                {
                    const double dx = -r.cy - r.cz;
                    const double dy = r.cx + 0.2 * r.cy;
                    const double dz = 0.2 + r.cz * (r.cx - 5.7);
                    r.cx += dx * dt * 8.0; r.cy += dy * dt * 8.0; r.cz += dz * dt * 8.0;
                    o0[i] = (float) (r.cx / 10.0) * range;
                    o1[i] = (float) (r.cy / 10.0) * range;
                }

                if (! std::isfinite (r.cx) || std::abs (r.cx) > 1.0e4) { r.cx = 0.1; r.cy = 0.0; r.cz = 0.0; }
            }
            break;
        }

        case Kind::pluck:
        {
            // Karplus-Strong
            const float* in = input (b, 0, n);
            const float* pitch = input (b, 1, n);
            auto& buf = r.delayBuf;
            const float damp = 0.05f + (1.0f - kv[1]) * 0.9f;
            const float fb = 0.9f + kv[0] * 0.0995f;

            for (int i = 0; i < n; ++i)
            {
                const float f = 261.6255653f * std::pow (2.0f, pitch[i] / 12.0f);
                const int len = imin ((int) buf.size() - 2,
                                          std::max (2, (int) (sr / std::max (20.0f, f))));
                const int readIdx = (r.delayIdx - len + (int) buf.size()) % (int) buf.size();
                float y = buf[(std::size_t) readIdx];

                r.tone += (y - r.tone) * damp;
                y = b.option == 0 ? r.tone : (y - r.tone);      // string / bell

                buf[(std::size_t) r.delayIdx] = clampf (in[i] + y * fb, -4.0f, 4.0f);
                r.delayIdx = (r.delayIdx + 1) % (int) buf.size();

                o0[i] = lerp (in[i], y, kv[2]);
            }
            break;
        }

        case Kind::macro:
        case Kind::reson:
        case Kind::scatter:
        case Kind::mallet:
        case Kind::xmod:
        case Kind::drum:
        case Kind::timbre:
        {
            if (r.unit == nullptr || ! r.unit->ready()) break;

            mi::Ports ports {};
            for (int i = 0; i < kMaxIn; ++i)
            {
                ports.in[i] = i < sp.numIn ? input (b, i, n) : silence.data();
                ports.connected[i] = i < sp.numIn && connected (b.id, i);
            }

            ports.out[0] = o0;
            ports.out[1] = o1;
            ports.n = n;

            // model from knob
            const int opt = b.kind == Kind::timbre
                              ? (int) std::lround (kv[0] * (optionCount (sp.options) - 1))
                              : b.option;

            r.unit->setKnobs (kv, sp.numKnobs, opt);
            r.unit->process (ports);
            break;
        }

        case Kind::ctrl:
        {
            // host values as cv
            const int bank = std::min (3, std::max (0, (int) std::round (kv[0] * 3.0f))) * 4;

            for (int port = 0; port < kMaxOut && port < sp.numOut; ++port)
            {
                const float v = macro (bank + port);
                std::fill (r.out[(std::size_t) port].begin(),
                           r.out[(std::size_t) port].begin() + n, v);
            }

            break;
        }

        case Kind::comp:
        {
            // feed-forward comp
            const float* in = input (b, 0, n);
            const float* key = connected (b.id, 1) ? input (b, 1, n) : in;

            const float thr = std::pow (10.0f, (-48.0f + kv[0] * 48.0f) / 20.0f);
            const float slope = 1.0f - 1.0f / (1.0f + kv[1] * 19.0f);        // to 20:1
            const float atk = 0.0003f + kv[2] * kv[2] * 0.15f;
            const float rel = 0.02f + kv[3] * kv[3] * 1.5f;
            const float makeup = std::pow (10.0f, kv[4] * 24.0f / 20.0f);
            const float ca = (float) std::exp (-1.0 / (sr * atk));
            const float cr = (float) std::exp (-1.0 / (sr * rel));
            float worst = 0.0f;

            for (int i = 0; i < n; ++i)
            {
                const float x = std::fabs (key[i]);
                const float c = x > r.det ? ca : cr;
                r.det = x + c * (r.det - x);

                const float g = r.det > thr ? std::pow (thr / r.det, slope) : 1.0f;

                o0[i] = in[i] * g * makeup;
                worst = std::max (worst, 1.0f - g);
            }

            r.grNow = worst;
            pushHistory (r.scope, worst);
            break;
        }

        case Kind::limit:
        {
            // brickwall, no lookahead
            const float* in = input (b, 0, n);
            const float ceiling = 0.05f + kv[0] * 0.95f;
            const float rel = 0.01f + kv[1] * kv[1] * 0.8f;
            const float drive = std::pow (10.0f, kv[2] * 24.0f / 20.0f);
            const float cr = (float) std::exp (-1.0 / (sr * rel));
            float worst = 0.0f;

            for (int i = 0; i < n; ++i)
            {
                const float x = std::fabs (in[i]) * drive;
                r.det = x > r.det ? x : x + cr * (r.det - x);

                const float g = r.det > ceiling ? ceiling / r.det : 1.0f;

                o0[i] = in[i] * drive * g;
                worst = std::max (worst, 1.0f - g);
            }

            r.grNow = worst;
            pushHistory (r.scope, worst);
            break;
        }

        case Kind::input:
        {
            // -48..+12 dB, 0 = off
            const float gain = kv[0] < 0.001f ? 0.0f : std::pow (10.0f, (-48.0f + kv[0] * 60.0f) / 20.0f);

            for (int i = 0; i < n; ++i)
            {
                o0[i] = i < inputLen ? inputL[(std::size_t) i] * gain : 0.0f;
                o1[i] = i < inputLen ? inputR[(std::size_t) i] * gain : 0.0f;
            }
            break;
        }

        case Kind::midi:
        {
            const float* clk = input (b, 0, n);
            const float* rst = input (b, 1, n);

            // pulse = step start
            const double stepTicks = kPpq / 4.0 * (b.option == 2 ? 4.0 : b.option == 1 ? 2.0 : 1.0);
            const double loop = std::max ((double) kPpq, (double) b.loopTicks);
            const int stepsInLoop = std::max (1, (int) std::round (loop / stepTicks));
            const double hostRate = (bpm / 60.0) * kPpq / sr;          // ticks/sample fallback
            const float transpose = std::round ((kv[0] * 2.0f - 1.0f) * 24.0f);
            const int retrigger = std::max (1, (int) (sr * 0.002));    // 2 ms retrig gap
            const double shortest = kPpq / 16.0;                      // min note: 1/64

            for (int i = 0; i < n; ++i)
            {
                const bool resetHi = rst[i] > 0.5f;

                if (resetHi && ! r.prevReset) { r.midiStep = -1; r.midiPos = 0.0; }
                r.prevReset = resetHi;

                const bool hi = clk[i] > 0.5f;

                if (hi && ! r.prevGate)
                {
                    if (r.midiStep >= 0 && r.sinceTick > 0) r.midiRate = stepTicks / r.sinceTick;

                    r.midiStep = (r.midiStep + 1) % stepsInLoop;
                    r.midiPos = r.midiStep * stepTicks;
                    r.sinceTick = 0;
                }

                r.prevGate = hi;

                int sounding = -1;

                if (r.midiStep >= 0)
                {
                    ++r.sinceTick;
                    const double rate = r.midiRate > 0.0 ? r.midiRate : hostRate;
                    r.midiPos = std::min (r.midiPos + rate, (r.midiStep + 1) * stepTicks - 1.0e-6);

                    // newest active note
                    int latest = -1;

                    for (std::size_t k = 0; k < b.phrase.size(); ++k)
                    {
                        const auto& note = b.phrase[k];
                        const double end = note.start + std::max ((double) note.length, shortest);

                        if (note.start <= r.midiPos && r.midiPos < end && note.start >= latest)
                        {
                            latest = note.start;
                            sounding = (int) k;
                        }
                    }
                }

                if (sounding != r.midiNote)
                {
                    if (sounding >= 0)
                    {
                        if (r.midiNote >= 0) r.retrig = retrigger;      // legato -> retrig
                        r.midiPitch = (float) (b.phrase[(std::size_t) sounding].pitch - 60);
                        r.midiVel = b.phrase[(std::size_t) sounding].velocity / 127.0f;
                    }

                    r.midiNote = sounding;
                }

                const bool gate = r.midiNote >= 0 && r.retrig == 0;
                if (r.retrig > 0) --r.retrig;

                o0[i] = r.midiPitch + transpose;
                o1[i] = gate ? 1.0f : 0.0f;
                o2[i] = r.midiVel;
            }

            r.phase = r.midiStep >= 0 ? r.midiPos / loop : 0.0;     // for the panel's playhead
            r.step = r.midiNote;                                  // lit note
            break;
        }

        case Kind::ball:
        {
            // gravity + bounces
            const float* kick = input (b, 0, n);
            const float dt = 1.0f / (float) sr;

            const float g = 1.5f + kv[2] * kv[2] * 48.0f;          // units per second²
            const float keep = 0.3f + kv[3] * 0.699f;              // restitution
            const float up = 0.5f + kv[0] * 5.5f;
            const float across = (kv[1] * 2.0f - 1.0f) * 3.0f;
            const bool wrap = b.option == 1;

            for (int i = 0; i < n; ++i)
            {
                const bool hi = kick[i] > 0.5f;
                bool floorHit = false, wallHit = false;

                if (hi && ! r.prevGate)                            // thrown again
                {
                    r.bvy = up;
                    r.bvx = across;
                    r.restFor = 0.0f;
                }

                r.prevGate = hi;

                if (r.restFor > 0.0f)
                {
                    // at rest: rethrow
                    r.by = -1.0f;
                    r.bvy = 0.0f;
                    r.restFor += dt;

                    if (r.restFor > 0.6f)
                    {
                        r.bvy = up;
                        r.bvx = across;
                        r.restFor = 0.0f;
                    }
                }
                else
                {
                    r.bvy -= g * dt;
                    r.by += r.bvy * dt;
                    r.bx += r.bvx * dt;

                    if (r.by <= -1.0f)
                    {
                        r.by = -1.0f;
                        r.bvy = -r.bvy * keep;
                        floorHit = true;

                        // settled
                        if (r.bvy < 0.35f) { r.bvy = 0.0f; r.restFor = 1.0e-6f; }
                    }

                    if (r.bx < -1.0f || r.bx > 1.0f)
                    {
                        if (wrap) r.bx = r.bx > 0.0f ? -1.0f : 1.0f;
                        else { r.bx = r.bx > 0.0f ? 1.0f : -1.0f; r.bvx = -r.bvx; }

                        wallHit = true;
                    }
                }

                r.floorLit = floorHit ? 1.0f : std::max (0.0f, r.floorLit - dt * 8.0f);
                r.wallLit = wallHit ? 1.0f : std::max (0.0f, r.wallLit - dt * 8.0f);

                o0[i] = clampf (r.bx, -1.0f, 1.0f);
                o1[i] = clampf (r.by, -1.0f, 1.0f);
                o2[i] = floorHit ? 1.0f : 0.0f;
                o3[i] = wallHit ? 1.0f : 0.0f;
            }

            break;
        }

        case Kind::meter:
        {
            // pass-through + needle
            const float* in = input (b, 0, n);
            const float range = 0.25f + kv[0] * 3.0f;
            const float fall = 0.004f + (1.0f - kv[1]) * 0.25f;

            float peak = 0.0f;
            double sum = 0.0;

            for (int i = 0; i < n; ++i)
            {
                o0[i] = in[i];
                peak = std::max (peak, std::fabs (in[i]));
                sum += (double) in[i] * in[i];
            }

            const float reading = clampf ((b.option == 1
                                             ? (float) std::sqrt (sum / std::max (1, n))
                                             : peak) * range, 0.0f, 1.0f);

            r.slewed = reading > r.slewed ? reading
                                          : r.slewed + (reading - r.slewed) * fall;
            break;
        }

        case Kind::out:
        default:
            for (int i = 0; i < n; ++i) o0[i] = 0.0f;
            break;
    }
}

} // namespace qd

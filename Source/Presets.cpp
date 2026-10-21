#include "Presets.h"
#include <random>

namespace blockPresets
{

// <0 = leave as is
struct BP { const char* name; int opt; float k[12]; };

#define NONE -1.0f

static const BP kEq[] = {
    { "flat",            0, { 0.0f, 0.35f, 0.5f, 0.35f, 0.5f, 0.4f, 0.5f, 0.6f, 0.5f, 0.7f } },
    { "vocal air",       1, { 0.22f, 0.3f, 0.46f, 0.3f, 0.5f, 0.45f, 0.58f, 0.62f, 0.68f, 0.85f } },
    { "vocal presence",  0, { 0.3f, 0.45f, 0.44f, 0.3f, 0.44f, 0.35f, 0.66f, 0.55f, 0.56f, 0.72f } },
    { "de-mud",          0, { 0.35f, 0.5f, 0.5f, 0.3f, 0.3f, 0.3f, 0.5f, 0.55f, 0.54f, 0.7f } },
    { "kick weight",     1, { 0.05f, 0.3f, 0.68f, 0.18f, 0.38f, 0.35f, 0.58f, 0.5f, 0.46f, 0.7f } },
    { "snare crack",     0, { 0.3f, 0.5f, 0.44f, 0.3f, 0.4f, 0.32f, 0.68f, 0.52f, 0.6f, 0.72f } },
    { "hat lift",        1, { 0.55f, 0.3f, 0.4f, 0.3f, 0.46f, 0.4f, 0.54f, 0.6f, 0.7f, 0.88f } },
    { "bass tighten",    0, { 0.28f, 0.45f, 0.6f, 0.2f, 0.36f, 0.3f, 0.52f, 0.5f, 0.46f, 0.6f } },
    { "guitar body cut", 0, { 0.25f, 0.5f, 0.46f, 0.3f, 0.32f, 0.38f, 0.56f, 0.58f, 0.54f, 0.72f } },
    { "drum bus glue",   1, { 0.15f, 0.3f, 0.58f, 0.25f, 0.46f, 0.4f, 0.54f, 0.58f, 0.6f, 0.78f } },
    { "telephone",       0, { 0.62f, 0.7f, 0.5f, 0.3f, 0.72f, 0.45f, 0.72f, 0.6f, 0.28f, 0.5f } },
    { "smile",           1, { 0.1f, 0.3f, 0.66f, 0.28f, 0.36f, 0.4f, 0.46f, 0.6f, 0.68f, 0.8f } },
    { "dark",            1, { 0.0f, 0.3f, 0.6f, 0.3f, 0.5f, 0.4f, 0.42f, 0.6f, 0.3f, 0.6f } },
    { "air only",        1, { 0.3f, 0.3f, 0.5f, 0.3f, 0.5f, 0.4f, 0.5f, 0.6f, 0.72f, 0.92f } },
    { "low cut",         0, { 0.5f, 0.35f, 0.5f, 0.35f, 0.5f, 0.4f, 0.5f, 0.6f, 0.5f, 0.7f } } };

static const BP kFilter[] = {
    { "acid",     1, { 0.2f, 0.78f, 0.68f } },
    { "warm lp",  0, { 0.5f, 0.25f, 0.5f } },
    { "sweep",    0, { 0.35f, 0.55f, 0.8f } },
    { "vocal bp", 3, { 0.5f, 0.7f, 0.5f } },
    { "thin hp",  4, { 0.55f, 0.3f, 0.5f } },
    { "scream",   5, { 0.45f, 0.85f, 0.6f } } };

static const BP kDrum[] = {
    { "deep kick",  0, { 0.22f, 0.68f, 0.4f, 0.6f } },
    { "tight kick", 0, { 0.4f, 0.35f, 0.5f, 0.7f } },
    { "click kick", 0, { 0.5f, 0.2f, 0.75f, 0.8f } },
    { "fat snare",  1, { 0.45f, 0.45f, 0.5f, 0.5f } },
    { "rim",        1, { 0.7f, 0.14f, 0.7f, 0.6f } },
    { "closed hat", 2, { 0.72f, 0.1f, 0.6f, 0.4f } },
    { "open hat",   2, { 0.68f, 0.55f, 0.55f, 0.4f } },
    { "fm tom",     3, { 0.35f, 0.5f, 0.45f, 0.5f } } };

static const BP kReson[] = {
    { "bells",    0, { 0.5f, 0.7f, 0.35f, 0.4f, 0.0f } },
    { "strings",  1, { 0.4f, 0.6f, 0.5f, 0.35f, 1.0f } },
    { "plucked",  2, { 0.35f, 0.55f, 0.45f, 0.3f, 0.0f } },
    { "fm voice", 3, { 0.5f, 0.6f, 0.5f, 0.5f, 0.0f } },
    { "plate",    5, { 0.7f, 0.7f, 0.6f, 0.5f, 0.0f } } };

static const BP kScatter[] = {
    { "cloud",    0, { 0.4f, 0.55f, 0.5f, 0.5f, 0.7f, 0.0f } },
    { "freeze",   0, { 0.4f, 0.6f, 0.4f, 0.4f, 1.0f, 1.0f } },
    { "stretch",  1, { 0.4f, 0.75f, 0.55f, 0.5f, 0.85f, 0.0f } },
    { "stutter",  2, { 0.3f, 0.2f, 0.7f, 0.35f, 0.8f, 0.0f } },
    { "shimmer",  3, { 0.5f, 0.7f, 0.5f, 0.7f, 0.75f, 0.0f } } };

static const BP kMacro[] = {
    { "analogue",  8,  { 0.4f, 0.5f, 0.5f, 0.5f, 0.5f, 0.0f } },
    { "fm bell",   10, { 0.55f, 0.6f, 0.4f, 0.3f, 0.6f, 0.0f } },
    { "wavetable", 13, { 0.45f, 0.55f, 0.5f, 0.4f, 0.5f, 0.0f } },
    { "chord",     14, { 0.4f, 0.5f, 0.5f, 0.5f, 0.5f, 0.0f } },
    { "speech",    15, { 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.0f } },
    { "swarm",     16, { 0.5f, 0.5f, 0.5f, 0.6f, 0.5f, 0.0f } },
    { "string",    19, { 0.35f, 0.5f, 0.55f, 0.3f, 0.55f, 0.0f } },
    { "kick",      21, { 0.3f, 0.6f, 0.4f, 0.4f, 0.5f, 0.0f } } };

static const BP kEnv[] = {
    { "pluck",   -1, { 0.01f, 0.16f, 0.0f, 0.14f } },
    { "percuss", -1, { 0.0f, 0.3f, 0.0f, 0.25f } },
    { "pad",     -1, { 0.5f, 0.6f, 0.8f, 0.7f } },
    { "swell",   -1, { 0.75f, 0.7f, 0.9f, 0.8f } },
    { "organ",   -1, { 0.0f, 0.2f, 1.0f, 0.12f } } };

static const BP kLfo[] = {
    { "slow sweep", 0, { 0.12f, 1.0f } },
    { "vibrato",    0, { 0.62f, 0.12f } },
    { "tremolo",    0, { 0.5f, 0.6f } },
    { "ramp",       2, { 0.3f, 1.0f } },
    { "square",     3, { 0.35f, 1.0f } },
    { "wander",     4, { 0.18f, 0.8f } } };

static const BP kDelay[] = {
    { "slapback", 0, { 0.15f, 0.25f, 0.6f } },
    { "dub",      3, { 0.8f, 0.45f, 0.4f } },
    { "ping",     1, { 0.55f, 0.4f, 0.8f } },
    { "wash",     4, { 0.85f, 0.6f, 0.3f } } };

static const BP kReverb[] = {
    { "room",    -1, { 0.25f, 0.25f } },
    { "hall",    -1, { 0.75f, 0.4f } },
    { "plate",   -1, { 0.55f, 0.5f } },
    { "cavern",  -1, { 0.98f, 0.6f } } };

static const BP kDrive[] = {
    { "warm",   0, { 0.2f, 0.5f } },
    { "crunch", 0, { 0.55f, 0.5f } },
    { "ruin",   0, { 0.9f, 0.55f } },
    { "fold",   1, { 0.45f, 0.5f } },
    { "buchla", 1, { 0.75f, 0.6f } } };

static const BP kLpg[] = {
    { "pluck", 0, { 0.2f, 0.85f } },
    { "bloom", 0, { 0.6f, 0.8f } },
    { "long",  0, { 0.85f, 0.75f } },
    { "gate",  1, { 0.3f, 0.9f } } };

static const BP kOsc[] = {
    { "saw",       0, { 0.5f, 0.5f, 0.5f } },
    { "sub",       0, { 0.4f, 0.5f, 0.5f } },
    { "narrow",    1, { 0.5f, 0.5f, 0.12f } },
    { "hollow",    1, { 0.5f, 0.5f, 0.5f } },
    { "soft tri",  2, { 0.5f, 0.5f, 0.5f } },
    { "pure sine", 3, { 0.5f, 0.5f, 0.5f } } };

// seq patterns: x . -
struct SeqPreset
{
    const char* group;
    const char* name;
    const char* steps;
    int   semis[8];
    float lane[8];
    float range, glide;
};

static const SeqPreset kSeq[] = {
    // drum loops, layer them
    { "drum loops", "four on the floor", "x.x.x.x.", { 0 }, { 1.0f, 0, 0.7f, 0, 0.9f, 0, 0.7f, 0 },    0.5f, 0.0f },
    { "drum loops", "backbeat",          "..x...x.", { 0 }, { 0, 0, 1.0f, 0, 0, 0, 1.0f, 0 },          0.5f, 0.0f },
    { "drum loops", "offbeat hat",       ".x.x.x.x", { 0 }, { 0, 0.8f, 0, 0.6f, 0, 0.8f, 0, 0.6f },    0.5f, 0.0f },
    { "drum loops", "eighths",           "xxxxxxxx", { 0 }, { 1.0f, 0.4f, 0.7f, 0.4f, 1.0f, 0.4f, 0.7f, 0.4f }, 0.5f, 0.0f },
    { "drum loops", "tresillo",          "x..x..x.", { 0 }, { 1.0f, 0, 0, 0.8f, 0, 0, 0.9f, 0 },       0.5f, 0.0f },
    { "drum loops", "boom bap",          "x....xx.", { 0 }, { 1.0f, 0, 0, 0, 0, 0.85f, 0.5f, 0 },      0.5f, 0.0f },
    { "drum loops", "breakbeat",         "x..x.x..", { 0 }, { 1.0f, 0, 0, 0.7f, 0, 0.9f, 0, 0 },       0.5f, 0.0f },
    { "drum loops", "half time",         "....x...", { 0 }, { 0, 0, 0, 0, 1.0f, 0, 0, 0 },             0.5f, 0.0f },

    // melodies (quant them)
    { "melodies", "arp up",        "xxxxxxxx", { 0, 3, 7, 12, 0, 3, 7, 12 }, { 0.4f, 0.5f, 0.6f, 0.8f, 0.4f, 0.5f, 0.6f, 0.8f }, 0.5f, 0.05f },
    { "melodies", "arp down",      "xxxxxxxx", { 12, 7, 3, 0, 12, 7, 3, 0 }, { 0.8f, 0.6f, 0.5f, 0.4f, 0.8f, 0.6f, 0.5f, 0.4f }, 0.5f, 0.05f },
    { "melodies", "octave bounce", "xxxxxxxx", { 0, 12, 0, 12, 0, 12, 0, 12 }, { 1.0f, 0.5f, 0.8f, 0.5f, 1.0f, 0.5f, 0.8f, 0.5f }, 0.5f, 0.0f },
    { "melodies", "acid line",     "xx-x.x-x", { 0, 0, 12, 0, 3, 0, 7, 5 },   { 0.9f, 0.4f, 1.0f, 0.5f, 0.3f, 0.7f, 1.0f, 0.5f }, 0.5f, 0.3f },
    { "melodies", "pedal fifths",  "x.x.xx.x", { 0, 0, 7, 0, 0, 7, 0, 5 },    { 0.6f, 0, 0.8f, 0, 0.6f, 0.9f, 0, 0.7f },          0.5f, 0.0f },
    { "melodies", "minor walk",    "xxxxxxx-", { 0, 2, 3, 5, 7, 8, 10, 12 },  { 0.3f, 0.4f, 0.5f, 0.6f, 0.7f, 0.8f, 0.9f, 1.0f }, 0.5f, 0.2f } };

static constexpr int kSeqCount = (int) (sizeof (kSeq) / sizeof (kSeq[0]));

static void applySeq (int i, qd::Block& b)
{
    if (i < 0 || i >= kSeqCount) return;

    const auto& sp = kSeq[i];
    b.knob[0] = sp.range;
    b.knob[1] = sp.glide;

    for (int st = 0; st < qd::kSteps; ++st)
    {
        // semis -> fader
        const float v = 0.5f + sp.semis[st] / (48.0f * std::max (0.05f, sp.range));
        b.steps[(std::size_t) st] = std::min (1.0f, std::max (0.0f, v));
        b.steps2[(std::size_t) st] = sp.lane[st];

        const char c = sp.steps[st];
        b.stepMode[(std::size_t) st] = c == 'x' ? qd::Block::Step::rise
                                     : c == '-' ? qd::Block::Step::fall
                                                : qd::Block::Step::off;
    }
}

struct Table { qd::Kind kind; const BP* items; int n; };

static const Table kTables[] = {
    { qd::Kind::filter,  kFilter,  (int) (sizeof (kFilter) / sizeof (BP)) },
    { qd::Kind::drum,    kDrum,    (int) (sizeof (kDrum) / sizeof (BP)) },
    { qd::Kind::reson,   kReson,   (int) (sizeof (kReson) / sizeof (BP)) },
    { qd::Kind::scatter, kScatter, (int) (sizeof (kScatter) / sizeof (BP)) },
    { qd::Kind::macro,   kMacro,   (int) (sizeof (kMacro) / sizeof (BP)) },
    { qd::Kind::env,     kEnv,     (int) (sizeof (kEnv) / sizeof (BP)) },
    { qd::Kind::lfo,     kLfo,     (int) (sizeof (kLfo) / sizeof (BP)) },
    { qd::Kind::delay,   kDelay,   (int) (sizeof (kDelay) / sizeof (BP)) },
    { qd::Kind::reverb,  kReverb,  (int) (sizeof (kReverb) / sizeof (BP)) },
    { qd::Kind::damage,  kDrive,   (int) (sizeof (kDrive) / sizeof (BP)) },
    { qd::Kind::lpg,     kLpg,     (int) (sizeof (kLpg) / sizeof (BP)) },
    { qd::Kind::osc,     kOsc,     (int) (sizeof (kOsc) / sizeof (BP)) } };

static const Table* tableFor (qd::Kind k)
{
    for (const auto& t : kTables) if (t.kind == k) return &t;
    return nullptr;
}

int count (qd::Kind k)
{
    if (k == qd::Kind::seq) return kSeqCount;

    const auto* t = tableFor (k);
    return t ? t->n : 0;
}

const char* name (qd::Kind k, int i)
{
    if (k == qd::Kind::seq) return (i >= 0 && i < kSeqCount) ? kSeq[i].name : "";

    const auto* t = tableFor (k);
    return (t && i >= 0 && i < t->n) ? t->items[i].name : "";
}

const char* group (qd::Kind k, int i)
{
    if (k == qd::Kind::seq) return (i >= 0 && i < kSeqCount) ? kSeq[i].group : "";
    return "";
}

void apply (qd::Kind k, int i, qd::Block& b)
{
    if (k == qd::Kind::seq) { applySeq (i, b); return; }

    const auto* t = tableFor (k);
    if (t == nullptr || i < 0 || i >= t->n) return;

    const auto& bp = t->items[i];
    const int knobs = qd::spec (k).numKnobs;

    for (int j = 0; j < knobs && j < 12; ++j)
        if (bp.k[j] >= 0.0f) b.knob[(std::size_t) j] = bp.k[j];

    if (bp.opt >= 0) b.option = bp.opt;
}

#undef NONE

} // namespace blockPresets

namespace presets
{

using K = qd::Kind;

static const char* kNames[] =
{
    "01 Start",             "02 Ladder Bass",      "03 Acid Line",        "04 Turing Drift",
    "05 Euclid Techno",     "06 Weather Vane",     "07 Hands Free",       "08 Breathing Grain",
    "09 Chaos Bench",  "10 Ring Metal",       "11 Crush Loop",       "12 Slew Glide",
    "13 Switch Canon",      "14 Logic Rhythm",     "15 Drone Cell",       "16 Chaos FM",
    "17 S&H Burble",        "18 Function Sweep",   "19 Feedback Tank",    "20 Generative Piece",
    "21 Techno Kick",       "22 Techno Stack",     "23 Acid Drive",       "24 Psych Resonator",
    "25 Granular Bloom",    "26 Mallet Garden",    "27 Ring Cascade",     "28 Chance Machine",
    "29 Tape Loop",         "30 Bell Choir",        "31 Four Hands",       "32 Easy Loop",
    "33 Simple Patch",      "34 Glacier Chord",    "35 Shifting Choir",   "36 Sympathetic Fog",
    "37 Phase Canon",       "38 Glass Rain"
};

int count() { return (int) (sizeof (kNames) / sizeof (kNames[0])); }

const char* name (int i) { return kNames[i >= 0 && i < count() ? i : 0]; }

// flow layout cursor
struct Row
{
    // half-square row gap
    Row (qd::Patch& patch, int row) : p (patch), y (row * qd::kSub * 3 / 2) {}

    qd::Patch& p;
    int y;
    int x = 0, prev = -1;

    // square gaps, cabled
    int add (K k)
    {
        const int id = p.add (k, x, y);

        if (prev >= 0 && id >= 0
            && qd::spec (p.find (prev)->kind).numOut > 0 && qd::spec (k).numIn > 0)
            p.connect (prev, 0, id, 0);

        prev = id;
        x += (qd::spec (k).cells + 1) * qd::kSub;
        return id;
    }

    void skip (int cells = 1) { x += cells * qd::kSub; prev = -1; }
};

static void set (qd::Patch& p, int id, float a, float b = -1.0f, float c = -1.0f, int opt = -1)
{
    if (auto* blk = p.find (id))
    {
        if (a >= 0.0f) blk->knob[0] = a;
        if (b >= 0.0f) blk->knob[1] = b;
        if (c >= 0.0f) blk->knob[2] = c;
        if (opt >= 0)  blk->option = opt;
    }
}

// <0 = default
static void setk (qd::Patch& p, int id, std::initializer_list<float> v, int opt = -1)
{
    if (auto* b = p.find (id))
    {
        int i = 0;
        for (float f : v)
        {
            if (i >= qd::kMaxKnobs) break;
            if (f >= 0.0f) b->knob[(std::size_t) i] = f;
            ++i;
        }

        if (opt >= 0) b->option = opt;
    }
}

// show the drawer
static void drawers (qd::Patch& p, std::initializer_list<int> ids)
{
    for (int id : ids) if (auto* b = p.find (id)) b->drawer = true;
}

// 0 note, 1 rest, 2 tie
static void lane2 (qd::Patch& p, int id, std::initializer_list<float> v)
{
    if (auto* b = p.find (id))
    {
        int i = 0;
        for (float f : v) if (i < qd::kSteps) b->steps2[(std::size_t) i++] = f;
    }
}

static void modes (qd::Patch& p, int id, std::initializer_list<int> v)
{
    if (auto* b = p.find (id))
    {
        int i = 0;
        for (int m : v)
            if (i < qd::kSteps) b->stepMode[(std::size_t) i++] = (qd::Block::Step) m;
    }
}

static void steps (qd::Patch& p, int id, std::initializer_list<float> v)
{
    if (auto* blk = p.find (id))
    {
        int i = 0;
        for (float f : v) if (i < qd::kSteps) blk->steps[(std::size_t) i++] = f;
    }
}

// random patch builder
// named from contents
static std::string nameFor (const qd::Patch& p, std::mt19937& rng)
{
    const qd::Block* voice = nullptr;
    const qd::Block* player = nullptr;
    const qd::Block* clock = nullptr;
    const qd::Block* mover = nullptr;
    std::vector<const qd::Block*> shaping;

    for (const auto& b : p.blocks())
    {
        switch (b.kind)
        {
            case K::osc: case K::macro: case K::timbre:
            case K::drum: case K::reson: case K::mallet:
                if (voice == nullptr) voice = &b;
                break;
            case K::seq: case K::euclid: case K::turing: player = &b; break;
            case K::clock: clock = &b; break;
            case K::lfo: case K::ball: mover = &b; break;
            case K::filter: case K::lpg: case K::damage: case K::ring:
            case K::delay: case K::reverb: case K::level:
                shaping.push_back (&b);
                break;
            default: break;
        }
    }

    // ── voice ──
    std::string noun = "Tone";

    if (voice != nullptr)
    {
        const int o = voice->option;

        switch (voice->kind)
        {
            case K::osc:
            {
                const char* waves[] = { "Saw", "Pulse", "Triangle", "Sine" };
                noun = waves[std::min (3, std::max (0, o))];

                // name osc by player
                if (player != nullptr && rng() % 2 == 0)
                    noun = player->kind == K::seq ? "Line"
                         : player->kind == K::euclid ? "Pattern" : "Loop";
                break;
            }

            case K::macro:
            {
                static const char* engines[] = {
                    "Analog", "Phase", "Glass", "Glass", "Glass", "Terrain", "Strings", "Chips",
                    "Analog", "Shapes", "Bells", "Grains", "Organ", "Tables", "Chords", "Voices",
                    "Swarm", "Noise", "Rain", "Strings", "Bars", "Kick", "Snare", "Hats" };
                noun = engines[std::min (23, std::max (0, o))];
                break;
            }

            case K::timbre:
            {
                const int m = (int) std::lround (voice->knob[0] * 47.0f);
                noun = m <= 8  ? "Buzz"   : m <= 15 ? "Swarm"  : m == 16 ? "Toy"
                     : m <= 20 ? "Sweep"  : m <= 24 ? "Vowels" : m <= 27 ? "Bells"
                     : m == 28 ? "Plucks" : m == 29 ? "Bows"   : m <= 31 ? "Flutes"
                     : m == 32 ? "Bells"  : m == 33 ? "Drums"  : m == 34 ? "Kick"
                     : m == 35 ? "Cymbals": m == 36 ? "Snare"  : m <= 40 ? "Tables"
                     : m == 41 ? "Noise"  : m == 42 ? "Peaks"  : m == 43 ? "Static"
                     : m == 44 ? "Grains" : m == 45 ? "Dust"   : m == 46 ? "Circuits"
                                                                          : "Mystery";
                break;
            }

            case K::drum:
            {
                const char* kinds[] = { "Kick", "Snare", "Hats", "Toms" };
                noun = kinds[std::min (3, std::max (0, o))];
                break;
            }

            case K::reson:
            {
                const char* kinds[] = { "Bars", "Strings", "String", "Bells", "Chimes", "Halls" };
                noun = kinds[std::min (5, std::max (0, o))];
                break;
            }

            case K::mallet:
            {
                const char* kinds[] = { "Mallets", "Strings", "Skins" };
                noun = kinds[std::min (2, std::max (0, o))];
                break;
            }

            default: break;
        }
    }

    // weighted traits
    std::vector<std::pair<int, std::string>> traits;

    for (const auto* b : shaping)
    {
        switch (b->kind)
        {
            case K::ring:   traits.push_back ({ 90, "Metal" }); break;
            case K::damage: traits.push_back (b->option == 1 ? std::make_pair (85, std::string ("Folded"))
                                            : b->option == 2 ? std::make_pair (85, std::string ("Crushed"))
                                                             : std::make_pair (40, std::string ("Warm")));
                            break;
            case K::filter:
                if      (b->option == 1)     traits.push_back ({ 80, "Acid" });
                else if (b->option == 3)     traits.push_back ({ 55, "Hollow" });
                else if (b->option == 4)     traits.push_back ({ 55, "Thin" });
                else if (b->knob[0] < 0.35f) traits.push_back ({ 45, "Dark" });
                break;
            case K::lpg:    traits.push_back ({ 60, "Wooden" }); break;
            case K::delay:  traits.push_back ({ 65, "Echoing" }); break;
            case K::reverb: traits.push_back ({ b->knob[0] > 0.6f ? 62 : 50,
                                                b->knob[0] > 0.6f ? "Vast" : "Distant" });
                            break;
            default: break;
        }
    }

    if (mover != nullptr)
    {
        if (mover->kind == K::ball) traits.push_back ({ 75, "Bouncing" });
        else
        {
            const char* moods[] = { "Breathing", "Swaying", "Rising", "Stepping", "Restless" };
            traits.push_back ({ 35, moods[std::min (4, std::max (0, mover->option))] });
        }
    }

    if (player != nullptr && player->kind == K::turing) traits.push_back ({ 30, "Drifting" });
    if (player != nullptr && player->kind == K::euclid) traits.push_back ({ 25, "Rolling" });

    if (clock != nullptr)
    {
        if (clock->option >= 6)      traits.push_back ({ 20, "Busy" });
        else if (clock->option <= 2) traits.push_back ({ 20, "Slow" });
    }

    if (traits.empty()) return "Little " + noun;

    std::stable_sort (traits.begin(), traits.end(),
                      [] (const auto& a, const auto& b) { return a.first > b.first; });

    const std::size_t pickFrom = std::min<std::size_t> (2, traits.size());
    std::string adjective = traits[rng() % pickFrom].second;

    // no "Acid Acid"
    if (adjective == noun) adjective = traits.size() > 1 ? traits[1].second : "Little";

    return adjective + " " + noun;
}

std::string randomPatch (qd::Patch& p, unsigned int seed)
{
    std::mt19937 rng (seed);
    auto pick = [&rng] (std::initializer_list<K> from)
    {
        std::vector<K> v (from);
        return v[std::uniform_int_distribution<std::size_t> (0, v.size() - 1) (rng)];
    };
    auto chance = [&rng] (double odds)
    {
        return std::uniform_real_distribution<double> (0.0, 1.0) (rng) < odds;
    };
    auto spread = [&rng] (float lo, float hi)
    {
        return std::uniform_real_distribution<float> (lo, hi) (rng);
    };

    p.clear();

    Row voice (p, 0), timing (p, 3), shaping (p, 6);

    // voice -> shapers -> out
    const K vk = pick ({ K::osc, K::osc, K::macro, K::timbre, K::drum, K::reson, K::mallet });
    const int v = voice.add (vk);
    const bool struck = vk == K::drum || vk == K::reson || vk == K::mallet;

    // exciter first, then gap
    if (vk == K::reson || vk == K::mallet)
    {
        const int noise = shaping.add (K::noise);
        shaping.skip();
        p.connect (noise, 0, v, 0);
        setk (p, noise, { spread (0.4f, 0.95f) });
    }

    std::vector<int> chain;

    if (chance (0.8)) chain.push_back (voice.add (pick ({ K::filter, K::filter, K::lpg, K::level })));
    if (chance (0.4)) chain.push_back (voice.add (pick ({ K::damage, K::ring })));
    if (chance (0.5)) chain.push_back (voice.add (pick ({ K::delay, K::reverb })));

    voice.add (K::out);

    // clock + player
    const int clk = timing.add (K::clock);
    const K player = pick ({ K::seq, K::seq, K::euclid, K::turing });
    const int play = timing.add (player);
    const int gatePort = player == K::seq || player == K::turing ? 1 : 0;
    p.connect (clk, 0, play, 0);

    const int env = timing.add (K::env);
    p.connect (play, gatePort, env, 0);

    // wire triggers + pitch
    auto wired = [&p] (int id, int port)
    {
        for (const auto& c : p.cables())
            if (! c.toKnob && c.to == id && c.toPort == port) return true;

        return false;
    };

    for (const auto& b : p.blocks())
    {
        const auto& sp = qd::spec (b.kind);

        for (int i = 1; i < sp.numIn; ++i)
        {
            const std::string nm (sp.in[i]);

            if ((nm == "trig" || nm == "gate" || nm == "strum" || nm == "ping") && ! wired (b.id, i))
                p.connect (play, gatePort, b.id, i);
        }
    }

    if (struck && ! wired (v, 0)) p.connect (play, gatePort, v, 0);

    for (int i = 0; i < qd::spec (vk).numIn; ++i)
        if (std::string (qd::spec (vk).in[i]) == "pitch" && player != K::euclid)
            p.connect (play, 0, v, i);

    // ring needs a carrier
    for (const int id : chain)
    {
        const auto* b = p.find (id);

        if (b != nullptr && b->kind == K::ring && ! wired (id, 1))
        {
            const int partner = p.add (K::osc, 0, 12 * qd::kSub);

            if (partner >= 0)
            {
                p.connect (partner, 0, id, 1);
                setk (p, partner, { spread (0.35f, 0.7f), 0.5f, spread (0.0f, 1.0f) },
                      (int) (rng() % 4));
            }
        }
    }

    // env -> level, else cutoff
    bool placed = false;

    for (const int id : chain)
    {
        if (placed) break;
        const auto* b = p.find (id);
        if (b == nullptr) continue;

        // never into lpg ping
        if (b->kind == K::level)       { p.connect (env, 0, id, 1); placed = true; }
        else if (b->kind == K::filter) { p.connect (env, 0, id, 0, true); placed = true; }
    }

    // one slow modulator
    const bool bouncing = chance (0.3);
    const int lfo = shaping.add (bouncing ? K::ball : K::lfo);

    if (bouncing) setk (p, lfo, { spread (0.5f, 0.95f), spread (0.15f, 0.85f),
                                  spread (0.2f, 0.7f), spread (0.6f, 0.95f) });
    else          setk (p, lfo, { spread (0.05f, 0.4f), spread (0.2f, 0.8f) }, (int) (rng() % 5));

    // tone knobs only
    auto shapesSound = [] (const char* knob)
    {
        const std::string n (knob != nullptr ? knob : "");
        return ! (n == "level" || n == "gain" || n == "mix" || n == "blend" || n.empty());
    };

    for (const int id : chain)
    {
        const auto* b = p.find (id);
        if (b == nullptr || ! chance (0.6)) continue;

        const auto& sp = qd::spec (b->kind);
        std::vector<int> aimable;

        for (int k = 0; k < sp.numKnobs; ++k)
            if (sp.knobs[k].type == qd::Ctl::knob && shapesSound (sp.knobs[k].name)) aimable.push_back (k);

        if (! aimable.empty() && lfo >= 0)
        {
            p.connect (lfo, 0, id, aimable[rng() % aimable.size()], true);
            break;
        }
    }

    setk (p, clk, { 0.5f }, 3 + (int) (rng() % 4));
    {
        const float d = spread (0.15f, 0.6f);
        setk (p, env, { spread (0.0f, 0.3f), d, spread (0.2f, 0.8f), d * spread (0.6f, 1.4f), 0.5f });
    }

    // pick named modes
    for (auto& b : p.blocks())
    {
        // preset xor random mode
        const int np = blockPresets::count (b.kind);

        if (np > 0 && chance (0.8))
            blockPresets::apply (b.kind, (int) (rng() % (unsigned) np), b);
        else if (const int opts = qd::optionCount (qd::spec (b.kind).options);
                 opts > 1 && b.kind != K::clock && b.kind != K::timbre && chance (0.5))
            b.option = (int) (rng() % (unsigned) opts);

        if (b.kind == K::timbre) b.knob[0] = spread (0.0f, 1.0f);

        // swing around setting
        if (b.id == lfo && b.kind == K::lfo) b.knob[1] = std::min (b.knob[1], 0.45f);

        if (b.kind == K::lpg) b.knob[1] = spread (0.6f, 1.0f);

        if (b.kind == K::seq)
            for (int i = 0; i < qd::kSteps; ++i)
            {
                b.steps[(std::size_t) i] = spread (0.3f, 0.7f);
                b.steps2[(std::size_t) i] = spread (0.0f, 1.0f);
                b.stepMode[(std::size_t) i] = chance (0.2)
                                                ? (chance (0.5) ? qd::Block::Step::off
                                                                : qd::Block::Step::fall)
                                                : qd::Block::Step::rise;
            }
    }

    p.rebuild();

    std::mt19937 naming (seed * 2654435761u);
    return nameFor (p, naming);
}

void build (int index, qd::Patch& p)
{
    p.clear();

    switch (index)
    {
        default:
        case 0:   // Start
        {
            Row r0 (p, 0);
            Row r1 (p, 3);

            const int clk = r0.add (K::clock), seq = r0.add (K::seq);
            const int osc = r0.add (K::osc), flt = r0.add (K::filter);
            const int vca = r0.add (K::level), rev = r0.add (K::reverb);
            r0.add (K::out);

            r1.skip (2);
            const int env = r1.add (K::env);
            r1.skip (3);
            const int lfo = r1.add (K::lfo);

            // seq, osc, env, lfo
            p.connect (seq, 0, osc, 0);      // pitch
            p.connect (seq, 1, env, 0);      // gate
            p.connect (env, 0, vca, 1);      // shape
            p.connect (seq, 2, flt, 0, true);   // lane 2 -> brightness
            p.connect (lfo, 0, flt, 1);      // lfo sweep

            set (p, clk, 0.5f, -1.0f, -1.0f, 5);
            setk (p, seq, { 0.4f, 0.3f });
            if (auto* b = p.find (osc)) blockPresets::apply (K::osc, 4, *b);   // soft tri
            setk (p, osc, { 0.42f });
            setk (p, flt, { 0.42f, 0.35f, 0.55f }, 0);
            setk (p, env, { 0.02f, 0.3f, 0.25f });
            setk (p, lfo, { 0.12f, 0.4f }, 0);
            setk (p, rev, { 0.5f, 0.22f });

            steps (p, seq, { 0.5f, 0.56f, 0.5f, 0.62f, 0.44f, 0.5f, 0.68f, 0.56f });
            lane2 (p, seq, { 0.7f, 0.3f, 0.5f, 0.85f, 0.2f, 0.45f, 0.9f, 0.4f });
            modes (p, seq, { 0, 0, 2, 0, 1, 0, 0, 2 });   // x x - x .

            if (auto* b = p.find (seq)) b->drawer = false;
            break;
        }

        case 1:   // Ladder Bass
        {
            Row r0 (p, 0);
            Row r1 (p, 2);
            const int key = r0.add (K::key), osc = r0.add (K::osc);
            const int flt = r0.add (K::filter), vca = r0.add (K::level);
            const int drv = r0.add (K::damage); r0.add (K::out);
            const int env = r1.add (K::env), att = r1.add (K::level);
            p.connect (key, 1, env, 0); p.connect (env, 0, vca, 1);
            p.connect (env, 0, att, 0); p.connect (att, 0, flt, 1);
            set (p, osc, 0.4f, 0.5f, 0.5f, 0);
            set (p, flt, 0.28f, 0.5f, 0.62f, 0);
            set (p, env, 0.02f, 0.3f, 0.25f);
            set (p, att, 0.85f, 0.5f);
            set (p, drv, 0.25f, 0.5f, -1.0f, 0);
            break;
        }

        case 2:   // Acid Line
        {
            Row r0 (p, 0);
            Row r1 (p, 2);
            const int clk = r0.add (K::clock), seq = r0.add (K::seq);
            const int qnt = r0.add (K::quant), osc = r0.add (K::osc);
            const int flt = r0.add (K::filter), vca = r0.add (K::level);
            r0.add (K::out);
            const int env = r1.add (K::env), att = r1.add (K::level);
            p.connect (seq, 1, env, 0); p.connect (env, 0, vca, 1);
            p.connect (env, 0, att, 0); p.connect (att, 0, flt, 1);
            set (p, clk, 0.5f, -1.0f, -1.0f, 5);
            set (p, qnt, 0.5f, 0.45f, -1.0f, 2);
            set (p, osc, 0.42f, 0.5f, 0.35f, 1);
            set (p, flt, 0.2f, 0.72f, 0.62f, 1);
            set (p, env, 0.01f, 0.16f, 0.0f);
            set (p, att, 0.95f, 0.5f);
            steps (p, seq, { 0.5f, 0.5f, 0.62f, 0.5f, 0.44f, 0.68f, 0.5f, 0.56f });
            break;
        }

        case 3:   // Turing Drift
        {
            Row r0 (p, 0);
            const int clk = r0.add (K::clock), tur = r0.add (K::turing);
            const int qnt = r0.add (K::quant), osc = r0.add (K::osc);
            const int lpg = r0.add (K::lpg), rev = r0.add (K::reverb);
            r0.add (K::out);
            p.connect (tur, 1, lpg, 1);
            set (p, clk, 0.4f, -1.0f, -1.0f, 5);
            set (p, tur, 0.12f, 0.55f, -1.0f, 4);
            set (p, qnt, 0.5f, 0.5f, -1.0f, 3);
            set (p, osc, 0.5f, 0.5f, 0.5f, 2);
            set (p, lpg, 0.42f, 0.85f, -1.0f, 0);
            set (p, rev, 0.65f, 0.35f);
            break;
        }

        case 4:   // Euclid Techno
        {
            Row r0 (p, 0);
            Row r1 (p, 2);
            const int clk = r0.add (K::clock), euc = r0.add (K::euclid);
            const int noi = r0.add (K::noise), lpg = r0.add (K::lpg);
            const int mix = r0.add (K::mix); r0.add (K::out);
            const int dv = r1.add (K::div), eu2 = r1.add (K::euclid);
            const int osc = r1.add (K::osc), lp2 = r1.add (K::lpg);
            p.connect (clk, 0, dv, 0);
            p.connect (euc, 0, lpg, 1);
            p.connect (eu2, 0, lp2, 1);
            p.connect (noi, 0, lpg, 0);
            p.connect (osc, 0, lp2, 0);
            p.connect (lp2, 0, mix, 1);
            set (p, clk, 0.5f, -1.0f, -1.0f, 5);
            set (p, euc, 0.32f, 0.0f, 1.0f, 2);
            set (p, eu2, 0.2f, 0.25f, 0.85f, 2);
            set (p, dv, 0.07f, -1.0f, -1.0f, 0);
            set (p, noi, 0.75f);
            set (p, lpg, 0.14f, 0.7f, -1.0f, 0);
            set (p, osc, 0.29f, 0.5f, 0.5f, 1);
            set (p, lp2, 0.3f, 0.8f, -1.0f, 0);
            set (p, mix, 0.8f, 0.8f, 0.0f);
            break;
        }

        case 5:   // Weather Vane
        {
            Row r0 (p, 0);
            Row r1 (p, 3);
            const int osc = r0.add (K::osc), drv = r0.add (K::damage);
            const int flt = r0.add (K::filter), dly = r0.add (K::delay);
            const int rev = r0.add (K::reverb); r0.add (K::out);
            const int clk = r1.add (K::clock); r1.skip();
            const int lf1 = r1.add (K::lfo);   r1.skip();
            const int lf2 = r1.add (K::lfo);   r1.skip();
            const int rnd = r1.add (K::rand);

            p.connect (clk, 0, lf1, 0);
            p.connect (clk, 0, rnd, 0);
            p.connect (lf1, 0, drv, 0, true);      // amount
            p.connect (lf2, 0, dly, 0, true);      // feedback
            p.connect (rnd, 0, osc, 2, true);      // shape

            set (p, clk, 0.5f, -1.0f, -1.0f, 5);
            setk (p, osc, { 0.4f, 0.5f, 0.4f }, 0);
            setk (p, drv, { 0.3f, 0.5f }, 1);
            setk (p, flt, { 0.58f, 0.3f, 0.5f }, 0);
            setk (p, dly, { 0.3f, 0.34f, 0.7f }, 1);
            setk (p, rev, { 0.6f, 0.3f });
            setk (p, lf1, { 0.3f, 0.5f }, 0);
            setk (p, lf2, { 0.06f, 0.4f }, 0);
            setk (p, rnd, { 0.5f, 0.4f });
            drawers (p, { drv, dly, osc });
            break;
        }

        case 6:   // Hands Free
        {
            Row r0 (p, 0);
            Row r1 (p, 3);
            Row r2 (p, 6);
            const int mac = r0.add (K::macro), flt = r0.add (K::filter);
            const int rev = r0.add (K::reverb); r0.add (K::out);
            const int clk = r1.add (K::clock); r1.skip();
            const int seq = r1.add (K::seq);
            const int ct = r2.add (K::ctrl);   r2.skip();
            const int lfo = r2.add (K::lfo);

            p.connect (clk, 0, seq, 0);
            p.connect (seq, 0, mac, 0);            // pitch
            p.connect (seq, 1, mac, 1);            // trigger
            p.connect (ct, 0, mac, 0, true);       // harm
            p.connect (ct, 1, mac, 1, true);       // timbre
            p.connect (ct, 2, flt, 0, true);       // cutoff
            p.connect (ct, 3, rev, 1, true);       // reverb mix
            p.connect (lfo, 0, mac, 2, true);      // self-drifting morph

            set (p, clk, 0.5f, -1.0f, -1.0f, 5);
            setk (p, mac, { 0.35f, 0.4f, 0.4f, 0.45f, 0.5f, 0.0f }, 8);
            setk (p, flt, { 0.5f, 0.25f, 0.5f }, 0);
            setk (p, rev, { 0.55f, 0.22f });
            setk (p, lfo, { 0.1f, 0.35f }, 0);
            steps (p, seq, { 0.5f, 0.58f, 0.5f, 0.66f, 0.42f, 0.5f, 0.62f, 0.5f });
            drawers (p, { mac, rev });
            break;
        }

        case 7:   // Breathing Grain
        {
            Row r0 (p, 0);
            Row r1 (p, 3);
            const int osc = r0.add (K::osc), sct = r0.add (K::scatter);
            const int rev = r0.add (K::reverb); r0.add (K::out);
            const int clk = r1.add (K::clock); r1.skip();
            const int lf1 = r1.add (K::lfo);   r1.skip();
            const int lf2 = r1.add (K::lfo);   r1.skip();
            const int rnd = r1.add (K::rand);

            p.connect (clk, 0, rnd, 0);
            p.connect (lf1, 0, sct, 0, true);      // position
            p.connect (lf2, 0, sct, 1, true);      // size
            p.connect (rnd, 0, sct, 3, true);      // texture

            set (p, clk, 0.4f, -1.0f, -1.0f, 3);
            setk (p, osc, { 0.36f, 0.5f, 0.6f }, 2);
            setk (p, sct, { 0.4f, 0.5f, 0.55f, 0.5f, 0.85f, 0.0f }, 0);
            setk (p, rev, { 0.7f, 0.35f });
            setk (p, lf1, { 0.08f, 0.45f }, 0);
            setk (p, lf2, { 0.14f, 0.35f }, 1);
            setk (p, rnd, { 0.45f, 0.6f });
            drawers (p, { sct });
            break;
        }

        case 8:   // Chaos Bench
        {
            Row r0 (p, 0);
            Row r1 (p, 3);
            const int euc = r0.add (K::euclid), drm = r0.add (K::drum);
            const int rev = r0.add (K::reverb); r0.add (K::out);
            const int clk = r1.add (K::clock); r1.skip();
            const int cha = r1.add (K::chaos); r1.skip();
            const int lfo = r1.add (K::lfo);

            p.connect (clk, 0, euc, 0);
            p.connect (cha, 0, drm, 0, true);      // tone
            p.connect (cha, 1, drm, 1, true);      // decay
            p.connect (lfo, 0, drm, 2, true);      // snap

            set (p, clk, 0.5f, -1.0f, -1.0f, 5);
            setk (p, euc, { 0.4f, 0.0f, 0.95f }, 2);
            setk (p, drm, { 0.45f, 0.4f, 0.5f, 0.6f }, 3);
            setk (p, rev, { 0.5f, 0.24f });
            setk (p, cha, { 0.3f, 0.45f }, 0);
            setk (p, lfo, { 0.12f, 0.4f }, 1);
            drawers (p, { drm });
            break;
        }

        case 9:   // Ring Metal
        {
            Row r0 (p, 0);
            Row r1 (p, 2);
            const int o1 = r0.add (K::osc), rng = r0.add (K::ring);
            const int lpg = r0.add (K::lpg), rev = r0.add (K::reverb);
            r0.add (K::out);
            const int o2 = r1.add (K::osc), clk = r1.add (K::clock);
            p.connect (o2, 0, rng, 1);
            p.connect (clk, 0, lpg, 1);
            set (p, o1, 0.5f, 0.5f, 0.5f, 3);
            set (p, o2, 0.62f, 0.54f, 0.5f, 3);
            set (p, rng, 1.0f);
            set (p, lpg, 0.5f, 0.8f, -1.0f, 0);
            set (p, clk, 0.5f, -1.0f, -1.0f, 3);
            set (p, rev, 0.75f, 0.4f);
            break;
        }

        case 10:  // Crush Loop
        {
            Row r0 (p, 0);
            Row r1 (p, 2);
            const int clk = r0.add (K::clock), seq = r0.add (K::seq);
            const int osc = r0.add (K::osc), cru = r0.add (K::damage);
            const int dly = r0.add (K::delay); r0.add (K::out);
            const int env = r1.add (K::env), vca = r1.add (K::level);
            p.connect (seq, 0, osc, 0); p.connect (seq, 1, env, 0);
            p.connect (osc, 0, vca, 0); p.connect (env, 0, vca, 1);
            p.connect (vca, 0, cru, 0);
            set (p, clk, 0.5f, -1.0f, -1.0f, 5);
            set (p, osc, 0.5f, 0.5f, 0.5f, 0);
            set (p, cru, 0.22f, 0.38f);
            set (p, env, 0.01f, 0.2f, 0.1f);
            set (p, dly, 0.45f, 0.35f, 0.6f, 1);
            steps (p, seq, { 0.5f, 0.58f, 0.5f, 0.66f, 0.42f, 0.5f, 0.62f, 0.5f });
            break;
        }

        case 11:  // Slew Glide
        {
            Row r0 (p, 0);
            const int clk = r0.add (K::clock), seq = r0.add (K::seq);
            const int slw = r0.add (K::slew), osc = r0.add (K::osc);
            const int flt = r0.add (K::filter), lpg = r0.add (K::lpg);
            r0.add (K::out);
            p.connect (seq, 1, lpg, 1);
            set (p, clk, 0.5f, -1.0f, -1.0f, 4);
            set (p, slw, 0.42f, 0.42f);
            set (p, osc, 0.42f, 0.5f, 0.5f, 0);
            set (p, flt, 0.42f, 0.45f, 0.5f, 0);
            set (p, lpg, 0.5f, 0.85f, -1.0f, 0);
            steps (p, seq, { 0.5f, 0.72f, 0.4f, 0.6f, 0.5f, 0.34f, 0.66f, 0.46f });
            break;
        }

        case 12:  // Switch Canon
        {
            Row r0 (p, 0);
            Row r1 (p, 2);
            const int clk = r0.add (K::clock), dv = r0.add (K::div);
            const int sw = r0.add (K::sswitch), osc = r0.add (K::osc);
            const int lpg = r0.add (K::lpg), rev = r0.add (K::reverb);
            r0.add (K::out);
            const int s1 = r1.add (K::seq), s2 = r1.add (K::turing);
            p.connect (clk, 0, s1, 0);
            p.connect (clk, 0, s2, 0);
            p.connect (s1, 0, sw, 1);
            p.connect (s2, 0, sw, 2);
            p.connect (dv, 0, sw, 0);
            p.connect (clk, 0, lpg, 1);
            set (p, clk, 0.5f, -1.0f, -1.0f, 5);
            set (p, dv, 0.2f, -1.0f, -1.0f, 0);
            set (p, sw, 0.05f);
            set (p, osc, 0.5f, 0.5f, 0.5f, 2);
            set (p, lpg, 0.3f, 0.8f, -1.0f, 0);
            set (p, s2, 0.18f, 0.45f, -1.0f, 3);
            set (p, rev, 0.6f, 0.3f);
            steps (p, s1, { 0.5f, 0.56f, 0.44f, 0.62f, 0.5f, 0.38f, 0.56f, 0.5f });
            break;
        }

        case 13:  // Logic Rhythm
        {
            Row r0 (p, 0);
            Row r1 (p, 2);
            const int clk = r0.add (K::clock), d1 = r0.add (K::div);
            const int lgc = r0.add (K::logic), lpg = r0.add (K::lpg);
            const int dly = r0.add (K::delay); r0.add (K::out);
            const int d2 = r1.add (K::div), noi = r1.add (K::noise);
            p.connect (clk, 0, d2, 0);
            p.connect (d2, 0, lgc, 1);
            p.connect (noi, 0, lpg, 0);
            p.connect (lgc, 0, lpg, 1);
            set (p, clk, 0.5f, -1.0f, -1.0f, 5);
            set (p, d1, 0.13f, -1.0f, -1.0f, 0);
            set (p, d2, 0.2f, -1.0f, -1.0f, 0);
            set (p, lgc, 1.0f, -1.0f, -1.0f, 2);
            set (p, noi, 0.6f);
            set (p, lpg, 0.12f, 0.8f, -1.0f, 0);
            set (p, dly, 0.5f, 0.3f, 0.55f, 0);
            break;
        }

        case 14:  // Drone Cell
        {
            Row r0 (p, 0);
            Row r1 (p, 2);
            Row r2 (p, 4);
            const int o1 = r0.add (K::osc), mix = r0.add (K::mix);
            const int flt = r0.add (K::filter), rev = r0.add (K::reverb);
            r0.add (K::out);
            const int o2 = r1.add (K::osc), o3 = r2.add (K::osc);
            const int lfo = r1.add (K::lfo);
            p.connect (o2, 0, mix, 1);
            p.connect (o3, 0, mix, 2);
            p.connect (lfo, 0, flt, 1);
            set (p, o1, 0.5f, 0.5f, 0.5f, 0);
            set (p, o2, 0.5f, 0.53f, 0.5f, 0);
            set (p, o3, 0.375f, 0.47f, 0.5f, 2);
            set (p, mix, 0.5f, 0.5f, 0.45f);
            set (p, flt, 0.4f, 0.55f, 0.62f, 5);
            set (p, lfo, 0.1f, 0.3f, -1.0f, 0);
            set (p, rev, 0.95f, 0.5f);
            break;
        }

        case 15:  // Chaos FM
        {
            Row r0 (p, 0);
            Row r1 (p, 2);
            const int cha = r0.add (K::chaos), att = r0.add (K::level);
            const int osc = r0.add (K::osc), flt = r0.add (K::filter);
            const int lpg = r0.add (K::lpg); r0.add (K::out);
            const int clk = r1.add (K::clock);
            p.connect (att, 0, osc, 1);
            p.connect (cha, 1, flt, 1);
            p.connect (clk, 0, lpg, 1);
            set (p, cha, 0.55f, 0.45f, -1.0f, 1);
            set (p, att, 0.62f, 0.5f);
            set (p, osc, 0.45f, 0.5f, 0.5f, 3);
            set (p, flt, 0.5f, 0.4f, 0.56f, 0);
            set (p, lpg, 0.4f, 0.8f, -1.0f, 0);
            set (p, clk, 0.5f, -1.0f, -1.0f, 4);
            break;
        }

        case 16:  // S&H Burble
        {
            Row r0 (p, 0);
            Row r1 (p, 2);
            const int noi = r0.add (K::noise), rnd = r0.add (K::rand);
            const int qnt = r0.add (K::quant), osc = r0.add (K::osc);
            const int flt = r0.add (K::filter), lpg = r0.add (K::lpg);
            r0.add (K::out);
            const int clk = r1.add (K::clock);
            p.connect (clk, 0, rnd, 0);
            p.connect (clk, 0, lpg, 1);
            set (p, noi, 0.8f);
            set (p, rnd, 0.75f, 0.0f);
            set (p, qnt, 0.5f, 0.55f, -1.0f, 3);
            set (p, osc, 0.5f, 0.5f, 0.5f, 1);
            set (p, flt, 0.48f, 0.5f, 0.5f, 0);
            set (p, lpg, 0.22f, 0.8f, -1.0f, 0);
            set (p, clk, 0.5f, -1.0f, -1.0f, 5);
            break;
        }

        case 17:  // Function Sweep
        {
            Row r0 (p, 0);
            const int fun = r0.add (K::env), att = r0.add (K::level);
            const int osc = r0.add (K::osc), flt = r0.add (K::filter);
            const int rev = r0.add (K::reverb); r0.add (K::out);
            p.connect (att, 0, flt, 1);
            set (p, fun, 0.85f, 0.9f, 0.5f, 1);
            set (p, att, 0.9f, 0.5f);
            set (p, osc, 0.42f, 0.5f, 0.5f, 0);
            set (p, flt, 0.25f, 0.62f, 0.62f, 0);
            set (p, rev, 0.8f, 0.35f);
            break;
        }

        case 18:  // Feedback Tank
        {
            Row r0 (p, 0);
            Row r1 (p, 2);
            const int osc = r0.add (K::osc), mix = r0.add (K::mix);
            const int dly = r0.add (K::delay), rev = r0.add (K::reverb);
            r0.add (K::out);
            const int cru = r1.add (K::damage);
            p.connect (dly, 0, cru, 0);
            p.connect (cru, 0, mix, 1);          // round it goes
            set (p, osc, 0.45f, 0.5f, 0.5f, 2);
            set (p, mix, 0.4f, 0.55f, 0.0f);
            set (p, dly, 0.55f, 0.6f, 0.5f, 1);
            set (p, cru, 0.5f, 0.55f);
            set (p, rev, 0.7f, 0.35f);
            break;
        }

        // rows from here on

        case 20:  // Techno Kick
        {
            Row voice (p, 0), rhythm (p, 3), clock (p, 6);
            const int drm = voice.add (K::drum);
            const int drv = voice.add (K::damage);
            const int eq  = voice.add (K::filter);
            const int out = voice.add (K::out);
            const int euc = rhythm.add (K::euclid);
            const int clk = clock.add (K::clock);
            p.connect (clk, 0, euc, 0);
            p.connect (euc, 0, drm, 0);
            setk (p, drm, { 0.32f, 0.55f, 0.45f, 0.6f }, 0);
            setk (p, drv, { 0.55f, 0.5f }, 0);
            setk (p, eq,  { 0.25f, 0.4f, 0.72f, 0.2f, 0.5f, 0.4f, 0.45f, 0.6f, 0.6f, 0.75f }, 0);
            setk (p, euc, { 0.26f, 0.0f, 1.0f }, 2);
            setk (p, clk, { 0.5f }, 5);
            setk (p, out, { 0.6f, 0.5f });      // headroom
            break;
        }

        case 21:  // Techno Stack
        {
            Row kick (p, 0), snare (p, 3), hat (p, 6), bus (p, 9), clock (p, 12);
            const int bd  = kick.add (K::drum);
            const int mix = kick.add (K::mix);
            const int drv = kick.add (K::damage);
            const int eq  = kick.add (K::filter);
            const int out = kick.add (K::out);
            const int e1 = snare.add (K::euclid);
            const int sd = snare.add (K::drum);
            const int e2 = hat.add (K::euclid);
            const int hh = hat.add (K::drum);
            const int e0 = bus.add (K::euclid);
            const int dv = bus.add (K::div);
            const int clk = clock.add (K::clock);

            p.connect (clk, 0, e0, 0);  p.connect (e0, 0, bd, 0);
            p.connect (clk, 0, e1, 0);  p.connect (e1, 0, sd, 0);
            p.connect (clk, 0, dv, 0);  p.connect (dv, 0, e2, 0);  p.connect (e2, 0, hh, 0);
            p.connect (sd, 0, mix, 1);  p.connect (hh, 0, mix, 2);

            setk (p, bd, { 0.3f, 0.6f, 0.4f, 0.62f }, 0);
            setk (p, sd, { 0.5f, 0.3f, 0.55f, 0.5f }, 1);
            setk (p, hh, { 0.7f, 0.12f, 0.6f, 0.4f }, 2);
            setk (p, e0, { 0.2f, 0.0f, 1.0f }, 2);
            setk (p, e1, { 0.14f, 0.25f, 1.0f }, 2);
            setk (p, e2, { 0.55f, 0.0f, 0.75f }, 2);
            setk (p, dv, { 0.0f }, 0);
            setk (p, mix, { 0.85f, 0.6f, 0.45f });
            setk (p, drv, { 0.4f, 0.5f }, 0);
            setk (p, eq, { 0.6f, 0.25f, 0.5f });
            setk (p, clk, { 0.5f }, 5);
            setk (p, out, { 0.55f, 0.5f });
            break;
        }

        case 22:  // Acid Drive
        {
            Row voice (p, 0), mod (p, 3), clock (p, 6);
            const int osc = voice.add (K::osc);
            const int flt = voice.add (K::filter);
            const int drv = voice.add (K::damage);
            const int vca = voice.add (K::level);
            voice.add (K::out);
            const int env = mod.add (K::env);
            const int att = mod.add (K::level);
            const int seq = mod.add (K::seq);
            const int clk = clock.add (K::clock);

            p.connect (clk, 0, seq, 0);
            p.connect (seq, 0, osc, 0);
            p.connect (seq, 1, env, 0);
            p.connect (env, 0, vca, 1);
            p.connect (env, 0, att, 0);
            p.connect (att, 0, flt, 1);

            setk (p, osc, { 0.42f, 0.5f, 0.35f }, 1);
            setk (p, flt, { 0.18f, 0.78f, 0.68f }, 1);
            setk (p, drv, { 0.45f, 0.5f }, 1);
            setk (p, env, { 0.01f, 0.14f, 0.0f });
            setk (p, att, { 0.95f, 0.5f });
            setk (p, clk, { 0.5f }, 5);
            steps (p, seq, { 0.5f, 0.5f, 0.62f, 0.5f, 0.44f, 0.68f, 0.5f, 0.56f });
            break;
        }

        case 23:  // Psych Resonator
        {
            Row voice (p, 0), mod (p, 3), clock (p, 6);
            const int tim = voice.add (K::macro);
            const int res = voice.add (K::reson);
            const int sca = voice.add (K::scatter);
            voice.add (K::out);
            const int tur = mod.add (K::turing);
            const int qnt = mod.add (K::quant);
            const int lfo = mod.add (K::lfo);
            const int clk = clock.add (K::clock);

            p.connect (clk, 0, tur, 0);
            p.connect (tur, 0, qnt, 0);
            p.connect (qnt, 0, tim, 0);
            p.connect (qnt, 0, res, 1);
            p.connect (clk, 0, res, 2);
            p.connect (lfo, 0, sca, 2);

            setk (p, tim, { 0.5f, 0.6f, 0.45f }, 24);      // plucked
            setk (p, res, { 0.4f, 0.62f, 0.5f, 0.35f, 0.5f }, 1);
            setk (p, sca, { 0.45f, 0.6f, 0.5f, 0.55f, 0.7f, 0.0f }, 0);
            setk (p, tur, { 0.12f, 0.5f }, 4);
            setk (p, qnt, { 0.5f, 0.4f }, 3);
            setk (p, lfo, { 0.1f, 0.25f }, 0);
            setk (p, clk, { 0.5f }, 4);
            break;
        }

        case 24:  // Granular Bloom
        {
            Row voice (p, 0), mod (p, 3), clock (p, 6);
            const int mac = voice.add (K::macro);
            const int sca = voice.add (K::scatter);
            const int rev = voice.add (K::reverb);
            voice.add (K::out);
            const int cha = mod.add (K::chaos);
            const int slw = mod.add (K::slew);
            const int qnt = mod.add (K::quant);
            const int clk = clock.add (K::clock);

            p.connect (cha, 0, slw, 0);
            p.connect (slw, 0, qnt, 0);
            p.connect (qnt, 0, mac, 0);
            p.connect (clk, 0, mac, 1);

            setk (p, mac, { 0.45f, 0.55f, 0.4f, 0.35f, 0.5f, 0.0f }, 5);   // wavetable
            setk (p, sca, { 0.4f, 0.7f, 0.55f, 0.6f, 0.8f, 0.0f }, 1);     // stretch
            setk (p, rev, { 0.85f, 0.45f });
            setk (p, cha, { 0.22f, 0.4f }, 0);
            setk (p, slw, { 0.55f, 0.55f });
            setk (p, qnt, { 0.5f, 0.35f }, 3);
            setk (p, clk, { 0.5f }, 3);
            break;
        }

        case 25:  // Mallet Garden
        {
            Row voice (p, 0), mod (p, 3), clock (p, 6);
            const int mal = voice.add (K::mallet);
            const int eq  = voice.add (K::filter);
            const int rev = voice.add (K::reverb);
            voice.add (K::out);
            const int euc = mod.add (K::euclid);
            const int tur = mod.add (K::turing);
            const int qnt = mod.add (K::quant);
            const int clk = clock.add (K::clock);

            p.connect (clk, 0, euc, 0);
            p.connect (euc, 0, mal, 2);
            p.connect (euc, 0, tur, 0);
            p.connect (tur, 0, qnt, 0);
            p.connect (qnt, 0, mal, 1);

            setk (p, mal, { 0.25f, 0.6f, 0.45f, 0.6f, 0.5f, 0.35f }, 0);
            setk (p, eq, { 0.6f, 0.25f, 0.5f });
            setk (p, rev, { 0.75f, 0.35f });
            setk (p, euc, { 0.34f, 0.12f, 0.85f }, 2);
            setk (p, tur, { 0.15f, 0.42f }, 4);
            setk (p, qnt, { 0.5f, 0.4f }, 3);
            setk (p, clk, { 0.5f }, 5);
            break;
        }

        case 26:  // Ring Cascade
        {
            Row voice (p, 0), second (p, 3), clock (p, 6);
            const int t1 = voice.add (K::macro);
            const int xm = voice.add (K::xmod);
            const int eq = voice.add (K::filter);
            const int out = voice.add (K::out);
            const int t2 = second.add (K::macro);
            const int lpg = second.add (K::lpg);
            const int clk = clock.add (K::clock);

            p.connect (t2, 0, xm, 1);
            p.connect (clk, 0, lpg, 1);
            p.connect (xm, 0, lpg, 0);

            setk (p, t1, { 0.5f, 0.55f, 0.5f }, 20);   // ring mod
            setk (p, t2, { 0.62f, 0.4f, 0.6f }, 5);    // sine tri
            setk (p, xm, { 0.55f, 0.6f, 0.8f, 0.6f, 0.7f }, 2);
            setk (p, eq, { 0.6f, 0.25f, 0.5f });
            setk (p, lpg, { 0.45f, 0.85f }, 0);
            setk (p, clk, { 0.5f }, 4);
            setk (p, out, { 0.6f, 0.5f });
            break;
        }

        case 27:  // Chance Machine
        {
            Row voice (p, 0), gates (p, 3), pitch (p, 6), clock (p, 9);
            const int mac = voice.add (K::macro);
            const int lpg = voice.add (K::lpg);
            const int rev = voice.add (K::reverb);
            voice.add (K::out);
            const int e1 = gates.add (K::euclid);
            const int e2 = gates.add (K::euclid);
            const int lg = gates.add (K::logic);
            const int tur = pitch.add (K::turing);
            const int qnt = pitch.add (K::quant);
            const int clk = clock.add (K::clock);
            const int dv  = clock.add (K::div);

            p.connect (clk, 0, e1, 0);
            p.connect (clk, 0, dv, 0);
            p.connect (dv, 0, e2, 0);
            p.connect (e1, 0, lg, 0);
            p.connect (e2, 0, lg, 1);
            p.connect (lg, 0, lpg, 1);
            p.connect (lg, 0, tur, 0);
            p.connect (tur, 0, qnt, 0);
            p.connect (qnt, 0, mac, 0);
            p.connect (lg, 0, mac, 1);

            setk (p, mac, { 0.35f, 0.5f, 0.55f, 0.3f, 0.55f, 0.0f }, 11);  // string
            setk (p, lpg, { 0.4f, 0.8f }, 0);
            setk (p, rev, { 0.8f, 0.4f });
            setk (p, e1, { 0.3f, 0.0f, 1.0f }, 2);
            setk (p, e2, { 0.42f, 0.2f, 0.9f }, 0);
            setk (p, lg, { 1.0f }, 1);                                     // or
            setk (p, tur, { 0.2f, 0.5f }, 5);
            setk (p, qnt, { 0.5f, 0.45f }, 3);
            setk (p, dv, { 0.07f }, 0);
            setk (p, clk, { 0.5f }, 5);
            break;
        }

        case 28:  // Tape Loop
        {
            Row voice (p, 0), mod (p, 3), clock (p, 6);
            const int osc = voice.add (K::osc);
            const int lpg = voice.add (K::lpg);
            const int sca = voice.add (K::scatter);
            const int dly = voice.add (K::delay);
            voice.add (K::out);
            const int seq = mod.add (K::seq);
            const int cru = mod.add (K::damage);
            const int clk = clock.add (K::clock);

            p.connect (clk, 0, seq, 0);
            p.connect (seq, 0, osc, 0);
            p.connect (seq, 1, lpg, 1);
            p.connect (dly, 0, cru, 0);
            p.connect (cru, 0, sca, 1);        // round it goes

            setk (p, osc, { 0.45f, 0.5f, 0.5f }, 2);
            setk (p, lpg, { 0.3f, 0.8f }, 0);
            setk (p, sca, { 0.5f, 0.65f, 0.5f, 0.6f, 0.75f, 0.0f }, 2);   // looping delay
            setk (p, dly, { 0.55f, 0.45f, 0.55f }, 1);
            setk (p, cru, { 0.5f, 0.5f });
            setk (p, clk, { 0.5f }, 5);
            steps (p, seq, { 0.5f, 0.6f, 0.44f, 0.56f, 0.5f, 0.66f, 0.4f, 0.54f });
            break;
        }

        case 29:  // Bell Choir
        {
            Row voice (p, 0), mod (p, 3), clock (p, 6);
            const int noi = voice.add (K::noise);
            const int vca = voice.add (K::level);
            const int res = voice.add (K::reson);
            const int rev = voice.add (K::reverb);
            voice.add (K::out);
            const int fun = mod.add (K::env);
            const int rnd = mod.add (K::rand);
            const int qnt = mod.add (K::quant);
            const int slw = mod.add (K::slew);
            const int clk = clock.add (K::clock);

            p.connect (clk, 0, fun, 0);
            p.connect (fun, 0, vca, 1);
            p.connect (clk, 0, rnd, 0);
            p.connect (rnd, 0, qnt, 0);
            p.connect (qnt, 0, slw, 0);
            p.connect (slw, 0, res, 1);
            p.connect (clk, 0, res, 2);

            setk (p, noi, { 0.85f });
            setk (p, res, { 0.55f, 0.7f, 0.35f, 0.4f, 0.5f }, 0);
            setk (p, rev, { 0.9f, 0.5f });
            setk (p, fun, { 0.0f, 0.04f, 0.25f }, 0);
            setk (p, rnd, { 0.7f, 0.0f });
            setk (p, qnt, { 0.55f, 0.55f }, 3);
            setk (p, slw, { 0.1f, 0.1f });
            setk (p, clk, { 0.5f }, 3);
            break;
        }

        case 19:  // Generative Piece
        {
            Row r0 (p, 0);
            Row r1 (p, 2);
            Row r2 (p, 4);
            const int clk = r0.add (K::clock), euc = r0.add (K::euclid);
            const int tur = r0.add (K::turing), qnt = r0.add (K::quant);
            const int res = r0.add (K::reson), rev = r0.add (K::reverb);
            r0.add (K::out);
            const int noi = r1.add (K::noise), vca = r1.add (K::level);
            const int fun = r1.add (K::env), cha = r2.add (K::chaos);
            const int slw = r2.add (K::slew), dv = r2.add (K::div);
            p.connect (euc, 0, fun, 0);
            p.connect (fun, 0, vca, 1);
            p.connect (vca, 0, res, 0);
            p.connect (qnt, 0, res, 1);
            p.connect (euc, 0, tur, 0);
            p.connect (clk, 0, dv, 0);
            p.connect (dv, 0, euc, 0);
            p.connect (cha, 0, slw, 0);
            p.connect (slw, 0, res, 1);
            set (p, clk, 0.5f, -1.0f, -1.0f, 5);
            set (p, dv, 0.07f, -1.0f, -1.0f, 0);
            set (p, euc, 0.3f, 0.12f, 0.82f, 2);
            set (p, tur, 0.16f, 0.5f, -1.0f, 5);
            set (p, qnt, 0.5f, 0.42f, -1.0f, 3);
            set (p, noi, 0.7f);
            set (p, fun, 0.0f, 0.05f, 0.25f, 0);
            set (p, res, 0.86f, 0.6f, 1.0f, 0);
            set (p, cha, 0.2f, 0.3f, -1.0f, 0);
            set (p, slw, 0.6f, 0.6f);
            set (p, rev, 0.9f, 0.45f);
            break;
        }
        case 30:   // Four Hands
        {
            // 4 parts, shared clock
            Row drone (p, 0), melody (p, 3), bass (p, 6), kit (p, 9), mod (p, 11);

            // melody
            const int clk = melody.add (K::clock), seq1 = melody.add (K::seq);
            const int qnt = melody.add (K::quant), tim = melody.add (K::timbre);
            const int flt1 = melody.add (K::filter), lvl1 = melody.add (K::level);
            const int dly = melody.add (K::delay);
            const int mout = melody.add (K::out);

            // drone
            const int mac = drone.add (K::macro), flt2 = drone.add (K::filter);
            const int rev = drone.add (K::reverb);
            const int dout = drone.add (K::out);

            // bass
            const int dv = bass.add (K::div), seq2 = bass.add (K::seq);
            const int bosc = bass.add (K::osc), flt3 = bass.add (K::filter);
            const int lvl2 = bass.add (K::level);
            const int bout = bass.add (K::out);

            // kit
            const int e1 = kit.add (K::euclid), d1 = kit.add (K::drum);
            kit.skip();
            const int e2 = kit.add (K::euclid), d2 = kit.add (K::drum);
            kit.skip();
            const int e3 = kit.add (K::euclid), d3 = kit.add (K::drum);
            kit.skip();
            const int mx = kit.add (K::mix);
            const int kout = kit.add (K::out);

            // modulators
            const int env1 = mod.add (K::env); mod.skip();
            const int env2 = mod.add (K::env); mod.skip();
            const int lfo1 = mod.add (K::lfo); mod.skip();
            const int lfo2 = mod.add (K::lfo); mod.skip();
            const int ball = mod.add (K::ball); mod.skip();
            const int d4 = mod.add (K::drum);

            p.connect (seq1, 1, env1, 0);
            p.connect (env1, 0, lvl1, 1);

            p.connect (clk, 0, dv, 0);
            p.connect (seq2, 1, env2, 0);
            p.connect (env2, 0, lvl2, 1);
            p.connect (env2, 0, flt3, 0, true);        // opens bass filter

            p.connect (lfo1, 0, flt2, 0, true);        // the drone breathes
            p.connect (lfo2, 0, mac, 2, true);         // and changes shape

            p.connect (clk, 0, e1, 0);
            p.connect (clk, 0, e2, 0);
            p.connect (clk, 0, e3, 0);
            p.connect (d1, 0, mx, 0);
            p.connect (d2, 0, mx, 1);
            p.connect (d3, 0, mx, 2);

            // ball plays drum 4
            p.connect (ball, 2, d4, 0);
            p.connect (d4, 0, kout, 1);
            p.connect (ball, 0, dly, 0, true);         // wanders the echo

            set (p, clk, 0.5f, -1.0f, -1.0f, 5);

            setk (p, seq1, { 0.45f, 0.2f });
            steps (p, seq1, { 0.62f, 0.5f, 0.68f, 0.56f, 0.5f, 0.74f, 0.56f, 0.62f });
            lane2 (p, seq1, { 0.8f, 0.3f, 0.9f, 0.5f, 0.25f, 1.0f, 0.45f, 0.6f });
            modes (p, seq1, { 0, 0, 2, 0, 1, 0, 0, 2 });
            setk (p, qnt, { 0.5f, 0.45f }, 2);
            setk (p, tim, { 0.78f, 0.5f, 0.42f, 0.6f });
            setk (p, flt1, { 0.52f, 0.42f, 0.5f }, 0);
            setk (p, lvl1, { 0.5f, 1.0f, 0.5f, 0.0f }, 0);
            setk (p, dly, { 0.42f, 0.3f, 0.65f }, 1);

            setk (p, mac, { 0.3f, 0.45f, 0.4f, 0.6f, 0.5f, 0.0f }, 14);   // chord
            setk (p, flt2, { 0.38f, 0.3f, 0.55f }, 2);
            setk (p, rev, { 0.85f, 0.42f });

            setk (p, dv, { 0.07f }, 0);
            setk (p, seq2, { 0.3f, 0.1f });
            steps (p, seq2, { 0.4f, 0.4f, 0.46f, 0.4f, 0.34f, 0.4f, 0.5f, 0.4f });
            modes (p, seq2, { 0, 1, 0, 0, 1, 0, 1, 0 });
            setk (p, bosc, { 0.22f, 0.5f, 0.4f }, 0);
            setk (p, flt3, { 0.14f, 0.6f, 0.55f }, 1);
            setk (p, lvl2, { 0.5f, 1.0f, 0.5f, 0.0f }, 0);

            setk (p, e1, { 0.28f, 0.0f, 1.0f }, 2);
            setk (p, e2, { 0.22f, 0.35f, 0.9f }, 2);
            setk (p, e3, { 0.62f, 0.12f, 0.85f }, 2);
            setk (p, d1, { 0.24f, 0.6f, 0.42f, 0.7f }, 0);    // bass drum
            setk (p, d2, { 0.5f, 0.4f, 0.5f, 0.5f }, 1);      // snare
            setk (p, d3, { 0.72f, 0.12f, 0.6f, 0.4f }, 2);    // hat
            setk (p, d4, { 0.4f, 0.3f, 0.55f, 0.45f }, 3);    // and an fm drum
            setk (p, mx, { 0.8f, 0.55f, 0.45f });

            setk (p, env1, { 0.02f, 0.22f, 0.25f, 0.5f }, 0);
            setk (p, env2, { 0.0f, 0.16f, 0.0f, 0.5f }, 0);
            setk (p, lfo1, { 0.06f, 0.5f }, 0);
            setk (p, lfo2, { 0.03f, 0.4f }, 1);
            setk (p, ball, { 0.62f, 0.3f, 0.45f, 0.88f }, 0);

            // mix levels
            setk (p, mout, { 0.44f, 0.58f });
            setk (p, dout, { 0.38f, 0.42f });
            setk (p, bout, { 0.4f, 0.5f });
            setk (p, kout, { 0.5f, 0.5f });
            break;
        }

        case 31:   // Easy Loop
        {
            // one seq per drum
            Row kick (p, 0), snare (p, 3), hats (p, 6);

            const int clk = kick.add (K::clock);
            const int s1 = kick.add (K::seq), d1 = kick.add (K::drum);
            kick.skip();
            const int mx = kick.add (K::mix);
            const int out = kick.add (K::out);

            snare.skip (3);
            const int s2 = snare.add (K::seq), d2 = snare.add (K::drum);

            hats.skip (3);
            const int s3 = hats.add (K::seq), d3 = hats.add (K::drum);

            if (auto* b = p.find (s1)) blockPresets::apply (K::seq, 0, *b);   // four on the floor
            if (auto* b = p.find (s2)) blockPresets::apply (K::seq, 1, *b);   // backbeat
            if (auto* b = p.find (s3)) blockPresets::apply (K::seq, 2, *b);   // offbeat hat

            for (const auto& [sq, dr] : { std::pair<int, int> { s1, d1 }, { s2, d2 }, { s3, d3 } })
            {
                p.connect (clk, 0, sq, 0);
                p.connect (sq, 1, dr, 0);           // the gate plays it
                p.connect (sq, 2, dr, 1, true);     // accent -> decay
            }

            p.connect (d1, 0, mx, 0);
            p.connect (d2, 0, mx, 1);
            p.connect (d3, 0, mx, 2);
            p.connect (mx, 0, out, 0);

            set (p, clk, 0.5f, -1.0f, -1.0f, 3);                   // eighths: a bar
            setk (p, d1, { 0.22f, 0.3f, 0.4f, 0.75f }, 0);         // bass drum
            setk (p, d2, { 0.5f, 0.2f, 0.55f, 0.5f }, 1);          // snare
            setk (p, d3, { 0.74f, 0.05f, 0.6f, 0.4f }, 2);         // hat
            setk (p, mx, { 0.85f, 0.6f, 0.45f });
            setk (p, out, { 0.6f, 0.5f });
            break;
        }

        case 32:   // Simple Patch
        {
            // textbook first patch
            Row voice (p, 0), mod (p, 4);

            const int clk = voice.add (K::clock), sq = voice.add (K::seq);
            const int osc = voice.add (K::osc), amp = voice.add (K::level);
            const int flt = voice.add (K::filter);
            const int out = voice.add (K::out);

            mod.skip (4);
            const int env = mod.add (K::env);
            mod.skip();
            const int lfo = mod.add (K::lfo);

            p.connect (sq, 1, env, 0);          // gate out → envelope
            p.connect (env, 0, amp, 1);         // envelope → amplifier cv
            p.connect (clk, 0, lfo, 0);         // lfo synced
            p.connect (lfo, 0, flt, 1);         // lfo -> cutoff

            set (p, clk, 0.5f, -1.0f, -1.0f, 3);
            if (auto* b = p.find (sq)) blockPresets::apply (K::seq, 8, *b);   // arp up
            setk (p, osc, { 0.45f, 0.5f, 0.5f }, 3);                     // sine
            setk (p, amp, { 0.5f, 1.0f, 0.5f, 0.0f }, 0);
            setk (p, flt, { 0.5f, 0.35f, 0.7f }, 0);
            setk (p, env, { 0.02f, 0.3f, 0.3f });
            setk (p, lfo, { 0.12f, 0.6f }, 2);                           // saw
            setk (p, out, { 0.7f, 0.5f });
            break;
        }

        case 33:  // Glacier Chord
        {
            Row r0 (p, 0), r1 (p, 2), r2 (p, 4), r3 (p, 6);

            // root walk: every 4 bars
            const int clk = r0.add (K::clock), dv = r0.add (K::div);
            const int tur = r0.add (K::turing), qnt = r0.add (K::quant);
            const int slw = r0.add (K::slew);

            // 3 sines, open voicing
            const int o1 = r1.add (K::osc), mix = r1.add (K::mix);
            const int flt = r1.add (K::filter), rev = r1.add (K::reverb);
            const int out = r1.add (K::out);
            const int o2 = r2.add (K::osc);
            r2.skip();
            const int l1 = r2.add (K::lfo);
            r2.skip();
            const int l3 = r2.add (K::lfo);
            const int o3 = r3.add (K::osc);
            r3.skip();
            const int l2 = r3.add (K::lfo);

            p.connect (slw, 0, o1, 0);           // parallel chord
            p.connect (slw, 0, o2, 0);
            p.connect (slw, 0, o3, 0);
            p.connect (o2, 0, mix, 1);
            p.connect (o3, 0, mix, 2);
            p.connect (l1, 0, o2, 1, true);      // drift = beating
            p.connect (l2, 0, o3, 1, true);
            p.connect (l3, 0, flt, 1);           // slow breath
            drawers (p, { o2, o3 });

            set (p, clk, 0.5f, -1.0f, -1.0f, 0);   // 1/1
            set (p, dv, 0.2f, -1.0f, -1.0f, 0);    // /4
            setk (p, tur, { 0.35f, 0.25f }, 4);    // 8 steps
            setk (p, qnt, { 0.5f, 0.4f }, 1);      // major
            setk (p, slw, { 0.65f, 0.65f });
            setk (p, o1, { 0.25f, 0.5f, 0.5f }, 3);     // root -12
            setk (p, o2, { 0.6458f, 0.5f, 0.5f }, 3);   // fifth
            setk (p, o3, { 0.7917f, 0.5f, 0.5f }, 2);   // ninth, tri
            setk (p, mix, { 0.2f, 0.16f, 0.13f });
            setk (p, flt, { 0.5f, 0.12f, 0.62f }, 5);
            setk (p, l1, { 0.15f, 0.06f }, 0);
            setk (p, l2, { 0.22f, 0.07f }, 1);
            setk (p, l3, { 0.05f, 0.5f }, 0);
            setk (p, rev, { 0.95f, 0.55f });
            setk (p, out, { 0.6f });
            if (auto* b = p.find (o2)) b->depth[1] = 0.5f;
            if (auto* b = p.find (o3)) b->depth[1] = 0.5f;
            break;
        }

        case 34:  // Shifting Choir
        {
            Row r0 (p, 0), r1 (p, 2);

            const int clk = r0.add (K::clock), dv = r0.add (K::div);
            const int tur = r0.add (K::turing), qnt = r0.add (K::quant);
            const int slw = r0.add (K::slew), mac = r0.add (K::macro);
            const int flt = r0.add (K::filter), rev = r0.add (K::reverb);
            r0.add (K::out);

            r1.skip (14);
            const int cha = r1.add (K::chaos);
            r1.skip();
            const int lfo = r1.add (K::lfo);

            // chaos picks the chord type
            p.connect (cha, 1, mac, 0, true);
            p.connect (lfo, 0, mac, 1, true);    // inversions
            drawers (p, { mac });

            set (p, clk, 0.5f, -1.0f, -1.0f, 1);   // 1/2
            set (p, dv, 0.2f, -1.0f, -1.0f, 0);    // /4
            setk (p, tur, { 0.2f, 0.2f }, 3);      // 6 steps
            setk (p, qnt, { 0.5f, 0.4f }, 4);      // dorian
            setk (p, slw, { 0.75f, 0.75f });
            setk (p, mac, { 0.35f, 0.5f, 0.3f, -1.0f, -1.0f, 0.0f }, 14);   // chord
            setk (p, flt, { 0.7f, 0.1f, 0.5f }, 5);
            setk (p, cha, { 0.08f, 0.03f }, 0);
            setk (p, lfo, { 0.1f, 0.35f }, 0);
            setk (p, rev, { 0.92f, 0.5f });
            if (auto* b = p.find (mac)) { b->depth[0] = 0.45f; b->depth[1] = 0.8f; }
            break;
        }

        case 35:  // Sympathetic Fog
        {
            Row r0 (p, 0), r1 (p, 2);

            const int noi = r0.add (K::noise), amp = r0.add (K::level);
            const int res = r0.add (K::reson), sca = r0.add (K::scatter);
            const int rev = r0.add (K::reverb);
            r0.add (K::out);

            // strange-attractor tuning
            const int cha = r1.add (K::chaos), qnt = r1.add (K::quant);
            const int slw = r1.add (K::slew);
            r1.skip();
            const int lfo = r1.add (K::lfo);

            p.connect (slw, 0, res, 1);
            p.connect (res, 1, sca, 1);          // even -> right
            p.connect (lfo, 0, res, 0, true);    // structure drifts
            drawers (p, { res });

            setk (p, noi, { 0.18f });
            setk (p, amp, { 0.85f });
            setk (p, res, { 0.55f, 0.35f, 0.78f, 0.4f, 1.0f }, 1);   // sympathetic, 4 voices
            setk (p, sca, { 0.5f, 0.8f, 0.55f, 0.75f, 0.5f });
            setk (p, rev, { 0.95f, 0.45f });
            setk (p, cha, { 0.06f, 0.3f }, 1);     // rossler
            setk (p, qnt, { 0.5f, 0.4f }, 3);      // penta
            setk (p, slw, { 0.8f, 0.8f });
            setk (p, lfo, { 0.08f, 0.3f }, 1);
            break;
        }

        case 36:  // Phase Canon
        {
            Row r0 (p, 0), r1 (p, 2), r2 (p, 4);

            // A steps per bar
            const int clk = r0.add (K::clock), sqa = r0.add (K::seq);
            const int sla = r0.add (K::slew), oa = r0.add (K::osc);
            const int mix = r0.add (K::mix), flt = r0.add (K::filter);
            const int dly = r0.add (K::delay), rev = r0.add (K::reverb);
            r0.add (K::out);

            // B every 3 bars
            const int dv = r1.add (K::div), sqb = r1.add (K::seq);
            const int slb = r1.add (K::slew), ob = r1.add (K::osc);

            const int oc = r2.add (K::osc);      // held root
            r2.skip();
            const int lfo = r2.add (K::lfo);

            p.connect (clk, 0, dv, 0);
            p.connect (ob, 0, mix, 1);
            p.connect (oc, 0, mix, 2);
            p.connect (lfo, 0, flt, 1);

            set (p, clk, 0.5f, -1.0f, -1.0f, 0);   // 1/1
            set (p, dv, 0.1333f, -1.0f, -1.0f, 0); // /3
            setk (p, sqa, { 0.5f, 0.0f });
            setk (p, sqb, { 0.5f, 0.0f });
            // minor pentatonic: never clashes
            steps (p, sqa, { 0.5f, 0.625f, 0.7917f, 0.7083f, 0.9167f, 0.7917f, 0.625f, 0.7083f });
            steps (p, sqb, { 0.7917f, 0.625f, 0.9167f, 0.5f, 0.7083f, 1.0f, 0.625f, 0.7917f });
            setk (p, sla, { 0.6f, 0.6f });
            setk (p, slb, { 0.7f, 0.7f });
            setk (p, oa, { 0.5f, 0.5f, 0.5f }, 3);
            setk (p, ob, { 0.75f, 0.52f, 0.5f }, 3);   // +12, a hair sharp
            setk (p, oc, { 0.25f, 0.5f, 0.5f }, 2);    // -12
            setk (p, mix, { 0.55f, 0.45f, 0.5f });
            setk (p, flt, { 0.48f, 0.15f, 0.6f }, 5);
            setk (p, dly, { 0.45f, 0.25f, 0.4f }, 4);  // 1/4.
            setk (p, rev, { 0.93f, 0.5f });
            setk (p, lfo, { 0.07f, 0.45f }, 0);
            break;
        }

        case 37:  // Glass Rain
        {
            Row r0 (p, 0), r1 (p, 2);

            const int clk = r0.add (K::clock), euc = r0.add (K::euclid);
            r0.skip();
            const int mal = r0.add (K::mallet), amp = r0.add (K::level), dly = r0.add (K::delay);
            const int sca = r0.add (K::scatter), rev = r0.add (K::reverb);
            r0.add (K::out);

            const int tur = r1.add (K::turing), qnt = r1.add (K::quant);
            r1.skip (6);
            const int lfo = r1.add (K::lfo);

            p.connect (euc, 0, mal, 2);          // strike
            p.connect (euc, 0, tur, 0);          // new note per hit
            p.connect (qnt, 0, mal, 1);
            p.connect (mal, 1, sca, 1);
            p.connect (lfo, 0, mal, 2, true);    // geometry drifts
            drawers (p, { mal });

            set (p, clk, 0.5f, -1.0f, -1.0f, 3);   // 1/8
            setk (p, euc, { 0.25f, 0.0f, 0.85f }, 2);   // 4 of 16, lightly thinned
            setk (p, tur, { 0.18f, 0.3f }, 5);     // 12 steps
            setk (p, qnt, { 0.6f, 0.4f }, 1);      // major, up a few
            setk (p, mal, { 0.0f, 0.6f, 0.6f, 0.45f, 0.8f, 0.75f }, 0);
            setk (p, amp, { 0.9f });
            setk (p, dly, { 0.55f, 0.35f, 0.45f }, 4);
            setk (p, sca, { 0.45f, 0.9f, 0.5f, 0.8f, 0.42f });
            setk (p, rev, { 0.96f, 0.5f });
            setk (p, lfo, { 0.06f, 0.25f }, 0);
            break;
        }
    }

    // release = decay (legacy)
    for (auto& b : p.blocks())
        if (b.kind == K::env) b.knob[3] = b.knob[1];

    p.needsRebuild = true;
}

} // namespace presets

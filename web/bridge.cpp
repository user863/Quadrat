/*  Quadrat on the web: a flat C surface over the same engine the plugin runs.

    The patch itself is JUCE-free, so nothing here adapts the DSP — it only
    hands the graph to JavaScript as JSON and lets it push buffers through.
    One patch, owned by the audio worklet; the page draws from a snapshot,
    exactly as the plugin's panel does.
*/
#include "../Source/Blocks.h"
#include "../Source/Presets.h"

#include <string>
#include <cstdio>
#include <vector>

namespace
{
    qd::Patch patch;
    std::vector<float> bufL, bufR;
    std::string json;                 // reused, so the pointer handed out stays put
    int blockSize = 128;

    void esc (std::string& s, const char* t)
    {
        s += '"';
        for (const char* p = t; p != nullptr && *p != 0; ++p)
        {
            if (*p == '"' || *p == '\\') s += '\\';
            s += *p;
        }
        s += '"';
    }

    void num (std::string& s, double v)
    {
        char tmp[32];
        std::snprintf (tmp, sizeof tmp, "%.4g", v);
        s += tmp;
    }
}

extern "C"
{

void qd_init (double sampleRate, int maxBlock)
{
    blockSize = maxBlock;
    bufL.assign ((std::size_t) maxBlock, 0.0f);
    bufR.assign ((std::size_t) maxBlock, 0.0f);
    patch.prepare (sampleRate, maxBlock);
    presets::build (0, patch);
    patch.rebuild();
}

float* qd_buffer_l() { return bufL.data(); }
float* qd_buffer_r() { return bufR.data(); }

void qd_process (int n)
{
    if (n > blockSize) n = blockSize;
    patch.process (bufL.data(), bufR.data(), n);
}

void qd_transport (double bpm, double ppq, int playing)
{
    patch.setTransport (bpm, ppq, playing != 0);
}

int qd_preset_count() { return presets::count(); }

void qd_preset (int i)
{
    presets::build (i, patch);
    patch.rebuild();
}

void qd_clear() { patch.clear(); }

// random patch; returns its name
const char* qd_roll (unsigned int seed)
{
    json = presets::randomPatch (patch, seed);
    patch.rebuild();
    return json.c_str();
}

void qd_note (int midi, float velocity, int on)
{
    if (on) patch.noteOn (midi, velocity);
    else    patch.noteOff (midi);
}

void qd_panic() { patch.panic(); }

void qd_set_knob (int id, int knob, float v)
{
    if (auto* b = patch.find (id))
        if (knob >= 0 && knob < qd::kMaxKnobs)
            b->knob[(std::size_t) knob] = v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v);
}

void qd_set_depth (int id, int knob, float v)
{
    if (auto* b = patch.find (id))
        if (knob >= 0 && knob < qd::kMaxKnobs)
            b->depth[(std::size_t) knob] = v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v);
}

// lane 0 pitch, 1 cv
void qd_set_step (int id, int step, int lane, float v)
{
    if (auto* b = patch.find (id))
        if (step >= 0 && step < qd::kSteps)
            (lane == 0 ? b->steps : b->steps2)[(std::size_t) step] = v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v);
}

// 0 note, 1 rest, 2 tie
void qd_set_mode (int id, int step, int mode)
{
    if (auto* b = patch.find (id))
        if (step >= 0 && step < qd::kSteps)
            b->stepMode[(std::size_t) step] = (qd::Block::Step) (mode < 0 ? 0 : (mode > 2 ? 2 : mode));
}

void qd_block_preset (int id, int i)
{
    if (auto* b = patch.find (id)) blockPresets::apply (b->kind, i, *b);
}

void qd_set_option (int id, int option)
{
    if (auto* b = patch.find (id)) b->option = option;
}

void qd_set_drawer (int id, int open)
{
    if (auto* b = patch.find (id)) b->drawer = open != 0;
}

// structural edits rebuild, as the plugin does
int qd_add (int kind, int x, int y)
{
    if (kind < 0 || kind >= qd::numKinds()) return -1;
    const int id = patch.add ((qd::Kind) kind, x, y);
    patch.rebuild();
    return id;
}

void qd_remove (int id) { patch.remove (id); patch.rebuild(); }

int qd_move (int id, int x, int y)
{
    const bool ok = patch.move (id, x, y);
    patch.rebuild();
    return ok ? 1 : 0;
}

void qd_connect (int from, int fromPort, int to, int toPort, int toKnob)
{
    patch.connect (from, fromPort, to, toPort, toKnob != 0);
    patch.rebuild();
}

void qd_disconnect (int to, int toPort, int toKnob)
{
    patch.disconnect (to, toPort, toKnob != 0);
    patch.rebuild();
}

/*  Everything the page needs to draw one frame: where the blocks are, what
    their controls read after modulation, what is wired to what, and how
    loud each one is.                                                        */
const char* qd_state()
{
    json.clear();
    json += "{\"blocks\":[";

    bool first = true;
    for (const auto& b : patch.blocks())
    {
        if (! first) json += ',';
        first = false;

        json += "{\"id\":"; num (json, b.id);
        json += ",\"kind\":"; num (json, (int) b.kind);
        json += ",\"x\":"; num (json, b.x);
        json += ",\"y\":"; num (json, b.y);
        json += ",\"option\":"; num (json, b.option);
        json += ",\"drawer\":"; json += b.drawer ? "1" : "0";
        json += ",\"meter\":"; num (json, patch.meter (b.id));
        json += ",\"knobs\":[";

        for (int k = 0; k < qd::spec (b.kind).numKnobs; ++k)
        {
            if (k > 0) json += ',';
            num (json, patch.liveKnob (b.id, k));
        }

        json += "],\"depth\":[";

        for (int k = 0; k < qd::spec (b.kind).numKnobs; ++k)
        {
            if (k > 0) json += ',';
            num (json, b.depth[(std::size_t) k]);
        }

        json += "]";

        if (b.kind == qd::Kind::seq)
        {
            json += ",\"steps\":[";
            for (int st = 0; st < qd::kSteps; ++st) { if (st > 0) json += ','; num (json, b.steps[(std::size_t) st]); }
            json += "],\"steps2\":[";
            for (int st = 0; st < qd::kSteps; ++st) { if (st > 0) json += ','; num (json, b.steps2[(std::size_t) st]); }
            json += "],\"modes\":[";
            for (int st = 0; st < qd::kSteps; ++st) { if (st > 0) json += ','; num (json, (int) b.stepMode[(std::size_t) st]); }
            json += "],\"playhead\":"; num (json, patch.playhead (b.id));
        }

        if (b.kind == qd::Kind::clock || b.kind == qd::Kind::midi)
        {
            json += ",\"phase\":"; num (json, patch.phase (b.id));
        }

        // ball: x, y, floor, wall
        if (b.kind == qd::Kind::ball)
        {
            float x = 0, y = 0, fl = 0, wl = 0;
            patch.ballAt (b.id, x, y, fl, wl);
            json += ",\"ball\":["; num (json, x); json += ','; num (json, y); json += ',';
            num (json, fl); json += ','; num (json, wl); json += "]";
        }

        // scope, 48 points
        if (const float* sc = patch.scope (b.id))
        {
            json += ",\"scope\":[";
            for (int k = 0; k < 48; ++k)
            {
                if (k > 0) json += ',';
                num (json, sc[k * qd::Patch::kScope / 48]);
            }
            json += "]";
        }

        json += "}";
    }

    json += "],\"cables\":[";
    first = true;

    for (const auto& c : patch.cables())
    {
        if (! first) json += ',';
        first = false;
        json += "{\"from\":"; num (json, c.from);
        json += ",\"fp\":"; num (json, c.fromPort);
        json += ",\"to\":"; num (json, c.to);
        json += ",\"tp\":"; num (json, c.toPort);
        json += ",\"toKnob\":"; json += c.toKnob ? "1" : "0";
        json += "}";
    }

    json += "],\"touching\":[";
    first = true;

    for (const auto& c : patch.touching())
    {
        if (! first) json += ',';
        first = false;
        json += "{\"from\":"; num (json, c.from);
        json += ",\"to\":"; num (json, c.to);
        json += "}";
    }

    json += "]}";
    return json.c_str();
}

/*  The block catalogue, sent once: the panel's whole vocabulary.            */
const char* qd_specs()
{
    json.clear();
    json += "{\"sub\":"; num (json, qd::kSub);
    json += ",\"kinds\":[";

    for (int i = 0; i < qd::numKinds(); ++i)
    {
        const auto k = (qd::Kind) i;
        const auto& sp = qd::spec (k);

        if (i > 0) json += ',';
        json += "{\"name\":"; esc (json, sp.name);
        json += ",\"group\":"; esc (json, sp.group);
        json += ",\"cells\":"; num (json, sp.cells);
        json += ",\"rows\":"; num (json, sp.rows);
        json += ",\"about\":"; esc (json, qd::describe (k));
        json += ",\"tags\":"; esc (json, qd::searchTerms (k));
        json += ",\"bp\":[";

        for (int n = 0; n < blockPresets::count (k); ++n)
        {
            if (n > 0) json += ',';
            esc (json, blockPresets::name (k, n));
        }

        json += "]";
        json += ",\"in\":[";

        for (int p = 0; p < sp.numIn; ++p)
        {
            if (p > 0) json += ',';
            json += "{\"name\":"; esc (json, qd::portName (k, true, p));
            json += ",\"sig\":"; num (json, (int) qd::portKind (k, true, p));
            json += "}";
        }

        json += "],\"out\":[";

        for (int p = 0; p < sp.numOut; ++p)
        {
            if (p > 0) json += ',';
            json += "{\"name\":"; esc (json, qd::portName (k, false, p));
            json += ",\"sig\":"; num (json, (int) qd::portKind (k, false, p));
            json += "}";
        }

        json += "],\"knobs\":[";

        for (int n = 0; n < sp.numKnobs; ++n)
        {
            if (n > 0) json += ',';
            json += "{\"name\":"; esc (json, sp.knobs[n].name ? sp.knobs[n].name : "");
            json += ",\"type\":"; num (json, (int) sp.knobs[n].type);
            json += ",\"def\":"; num (json, sp.knobs[n].def);
            json += ",\"opts\":"; esc (json, sp.knobs[n].opts ? sp.knobs[n].opts : "");
            json += "}";
        }

        json += "],\"options\":"; esc (json, sp.options ? sp.options : "");
        json += "}";
    }

    json += "],\"presets\":[";

    for (int i = 0; i < presets::count(); ++i)
    {
        if (i > 0) json += ',';
        esc (json, presets::name (i));
    }

    json += "]}";
    return json.c_str();
}

} // extern "C"

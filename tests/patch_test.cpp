// engine tests, no host
#include "../Source/Blocks.h"
#include "../Source/Presets.h"
#include "../Source/Tour.h"
#include "../Source/MidiText.h"
#include <cstdio>
#include <cmath>
#include <cstdlib>
#include <string>
#include <functional>
#include <vector>

static int failures = 0;

static void check (bool ok, const std::string& what)
{
    std::printf ("%s  %s\n", ok ? "  ok  " : "  FAIL", what.c_str());
    if (! ok) ++failures;
}

// flow layout, like presets
struct Row
{
    // half-square row gap
    Row (qd::Patch& patch, int row) : p (patch), y (row * qd::kSub * 3 / 2) {}
    qd::Patch& p;
    int y, x = 0, prev = -1;
    // square gaps, cabled
    int add (qd::Kind k)
    {
        const int id = p.add (k, x, y);

        if (prev >= 0 && id >= 0
            && qd::spec (p.find (prev)->kind).numOut > 0 && qd::spec (k).numIn > 0)
            p.connect (prev, 0, id, 0);

        prev = id;
        x += (qd::spec (k).cells + 1) * qd::kSub;
        return id;
    }

    void gap (int cells = 1) { x += cells * qd::kSub; prev = -1; }
};

struct Out { float peak; double rms; bool finite; int crossings; };

static Out render (qd::Patch& p, double sr, double seconds)
{
    const int n = 256;
    std::vector<float> L (n), R (n);
    Out o { 0.0f, 0.0, true, 0 };
    double sum = 0.0;
    int total = 0;
    float prev = 0.0f;

    for (int done = 0; done < (int) (sr * seconds); done += n)
    {
        p.process (L.data(), R.data(), n);

        for (int i = 0; i < n; ++i)
        {
            const float v = L[i];
            if (! std::isfinite (v)) o.finite = false;
            o.peak = std::max (o.peak, std::fabs (v));
            sum += (double) v * v;
            if ((prev <= 0.0f && v > 0.0f)) ++o.crossings;
            prev = v;
            ++total;
        }
    }

    o.rms = std::sqrt (sum / std::max (1, total));
    return o;
}

int main()
{
    std::setvbuf (stdout, nullptr, _IONBF, 0);   // flush before crashes
    const double sr = 48000.0;

    // ── 1. empty = silent ──
    {
        qd::Patch p;
        p.prepare (sr, 256);
        auto r = render (p, sr, 0.2);
        check (r.peak == 0.0f && r.finite, "an empty field is silent");
    }

    // ── 2. touching wires ──
    {
        qd::Patch p;
        p.prepare (sr, 256);
        Row row (p, 0);
        row.add (qd::Kind::osc);
        row.add (qd::Kind::out);
        auto r = render (p, sr, 0.3);
        check (r.peak > 0.1f, "osc touching out makes sound with no cable (" + std::to_string (r.peak) + ")");
    }

    // ── 3. apart until cabled ──
    {
        qd::Patch p;
        p.prepare (sr, 256);
        Row row (p, 0);
        const int osc = row.add (qd::Kind::osc);
        row.gap (3);
        const int out = row.add (qd::Kind::out);
        check (render (p, sr, 0.2).peak == 0.0f, "blocks apart are silent");

        p.connect (osc, 0, out, 0);
        check (render (p, sr, 0.2).peak > 0.1f, "a cable connects them");

        p.disconnect (out, 0);
        check (render (p, sr, 0.2).peak == 0.0f, "pulling the cable stops it");
    }

    // ── 4. playable voice ──
    {
        qd::Patch p;
        p.prepare (sr, 256);
        // wide blocks, spaced
        Row top (p, 0), bottom (p, 4);
        const int key = top.add (qd::Kind::key);
        const int env = top.add (qd::Kind::env);
        const int osc = bottom.add (qd::Kind::osc);
        const int vca = bottom.add (qd::Kind::level);
        const int out = bottom.add (qd::Kind::out);

        p.connect (key, 0, osc, 0);     // pitch
        p.connect (key, 1, env, 0);     // gate
        p.connect (env, 0, vca, 1);     // envelope opens the vca
        (void) out; (void) vca;

        check (render (p, sr, 0.2).peak < 0.02f, "no note, no sound");

        p.noteOn (60, 1.0f);
        const auto on = render (p, sr, 0.4);
        check (on.peak > 0.1f, "a held note sounds (" + std::to_string (on.peak) + ")");

        p.noteOff (60);
        render (p, sr, 3.0);                       // let the release finish
        check (render (p, sr, 0.2).peak < 0.02f, "and stops when it is let go");
    }

    // ── 5. filter filters ──
    {
        auto run = [&] (float cutoff, int mode)
        {
            qd::Patch p;
            p.prepare (sr, 256);
            Row row (p, 0);
            const int nz = row.add (qd::Kind::noise);
            const int fl = row.add (qd::Kind::filter);
            row.add (qd::Kind::out);
            (void) nz;
            auto* f = p.find (fl);
            f->knob[0] = cutoff;
            f->knob[1] = 0.1f;
            f->option = mode;
            return render (p, sr, 0.5);
        };

        const auto dark = run (0.25f, 0);     // low lowpass
        const auto bright = run (0.95f, 2);   // high highpass
        check (dark.crossings < bright.crossings / 2,
               "a low lowpass is duller than a high highpass ("
               + std::to_string (dark.crossings) + " vs " + std::to_string (bright.crossings) + ")");
    }

    // ── 6. seq moves pitch ──
    {
        qd::Patch p;
        p.prepare (sr, 256);
        p.setTransport (120.0, 0.0, true);
        Row row (p, 0);
        const int clk = row.add (qd::Kind::clock);
        const int seq = row.add (qd::Kind::seq);
        const int osc = row.add (qd::Kind::osc);
        row.add (qd::Kind::out);
        p.connect (seq, 0, osc, 0);
        (void) clk;

        auto* s = p.find (seq);
        for (int i = 0; i < qd::kSteps; ++i)
            s->steps[(std::size_t) i] = 0.5f + 0.4f * (i % 2 ? 1 : -1);

        const auto r = render (p, sr, 2.0);
        check (r.finite && r.peak > 0.1f, "a clocked sequence plays");
    }

    // ── 7. all finite ──
    {
        for (int k = 0; k < qd::numKinds(); ++k)
        {
            qd::Patch p;
            p.prepare (sr, 256);
            p.setTransport (120.0, 0.0, true);
            p.noteOn (64, 1.0f);

            const auto& sp = qd::spec (static_cast<qd::Kind> (k));
            Row row (p, 0);
            const int src = row.add (qd::Kind::osc);
            const int mid = row.add (static_cast<qd::Kind> (k));
            row.add (qd::Kind::out);
            (void) src; (void) mid;

            // pitch outputs exempt
            const bool isPitch = sp.numOut > 0 && std::string (sp.out[0]) == "pitch";
            const float limit = isPitch ? 60.0f : 8.0f;

            const auto r = render (p, sr, 0.3);
            check (r.finite && r.peak < limit,
                   std::string ("block '") + sp.name + "' renders in range ("
                   + std::to_string (r.peak) + ")");
        }
    }

    // ── 8. feedback loops ──
    {
        qd::Patch p;
        p.prepare (sr, 256);
        Row row (p, 0), below (p, 4);
        const int osc = row.add (qd::Kind::osc);
        const int mx  = row.add (qd::Kind::mix);        // adjacent
        row.add (qd::Kind::out);
        const int dly = below.add (qd::Kind::delay);
        p.setTransport (120.0, 0.0, true);

        p.connect (mx, 0, dly, 0);        // mix → delay …
        p.connect (dly, 0, mx, 1);        // loop back
        p.find (dly)->knob[0] = 0.9f;     // and wound up high
        p.find (dly)->knob[1] = 1.0f;
        (void) osc;

        const auto r = render (p, sr, 2.0);
        check (r.finite && r.peak > 0.05f, "a patched feedback loop sounds");
        check (r.peak <= 1.0f, "and the output stage holds it at full scale ("
                               + std::to_string (r.peak) + ")");
    }

    // ── 11. vendored units ──
    {
        struct Case { qd::Kind kind; const char* what; bool needsTrigger; bool needsAudio; };
        const Case cases[] = {
            { qd::Kind::macro,   "macro",   true,  false },
            { qd::Kind::drum,    "drum",    true,  false },
            { qd::Kind::timbre,  "timbre",  true,  false },
            { qd::Kind::reson,   "reson",   true,  false },
            { qd::Kind::mallet,  "mallet",  true,  false },
            { qd::Kind::scatter, "scatter", false, true  },
            { qd::Kind::xmod,    "xmod",    false, true  } };

        for (const auto& c : cases)
        {
            qd::Patch p;
            p.prepare (sr, 256);
            p.setTransport (120.0, 0.0, true);

            const auto& sp = qd::spec (c.kind);
            Row row (p, 0);
            const int unit = row.add (c.kind);
            row.add (qd::Kind::out);

            if (c.needsTrigger)
            {
                const int clk = p.add (qd::Kind::clock, 0, 6 * qd::kSub);
                // trigger where wanted
                for (int i = 0; i < sp.numIn; ++i)
                {
                    const std::string nm (sp.in[i]);
                    if (nm == "trig" || nm == "gate" || nm == "strum") p.connect (clk, 0, unit, i);
                }
                if (c.kind == qd::Kind::drum) p.connect (clk, 0, unit, 0);
            }

            if (c.needsAudio)
            {
                const int osc = p.add (qd::Kind::osc, 0, 6 * qd::kSub);
                const int os2 = p.add (qd::Kind::noise, 0, 9 * qd::kSub);
                p.connect (osc, 0, unit, 0);
                p.connect (os2, 0, unit, 1);
            }

            const auto r = render (p, sr, 2.0);
            check (r.finite && r.peak > 0.01f && r.peak <= 1.0f,
                   std::string ("vendored '") + c.what + "' sounds ("
                   + std::to_string (r.peak) + ")");
        }
    }

    // ── 12. every engine ──
    {
        for (const auto kind : { qd::Kind::macro, qd::Kind::timbre })
        {
            const auto& sp = qd::spec (kind);
            const int count = qd::optionCount (sp.options);
            int silent = 0;

            for (int opt = 0; opt < count; ++opt)
            {
                qd::Patch p;
                p.prepare (sr, 256);
                p.setTransport (120.0, 0.0, true);

                Row row (p, 0);
                const int clk = row.add (qd::Kind::clock);
                const int u = row.add (kind);
                row.add (qd::Kind::out);
                p.connect (clk, 0, u, 1);                  // all triggered

                // switch or knob
                if (auto* b = p.find (u))
                {
                    if (kind == qd::Kind::timbre) b->knob[0] = count > 1 ? opt / (float) (count - 1) : 0.0f;
                    else                          b->option = opt;
                }

                if (auto* b = p.find (clk)) b->option = 3;

                if (render (p, sr, 1.5).peak <= 0.005f)
                {
                    ++silent;
                    std::printf ("    silent: %s / %s\n", sp.name,
                                 qd::optionName (sp.options, opt).c_str());
                }
            }

            check (silent == 0, std::string ("every one of ") + std::to_string (count)
                                + " '" + sp.name + "' settings makes a sound");
        }
    }

    // ── 13. knob cables ──
    {
        qd::Patch p;
        p.prepare (sr, 256);
        p.setTransport (120.0, 0.0, true);

        Row row (p, 0), below (p, 3);
        const int osc = row.add (qd::Kind::osc);
        const int flt = row.add (qd::Kind::filter);
        row.add (qd::Kind::out);
        const int lfo = below.add (qd::Kind::lfo);

        if (auto* b = p.find (flt)) { b->knob[0] = 0.08f; b->knob[1] = 0.1f; }   // shut
        if (auto* b = p.find (lfo)) { b->knob[0] = 0.55f; b->knob[1] = 1.0f; }
        (void) osc;

        const auto shut = render (p, sr, 1.0);

        p.connect (lfo, 0, flt, 0, true);         // lfo -> cutoff
        const auto swept = render (p, sr, 1.0);

        check (swept.rms > shut.rms * 1.5,
               "an lfo cabled to a knob opens the filter (rms "
               + std::to_string (shut.rms) + " → " + std::to_string (swept.rms) + ")");

        p.disconnect (flt, 0, true);
        check (render (p, sr, 1.0).rms < swept.rms * 0.8, "unplugging it shuts it again");
    }

    // ── 14. host macros ──
    {
        qd::Patch p;
        p.prepare (sr, 256);

        Row row (p, 0), below (p, 3);
        const int osc = row.add (qd::Kind::osc);
        const int amp = row.add (qd::Kind::level);
        row.add (qd::Kind::out);
        const int ct = below.add (qd::Kind::ctrl);

        if (auto* b = p.find (amp)) b->knob[0] = 0.0f;        // shut
        p.connect (ct, 0, amp, 0, true);                      // ctrl a → gain
        (void) osc;

        p.setMacro (0, 0.0f);
        check (render (p, sr, 0.5).peak < 0.01f, "with the host control down, nothing passes");

        p.setMacro (0, 1.0f);
        check (render (p, sr, 0.5).peak > 0.1f, "turning it up opens the vca");
    }

    // ── ball ──
    {
        qd::Patch p;
        p.prepare (sr, 256);

        Row row (p, 0);
        const int ball = row.add (qd::Kind::ball);
        const int lpg = row.add (qd::Kind::lpg);
        row.add (qd::Kind::out);

        const int noise = p.add (qd::Kind::noise, 0, 8 * qd::kSub);
        p.connect (noise, 0, lpg, 0);
        p.connect (ball, 2, lpg, 1);          // floor = gate

        const auto heard = render (p, sr, 6.0);
        check (heard.peak > 0.01f && heard.finite, "a ball left alone keeps making a sound");

        // bouncing after 60 s
        float low = 1.0f, high = -1.0f;
        std::vector<float> L (256), R (256);

        for (int k = 0; k < 900; ++k)
        {
            p.process (L.data(), R.data(), 256);
            float x = 0.0f, y = 0.0f, f = 0.0f, w = 0.0f;
            p.ballAt (ball, x, y, f, w);
            low = std::fmin (low, y);
            high = std::fmax (high, y);
        }

        check (high - low > 0.3f, "and is still bouncing a minute later");

        // walls bounce
        float x = 0.0f, y = 0.0f, f = 0.0f, w = 0.0f;
        p.ballAt (ball, x, y, f, w);
        check (x >= -1.001f && x <= 1.001f, "and never leaves the box");
    }

    // ── seq presets ──
    {
        int wrong = 0;

        for (int i = 0; i < blockPresets::count (qd::Kind::seq); ++i)
        {
            qd::Patch p;
            p.prepare (sr, 256);

            Row row (p, 0);
            const int clk = row.add (qd::Kind::clock);
            const int sq = row.add (qd::Kind::seq);
            const int out = row.add (qd::Kind::out);

            if (auto* b = p.find (clk)) b->option = 3;         // 8 steps/bar
            if (auto* b = p.find (out)) b->knob[0] = 0.5f;

            int expected = 0;

            if (auto* b = p.find (sq))
            {
                blockPresets::apply (qd::Kind::seq, i, *b);

                for (const auto m : b->stepMode)
                    if (m == qd::Block::Step::rise) ++expected;
            }

            // count gate pulses
            p.connect (sq, 1, out, 0);
            p.setTransport (120.0, 0.0, true);

            std::vector<float> L (256), R (256);
            int pulses = 0;
            bool high = false;

            for (int done = 0; done < (int) (sr * 2.0); done += 256)       // one bar at 120
            {
                p.process (L.data(), R.data(), 256);

                for (float v : L)
                {
                    if (! high && v > 0.1f) { ++pulses; high = true; }   // after -6 dB bus
                    if (high && v < 0.025f) high = false;
                }
            }

            if (pulses != expected)
            {
                ++wrong;
                std::printf ("    %s: %d pulses, written as %d\n",
                             blockPresets::name (qd::Kind::seq, i), pulses, expected);
            }
        }

        check (wrong == 0, std::to_string (blockPresets::count (qd::Kind::seq))
                           + " sequencer presets each play the pattern they are written as");
    }

    // ── roll ──
    {
        int silent = 0, loud = 0;

        for (unsigned int seed = 1; seed <= 40; ++seed)
        {
            qd::Patch p;
            p.prepare (sr, 256);
            presets::randomPatch (p, seed);
            p.setTransport (120.0, 0.0, true);

            const auto r = render (p, sr, 2.5);

            if (! (r.peak > 0.005f) || ! r.finite) { ++silent; std::printf ("    silent seed %u (peak %.5f)\n", seed, r.peak); }
            if (r.peak > 1.001f) ++loud;
        }

        check (silent == 0, "forty patches nobody wrote, and every one of them sounds");
        check (loud == 0, "and none of them leaves full scale");
    }

    // ── level block ──
    {
        auto peakOf = [&] (float knob, float invert, int option)
        {
            qd::Patch p;
            p.prepare (sr, 256);

            Row row (p, 0);
            const int osc = row.add (qd::Kind::osc);
            const int amp = row.add (qd::Kind::level);
            const int out = row.add (qd::Kind::out);

            if (auto* b = p.find (osc)) b->knob[0] = 0.5f;

            // dodge master clip
            if (auto* b = p.find (out)) b->knob[0] = 0.22f;

            if (auto* b = p.find (amp))
            {
                b->knob[0] = knob; b->knob[3] = invert; b->option = option;
            }

            return render (p, sr, 0.3);
        };

        const auto unity = peakOf (0.5f, 0.0f, 0);
        const auto low = peakOf (0.1f, 0.0f, 0);           // near -12 dB
        const auto boost = peakOf (1.0f, 0.0f, 0);         // +12 dB

        check (peakOf (0.0f, 0.0f, 0).peak < 1.0e-4f, "level closes fully at the bottom");
        check (low.rms < unity.rms * 0.6, "and cuts on the way down");
        check (boost.rms > unity.rms * 2.0, "at the top it boosts");
        check (std::fabs (peakOf (0.5f, 1.0f, 0).rms - unity.rms) < unity.rms * 0.02,
               "invert changes the sign, not the level");
        check (peakOf (0.1f, 0.0f, 2).rms < low.rms,
               "and the range switch reaches further down");
    }

    // ── dynamics ──
    {
        auto through = [&] (qd::Kind k, std::initializer_list<float> knobs, float outLevel)
        {
            qd::Patch p;
            p.prepare (sr, 256);

            Row row (p, 0);
            row.add (qd::Kind::osc);
            const int unit = row.add (k);
            const int out = row.add (qd::Kind::out);

            if (auto* b = p.find (out)) b->knob[0] = outLevel;

            if (auto* b = p.find (unit))
            {
                int i = 0;
                for (float v : knobs) if (i < qd::kMaxKnobs) b->knob[(std::size_t) i++] = v;
            }

            return render (p, sr, 0.4);
        };

        // squash it
        const auto open = through (qd::Kind::comp, { 1.0f, 0.0f, 0.1f, 0.3f, 0.0f }, 0.5f);
        const auto squashed = through (qd::Kind::comp, { 0.15f, 1.0f, 0.02f, 0.3f, 0.0f }, 0.5f);

        check (squashed.rms < open.rms * 0.6 && squashed.rms > 0.001,
               "the compressor pulls a loud signal down, and still passes it");
        check (squashed.finite, "and stays finite doing it");

        const auto capped = through (qd::Kind::limit, { 0.3f, 0.3f, 1.0f }, 1.0f);
        check (capped.peak <= 0.95f, "nothing gets past the limiter's ceiling");

        const auto lifted = through (qd::Kind::limit, { 0.95f, 0.3f, 0.0f }, 0.25f);
        check (lifted.peak > 0.01f && lifted.finite, "and with room to spare it is transparent");
    }

    // ── live knob readback ──
    {
        qd::Patch p;
        p.prepare (sr, 256);

        Row row (p, 0), below (p, 3);
        row.add (qd::Kind::osc);
        const int flt = row.add (qd::Kind::filter);
        row.add (qd::Kind::out);
        const int lfo = below.add (qd::Kind::lfo);

        if (auto* b = p.find (flt)) b->knob[0] = 0.2f;        // cutoff, shut
        if (auto* b = p.find (lfo)) { b->knob[0] = 0.5f; b->knob[1] = 1.0f; }
        p.connect (lfo, 0, flt, 0, true);

        render (p, sr, 0.25);
        const float shown = p.liveKnob (flt, 0);

        check (shown != 0.2f && shown >= 0.0f && shown <= 1.0f,
               "the panel can read a knob where its modulation left it");

        p.disconnect (flt, 0, true);
        render (p, sr, 0.05);
        check (std::fabs (p.liveKnob (flt, 0) - 0.2f) < 1e-4f,
               "and back at its setting once the cable is gone");
    }

    // ── depth rings ──
    {
        auto swing = [&] (float depth)
        {
            qd::Patch p;
            p.prepare (sr, 256);

            Row row (p, 0), below (p, 3);
            row.add (qd::Kind::osc);
            const int flt = row.add (qd::Kind::filter);
            row.add (qd::Kind::out);
            const int lfo = below.add (qd::Kind::lfo);

            if (auto* b = p.find (flt)) { b->knob[0] = 0.5f; b->depth[0] = depth; }
            if (auto* b = p.find (lfo)) { b->knob[0] = 0.7f; b->knob[1] = 0.4f; }
            p.connect (lfo, 0, flt, 0, true);

            float lo = 1.0f, hi = 0.0f;
            std::vector<float> L (256), R (256);

            for (int k = 0; k < 400; ++k)
            {
                p.process (L.data(), R.data(), 256);
                const float v = p.liveKnob (flt, 0);
                lo = std::fmin (lo, v);
                hi = std::fmax (hi, v);
            }

            return hi - lo;
        };

        const float full = swing (1.0f), half = swing (0.5f), none = swing (0.0f);

        check (none < 1.0e-5f, "at zero depth the cable leaves the knob alone");
        check (half > full * 0.35f && half < full * 0.65f, "at half depth it moves it half as far");
    }

    // ── edits keep state ──
    {
        qd::Patch p;
        p.prepare (sr, 256);

        Row row (p, 0);
        const int osc = row.add (qd::Kind::osc);
        row.add (qd::Kind::reverb);
        row.add (qd::Kind::out);

        if (auto* b = p.find (osc)) b->knob[0] = 0.5f;

        render (p, sr, 1.0);                       // fill the reverb
        p.remove (osc);                            // an edit, mid-tail

        const auto tail = render (p, sr, 0.05);
        check (tail.peak > 0.001f && tail.finite,
               "a reverb tail survives an edit elsewhere in the field");
    }

    // ── tour ──
    {
        using K = qd::Kind;
        constexpr int S = qd::kSub;

        qd::Patch p;
        p.prepare (sr, 256);
        bool muted = false;
        std::string title = "Simple Patch";

        tour::Runner run;
        auto ctx = [&] { return tour::Context { p, muted, title, false }; };
        auto idOf = [&p] (K k) { for (const auto& b : p.blocks()) if (b.kind == k) return b.id; return -1; };
        auto blk = [&p, &idOf] (K k) { return p.find (idOf (k)); };

        run.start (ctx());

        // do it, check twice
        const std::vector<std::function<void()>> doIt = {
            [&] { p.add (K::osc, 4 * S, 0); },
            [&] { p.add (K::out, 7 * S, 0); },
            [&] { blk (K::osc)->knob[0] += 0.2f; },
            [&] { blk (K::osc)->option = 3; },
            [&] { p.add (K::clock, 0, 4 * S); },
            [&] { p.add (K::seq, 2 * S, 4 * S); },
            [&] { p.connect (idOf (K::seq), 0, idOf (K::osc), 0); },
            [&] { blk (K::seq)->steps[2] = 0.9f; blk (K::seq)->stepMode[5] = qd::Block::Step::off; },
            [&] { p.move (idOf (K::out), 10 * S, 0); },
            [&] { p.add (K::level, 7 * S, 0); },
            [&] { p.add (K::env, 7 * S, 4 * S); },
            [&] { p.connect (idOf (K::seq), 1, idOf (K::env), 0); },
            [&] { p.connect (idOf (K::env), 0, idOf (K::level), 1); },
            [&] { p.add (K::lfo, 11 * S, 4 * S); },
            [&] { blk (K::osc)->drawer = true; },
            [&] { p.connect (idOf (K::lfo), 0, idOf (K::osc), 1, true); },
            [&] { blk (K::osc)->depth[1] = 0.4f; },
            [&] { blockPresets::apply (K::seq, 0, *blk (K::seq)); },
            [&] { muted = true; run.update (ctx()); muted = false; },
            [&] { p.notes().push_back ({ S, S, "my first patch" }); },
            [&] { title = presets::randomPatch (p, 7); },
            [&] { /* fake the undo */ } };

        check (doIt.size() == tour::steps().size(), "a player action for every tour step");

        bool numbered = true;
        for (std::size_t i = 0; i < tour::steps().size(); ++i)
            numbered = numbered && (int) tour::steps()[i].id == (int) i;
        check (numbered, "the tour's steps are listed in the order of their names");

        bool inOrder = true, noneEarly = true;
        std::vector<qd::Block> tourPatch;
        std::vector<qd::Cable> tourCables;

        for (std::size_t i = 0; i < doIt.size() && i < tour::steps().size(); ++i)
        {
            p.rebuild();
            if (run.at() != (int) i) { inOrder = false; break; }
            if (run.update (ctx()) != tour::Runner::Event::none && i != 21) noneEarly = false;

            if (i == 20) { tourPatch = p.blocks(); tourCables = p.cables(); }

            doIt[i]();

            if (i == 21) { p.blocks() = tourPatch; p.cables() = tourCables; title = "Simple Patch"; }

            p.rebuild();

            if (run.update (ctx()) != tour::Runner::Event::completed)
            {
                std::printf ("        step %d did not finish\n", (int) i + 1);
                inOrder = false;
                break;
            }

            run.settle (p);
            if (i + 1 < doIt.size()) run.next (ctx());
        }

        check (inOrder, "all 22 tour steps finish when done as asked");
        check (noneEarly, "and none finishes before it is done");

        // tour patch plays
        {
            p.rebuild();
            const auto heard = render (p, sr, 0.6);
            check (p.blocks().size() == 7 && heard.peak > 0.01f && heard.finite,
                   "the tour patch makes a sound, and without waiting a bar for it");
        }

        // regression steps back
        {
            qd::Patch q;
            std::string t = "x";
            bool m = false;
            tour::Runner r;
            auto c = [&] { return tour::Context { q, m, t, false }; };

            r.start (c());
            for (int i = 0; i < 8; ++i) { r.skip (q); r.next (c()); }   // through the melody
            check (r.at() == 8 && q.blocks().size() == 4, "skipping builds as it goes");

            for (const auto& b : q.blocks())
                if (b.kind == K::clock) { q.remove (b.id); break; }
            q.rebuild();

            check (r.update (c()) == tour::Runner::Event::steppedBack && r.at() == 4,
                   "removing the clock goes back to the step that places it");
        }

        // skip-all still plays
        {
            qd::Patch q;
            q.prepare (sr, 256);
            std::string t = "x";
            bool m = false;
            tour::Runner r;
            auto c = [&] { return tour::Context { q, m, t, false }; };

            r.start (c());
            for (int i = 0; i < 17; ++i) { r.skip (q); r.next (c()); }
            const auto heard = render (q, sr, 2.0);
            check (q.blocks().size() == 7 && q.cables().size() == 4 && heard.peak > 0.01f,
                   "skipping through builds the whole tour patch, and it plays");
        }
    }

    // ── Logic paste + midi ──
    {
        // real Logic clipboard bytes
        const std::string clip =
            " \t  \t 1 1 1 1 \t Note\t 1\t F3\t 80\t 0 1 1 160\t\n\t\t\t Rel Vel\t\t\t 64\t\t\n"
            " \t  \t 1 1 2 1 \t Note\t 1\t D3\t 80\t 0 0 1 0\t\n\t\t\t Rel Vel\t\t\t 64\t\t\n"
            " \t  \t 1 2 1 1 \t Note\t 1\t C\xe2\x99\xaf" "3\t 80\t 0 0 1 0\t\n\t\t\t Rel Vel\t\t\t 64\t\t\n"
            " \t  \t 1 2 2 1 \t Note\t 1\t A\xe2\x99\xaf" "2\t 80\t 0 0 0 1\t\n\t\t\t Rel Vel\t\t\t 64\t\t\n";

        std::vector<qd::MidiNote> phrase;
        int loop = 0;
        const bool read = midiText::fromLogic (clip, phrase, loop);

        auto is = [&] (std::size_t i, int start, int length, int pitch)
        {
            return i < phrase.size() && phrase[i].start == start && phrase[i].length == length
                   && phrase[i].pitch == pitch && phrase[i].velocity == 80;
        };

        check (read && phrase.size() == 4, "Logic's copied notes are read off the clipboard text");
        check (is (0, 0, 1360, 65) && is (1, 240, 240, 62) && is (2, 960, 240, 61) && is (3, 1200, 1, 58),
               "with their positions, lengths and pitches (C3 is 60, sharps as Logic writes them)");
        check (loop == 3840, "and a loop of the one bar they fit in");

        check (midiText::pitchFromName ("C3") == 60 && midiText::pitchFromName ("B\xe2\x99\xad" "2") == 58
                 && midiText::pitchFromName ("C-2") == 0 && midiText::pitchFromName ("G8") == 127 && midiText::pitchFromName ("G#8") == -1,
               "note names: flats, the lowest C and the highest G");

        // rebased to bar 1
        {
            const std::string moved = " \t 17 3 1 1 \t Note\t 1\t C3\t 100\t 0 0 2 0\t\n";
            std::vector<qd::MidiNote> m;
            int l = 0;
            check (midiText::fromLogic (moved, m, l) && m.size() == 1 && m[0].start == 2 * 960 && l == 3840,
                   "a phrase copied from bar 17 lands in its own first bar");
        }

        std::vector<qd::MidiNote> untouched = phrase;
        check (! midiText::fromLogic ("osascript -e 'clipboard info'", untouched, loop)
                 && untouched.size() == 4,
               "text that is not Logic notes is refused and changes nothing");

        // clock -> midi -> osc
        qd::Patch p;
        p.prepare (sr, 256);

        const int S = qd::kSub;
        const int clk = p.add (qd::Kind::clock, 0, 0);
        const int mid = p.add (qd::Kind::midi, 2 * S, 0);            // adjacent
        const int osc = p.add (qd::Kind::osc, 0, 4 * S);
        const int lvl = p.add (qd::Kind::level, 3 * S, 4 * S);
        p.add (qd::Kind::out, 6 * S, 4 * S);
        p.connect (mid, 0, osc, 0);
        p.connect (mid, 1, lvl, 1);

        if (auto* b = p.find (clk)) b->option = 5;                   // 1/16
        if (auto* b = p.find (osc)) b->option = 3;                   // sine, count crossings

        if (auto* b = p.find (mid))
        {
            b->phrase = { { 0, 960, 72, 100 } };                     // C4 quarter, then rest
            b->loopTicks = 3840;
        }

        p.rebuild();

        // first pulse at t=0
        render (p, sr, 0.05);
        const auto sounding = render (p, sr, 0.3);
        render (p, sr, 0.25);
        const auto resting = render (p, sr, 0.5);

        const double hz = sounding.crossings / 0.3;
        std::printf ("        midi: %.0f Hz while the note plays, peak %.4f in the rest\n", hz, resting.peak);
        check (sounding.peak > 0.05f && std::abs (hz - 523.25) < 25.0,
               "the midi block plays its note at the pitch written (C4, 523 Hz)");
        check (resting.peak < 0.001f, "and is silent where the phrase rests");

        // no clock, no notes
        {
            qd::Patch q;
            q.prepare (sr, 256);
            const int m2 = q.add (qd::Kind::midi, 0, 0);
            const int o2 = q.add (qd::Kind::osc, 0, 4 * S);
            const int l2 = q.add (qd::Kind::level, 3 * S, 4 * S);
            q.add (qd::Kind::out, 6 * S, 4 * S);
            q.connect (m2, 0, o2, 0);
            q.connect (m2, 1, l2, 1);
            if (auto* b = q.find (m2)) b->phrase = { { 0, 3840, 60, 100 } };
            q.rebuild();
            check (render (q, sr, 0.5).peak < 0.001f, "without a clock it waits");
        }
    }

    // ── in block ──
    {
        qd::Patch p;
        p.prepare (sr, 256);
        const int in = p.add (qd::Kind::input, 0, 0);
        p.add (qd::Kind::out, 2 * qd::kSub, 0);                     // touching
        p.rebuild();

        // blockwise sine in
        auto listen = [&] (bool feed, float gainKnob, double seconds)
        {
            if (auto* b = p.find (in)) b->knob[0] = gainKnob;

            std::vector<float> inL (256), inR (256), outL (256), outR (256);
            double ph = 0.0;
            Out o { 0.0f, 0.0, true, 0 };
            float prev = 0.0f;

            for (int done = 0; done < (int) (sr * seconds); done += 256)
            {
                for (int i = 0; i < 256; ++i)
                {
                    inL[(std::size_t) i] = inR[(std::size_t) i] = 0.5f * (float) std::sin (ph);
                    ph += 2.0 * 3.14159265358979 * 440.0 / sr;
                }

                p.setInput (feed ? inL.data() : nullptr, inR.data(), 256);
                p.process (outL.data(), outR.data(), 256);

                for (const float v : outL)
                {
                    o.peak = std::max (o.peak, std::fabs (v));
                    if (prev <= 0.0f && v > 0.0f) ++o.crossings;
                    prev = v;
                }
            }

            return o;
        };

        const auto heard = listen (true, 0.8f, 0.5);              // 0 dB
        const auto quiet = listen (true, 0.6f, 0.5);              // -12 dB default
        std::printf ("        in: %.0f Hz, peak %.3f at 0 dB, %.3f at the start setting\n",
                     heard.crossings / 0.5, heard.peak, quiet.peak);

        check (std::abs (heard.crossings / 0.5 - 440.0) < 8.0 && heard.peak > 0.05f,
               "the in block brings the incoming audio onto the field");
        check (quiet.peak < heard.peak * 0.4f && quiet.peak > heard.peak * 0.1f,
               "its gain starts twelve decibels down");
        check (listen (false, 0.8f, 0.2).peak == 0.0f, "with nothing coming in it is silent");
        check (listen (true, 0.0f, 0.2).peak == 0.0f, "and fully down it is off");
    }

    // search tags
    {
        bool all = true;
        for (int k = 0; k < qd::numKinds(); ++k)
            all = all && std::string (qd::searchTerms ((qd::Kind) k)).size() > 3;
        const std::string lpg = qd::searchTerms (qd::Kind::lpg), lvl = qd::searchTerms (qd::Kind::level);
        check (all && lpg.find ("filter") != std::string::npos && lvl.find ("vca") != std::string::npos,
               "every block has search terms (filter finds lpg, vca finds level)");
    }


    // ── reverb: no resonant blow-ups ──
    {
        float worst = 0.0f;

        for (int semi = -24; semi <= 12; ++semi)
        {
            qd::Patch p;
            p.prepare (sr, 256);
            const int osc = p.add (qd::Kind::osc, 0, 0);
            const int rev = p.add (qd::Kind::reverb, 4 * qd::kSub, 0);
            p.add (qd::Kind::out, 6 * qd::kSub, 0);    // touching the reverb
            p.connect (osc, 0, rev, 0);
            if (auto* b = p.find (osc)) { b->option = 3; b->knob[0] = 0.5f + semi / 48.0f; }
            if (auto* b = p.find (rev)) { b->knob[0] = 0.95f; b->knob[1] = 1.0f; }
            p.rebuild();
            render (p, sr, 1.0);
            worst = std::max (worst, render (p, sr, 2.0).peak);
        }

        std::printf ("        reverb: worst sustained-sine peak %.3f\n", worst);
        check (worst < 0.8f, "a long reverb never drives the output into its soft clip, at any pitch");

        // still a tail
        qd::Patch p;
        p.prepare (sr, 256);
        const int osc = p.add (qd::Kind::osc, 0, 0);
        const int rev = p.add (qd::Kind::reverb, 4 * qd::kSub, 0);
        p.add (qd::Kind::out, 6 * qd::kSub, 0);    // touching the reverb
        p.connect (osc, 0, rev, 0);
        if (auto* b = p.find (rev)) { b->knob[0] = 0.6f; b->knob[1] = 1.0f; }
        p.rebuild();
        render (p, sr, 1.0);
        p.remove (osc);
        p.rebuild();
        render (p, sr, 0.3);
        const auto tail = render (p, sr, 0.3);
        check (tail.peak > 0.005f && tail.finite, "and it still rings after the input stops");
    }


    // ── MIDI-file notes: rebase + bar length ──
    {
        std::vector<qd::MidiNote> notes = { { 8 * 3840 + 960, 480, 64, 90 }, { 8 * 3840, 960, 60, 100 } };
        std::vector<qd::MidiNote> phrase;
        int loop = 0;
        const bool ok = midiText::fromNotes (notes, 4, phrase, loop);
        check (ok && phrase.size() == 2 && phrase[0].start == 0 && phrase[0].pitch == 60
                 && phrase[1].start == 960 && loop == 3840,
               "notes from bar 9 of a file land in a one-bar loop, in order");

        std::vector<qd::MidiNote> waltz = { { 0, 960, 60, 100 }, { 2 * 960, 960, 62, 100 } };
        check (midiText::fromNotes (waltz, 3, phrase, loop) && loop == 3 * 960, "a 3/4 file loops in 3/4 bars");
        check (! midiText::fromNotes ({}, 4, phrase, loop), "an empty file is refused");
    }


    // ── bad host values: rate 0, block 0 ──
    {
        bool fine = true;
        for (auto [sr, bs] : { std::pair<double, int> { 0.0, 0 }, { 44100.0, 0 }, { 0.0, 512 }, { 384000.0, 8192 }, { 8000.0, 1 } })
            for (int i = 0; i < presets::count(); ++i)
            {
                qd::Patch p; presets::build (i, p); p.prepare (sr, bs);
                std::vector<float> l (512), r (512), in (512, 0.3f);
                const int len = std::min (256, std::max (32, bs));     // the processor's chunk cap
                for (int k = 0; k < 8; ++k) { p.setInput (in.data(), in.data(), len); p.process (l.data(), r.data(), len); }
                for (float v : l) if (! std::isfinite (v)) fine = false;
            }
        check (fine, "odd sample rates and block sizes neither hang nor blow up");
    }

    std::printf ("\n%s\n", failures == 0 ? "all checks passed"
                                         : (std::to_string (failures) + " check(s) failed").c_str());
    return failures == 0 ? 0 : 1;
}

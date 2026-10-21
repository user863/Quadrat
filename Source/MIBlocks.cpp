#include "MIBlocks.h"

#include "plaits/dsp/voice.h"
#include "rings/dsp/part.h"
#include "rings/dsp/patch.h"
#include "rings/dsp/performance_state.h"
#include "clouds/dsp/granular_processor.h"
#include "elements/dsp/part.h"
#include "elements/dsp/patch.h"
#include "warps/dsp/modulator.h"
#include "braids/macro_oscillator.h"
#include "peaks/processors.h"
#include "stmlib/utils/gate_flags.h"

#include <vector>
#include <cmath>
#include <cstring>
#include <type_traits>
#include <algorithm>

namespace qd { namespace mi {

static float clamp01 (float v) { return v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v); }

// LUTs read idx+1: clamp
static float inside01 (float v) { return v < 0.0f ? 0.0f : (v > 0.9995f ? 0.9995f : v); }

// resample to native rate
// firmware expects zeroed RAM
template <typename T>
static T* zeroed (std::unique_ptr<T>& slot)
{
    // no vtables!
    static_assert (! std::is_polymorphic<T>::value,
                   "this module has virtual functions and must not be memset");
    slot.reset (new T());
    std::memset (static_cast<void*> (slot.get()), 0, sizeof (T));
    return slot.get();
}

static float rateShift (double moduleRate, double hostRate)
{
    return 12.0f * std::log2 ((float) (moduleRate / std::max (1.0, hostRate)));
}

struct Unitary::Impl
{
    Unit unit = Unit::macro;
    double host = 48000.0;
    float shift = 0.0f;
    float k[8] { 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f };
    int option = 0;
    bool built = false;

    // ── plaits ──────────────────────────────────────────────────────────────
    std::unique_ptr<plaits::Voice> voice;
    std::vector<char> voiceMem;
    plaits::Patch patch {};
    plaits::Modulations mods {};

    // ── rings ───────────────────────────────────────────────────────────────
    std::unique_ptr<rings::Part> ringsPart;
    std::vector<uint16_t> ringsReverb;
    rings::Patch ringsPatch {};
    rings::PerformanceState ringsState {};

    // ── clouds ──────────────────────────────────────────────────────────────
    std::unique_ptr<clouds::GranularProcessor> clouds;
    std::vector<uint8_t> cloudsBig, cloudsSmall;

    // ── elements ────────────────────────────────────────────────────────────
    std::unique_ptr<elements::Part> elements;
    std::vector<uint16_t> elementsReverb;
    elements::PerformanceState elementsState {};

    // ── warps ───────────────────────────────────────────────────────────────
    std::unique_ptr<warps::Modulator> warps;

    // ── braids ──────────────────────────────────────────────────────────────
    std::unique_ptr<braids::MacroOscillator> braids;

    // ── peaks ───────────────────────────────────────────────────────────────
    std::unique_ptr<peaks::Processors> peaks;

    bool prevTrig = false;
    int lastOption = -1;
    stmlib::GateFlags prevFlags = stmlib::GATE_FLAG_LOW;
    std::vector<float> scratchA, scratchB, scratchC;
    std::vector<stmlib::GateFlags> gates;

    void build (Unit u, double hostRate);
    void run (const Ports&);
};

void Unitary::Impl::build (Unit u, double hostRate)
{
    unit = u;
    host = hostRate > 0.0 ? hostRate : 48000.0;
    scratchA.assign (4096, 0.0f);
    scratchB.assign (4096, 0.0f);
    scratchC.assign (4096, 0.0f);
    gates.assign (4096, 0);

    switch (u)
    {
        case Unit::macro:
            shift = rateShift (48000.0, host);
            // vtable: zero arena only
            voiceMem.assign (32768, 0);
            voice.reset (new plaits::Voice());
            { stmlib::BufferAllocator alloc (voiceMem.data(), voiceMem.size()); voice->Init (&alloc); }
            break;

        case Unit::reson:
            shift = rateShift (48000.0, host);
            ringsReverb.assign (32768, 0);
            zeroed (ringsPart);
            ringsPart->Init (ringsReverb.data());
            ringsPart->set_polyphony (1);
            break;

        case Unit::scatter:
            shift = rateShift (32000.0, host);
            cloudsBig.assign (118784, 0);
            cloudsSmall.assign (65536 - 128, 0);
            zeroed (clouds);
            clouds->Init (cloudsBig.data(), cloudsBig.size(), cloudsSmall.data(), cloudsSmall.size());
            break;

        case Unit::mallet:
            shift = rateShift (32000.0, host);
            elementsReverb.assign (32768, 0);
            zeroed (elements);
            elements->Init (elementsReverb.data());
            break;

        case Unit::xmod:
            shift = rateShift (96000.0, host);
            zeroed (warps);
            warps->Init ((float) host);
            break;

        case Unit::timbre:
            shift = rateShift (96000.0, host);
            zeroed (braids);
            braids->Init();
            break;

        case Unit::drum:
            shift = rateShift (48000.0, host);
            zeroed (peaks);
            peaks->Init (0);
            break;

        default:
            break;
    }

    built = true;
}

void Unitary::Impl::run (const Ports& p)
{
    const int n = p.n;
    std::fill (p.out[0], p.out[0] + n, 0.0f);
    std::fill (p.out[1], p.out[1] + n, 0.0f);

    switch (unit)
    {
        case Unit::macro:
        {
            if (voice == nullptr) return;

            patch.engine = std::min (23, std::max (0, option));
            patch.harmonics = inside01 (k[0]);
            patch.timbre = inside01 (k[1]);
            patch.morph = inside01 (k[2]);
            patch.decay = inside01 (k[3]);
            patch.lpg_colour = inside01 (k[4]);
            // fm needs patched flag
            patch.frequency_modulation_amount = clamp01 (k[5]);
            patch.timbre_modulation_amount = 0.0f;
            patch.morph_modulation_amount = 0.0f;

            mods.engine = 0.0f;
            mods.note = 0.0f;
            mods.frequency_patched = p.connected[2];
            mods.timbre_patched = false;
            mods.morph_patched = false;
            mods.trigger_patched = p.connected[1];
            mods.level_patched = false;
            mods.level = 1.0f;

            int done = 0;

            while (done < n)
            {
                const int chunk = std::min ((int) plaits::kMaxBlockSize, n - done);
                patch.note = 60.0f + p.in[0][done] + shift;
                mods.frequency = p.in[2][done];

                const bool hi = p.in[1][done] > 0.5f;
                mods.trigger = (hi && ! prevTrig) ? 1.0f : 0.0f;
                prevTrig = hi;

                plaits::Voice::Frame frames[plaits::kMaxBlockSize];
                voice->Render (patch, mods, frames, (size_t) chunk);

                for (int i = 0; i < chunk; ++i)
                {
                    p.out[0][done + i] = frames[i].out / 32768.0f;
                    p.out[1][done + i] = frames[i].aux / 32768.0f;
                }

                done += chunk;
            }
            break;
        }

        case Unit::reson:
        {
            if (ringsPart == nullptr) return;

            ringsPatch.structure = inside01 (k[0]);
            ringsPatch.brightness = inside01 (k[1]);
            ringsPatch.damping = inside01 (k[2]);
            // pos cv offsets knob
            ringsPatch.position = inside01 (k[3] + (p.connected[3] ? p.in[3][0] : 0.0f));

            ringsState.internal_exciter = ! p.connected[0];
            ringsState.internal_strum = ! p.connected[2];
            ringsState.internal_note = false;
            ringsState.tonic = 12.0f;
            ringsState.fm = 0.0f;
            ringsState.chord = 0;
            ringsPart->set_model ((rings::ResonatorModel) std::min (5, std::max (0, option)));
            {
                const int voices[3] = { 1, 2, 4 };
                ringsPart->set_polyphony (voices[std::min (2, std::max (0, (int) std::round (k[4] * 2.0f)))]);
            }

            int done = 0;

            while (done < n)
            {
                const int chunk = std::min (24, n - done);
                ringsState.note = 48.0f + p.in[1][done] + shift;

                const bool hi = p.in[2][done] > 0.5f;
                ringsState.strum = (hi && ! prevTrig);
                prevTrig = hi;

                ringsPart->Process (ringsState, ringsPatch, p.in[0] + done,
                                    p.out[0] + done, p.out[1] + done, (size_t) chunk);
                done += chunk;
            }
            break;
        }

        case Unit::scatter:
        {
            if (clouds == nullptr) return;

            auto* par = clouds->mutable_parameters();
            par->position = inside01 (k[0]);
            par->size = inside01 (k[1]);
            par->density = inside01 (k[2]);
            par->texture = inside01 (k[3]);
            par->dry_wet = inside01 (k[4]);
            par->stereo_spread = 0.5f;
            par->feedback = 0.0f;
            par->reverb = 0.0f;
            par->pitch = p.in[2][0] + shift;
            // switch or gate
            par->freeze = k[5] > 0.5f || (p.connected[3] && p.in[3][0] > 0.5f);
            par->trigger = false;
            par->gate = false;
            clouds->set_playback_mode ((clouds::PlaybackMode) std::min (3, std::max (0, option)));
            clouds->set_quality (0);
            clouds->Prepare();

            int done = 0;

            while (done < n)
            {
                const int chunk = std::min (32, n - done);
                clouds::ShortFrame in[32], out[32];

                // mono in -> both sides
                const float* inR = p.connected[1] ? p.in[1] : p.in[0];

                for (int i = 0; i < chunk; ++i)
                {
                    in[i].l = (short) (std::max (-1.0f, std::min (1.0f, p.in[0][done + i])) * 32767.0f);
                    in[i].r = (short) (std::max (-1.0f, std::min (1.0f, inR[done + i])) * 32767.0f);
                }

                clouds->Process (in, out, (size_t) chunk);

                for (int i = 0; i < chunk; ++i)
                {
                    p.out[0][done + i] = out[i].l / 32768.0f;
                    p.out[1][done + i] = out[i].r / 32768.0f;
                }

                done += chunk;
            }
            break;
        }

        case Unit::mallet:
        {
            if (elements == nullptr) return;

            // set fields, not struct
            auto* pt = elements->mutable_patch();
            pt->exciter_envelope_shape = 0.4f;
            pt->exciter_bow_level = 0.0f;
            pt->exciter_bow_timbre = 0.5f;
            pt->exciter_blow_level = clamp01 (k[0]);
            pt->exciter_blow_meta = 0.4f;
            pt->exciter_blow_timbre = 0.5f;
            pt->exciter_strike_level = 1.0f - clamp01 (k[0]) * 0.6f;
            pt->exciter_strike_meta = inside01 (k[1]);
            pt->exciter_strike_timbre = 0.5f;
            pt->exciter_signature = 0.0f;
            pt->resonator_geometry = inside01 (k[2]);
            pt->resonator_brightness = inside01 (k[3]);
            pt->resonator_damping = inside01 (k[4]);
            pt->resonator_position = 0.3f;
            pt->space = inside01 (k[5]);
            elements->set_resonator_model ((elements::ResonatorModel) std::min (2, std::max (0, option)));

            int done = 0;

            while (done < n)
            {
                const int chunk = std::min ((int) elements::kMaxBlockSize, n - done);
                elementsState.note = 48.0f + p.in[1][done] + shift;
                elementsState.modulation = 0.0f;
                elementsState.strength = 0.7f;
                elementsState.gate = p.in[2][done] > 0.5f;

                // main = R, aux = L
                elements->Process (elementsState, p.in[0] + done, p.in[0] + done,
                                   p.out[1] + done, p.out[0] + done, (size_t) chunk);
                done += chunk;
            }
            break;
        }

        case Unit::xmod:
        {
            if (warps == nullptr) return;

            auto* par = warps->mutable_parameters();
            par->channel_drive[0] = inside01 (k[2]);
            par->channel_drive[1] = inside01 (k[3]);
            par->modulation_algorithm = inside01 (k[0]);
            par->modulation_parameter = inside01 (k[1]);
            par->carrier_shape = std::min (3, std::max (0, option));
            par->note = 48.0f + shift;
            par->frequency_shift_pot = 0.5f;
            par->frequency_shift_cv = 0.0f;
            par->phase_shift = 0.0f;

            int done = 0;

            while (done < n)
            {
                const int chunk = std::min ((int) warps::kMaxBlockSize, n - done);
                warps::ShortFrame in[warps::kMaxBlockSize], out[warps::kMaxBlockSize];

                for (int i = 0; i < chunk; ++i)
                {
                    in[i].l = (short) (std::max (-1.0f, std::min (1.0f, p.in[0][done + i])) * 32767.0f);
                    in[i].r = (short) (std::max (-1.0f, std::min (1.0f, p.in[1][done + i])) * 32767.0f);
                }

                warps->Process (in, out, (size_t) chunk);

                for (int i = 0; i < chunk; ++i)
                {
                    p.out[0][done + i] = out[i].l / 32768.0f * clamp01 (k[4]) * 2.0f;
                    p.out[1][done + i] = out[i].r / 32768.0f * clamp01 (k[4]) * 2.0f;
                }

                done += chunk;
            }
            break;
        }

        case Unit::timbre:
        {
            if (braids == nullptr) return;

            braids->set_shape ((braids::MacroOscillatorShape)
                               std::min ((int) braids::MACRO_OSC_SHAPE_LAST - 1, std::max (0, option)));
            // k[0] = model
            braids->set_parameters ((int16_t) (clamp01 (k[2]) * 32767.0f),
                                    (int16_t) (clamp01 (k[3]) * 32767.0f));

            int done = 0;

            while (done < n)
            {
                const int chunk = std::min (24, n - done);
                const float semis = p.in[0][done] + (clamp01 (k[1]) * 48.0f - 24.0f) + shift;
                braids->set_pitch ((int16_t) ((60.0f + semis) * 128.0f));

                const bool hi = p.in[1][done] > 0.5f;
                if (hi && ! prevTrig) braids->Strike();
                prevTrig = hi;

                uint8_t sync[24] = { 0 };
                int16_t buf[24];
                braids->Render (sync, buf, (size_t) chunk);

                for (int i = 0; i < chunk; ++i)
                    p.out[0][done + i] = buf[i] / 32768.0f;

                done += chunk;
            }
            break;
        }

        case Unit::drum:
        {
            if (peaks == nullptr) return;

            // jump to drum functions
            static const peaks::ProcessorFunction kDrums[4] = {
                peaks::PROCESSOR_FUNCTION_BASS_DRUM,
                peaks::PROCESSOR_FUNCTION_SNARE_DRUM,
                peaks::PROCESSOR_FUNCTION_HIGH_HAT,
                peaks::PROCESSOR_FUNCTION_FM_DRUM };

            const int which = std::min (3, std::max (0, option));

            uint16_t params[4] = { (uint16_t) (clamp01 (k[0]) * 65535.0f),
                                   (uint16_t) (clamp01 (k[1]) * 65535.0f),
                                   (uint16_t) (clamp01 (k[2]) * 65535.0f),
                                   (uint16_t) (clamp01 (k[3]) * 65535.0f) };
            peaks->CopyParameters (&params[0], 4);

            if (which != lastOption)
            {
                peaks->set_control_mode (peaks::CONTROL_MODE_FULL);
                peaks->set_function (kDrums[which]);
                lastOption = which;
            }
            else
            {
                peaks->set_parameter (3, params[3]);      // forces a reconfigure
            }

            int done = 0;

            while (done < n)
            {
                const int chunk = std::min (32, n - done);

                for (int i = 0; i < chunk; ++i)
                {
                    prevFlags = stmlib::ExtractGateFlags (prevFlags, p.in[0][done + i] > 0.5f);
                    gates[(std::size_t) i] = prevFlags;
                }

                int16_t buf[32];
                peaks->Process (gates.data(), buf, (size_t) chunk);

                for (int i = 0; i < chunk; ++i)
                    p.out[0][done + i] = buf[i] / 32768.0f;

                done += chunk;
            }
            break;
        }

        default:
            break;
    }
}

// ── public wrapper ──
Unitary::Unitary() : impl (new Impl) {}
Unitary::~Unitary() = default;
Unitary::Unitary (Unitary&&) noexcept = default;
Unitary& Unitary::operator= (Unitary&&) noexcept = default;

void Unitary::init (Unit u, double hostRate) { impl->build (u, hostRate); }
void Unitary::setKnobs (const float* values, int count, int option)
{
    for (int i = 0; i < 8; ++i)
        impl->k[i] = i < count ? values[i] : 0.5f;

    impl->option = option;
}
void Unitary::process (const Ports& p) { if (impl->built) impl->run (p); }
bool Unitary::ready() const { return impl->built; }

static const char* kOptions[(int) Unit::count][16] =
{
    { "va", "waveshape", "fm", "grain", "additive", "wavetable", "chord", "speech",
      "swarm", "noise", "particle", "string", "modal", "bass", "snare", "hat" },   // macro
    { "modal", "sympathetic", "string", "fm", "quantized", "reverb" },             // reson
    { "granular", "stretch", "delay", "spectral" },                                // scatter
    { "modal", "string", "drum" },                                                 // mallet
    { "xfade", "fold", "analog ring", "digital ring" },                            // xmod
    { "" }, { "" }, { "" },                                                        // chance, contour, dyn
    { "csaw", "morph", "saw square", "sub", "dual sync", "sine tri", "buzz",
      "digital" },                                                                 // timbre
    { "bass", "snare", "hat", "fm drum" }                                          // drum
};

static const int kOptionCounts[(int) Unit::count] = { 16, 6, 4, 3, 4, 0, 0, 0, 8, 4 };

int optionsFor (Unit u) { return kOptionCounts[(int) u]; }

const char* optionName (Unit u, int i)
{
    const int c = kOptionCounts[(int) u];
    return c > 0 ? kOptions[(int) u][std::min (std::max (0, i), c - 1)] : "";
}

}} // namespace qd::mi

#include "PluginProcessor.h"
#include <thread>
#include <cstring>
#include "PluginEditor.h"

juce::AudioProcessorValueTreeState::ParameterLayout QuadratProcessor::makeLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout l;
    l.add (std::make_unique<juce::AudioParameterFloat> (
               juce::ParameterID { ids::level, 1 }, "Level",
               juce::NormalisableRange<float> (-60.0f, 6.0f, 0.1f, 2.0f), 0.0f));

    // 16 automatable macros
    for (int i = 1; i <= qd::kMacros; ++i)
        l.add (std::make_unique<juce::AudioParameterFloat> (
                   juce::ParameterID { juce::String (ids::ctrl) + juce::String (i), 1 },
                   "Ctrl " + juce::String (i),
                   juce::NormalisableRange<float> (0.0f, 1.0f), 0.0f));

    return l;
}

QuadratProcessor::QuadratProcessor()
    // sidechain, default off
    : AudioProcessor (BusesProperties().withInput ("Side Chain", juce::AudioChannelSet::stereo(), false)
                                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "state", makeLayout())
{
    loadPreset (0);
}

void QuadratProcessor::setCurrentProgram (int index)
{
    loadPreset (juce::jlimit (0, presets::count() - 1, index));
}

juce::String QuadratProcessor::patchTitle() const
{
    const juce::ScopedLock sl (patchLock);
    return title.isNotEmpty() ? title : juce::String (presets::name (current));
}

void QuadratProcessor::setTitle (const juce::String& t)
{
    const juce::ScopedLock sl (patchLock);
    title = t;
}

void QuadratProcessor::loadPreset (int index)
{
    const juce::ScopedLock sl (patchLock);
    current = index;
    title.clear();                        // preset names fixed
    presets::build (index, patch);
    patch.prepare (hostRate, hostBlock);
}

void QuadratProcessor::setMuted (bool shouldMute)
{
    muted.store (shouldMute);
}

void QuadratProcessor::prepareToPlay (double sampleRate, int blockSize)
{
    hostRate = sampleRate;
    hostBlock = blockSize;
    sidechain.setSize (2, juce::jmax (32, blockSize));
    const juce::ScopedLock sl (patchLock);
    patch.prepare (sampleRate, blockSize);
}

bool QuadratProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto in = layouts.getMainInputChannelSet();

    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo()
        && (in.isDisabled() || in == juce::AudioChannelSet::mono() || in == juce::AudioChannelSet::stereo());
}

void QuadratProcessor::handleMidi (const juce::MidiMessage& m)
{
    if (m.isNoteOn())                       patch.noteOn (m.getNoteNumber(), m.getFloatVelocity());
    else if (m.isNoteOff())                 patch.noteOff (m.getNoteNumber());     // vel-0 note-ons too
    else if (m.isPitchWheel())              patch.pitchBend ((m.getPitchWheelValue() - 8192) / 8192.0f * 2.0f);
    else if (m.isSustainPedalOn())          patch.sustain (true);
    else if (m.isSustainPedalOff())         patch.sustain (false);
    else if (m.isResetAllControllers())     { patch.pitchBend (0.0f); patch.sustain (false); }
    else if (m.isAllNotesOff() || m.isAllSoundOff()) patch.allNotesOff();
}

void QuadratProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;

    const int n = buffer.getNumSamples();

    // grab input before clear
    const int inputs = getTotalNumInputChannels();
    const int inputLen = inputs > 0 ? juce::jmin (n, sidechain.getNumSamples()) : 0;

    if (inputLen > 0)
    {
        sidechain.copyFrom (0, 0, buffer, 0, 0, inputLen);
        sidechain.copyFrom (1, 0, buffer, inputs > 1 ? 1 : 0, 0, inputLen);
    }

    buffer.clear();

    double bpm = 120.0, ppq = 0.0;
    bool playing = false;

    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
        {
            if (auto b = pos->getBpm()) bpm = *b;
            if (auto p = pos->getPpqPosition()) ppq = *p;
            playing = pos->getIsPlaying();
        }

    // UI holds it for µs: spin a little, don't drop
    struct Hold
    {
        juce::CriticalSection& cs;
        bool ok = false;
        explicit Hold (juce::CriticalSection& c) : cs (c)
        {
            for (int spin = 0; spin < 400 && ! (ok = cs.tryEnter()); ++spin)
                std::this_thread::yield();
        }
        ~Hold() { if (ok) cs.exit(); }
    } sl (patchLock);

    if (! sl.ok)
    {
        dropped.fetch_add (1);      // long edit; should be rare
        return;
    }

    if (buffer.getNumChannels() < 2)
        return;

    // stop: reset, then idle
    const bool mute = muted.load();

    if (mute)
    {
        for (const auto meta : midi) handleMidi (meta.getMessage());
        if (! wasMuted) { patch.panic(); wasMuted = true; }
        level.store (0.0f);
        return;
    }

    wasMuted = false;

    for (int i = 0; i < qd::kMacros; ++i)
        patch.setMacro (i, apvts.getRawParameterValue (
                               juce::String (ids::ctrl) + juce::String (i + 1))->load());

    // tap tempo when stopped
    if (! playing && tapped > 0.0) bpm = tapped;

    patch.setTransport (bpm, ppq, playing);

    // sample-accurate MIDI: split at events, never past the prepared size
    auto* outL = buffer.getWritePointer (0);
    auto* outR = buffer.getWritePointer (1);
    const int chunkMax = juce::jmax (32, hostBlock);
    auto event = midi.begin();
    int pos = 0;

    while (pos < n)
    {
        while (event != midi.end() && (*event).samplePosition <= pos)
        {
            handleMidi ((*event).getMessage());
            ++event;
        }

        const int nextEvent = event != midi.end() ? juce::jlimit (pos + 1, n, (*event).samplePosition) : n;
        const int len = juce::jmin (nextEvent - pos, chunkMax);
        const int inLen = juce::jlimit (0, len, inputLen - pos);

        patch.setInput (inLen > 0 ? sidechain.getReadPointer (0) + pos : nullptr,
                        sidechain.getReadPointer (1) + pos, inLen);
        patch.process (outL + pos, outR + pos, len);
        pos += len;
    }

    const float gain = juce::Decibels::decibelsToGain (apvts.getRawParameterValue (ids::level)->load(), -60.0f);
    buffer.applyGain (gain);

    level.store (juce::jlimit (0.0f, 1.0f, buffer.getMagnitude (0, n)));
}

// ── state ────────────────────────────────────────────────────────────────────
void QuadratProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    auto state = apvts.copyState();
    state.setProperty ("program", current, nullptr);

    auto field = state.getOrCreateChildWithName ("patch", nullptr);
    field.removeAllChildren (nullptr);
    field.setProperty ("sub", qd::kSub, nullptr);      // grid units
    field.setProperty ("kinds", qd::numKinds(), nullptr);   // and which block numbering
    field.setProperty ("envRelease", 1, nullptr);             // env has release
    field.setProperty ("title", title, nullptr);

    const juce::ScopedLock sl (patchLock);

    for (const auto& b : patch.blocks())
    {
        juce::ValueTree v ("block");
        v.setProperty ("kind", (int) b.kind, nullptr);
        v.setProperty ("id", b.id, nullptr);
        v.setProperty ("x", b.x, nullptr);
        v.setProperty ("y", b.y, nullptr);
        v.setProperty ("option", b.option, nullptr);
        v.setProperty ("drawer", b.drawer, nullptr);

        for (int k = 0; k < qd::kMaxKnobs; ++k)
            v.setProperty ("k" + juce::String (k), b.knob[(std::size_t) k], nullptr);

        // only non-default depths
        for (int k = 0; k < qd::kMaxKnobs; ++k)
            if (b.depth[(std::size_t) k] < 0.999f)
                v.setProperty ("d" + juce::String (k), b.depth[(std::size_t) k], nullptr);

        if (b.kind == qd::Kind::seq)
            for (int s = 0; s < qd::kSteps; ++s)
            {
                v.setProperty ("s" + juce::String (s), b.steps[(std::size_t) s], nullptr);
                v.setProperty ("c" + juce::String (s), b.steps2[(std::size_t) s], nullptr);
                v.setProperty ("m" + juce::String (s), (int) b.stepMode[(std::size_t) s], nullptr);
            }

        // start:len:pitch:vel
        if (b.kind == qd::Kind::midi)
        {
            juce::StringArray notes;
            for (const auto& m : b.phrase)
                notes.add (juce::String (m.start) + ":" + juce::String (m.length) + ":"
                           + juce::String (m.pitch) + ":" + juce::String (m.velocity));

            v.setProperty ("loop", b.loopTicks, nullptr);
            v.setProperty ("phrase", notes.joinIntoString (" "), nullptr);
        }

        field.appendChild (v, nullptr);
    }

    for (const auto& t : patch.notes())
    {
        juce::ValueTree v ("note");
        v.setProperty ("x", t.x, nullptr);
        v.setProperty ("y", t.y, nullptr);
        v.setProperty ("text", juce::String::fromUTF8 (t.text.c_str()), nullptr);
        field.appendChild (v, nullptr);
    }

    for (const auto& c : patch.cables())
    {
        juce::ValueTree v ("cable");
        v.setProperty ("from", c.from, nullptr);
        v.setProperty ("fromPort", c.fromPort, nullptr);
        v.setProperty ("to", c.to, nullptr);
        v.setProperty ("toPort", c.toPort, nullptr);
        v.setProperty ("toKnob", c.toKnob, nullptr);
        field.appendChild (v, nullptr);
    }

    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, dest);
}

juce::String QuadratProcessor::patchCode()
{
    juce::MemoryBlock raw;
    getStateInformation (raw);

    juce::MemoryOutputStream packed;
    {
        juce::GZIPCompressorOutputStream zip (packed, 9);
        zip.write (raw.getData(), raw.getSize());
    }

    return "QDR1" + juce::Base64::toBase64 (packed.getData(), packed.getDataSize());
}

bool QuadratProcessor::loadPatchCode (const juce::String& code)
{
    const auto trimmed = code.trim();
    if (! trimmed.startsWith ("QDR1")) return false;

    juce::MemoryOutputStream packed;
    if (! juce::Base64::convertFromBase64 (packed, trimmed.substring (4))) return false;

    juce::MemoryInputStream in (packed.getData(), packed.getDataSize(), false);
    juce::GZIPDecompressorInputStream unzip (in);

    // binary-safe read
    juce::MemoryOutputStream raw;
    raw.writeFromInputStream (unzip, -1);

    if (raw.getDataSize() == 0) return false;

    setStateInformation (raw.getData(), (int) raw.getDataSize());
    return true;
}

void QuadratProcessor::setStateInformation (const void* data, int size)
{
    auto xml = getXmlFromBinary (data, size);
    if (xml == nullptr) return;

    auto state = juce::ValueTree::fromXml (*xml);
    if (! state.isValid()) return;

    apvts.replaceState (state);
    current = state.getProperty ("program", 0);

    auto field = state.getChildWithName ("patch");
    if (! field.isValid()) return;

    const juce::ScopedLock sl (patchLock);
    patch.clear();
    report.clear();
    title = field.getProperty ("title", juce::String()).toString();

    std::map<int, int> idMap;      // old id -> new id
    juce::StringArray refused;

    for (int i = 0; i < field.getNumChildren(); ++i)
    {
        auto v = field.getChild (i);
        if (! v.hasType ("block")) continue;

        // legacy kind ids
        const int saved = v.getProperty ("kind", 0);
        const bool old = ! field.hasProperty ("kinds");
        const auto kind = old ? qd::kindFromLegacy (saved) : (qd::Kind) saved;

        if (kind == qd::Kind::count)
        {
            refused.add ("eq");
            continue;
        }

        // pre-subgrid: scale up
        const int scale = qd::kSub / juce::jmax (1, (int) field.getProperty ("sub", 1));
        const int id = patch.add (kind, (int) v.getProperty ("x", 0) * scale,
                                        (int) v.getProperty ("y", 0) * scale);

        if (id < 0)
        {
            // doesn't fit: report
            refused.add (qd::spec (kind).name);
            continue;
        }

        idMap[(int) v.getProperty ("id", -1)] = id;

        if (auto* b = patch.find (id))
        {
            b->option = v.getProperty ("option", 0);
            b->drawer = (bool) v.getProperty ("drawer", false);

            for (int k = 0; k < qd::kMaxKnobs; ++k)
                b->knob[(std::size_t) k] = v.getProperty ("k" + juce::String (k), 0.5f);

            for (int k = 0; k < qd::kMaxKnobs; ++k)
                b->depth[(std::size_t) k] = v.getProperty ("d" + juce::String (k), 1.0f);

            // migrate env, pre-release
            if (kind == qd::Kind::env && ! field.hasProperty ("envRelease"))
            {
                b->knob[4] = b->knob[3];
                b->knob[3] = b->knob[1];
            }

            if (kind == qd::Kind::seq)
                for (int s = 0; s < qd::kSteps; ++s)
                {
                    b->steps[(std::size_t) s] = v.getProperty ("s" + juce::String (s), 0.5f);
                    b->steps2[(std::size_t) s] = v.getProperty ("c" + juce::String (s), 0.5f);

                    // old on/off -> rest
                    const int mode = v.hasProperty ("m" + juce::String (s))
                                       ? (int) v.getProperty ("m" + juce::String (s), 0)
                                       : (v.getProperty ("o" + juce::String (s), true) ? 0 : 1);

                    b->stepMode[(std::size_t) s] = (qd::Block::Step) juce::jlimit (0, 2, mode);
                }

            if (kind == qd::Kind::midi)
            {
                b->loopTicks = juce::jlimit (qd::kPpq, qd::kPpq * 4 * 64, (int) v.getProperty ("loop", qd::kPpq * 4));
                b->phrase.clear();

                for (const auto& word : juce::StringArray::fromTokens (v.getProperty ("phrase", "").toString(), " ", ""))
                {
                    const auto f = juce::StringArray::fromTokens (word, ":", "");
                    if (f.size() != 4) continue;

                    qd::MidiNote m;
                    m.start = juce::jmax (0, f[0].getIntValue());
                    m.length = juce::jmax (1, f[1].getIntValue());
                    m.pitch = juce::jlimit (0, 127, f[2].getIntValue());
                    m.velocity = juce::jlimit (1, 127, f[3].getIntValue());
                    b->phrase.push_back (m);
                }
            }
        }
    }

    for (int i = 0; i < field.getNumChildren(); ++i)
    {
        auto v = field.getChild (i);
        if (! v.hasType ("cable")) continue;

        const auto f = idMap.find ((int) v.getProperty ("from", -1));
        const auto t = idMap.find ((int) v.getProperty ("to", -1));

        if (f != idMap.end() && t != idMap.end())
            patch.connect (f->second, v.getProperty ("fromPort", 0),
                           t->second, v.getProperty ("toPort", 0),
                           (bool) v.getProperty ("toKnob", false));
    }

    for (int i = 0; i < field.getNumChildren(); ++i)
    {
        auto v = field.getChild (i);
        if (! v.hasType ("note")) continue;

        qd::Note t;
        t.x = v.getProperty ("x", 0);
        t.y = v.getProperty ("y", 0);
        t.text = v.getProperty ("text", juce::String()).toString().toStdString();
        patch.notes().push_back (t);
    }

    if (! refused.isEmpty())
        report = juce::String (refused.size()) + " block"
                   + (refused.size() == 1 ? "" : "s") + " did not fit: "
                   + refused.joinIntoString (", ");

    patch.prepare (hostRate, hostBlock);
}

juce::AudioProcessorEditor* QuadratProcessor::createEditor() { return new QuadratEditor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new QuadratProcessor(); }

// one Patch, host glue
#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "Blocks.h"
#include <atomic>
#include "Presets.h"

namespace ids
{
    inline constexpr auto level = "level";
    inline constexpr auto ctrl  = "ctrl";     // ctrl1 … ctrl16
    inline constexpr auto link  = "knob";     // knob1 … knob32, borrowed by the ui
}

class QuadratProcessor : public juce::AudioProcessor,
                         private juce::AsyncUpdater
{
public:
    QuadratProcessor();
    ~QuadratProcessor() override = default;

    void prepareToPlay (double, int) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    void handleMidi (const juce::MidiMessage&);

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override                  { return true; }
    const juce::String getName() const override      { return "Quadrat"; }
    bool acceptsMidi() const override                { return true; }
    bool producesMidi() const override               { return false; }
    bool isMidiEffect() const override               { return false; }
    double getTailLengthSeconds() const override     { return 4.0; }

    // factory presets via host
    int getNumPrograms() override                    { return presets::count(); }
    int getCurrentProgram() override                 { return current; }
    void setCurrentProgram (int) override;
    const juce::String getProgramName (int i) override { return presets::name (i); }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    juce::AudioProcessorValueTreeState apvts;

    // guards patch vs audio
    qd::Patch patch;
    juce::CriticalSection patchLock;
    std::atomic<double> tapped { 0.0 };

    // tap tempo, host wins
    // patch <-> text code
    juce::String patchCode();
    bool loadPatchCode (const juce::String&);

    void setTapTempo (double bpm) { tapped = bpm > 20.0 && bpm < 400.0 ? bpm : 0.0; }
    double tapTempo() const { return tapped; }

    void loadPreset (int index);

    // rolled name or preset
    juce::String patchTitle() const;
    void setTitle (const juce::String&);

    // restore report
    juce::String loadReport() const { return report; }
    void setMuted (bool);
    bool isMuted() const { return muted.load(); }
    float outputLevel() const { return level.load(); }

    // dropped buffers
    int droppedBlocks() const { return dropped.load(); }

    // ui knobs as host params ("last tweaked")
    static constexpr int kLinks = 32;
    int linkKnob (int blockId, int knob);            // claims a slot; kMaxKnobs + k = depth ring
    void linkSend (int slot, float value);           // ui moved it
    juce::RangedAudioParameter* linkParam (int slot) const;
    juce::String linkName (int slot) const;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout makeLayout (const QuadratProcessor*);

    struct Link { int block = -1, knob = -1; juce::uint32 used = 0; };
    std::array<Link, kLinks> links;
    std::array<std::atomic<float>, kLinks> sent {};
    std::array<juce::String, kLinks> linkNames;
    juce::uint32 linkClock = 0;
    void applyLinks();                    // audio, locked
    void clearLinks();
    void handleAsyncUpdate() override;    // names to host

    std::atomic<float> level { 0.0f };
    std::atomic<int> dropped { 0 };
    std::atomic<bool> muted { false };
    bool wasMuted = false;
    int current = 0;
    juce::String title;                   // empty = preset name
    int hostBlock = 512;
    juce::AudioBuffer<float> sidechain;      // sidechain copy
    juce::String report;
    double hostRate = 44100.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (QuadratProcessor)
};

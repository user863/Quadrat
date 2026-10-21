// Logic clipboard -> phrase
#pragma once
#include "Blocks.h"
#include <string>
#include <vector>

namespace midiText
{
    // false = untouched
    bool fromLogic (const std::string& text, std::vector<qd::MidiNote>& phrase, int& loopTicks);

    // ticks at kPpq: rebase, sort, loop to whole bars
    bool fromNotes (std::vector<qd::MidiNote> notes, int beatsPerBar,
                    std::vector<qd::MidiNote>& phrase, int& loopTicks);

    // "F♯2" -> 54
    int pitchFromName (const std::string& name);

    constexpr std::size_t kMaxNotes = 512;
    constexpr int kMaxBars = 64;
}

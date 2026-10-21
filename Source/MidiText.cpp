#include "MidiText.h"
#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <sstream>

namespace midiText
{

static std::string trim (const std::string& s)
{
    std::size_t a = 0, b = s.size();
    while (a < b && std::isspace ((unsigned char) s[a])) ++a;
    while (b > a && std::isspace ((unsigned char) s[b - 1])) --b;
    return s.substr (a, b - a);
}

static std::vector<std::string> split (const std::string& s, char by)
{
    std::vector<std::string> parts;
    std::string part;
    std::istringstream in (s);
    while (std::getline (in, part, by)) parts.push_back (part);
    return parts;
}

// four ints or bust
static bool fourNumbers (const std::string& s, int (&v)[4])
{
    std::istringstream in (s);
    for (auto& x : v)
        if (! (in >> x)) return false;

    std::string rest;
    return ! (in >> rest);
}

int pitchFromName (const std::string& name)
{
    static const int semis[] = { 9, 11, 0, 2, 4, 5, 7 };       // A B C D E F G

    if (name.empty()) return -1;

    const char letter = (char) std::toupper ((unsigned char) name[0]);
    if (letter < 'A' || letter > 'G') return -1;

    int pc = semis[letter - 'A'];
    std::size_t i = 1;

    for (;;)
    {
        if (name.compare (i, 3, "\xe2\x99\xaf") == 0) { ++pc; i += 3; }        // ♯
        else if (name.compare (i, 3, "\xe2\x99\xad") == 0) { --pc; i += 3; }   // ♭
        else if (i < name.size() && name[i] == '#') { ++pc; ++i; }
        else if (i < name.size() && name[i] == 'b') { --pc; ++i; }
        else break;
    }

    const std::string octave = name.substr (i);
    if (octave.empty()) return -1;

    char* end = nullptr;
    const long oct = std::strtol (octave.c_str(), &end, 10);
    if (end == octave.c_str() || *end != 0) return -1;

    const int note = (int) (oct + 2) * 12 + pc;                 // C3 = 60
    return note >= 0 && note <= 127 ? note : -1;
}

namespace
{
    struct Raw { int pos[4]; int len[4]; int pitch; int velocity; };
}

bool fromLogic (const std::string& text, std::vector<qd::MidiNote>& phrase, int& loopTicks)
{
    std::vector<Raw> raw;

    for (auto line : split (text, '\n'))
    {
        if (! line.empty() && line.back() == '\r') line.pop_back();

        std::vector<std::string> cells;
        for (const auto& c : split (line, '\t'))
            if (! trim (c).empty()) cells.push_back (trim (c));

        const auto at = std::find (cells.begin(), cells.end(), "Note");
        if (at == cells.end() || at == cells.begin() || cells.end() - at < 5) continue;

        Raw r {};
        if (! fourNumbers (*(at - 1), r.pos)) continue;          // position precedes it
        r.pitch = pitchFromName (*(at + 2));                     // after it: channel, pitch,
        r.velocity = std::atoi ((at + 3)->c_str());              // velocity, length
        if (! fourNumbers (*(at + 4), r.len) || r.pitch < 0) continue;

        raw.push_back (r);
        if (raw.size() >= kMaxNotes) break;
    }

    if (raw.empty()) return false;

    // guess divisions/beat
    int maxDiv = 1, maxTick = 0, maxBeat = 4;

    for (const auto& r : raw)
    {
        maxDiv = std::max ({ maxDiv, r.pos[2], r.len[2] + 1 });
        maxTick = std::max ({ maxTick, r.pos[3] - 1, r.len[3] });
        maxBeat = std::max ({ maxBeat, r.pos[1], r.len[1] + 1 });
    }

    int perBeat = 4;

    for (const int d : { 4, 1, 2, 3, 6, 8, 12, 16, 24, 32, 48, 64, 96, 192, 240, 480, 960 })
        if (d >= maxDiv && qd::kPpq / d > maxTick) { perBeat = d; break; }

    const int perDiv = qd::kPpq / perBeat;
    const int perBar = qd::kPpq * maxBeat;

    std::vector<qd::MidiNote> notes;

    for (const auto& r : raw)
    {
        qd::MidiNote n;
        n.start = ((r.pos[0] - 1) * maxBeat + (r.pos[1] - 1)) * qd::kPpq
                  + (r.pos[2] - 1) * perDiv + (r.pos[3] - 1);
        n.length = std::max (1, (r.len[0] * maxBeat + r.len[1]) * qd::kPpq + r.len[2] * perDiv + r.len[3]);
        n.pitch = r.pitch;
        n.velocity = std::min (127, std::max (1, r.velocity));
        notes.push_back (n);
    }

    return fromNotes (notes, maxBeat, phrase, loopTicks);
}

bool fromNotes (std::vector<qd::MidiNote> notes, int beatsPerBar, std::vector<qd::MidiNote>& phrase, int& loopTicks)
{
    if (notes.empty()) return false;
    if (notes.size() > kMaxNotes) notes.resize (kMaxNotes);

    const int perBar = qd::kPpq * std::max (1, beatsPerBar);
    int first = 1 << 30;
    for (const auto& n : notes) first = std::min (first, n.start);

    // rebase to first bar
    const int origin = (first / perBar) * perBar;
    int last = 0;

    for (auto& n : notes)
    {
        n.start -= origin;
        last = std::max (last, n.start + n.length);
    }

    std::sort (notes.begin(), notes.end(), [] (const qd::MidiNote& a, const qd::MidiNote& b)
               { return a.start < b.start || (a.start == b.start && a.pitch < b.pitch); });   // chord: top note wins

    const int bars = std::min (kMaxBars, std::max (1, (last + perBar - 1) / perBar));
    const int loop = bars * perBar;

    notes.erase (std::remove_if (notes.begin(), notes.end(),
                                 [loop] (const qd::MidiNote& n) { return n.start >= loop; }),
                 notes.end());

    for (auto& n : notes) n.length = std::min (n.length, loop - n.start);

    phrase = notes;
    loopTicks = loop;
    return true;
}

} // namespace midiText

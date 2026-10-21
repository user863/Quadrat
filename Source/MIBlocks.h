// Mutable DSP, opaque
#pragma once
#include <memory>

namespace qd { namespace mi {

enum class Unit
{
    macro = 0,   // Plaits
    reson,       // Rings
    scatter,     // granular texture processor
    mallet,      // Elements
    xmod,        // Warps
    chance,      // Marbles-ish
    contour,     // Peaks / Tides
    dyn,         // compressor and dynamics
    timbre,      // Braids
    drum,        // analogue drum voices
    count
};

// must equal qd::kMaxIn
struct Ports
{
    const float* in[4];    // never null
    bool connected[4];
    float* out[2];
    int n;
};

class Unitary
{
public:
    Unitary();
    ~Unitary();
    Unitary (Unitary&&) noexcept;
    Unitary& operator= (Unitary&&) noexcept;

    void init (Unit, double hostRate);
    void setKnobs (const float* values, int count, int option);
    void process (const Ports&);
    bool ready() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl;
};

int optionsFor (Unit);          // selector size
const char* optionName (Unit, int index);

}} // namespace qd::mi

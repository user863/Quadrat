#pragma once
#include "Blocks.h"
#include <string>

// per-block presets
namespace blockPresets
{
    int count (qd::Kind);
    const char* name (qd::Kind, int index);

    // menu heading
    const char* group (qd::Kind, int index);
    void apply (qd::Kind, int index, qd::Block&);
}

namespace presets
{
    int count();
    const char* name (int index);

    // factory patch -> patch
    void build (int index, qd::Patch&);

    // random patch, named
    std::string randomPatch (qd::Patch&, unsigned int seed);   // returns the patch's name
}

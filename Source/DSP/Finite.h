#pragma once
#include <cmath>

namespace dspguard
{
// Replaces every NaN/Inf with 0; returns true if any was found. For the input and output of stages that
// hold recursive (IIR) state, where a single non-finite sample would otherwise poison all later audio.
inline bool zeroNonFinite (float* x, int n)
{
    bool found = false;
    for (int i = 0; i < n; ++i)
        if (! std::isfinite (x[i]))
        {
            x[i] = 0.0f;
            found = true;
        }
    return found;
}
}

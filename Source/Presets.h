#pragma once

#include <initializer_list>
#include <utility>

namespace avel::ess
{
    struct FactoryPreset
    {
        const char* name;
        std::initializer_list<std::pair<const char*, float>> values;   // plain (denormalised) values
    };

    // Unlisted parameters fall back to their defaults.
    // mode: 0 Single Vocal, 1 Allround · processing: 0 Wide, 1 Split · channels: 0 Stereo, 1 Mid, 2 Side
    inline const FactoryPreset factoryPresets[] =
    {
        { "Default",                 {} },
        { "Vocal · Gentle",          { { "threshold", -28 }, { "range", 6 },  { "release", 50 }, { "low", 5500 } } },
        { "Vocal · Female",          { { "threshold", -26 }, { "range", 8 },  { "release", 40 }, { "low", 6800 } } },
        { "Vocal · Male",            { { "threshold", -27 }, { "range", 8 },  { "release", 45 }, { "low", 4600 } } },
        { "Vocal · Hard Esses",      { { "threshold", -34 }, { "range", 14 }, { "release", 30 }, { "low", 5200 }, { "lookahead", 6 } } },
        { "Vocal · Rap",             { { "threshold", -30 }, { "range", 10 }, { "release", 25 }, { "low", 5000 }, { "lookahead", 4 } } },
        { "Vocal · Whisper / ASMR",  { { "threshold", -38 }, { "range", 12 }, { "release", 60 }, { "low", 4200 } } },
        { "Speech · Podcast",        { { "threshold", -30 }, { "range", 10 }, { "release", 60 }, { "low", 4200 }, { "high", 12000 } } },
        { "Speech · Broadcast",      { { "threshold", -34 }, { "range", 16 }, { "release", 35 }, { "low", 4500 }, { "lookahead", 8 } } },
        { "Vocal · Classic Wide",    { { "processing", 0 }, { "threshold", -26 }, { "range", 6 }, { "release", 45 }, { "low", 5500 } } },
        { "Backing Vocals",          { { "mode", 1 }, { "threshold", -30 }, { "range", 8 }, { "release", 50 }, { "low", 5800 } } },
        { "Mix Bus · Smooth Top",    { { "mode", 1 }, { "threshold", -24 }, { "range", 4 }, { "release", 90 }, { "low", 6000 } } },
        { "Drums · Cymbal Tamer",    { { "mode", 1 }, { "threshold", -28 }, { "range", 6 }, { "release", 110 }, { "low", 7000 } } },
        { "Acoustic · Pick Noise",   { { "mode", 1 }, { "threshold", -32 }, { "range", 7 }, { "release", 35 }, { "low", 2800 }, { "high", 7500 } } },
        { "Synth · Harsh Mids",      { { "mode", 1 }, { "threshold", -26 }, { "range", 6 }, { "release", 70 }, { "low", 2400 }, { "high", 5500 } } },
        { "Reverb Return · Side",    { { "mode", 1 }, { "channels", 2 }, { "threshold", -36 }, { "range", 10 }, { "release", 80 }, { "low", 5000 } } },
    };
}

#pragma once

#include <cstdint>

enum class ChamsTarget : int
{
    All = 0,
    Axis = 1,
    Allies = 2
};

struct ChamsColor
{
    float r;
    float g;
    float b;
    float a;
};

struct ChamsSettings
{
    bool enabled = false;

    ChamsTarget target =
        ChamsTarget::All;

    // ON:
    // hidden pass zonder depth-test + normale visible pass.
    //
    // OFF:
    // alleen normale visible pass.
    bool wallHack = true;

    bool deadBodies = true;

    ChamsColor hiddenColor
    {
        1.0f,
        0.15f,
        0.15f,
        1.0f
    };

    ChamsColor visibleColor
    {
        0.15f,
        1.0f,
        0.15f,
        1.0f
    };
};

ChamsSettings& GetChamsSettings();

bool InstallChamsHooks();
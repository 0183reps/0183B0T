#include "../pch.h"

#include <Windows.h>
#include <cstdint>

#include "fov.h"

constexpr std::uintptr_t kFovPointerRva = 0x7D2688;

float* GetFovAddress()
{
    const auto gameBase = reinterpret_cast<std::uintptr_t>(
        GetModuleHandleA(nullptr)
        );

    if (!gameBase)
    {
        return nullptr;
    }

    const auto cgFovObject =
        *reinterpret_cast<std::uintptr_t*>(
            gameBase + kFovPointerRva
            );

    if (!cgFovObject)
    {
        return nullptr;
    }

    return reinterpret_cast<float*>(
        cgFovObject + 0x10
        );
}

void SetFov(float value)
{
    if (float* fov = GetFovAddress())
    {
        *fov = value;
    }
}

void ResetFov()
{
    SetFov(65.0f);
}
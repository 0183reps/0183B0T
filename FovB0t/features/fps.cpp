#include "../pch.h"

#include <Windows.h>
#include <cstdint>

#include "fps.h"

using FindDvarFn = void* (__fastcall*)(
    const char* name
    );

constexpr std::uintptr_t kFindDvarRva = 0x324270;
constexpr std::uintptr_t kCurrentValueOffset = 0x10;

constexpr int kDefaultFpsLimit = 85;
constexpr int kMinimumFpsLimit = 0;
constexpr int kMaximumFpsLimit = 240;

int* GetFpsLimitAddress()
{
    const auto gameBase =
        reinterpret_cast<std::uintptr_t>(
            GetModuleHandleA(nullptr)
            );

    if (!gameBase)
    {
        return nullptr;
    }

    const auto findDvar =
        reinterpret_cast<FindDvarFn>(
            gameBase + kFindDvarRva
            );

    void* dvar =
        findDvar(
            "com_maxfps"
        );

    if (!dvar)
    {
        return nullptr;
    }

    return reinterpret_cast<int*>(
        reinterpret_cast<std::uintptr_t>(dvar)
        + kCurrentValueOffset
        );
}

int GetFpsLimit()
{
    if (int* fpsLimit = GetFpsLimitAddress())
    {
        return *fpsLimit;
    }

    return -1;
}

void SetFpsLimit(int value)
{
    if (value < kMinimumFpsLimit)
    {
        value = kMinimumFpsLimit;
    }

    if (value > kMaximumFpsLimit)
    {
        value = kMaximumFpsLimit;
    }

    if (int* fpsLimit = GetFpsLimitAddress())
    {
        *fpsLimit = value;
    }
}

void ResetFpsLimit()
{
    SetFpsLimit(
        kDefaultFpsLimit
    );
}
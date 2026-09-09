#pragma once

#include <Windows.h>

bool ShouldIgnoreMessage(UINT message);

BOOL WINAPI HookedSetCursorPos(
    int x,
    int y
);

BOOL WINAPI HookedClipCursor(
    const RECT* rect
);

__int64 __fastcall HookedMouseInput(
    float* inputState,
    float* mouseX,
    float* mouseY
);

void UpdateCursorState();

bool InstallMouseInputHook();
bool InstallCursorHooks();
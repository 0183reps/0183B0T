#pragma once

#include <Windows.h>

bool InstallMouseInputHook();
bool InstallCursorHooks();

bool ShouldIgnoreMessage(UINT message);
void UpdateCursorState();
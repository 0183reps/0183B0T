#include <Windows.h>

#include "MinHook.h"

#include "core/input.h"
#include "core/renderer.h"

bool g_imguiInitialized = false;
bool g_menuOpen = true;

float g_fovValue = 65.0f;

DWORD WINAPI MainThread(
    LPVOID parameter
)
{
    if (MH_Initialize() != MH_OK)
    {
        return 0;
    }

    if (!InstallEndSceneHook())
    {
        return 0;
    }

    if (!InstallMouseInputHook())
    {
        return 0;
    }

    if (!InstallCursorHooks())
    {
        return 0;
    }

    if (MH_EnableHook(MH_ALL_HOOKS) != MH_OK)
    {
        return 0;
    }

    return 0;
}

BOOL APIENTRY DllMain(
    HMODULE module,
    DWORD reason,
    LPVOID reserved
)
{
    if (reason == DLL_PROCESS_ATTACH)
    {
        DisableThreadLibraryCalls(
            module
        );

        HANDLE thread =
            CreateThread(
                nullptr,
                0,
                MainThread,
                module,
                0,
                nullptr
            );

        if (thread)
        {
            CloseHandle(
                thread
            );
        }
    }

    return TRUE;
}
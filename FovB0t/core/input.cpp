#include "../pch.h"

#include <Windows.h>
#include <cstdint>

#include "MinHook.h"
#include "imgui.h"

#include "input.h"

using MouseInputFn = __int64(__fastcall*)(
    float* inputState,
    float* mouseX,
    float* mouseY
    );

using SetCursorPosFn = BOOL(WINAPI*)(
    int x,
    int y
    );

using ClipCursorFn = BOOL(WINAPI*)(
    const RECT* rect
    );

extern bool g_menuOpen;
extern bool g_imguiInitialized;

static MouseInputFn g_originalMouseInput = nullptr;
static SetCursorPosFn g_originalSetCursorPos = nullptr;
static ClipCursorFn g_originalClipCursor = nullptr;

constexpr std::uintptr_t kMouseInputRva = 0xCFB60;

bool ShouldIgnoreMessage(UINT message)
{
    switch (message)
    {
    case WM_MOUSEMOVE:

    case WM_LBUTTONDOWN:
    case WM_LBUTTONUP:
    case WM_LBUTTONDBLCLK:

    case WM_RBUTTONDOWN:
    case WM_RBUTTONUP:
    case WM_RBUTTONDBLCLK:

    case WM_MBUTTONDOWN:
    case WM_MBUTTONUP:
    case WM_MBUTTONDBLCLK:

    case WM_XBUTTONDOWN:
    case WM_XBUTTONUP:
    case WM_XBUTTONDBLCLK:

    case WM_MOUSEWHEEL:
    case WM_MOUSEHWHEEL:

    case WM_KEYDOWN:
    case WM_KEYUP:
    case WM_SYSKEYDOWN:
    case WM_SYSKEYUP:
    case WM_CHAR:

    case WM_SETCURSOR:
    case WM_NCHITTEST:
    case WM_MOUSEACTIVATE:

    case WM_INPUT:
        return true;

    default:
        return false;
    }
}

BOOL WINAPI HookedSetCursorPos(
    int x,
    int y
)
{
    if (g_menuOpen)
    {
        return TRUE;
    }

    return g_originalSetCursorPos(
        x,
        y
    );
}

BOOL WINAPI HookedClipCursor(
    const RECT* rect
)
{
    if (g_menuOpen && rect != nullptr)
    {
        return TRUE;
    }

    return g_originalClipCursor(
        rect
    );
}

__int64 __fastcall HookedMouseInput(
    float* inputState,
    float* mouseX,
    float* mouseY
)
{
    const __int64 result = g_originalMouseInput(
        inputState,
        mouseX,
        mouseY
    );

    if (g_menuOpen)
    {
        if (mouseX)
        {
            *mouseX = 0.0f;
        }

        if (mouseY)
        {
            *mouseY = 0.0f;
        }
    }

    return result;
}

void UpdateCursorState()
{
    if (!g_imguiInitialized)
    {
        return;
    }

    ImGuiIO& io = ImGui::GetIO();

    io.MouseDrawCursor = g_menuOpen;

    if (g_menuOpen)
    {
        ReleaseCapture();

        if (g_originalClipCursor)
        {
            g_originalClipCursor(nullptr);
        }
        else
        {
            ClipCursor(nullptr);
        }
    }
}

bool InstallMouseInputHook()
{
    const auto gameBase =
        reinterpret_cast<std::uintptr_t>(
            GetModuleHandleA(nullptr)
            );

    if (!gameBase)
    {
        return false;
    }

    void* mouseInputAddress =
        reinterpret_cast<void*>(
            gameBase + kMouseInputRva
            );

    const MH_STATUS status =
        MH_CreateHook(
            mouseInputAddress,
            reinterpret_cast<void*>(
                HookedMouseInput
                ),
            reinterpret_cast<void**>(
                &g_originalMouseInput
                )
        );

    return status == MH_OK;
}

bool InstallCursorHooks()
{
    MH_STATUS status =
        MH_CreateHookApi(
            L"user32.dll",
            "SetCursorPos",
            reinterpret_cast<void*>(
                HookedSetCursorPos
                ),
            reinterpret_cast<void**>(
                &g_originalSetCursorPos
                )
        );

    if (status != MH_OK)
    {
        return false;
    }

    status =
        MH_CreateHookApi(
            L"user32.dll",
            "ClipCursor",
            reinterpret_cast<void*>(
                HookedClipCursor
                ),
            reinterpret_cast<void**>(
                &g_originalClipCursor
                )
        );

    if (status != MH_OK)
    {
        return false;
    }

    return true;
}
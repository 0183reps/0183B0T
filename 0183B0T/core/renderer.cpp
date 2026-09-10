#include "../pch.h"

#include <Windows.h>
#include <d3d9.h>
#include <cstdint>
#include <intrin.h>

#include "MinHook.h"

#include "imgui.h"
#include "imgui_impl_dx9.h"
#include "imgui_impl_win32.h"

#include "renderer.h"
#include "input.h"

#include "../features/fov.h"
#include "../ui/menu.h"

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(
    HWND hWnd,
    UINT msg,
    WPARAM wParam,
    LPARAM lParam
);

using EndSceneFn = HRESULT(APIENTRY*)(
    IDirect3DDevice9* device
    );

using ResetFn = HRESULT(APIENTRY*)(
    IDirect3DDevice9* device,
    D3DPRESENT_PARAMETERS* presentationParameters
    );

constexpr std::uintptr_t kGameDevicePointerRva =
0x264DCC0;

constexpr std::uintptr_t kGameWindowHandleRva =
0x2651230;

static EndSceneFn g_originalEndScene = nullptr;
static ResetFn g_originalReset = nullptr;

static IDirect3DDevice9* g_currentDevice = nullptr;
static HWND g_gameWindow = nullptr;
static WNDPROC g_originalWndProc = nullptr;

extern bool g_imguiInitialized;
extern bool g_menuOpen;
extern float g_fovValue;

static std::uintptr_t GetGameBase()
{
    return reinterpret_cast<std::uintptr_t>(
        GetModuleHandleA(nullptr)
        );
}

static IDirect3DDevice9* GetGameDevice()
{
    const std::uintptr_t gameBase =
        GetGameBase();

    if (!gameBase)
    {
        return nullptr;
    }

    return *reinterpret_cast<IDirect3DDevice9**>(
        gameBase +
        kGameDevicePointerRva
        );
}

static HWND GetGameWindow()
{
    const std::uintptr_t gameBase =
        GetGameBase();

    if (!gameBase)
    {
        return nullptr;
    }

    return *reinterpret_cast<HWND*>(
        gameBase +
        kGameWindowHandleRva
        );
}

static bool IsWindowReady(
    HWND window
)
{
    if (
        !window ||
        !IsWindow(window) ||
        IsIconic(window)
        )
    {
        return false;
    }

    RECT clientRect{};

    if (!GetClientRect(
        window,
        &clientRect
    ))
    {
        return false;
    }

    return
        clientRect.right > clientRect.left &&
        clientRect.bottom > clientRect.top;
}

static bool GetStableGameRenderer(
    IDirect3DDevice9** device,
    HWND* window
)
{
    if (
        !device ||
        !window
        )
    {
        return false;
    }

    IDirect3DDevice9* firstDevice =
        GetGameDevice();

    HWND firstWindow =
        GetGameWindow();

    if (
        !firstDevice ||
        !IsWindowReady(firstWindow)
        )
    {
        return false;
    }

    Sleep(
        100
    );

    IDirect3DDevice9* secondDevice =
        GetGameDevice();

    HWND secondWindow =
        GetGameWindow();

    if (
        firstDevice != secondDevice ||
        firstWindow != secondWindow ||
        !secondDevice ||
        !IsWindowReady(secondWindow)
        )
    {
        return false;
    }

    *device =
        secondDevice;

    *window =
        secondWindow;

    return true;
}

static bool IsAddressInGameModule(
    void* address
)
{
    const std::uintptr_t moduleBase =
        GetGameBase();

    if (!moduleBase)
    {
        return false;
    }

    const auto dosHeader =
        reinterpret_cast<IMAGE_DOS_HEADER*>(
            moduleBase
            );

    if (
        dosHeader->e_magic != IMAGE_DOS_SIGNATURE
        )
    {
        return false;
    }

    const auto ntHeaders =
        reinterpret_cast<IMAGE_NT_HEADERS*>(
            moduleBase +
            dosHeader->e_lfanew
            );

    if (
        ntHeaders->Signature != IMAGE_NT_SIGNATURE
        )
    {
        return false;
    }

    const std::uintptr_t moduleEnd =
        moduleBase +
        ntHeaders->OptionalHeader.SizeOfImage;

    const std::uintptr_t targetAddress =
        reinterpret_cast<std::uintptr_t>(
            address
            );

    return
        targetAddress >= moduleBase &&
        targetAddress < moduleEnd;
}

LRESULT CALLBACK HookedWndProc(
    HWND hwnd,
    UINT message,
    WPARAM wParam,
    LPARAM lParam
)
{
    if (
        g_imguiInitialized &&
        g_menuOpen
        )
    {
        ImGui_ImplWin32_WndProcHandler(
            hwnd,
            message,
            wParam,
            lParam
        );

        if (ShouldIgnoreMessage(
            message
        ))
        {
            return TRUE;
        }
    }

    if (g_originalWndProc)
    {
        return CallWindowProc(
            g_originalWndProc,
            hwnd,
            message,
            wParam,
            lParam
        );
    }

    return DefWindowProc(
        hwnd,
        message,
        wParam,
        lParam
    );
}

static void ShutdownImGui()
{
    g_imguiInitialized =
        false;

    if (
        g_gameWindow &&
        g_originalWndProc &&
        IsWindow(g_gameWindow)
        )
    {
        SetWindowLongPtr(
            g_gameWindow,
            GWLP_WNDPROC,
            reinterpret_cast<LONG_PTR>(
                g_originalWndProc
                )
        );
    }

    if (ImGui::GetCurrentContext())
    {
        ImGui_ImplDX9_Shutdown();
        ImGui_ImplWin32_Shutdown();
        ImGui::DestroyContext();
    }

    g_currentDevice =
        nullptr;

    g_gameWindow =
        nullptr;

    g_originalWndProc =
        nullptr;
}

static bool InitializeImGui(
    IDirect3DDevice9* device,
    HWND window
)
{
    if (
        !device ||
        !IsWindowReady(window)
        )
    {
        return false;
    }

    IMGUI_CHECKVERSION();

    ImGui::CreateContext();

    ImGuiIO& io =
        ImGui::GetIO();

    io.ConfigFlags |=
        ImGuiConfigFlags_NavEnableKeyboard;

    ImGui::StyleColorsDark();

    if (!ImGui_ImplWin32_Init(
        window
    ))
    {
        ImGui::DestroyContext();

        return false;
    }

    if (!ImGui_ImplDX9_Init(
        device
    ))
    {
        ImGui_ImplWin32_Shutdown();
        ImGui::DestroyContext();

        return false;
    }

    SetLastError(
        ERROR_SUCCESS
    );

    g_originalWndProc =
        reinterpret_cast<WNDPROC>(
            SetWindowLongPtr(
                window,
                GWLP_WNDPROC,
                reinterpret_cast<LONG_PTR>(
                    HookedWndProc
                    )
            )
            );

    if (!g_originalWndProc)
    {
        ImGui_ImplDX9_Shutdown();
        ImGui_ImplWin32_Shutdown();
        ImGui::DestroyContext();

        return false;
    }

    g_currentDevice =
        device;

    g_gameWindow =
        window;

    if (float* fov = GetFovAddress())
    {
        g_fovValue =
            *fov;
    }

    g_imguiInitialized =
        true;

    UpdateCursorState();

    return true;
}

static void UpdateRenderer(
    IDirect3DDevice9* device
)
{
    IDirect3DDevice9* gameDevice =
        GetGameDevice();

    HWND gameWindow =
        GetGameWindow();

    if (
        !gameDevice ||
        !gameWindow ||
        device != gameDevice ||
        !IsWindowReady(gameWindow)
        )
    {
        return;
    }

    if (!g_imguiInitialized)
    {
        InitializeImGui(
            device,
            gameWindow
        );

        return;
    }

    const bool deviceChanged =
        device != g_currentDevice;

    const bool windowChanged =
        gameWindow != g_gameWindow;

    if (
        !deviceChanged &&
        !windowChanged
        )
    {
        return;
    }

    ShutdownImGui();

    InitializeImGui(
        device,
        gameWindow
    );
}

HRESULT APIENTRY HookedReset(
    IDirect3DDevice9* device,
    D3DPRESENT_PARAMETERS* presentationParameters
)
{
    const bool isCurrentDevice =
        g_imguiInitialized &&
        device == g_currentDevice;

    if (isCurrentDevice)
    {
        ImGui_ImplDX9_InvalidateDeviceObjects();
    }

    const HRESULT result =
        g_originalReset(
            device,
            presentationParameters
        );

    if (
        SUCCEEDED(result) &&
        isCurrentDevice
        )
    {
        ImGui_ImplDX9_CreateDeviceObjects();
    }

    return result;
}

HRESULT APIENTRY HookedEndScene(
    IDirect3DDevice9* device
)
{
    void* returnAddress =
        _ReturnAddress();

    const bool callerInGame =
        IsAddressInGameModule(
            returnAddress
        );

    const HRESULT result =
        g_originalEndScene(
            device
        );

    if (!callerInGame)
    {
        return result;
    }

    IDirect3DDevice9* gameDevice =
        GetGameDevice();

    if (
        !gameDevice ||
        device != gameDevice
        )
    {
        return result;
    }

    UpdateRenderer(
        device
    );

    if (
        GetAsyncKeyState(
            VK_INSERT
        ) & 1
        )
    {
        g_menuOpen =
            !g_menuOpen;

        UpdateCursorState();
    }

    if (g_imguiInitialized)
    {
        ImGui_ImplWin32_NewFrame();
        ImGui_ImplDX9_NewFrame();

        ImGui::NewFrame();

        RenderMenu();

        ImGui::EndFrame();

        ImGui::Render();

        ImGui_ImplDX9_RenderDrawData(
            ImGui::GetDrawData()
        );
    }

    return result;
}

bool InstallRendererHooks()
{
    IDirect3DDevice9* gameDevice =
        nullptr;

    HWND gameWindow =
        nullptr;

    while (!GetStableGameRenderer(
        &gameDevice,
        &gameWindow
    ))
    {
        Sleep(
            100
        );
    }

    void** vtable =
        *reinterpret_cast<void***>(
            gameDevice
            );

    if (!vtable)
    {
        return false;
    }

    void* resetAddress =
        vtable[16];

    void* endSceneAddress =
        vtable[42];

    if (
        !resetAddress ||
        !endSceneAddress
        )
    {
        return false;
    }

    if (MH_CreateHook(
        resetAddress,
        reinterpret_cast<void*>(
            HookedReset
            ),
        reinterpret_cast<void**>(
            &g_originalReset
            )
    ) != MH_OK)
    {
        return false;
    }

    if (MH_CreateHook(
        endSceneAddress,
        reinterpret_cast<void*>(
            HookedEndScene
            ),
        reinterpret_cast<void**>(
            &g_originalEndScene
            )
    ) != MH_OK)
    {
        return false;
    }

    return true;
}
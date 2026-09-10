#include "../pch.h"

#include <Windows.h>
#include <d3d9.h>
#include <cstdint>
#include <intrin.h>
#include <cstdio>
#include <cstdarg>

#include "MinHook.h"

#include "imgui.h"
#include "imgui_impl_dx9.h"
#include "imgui_impl_win32.h"

#include "renderer.h"
#include "input.h"

#include "../features/fov.h"
#include "../ui/menu.h"

#pragma comment(lib, "d3d9.lib")

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

static EndSceneFn g_originalEndScene = nullptr;
static ResetFn g_originalReset = nullptr;

static IDirect3DDevice9* g_currentDevice = nullptr;
static HWND g_gameWindow = nullptr;
static WNDPROC g_originalWndProc = nullptr;

extern bool g_imguiInitialized;
extern bool g_menuOpen;
extern float g_fovValue;

constexpr std::uintptr_t kGameDevicePointerRva =
0x264DCC0;

constexpr std::uintptr_t kGameWindowHandleRva =
0x2651230;

static const char* GetRendererLogPath()
{
    static char logPath[MAX_PATH]{};

    if (logPath[0] != '\0')
    {
        return logPath;
    }

    char tempPath[MAX_PATH]{};

    const DWORD length =
        GetTempPathA(
            MAX_PATH,
            tempPath
        );

    if (
        length == 0 ||
        length >= MAX_PATH
        )
    {
        strcpy_s(
            logPath,
            "0183B0T_renderer.log"
        );

        return logPath;
    }

    sprintf_s(
        logPath,
        "%s0183B0T_renderer.log",
        tempPath
    );

    return logPath;
}

static void ClearRendererLog()
{
    FILE* file = nullptr;

    if (
        fopen_s(
            &file,
            GetRendererLogPath(),
            "w"
        ) != 0 ||
        !file
        )
    {
        return;
    }

    fprintf(
        file,
        "0183B0T renderer log\n"
    );

    fclose(
        file
    );
}

static void LogRenderer(
    const char* format,
    ...
)
{
    FILE* file = nullptr;

    if (
        fopen_s(
            &file,
            GetRendererLogPath(),
            "a"
        ) != 0 ||
        !file
        )
    {
        return;
    }

    SYSTEMTIME time{};

    GetLocalTime(
        &time
    );

    fprintf(
        file,
        "[%02u:%02u:%02u.%03u] ",
        time.wHour,
        time.wMinute,
        time.wSecond,
        time.wMilliseconds
    );

    va_list arguments;

    va_start(
        arguments,
        format
    );

    vfprintf(
        file,
        format,
        arguments
    );

    va_end(
        arguments
    );

    fprintf(
        file,
        "\n"
    );

    fclose(
        file
    );
}

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

    const long width =
        clientRect.right -
        clientRect.left;

    const long height =
        clientRect.bottom -
        clientRect.top;

    return
        width > 0 &&
        height > 0;
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
        firstWindow != secondWindow
        )
    {
        return false;
    }

    if (
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

static HWND GetRenderWindow(
    IDirect3DDevice9* device
)
{
    IDirect3DDevice9* gameDevice =
        GetGameDevice();

    HWND gameWindow =
        GetGameWindow();

    if (
        device == gameDevice &&
        gameWindow &&
        IsWindow(gameWindow)
        )
    {
        return gameWindow;
    }

    IDirect3DSwapChain9* swapChain =
        nullptr;

    if (SUCCEEDED(
        device->GetSwapChain(
            0,
            &swapChain
        )
    ))
    {
        D3DPRESENT_PARAMETERS presentationParameters{};

        if (SUCCEEDED(
            swapChain->GetPresentParameters(
                &presentationParameters
            )
        ))
        {
            HWND window =
                presentationParameters.hDeviceWindow;

            swapChain->Release();

            if (
                window &&
                IsWindow(window)
                )
            {
                return window;
            }
        }
        else
        {
            swapChain->Release();
        }
    }

    D3DDEVICE_CREATION_PARAMETERS creationParameters{};

    if (SUCCEEDED(
        device->GetCreationParameters(
            &creationParameters
        )
    ))
    {
        HWND window =
            creationParameters.hFocusWindow;

        if (
            window &&
            IsWindow(window)
            )
        {
            return window;
        }
    }

    return nullptr;
}

static bool IsAddressInGameModule(
    void* address
)
{
    const auto moduleBase =
        reinterpret_cast<std::uintptr_t>(
            GetModuleHandleA(nullptr)
            );

    if (!moduleBase)
    {
        return false;
    }

    const auto dosHeader =
        reinterpret_cast<IMAGE_DOS_HEADER*>(
            moduleBase
            );

    if (
        !dosHeader ||
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
        !ntHeaders ||
        ntHeaders->Signature != IMAGE_NT_SIGNATURE
        )
    {
        return false;
    }

    const std::uintptr_t moduleEnd =
        moduleBase +
        ntHeaders->OptionalHeader.SizeOfImage;

    const auto targetAddress =
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
    LogRenderer(
        "ShutdownImGui: initialized=%d device=%p window=%p wndProc=%p",
        g_imguiInitialized ? 1 : 0,
        g_currentDevice,
        g_gameWindow,
        g_originalWndProc
    );

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
    LogRenderer(
        "InitializeImGui attempt: device=%p window=%p IsWindow=%d IsIconic=%d",
        device,
        window,
        window ? (IsWindow(window) ? 1 : 0) : 0,
        window ? (IsIconic(window) ? 1 : 0) : 0
    );

    if (
        !device ||
        !window ||
        !IsWindow(window)
        )
    {
        LogRenderer(
            "InitializeImGui rejected: invalid device or window"
        );

        return false;
    }

    if (IsIconic(window))
    {
        LogRenderer(
            "InitializeImGui rejected: window is iconic"
        );

        return false;
    }

    RECT clientRect{};

    if (!GetClientRect(
        window,
        &clientRect
    ))
    {
        LogRenderer(
            "InitializeImGui rejected: GetClientRect failed error=%lu",
            GetLastError()
        );

        return false;
    }

    const long width =
        clientRect.right -
        clientRect.left;

    const long height =
        clientRect.bottom -
        clientRect.top;

    LogRenderer(
        "InitializeImGui client size: %ldx%ld",
        width,
        height
    );

    if (
        width <= 0 ||
        height <= 0
        )
    {
        LogRenderer(
            "InitializeImGui rejected: invalid client size"
        );

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
        LogRenderer(
            "InitializeImGui failed: ImGui_ImplWin32_Init"
        );

        ImGui::DestroyContext();

        return false;
    }

    LogRenderer(
        "InitializeImGui: Win32 backend initialized"
    );

    if (!ImGui_ImplDX9_Init(
        device
    ))
    {
        LogRenderer(
            "InitializeImGui failed: ImGui_ImplDX9_Init"
        );

        ImGui_ImplWin32_Shutdown();
        ImGui::DestroyContext();

        return false;
    }

    LogRenderer(
        "InitializeImGui: DX9 backend initialized"
    );

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
        const DWORD error =
            GetLastError();

        LogRenderer(
            "InitializeImGui failed: SetWindowLongPtr returned null error=%lu",
            error
        );

        ImGui_ImplDX9_Shutdown();
        ImGui_ImplWin32_Shutdown();
        ImGui::DestroyContext();

        return false;
    }

    LogRenderer(
        "InitializeImGui: WndProc subclassed original=%p hook=%p",
        g_originalWndProc,
        HookedWndProc
    );

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

    LogRenderer(
        "InitializeImGui SUCCESS: device=%p window=%p size=%ldx%ld",
        g_currentDevice,
        g_gameWindow,
        width,
        height
    );

    UpdateCursorState();

    return true;
}

static void UpdateRenderer(
    IDirect3DDevice9* device
)
{
    IDirect3DDevice9* gameDevice =
        GetGameDevice();

    if (
        gameDevice &&
        device != gameDevice
        )
    {
        return;
    }

    HWND currentWindow =
        GetRenderWindow(
            device
        );

    static IDirect3DDevice9* lastLoggedDevice =
        nullptr;

    static HWND lastLoggedWindow =
        nullptr;

    if (
        device != lastLoggedDevice ||
        currentWindow != lastLoggedWindow
        )
    {
        LogRenderer(
            "UpdateRenderer: device=%p window=%p gameDevice=%p initialized=%d currentDevice=%p currentWindow=%p",
            device,
            currentWindow,
            gameDevice,
            g_imguiInitialized ? 1 : 0,
            g_currentDevice,
            g_gameWindow
        );

        lastLoggedDevice =
            device;

        lastLoggedWindow =
            currentWindow;
    }

    if (!currentWindow)
    {
        return;
    }

    if (!g_imguiInitialized)
    {
        if (!IsWindowReady(
            currentWindow
        ))
        {
            return;
        }

        LogRenderer(
            "UpdateRenderer: ImGui not initialized, starting initialization"
        );

        InitializeImGui(
            device,
            currentWindow
        );

        return;
    }

    const bool deviceChanged =
        device != g_currentDevice;

    const bool windowChanged =
        currentWindow != g_gameWindow;

    if (
        !deviceChanged &&
        !windowChanged
        )
    {
        return;
    }

    LogRenderer(
        "UpdateRenderer CHANGE: deviceChanged=%d windowChanged=%d oldDevice=%p newDevice=%p oldWindow=%p newWindow=%p",
        deviceChanged ? 1 : 0,
        windowChanged ? 1 : 0,
        g_currentDevice,
        device,
        g_gameWindow,
        currentWindow
    );

    ShutdownImGui();

    if (IsWindowReady(
        currentWindow
    ))
    {
        InitializeImGui(
            device,
            currentWindow
        );
    }
}

HRESULT APIENTRY HookedReset(
    IDirect3DDevice9* device,
    D3DPRESENT_PARAMETERS* presentationParameters
)
{
    const bool isCurrentDevice =
        g_imguiInitialized &&
        device == g_currentDevice;

    LogRenderer(
        "Reset ENTER: device=%p currentDevice=%p initialized=%d isCurrentDevice=%d windowed=%d backBuffer=%ux%u",
        device,
        g_currentDevice,
        g_imguiInitialized ? 1 : 0,
        isCurrentDevice ? 1 : 0,
        presentationParameters
        ? (presentationParameters->Windowed ? 1 : 0)
        : -1,
        presentationParameters
        ? presentationParameters->BackBufferWidth
        : 0,
        presentationParameters
        ? presentationParameters->BackBufferHeight
        : 0
    );

    if (isCurrentDevice)
    {
        ImGui_ImplDX9_InvalidateDeviceObjects();

        LogRenderer(
            "Reset: ImGui device objects invalidated"
        );
    }

    const HRESULT result =
        g_originalReset(
            device,
            presentationParameters
        );

    LogRenderer(
        "Reset RESULT: device=%p result=0x%08lX",
        device,
        static_cast<unsigned long>(
            result
            )
    );

    if (
        SUCCEEDED(result) &&
        isCurrentDevice
        )
    {
        ImGui_ImplDX9_CreateDeviceObjects();

        LogRenderer(
            "Reset: ImGui device objects recreated"
        );
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
        gameDevice &&
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

        LogRenderer(
            "INSERT toggled: menuOpen=%d device=%p window=%p initialized=%d",
            g_menuOpen ? 1 : 0,
            g_currentDevice,
            g_gameWindow,
            g_imguiInitialized ? 1 : 0
        );

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
    ClearRendererLog();

    LogRenderer(
        "InstallRendererHooks START"
    );

    const std::uintptr_t gameBase =
        GetGameBase();

    if (!gameBase)
    {
        LogRenderer(
            "InstallRendererHooks failed: game module not found"
        );

        return false;
    }

    LogRenderer(
        "Game module base=%p",
        reinterpret_cast<void*>(
            gameBase
            )
    );

    LogRenderer(
        "Game device pointer address=%p",
        reinterpret_cast<void*>(
            gameBase +
            kGameDevicePointerRva
            )
    );

    LogRenderer(
        "Game window handle address=%p",
        reinterpret_cast<void*>(
            gameBase +
            kGameWindowHandleRva
            )
    );

    IDirect3DDevice9* gameDevice =
        nullptr;

    HWND gameWindow =
        nullptr;

    LogRenderer(
        "Waiting for stable game D3D9 device and window"
    );

    while (!GetStableGameRenderer(
        &gameDevice,
        &gameWindow
    ))
    {
        Sleep(
            100
        );
    }

    LogRenderer(
        "Game renderer ready: device=%p window=%p",
        gameDevice,
        gameWindow
    );

    IDirect3DDevice9* verifyDevice =
        GetGameDevice();

    HWND verifyWindow =
        GetGameWindow();

    if (
        gameDevice != verifyDevice ||
        gameWindow != verifyWindow ||
        !gameDevice ||
        !IsWindowReady(gameWindow)
        )
    {
        LogRenderer(
            "Game renderer changed before hook creation, retrying"
        );

        do
        {
            Sleep(
                100
            );
        } while (!GetStableGameRenderer(
            &gameDevice,
            &gameWindow
        ));
    }

    void** vtable =
        *reinterpret_cast<void***>(
            gameDevice
            );

    if (!vtable)
    {
        LogRenderer(
            "InstallRendererHooks failed: invalid device vtable"
        );

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
        LogRenderer(
            "InstallRendererHooks failed: invalid Reset or EndScene address"
        );

        return false;
    }

    LogRenderer(
        "Game D3D9 addresses: Reset=%p EndScene=%p",
        resetAddress,
        endSceneAddress
    );

    const MH_STATUS resetStatus =
        MH_CreateHook(
            resetAddress,
            reinterpret_cast<void*>(
                HookedReset
                ),
            reinterpret_cast<void**>(
                &g_originalReset
                )
        );

    LogRenderer(
        "MH_CreateHook Reset status=%d original=%p",
        static_cast<int>(
            resetStatus
            ),
        g_originalReset
    );

    if (resetStatus != MH_OK)
    {
        return false;
    }

    const MH_STATUS endSceneStatus =
        MH_CreateHook(
            endSceneAddress,
            reinterpret_cast<void*>(
                HookedEndScene
                ),
            reinterpret_cast<void**>(
                &g_originalEndScene
                )
        );

    LogRenderer(
        "MH_CreateHook EndScene status=%d original=%p",
        static_cast<int>(
            endSceneStatus
            ),
        g_originalEndScene
    );

    if (endSceneStatus != MH_OK)
    {
        return false;
    }

    LogRenderer(
        "InstallRendererHooks END success=1"
    );

    return true;
}
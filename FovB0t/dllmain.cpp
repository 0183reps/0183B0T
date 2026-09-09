#include <Windows.h>
#include <d3d9.h>
#include <cstdint>

#include "MinHook.h"

#include "imgui.h"
#include "imgui_impl_dx9.h"
#include "imgui_impl_win32.h"

#include "features/fov.h"

#pragma comment(lib, "d3d9.lib")

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(
    HWND hWnd,
    UINT msg,
    WPARAM wParam,
    LPARAM lParam
);

using EndSceneFn = HRESULT(APIENTRY*)(IDirect3DDevice9* device);

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

static EndSceneFn g_originalEndScene = nullptr;
static MouseInputFn g_originalMouseInput = nullptr;

static SetCursorPosFn g_originalSetCursorPos = nullptr;
static ClipCursorFn g_originalClipCursor = nullptr;

static HWND g_gameWindow = nullptr;
static WNDPROC g_originalWndProc = nullptr;

static bool g_imguiInitialized = false;
static bool g_menuOpen = true;

static float g_fovValue = 65.0f;

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


LRESULT CALLBACK HookedWndProc(
    HWND hwnd,
    UINT message,
    WPARAM wParam,
    LPARAM lParam
)
{
    if (g_imguiInitialized && g_menuOpen)
    {
        ImGui_ImplWin32_WndProcHandler(
            hwnd,
            message,
            wParam,
            lParam
        );

        if (ShouldIgnoreMessage(message))
        {
            return TRUE;
        }
    }

    return CallWindowProc(
        g_originalWndProc,
        hwnd,
        message,
        wParam,
        lParam
    );
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


void InitializeImGui(
    IDirect3DDevice9* device
)
{
    if (g_imguiInitialized)
    {
        return;
    }

    D3DDEVICE_CREATION_PARAMETERS creationParameters{};

    if (SUCCEEDED(
        device->GetCreationParameters(
            &creationParameters
        )
    ))
    {
        g_gameWindow = creationParameters.hFocusWindow;
    }

    if (!g_gameWindow)
    {
        g_gameWindow = GetForegroundWindow();
    }

    IMGUI_CHECKVERSION();

    ImGui::CreateContext();

    ImGuiIO& io = ImGui::GetIO();

    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    ImGui::StyleColorsDark();

    ImGui_ImplWin32_Init(
        g_gameWindow
    );

    ImGui_ImplDX9_Init(
        device
    );

    g_originalWndProc =
        reinterpret_cast<WNDPROC>(
            SetWindowLongPtr(
                g_gameWindow,
                GWLP_WNDPROC,
                reinterpret_cast<LONG_PTR>(
                    HookedWndProc
                    )
            )
            );

    if (float* fov = GetFovAddress())
    {
        g_fovValue = *fov;
    }

    g_imguiInitialized = true;

    UpdateCursorState();
}


void RenderMenu()
{
    if (!g_menuOpen)
    {
        return;
    }

    ImGui::SetNextWindowSize(
        ImVec2(360.0f, 180.0f),
        ImGuiCond_FirstUseEver
    );

    ImGui::Begin(
        "FovB0t | By: 0183",
        nullptr,
        ImGuiWindowFlags_NoCollapse
    );

    ImGui::Text("Camera");

    ImGui::Separator();

    if (ImGui::SliderFloat(
        "FOV",
        &g_fovValue,
        40.0f,
        120.0f,
        "%.1f"
    ))
    {
        SetFov(g_fovValue);
    }

    if (ImGui::Button(
        "Reset FOV"
    ))
    {
        g_fovValue = 65.0f;
        ResetFov();
    }

    ImGui::Spacing();

    ImGui::Text(
        "INSERT - Toggle menu"
    );

    ImGui::End();
}


HRESULT APIENTRY HookedEndScene(
    IDirect3DDevice9* device
)
{
    if (!g_imguiInitialized)
    {
        InitializeImGui(
            device
        );
    }

    if (GetAsyncKeyState(VK_INSERT) & 1)
    {
        g_menuOpen = !g_menuOpen;

        UpdateCursorState();
    }

    if (g_imguiInitialized)
    {
        ImGui_ImplDX9_NewFrame();
        ImGui_ImplWin32_NewFrame();

        ImGui::NewFrame();

        RenderMenu();

        ImGui::EndFrame();

        ImGui::Render();

        ImGui_ImplDX9_RenderDrawData(
            ImGui::GetDrawData()
        );
    }

    return g_originalEndScene(
        device
    );
}


bool InstallEndSceneHook()
{
    WNDCLASSEXA windowClass{};

    windowClass.cbSize = sizeof(
        WNDCLASSEXA
        );

    windowClass.lpfnWndProc = DefWindowProcA;

    windowClass.hInstance =
        GetModuleHandleA(nullptr);

    windowClass.lpszClassName =
        "FovB0tDummyWindow";

    RegisterClassExA(
        &windowClass
    );

    HWND dummyWindow =
        CreateWindowExA(
            0,
            windowClass.lpszClassName,
            "FovB0t",
            WS_OVERLAPPEDWINDOW,
            0,
            0,
            100,
            100,
            nullptr,
            nullptr,
            windowClass.hInstance,
            nullptr
        );

    if (!dummyWindow)
    {
        return false;
    }

    IDirect3D9* d3d =
        Direct3DCreate9(
            D3D_SDK_VERSION
        );

    if (!d3d)
    {
        DestroyWindow(
            dummyWindow
        );

        UnregisterClassA(
            windowClass.lpszClassName,
            windowClass.hInstance
        );

        return false;
    }

    D3DPRESENT_PARAMETERS presentationParameters{};

    presentationParameters.Windowed = TRUE;

    presentationParameters.SwapEffect =
        D3DSWAPEFFECT_DISCARD;

    presentationParameters.hDeviceWindow =
        dummyWindow;

    IDirect3DDevice9* dummyDevice =
        nullptr;

    HRESULT result =
        d3d->CreateDevice(
            D3DADAPTER_DEFAULT,
            D3DDEVTYPE_HAL,
            dummyWindow,
            D3DCREATE_SOFTWARE_VERTEXPROCESSING,
            &presentationParameters,
            &dummyDevice
        );

    if (FAILED(result))
    {
        result =
            d3d->CreateDevice(
                D3DADAPTER_DEFAULT,
                D3DDEVTYPE_REF,
                dummyWindow,
                D3DCREATE_SOFTWARE_VERTEXPROCESSING,
                &presentationParameters,
                &dummyDevice
            );
    }

    if (FAILED(result) || !dummyDevice)
    {
        d3d->Release();

        DestroyWindow(
            dummyWindow
        );

        UnregisterClassA(
            windowClass.lpszClassName,
            windowClass.hInstance
        );

        return false;
    }

    void** vtable =
        *reinterpret_cast<void***>(
            dummyDevice
            );

    void* endSceneAddress =
        vtable[42];

    const MH_STATUS hookStatus =
        MH_CreateHook(
            endSceneAddress,
            reinterpret_cast<void*>(
                HookedEndScene
                ),
            reinterpret_cast<void**>(
                &g_originalEndScene
                )
        );

    dummyDevice->Release();

    d3d->Release();

    DestroyWindow(
        dummyWindow
    );

    UnregisterClassA(
        windowClass.lpszClassName,
        windowClass.hInstance
    );

    if (hookStatus != MH_OK)
    {
        return false;
    }

    return true;
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
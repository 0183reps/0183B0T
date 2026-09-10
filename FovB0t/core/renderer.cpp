#include "../pch.h"

#include <Windows.h>
#include <d3d9.h>

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

static HWND g_gameWindow = nullptr;
static WNDPROC g_originalWndProc = nullptr;

extern bool g_imguiInitialized;
extern bool g_menuOpen;
extern float g_fovValue;

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

static bool InitializeImGui(
    IDirect3DDevice9* device
)
{
    if (g_imguiInitialized)
    {
        return true;
    }

    D3DDEVICE_CREATION_PARAMETERS creationParameters{};

    if (FAILED(
        device->GetCreationParameters(
            &creationParameters
        )
    ))
    {
        return false;
    }

    g_gameWindow =
        creationParameters.hFocusWindow;

    if (
        !g_gameWindow ||
        !IsWindow(g_gameWindow)
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
        g_gameWindow
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

    if (!g_originalWndProc)
    {
        ImGui_ImplDX9_Shutdown();
        ImGui_ImplWin32_Shutdown();
        ImGui::DestroyContext();

        return false;
    }

    if (float* fov = GetFovAddress())
    {
        g_fovValue =
            *fov;
    }

    g_imguiInitialized = true;

    UpdateCursorState();

    return true;
}

HRESULT APIENTRY HookedReset(
    IDirect3DDevice9* device,
    D3DPRESENT_PARAMETERS* presentationParameters
)
{
    if (g_imguiInitialized)
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
        g_imguiInitialized
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
    if (!g_imguiInitialized)
    {
        InitializeImGui(
            device
        );
    }

    if (GetAsyncKeyState(VK_INSERT) & 1)
    {
        g_menuOpen =
            !g_menuOpen;

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

bool InstallRendererHooks()
{
    WNDCLASSEXA windowClass{};

    windowClass.cbSize =
        sizeof(
            windowClass
            );

    windowClass.lpfnWndProc =
        DefWindowProcA;

    windowClass.hInstance =
        GetModuleHandleA(nullptr);

    windowClass.lpszClassName =
        "FovB0tDummyWindow";

    if (!RegisterClassExA(
        &windowClass
    ))
    {
        return false;
    }

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
        UnregisterClassA(
            windowClass.lpszClassName,
            windowClass.hInstance
        );

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

    presentationParameters.Windowed =
        TRUE;

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

    if (
        FAILED(result) ||
        !dummyDevice
        )
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

    void* resetAddress =
        vtable[16];

    void* endSceneAddress =
        vtable[42];

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

    dummyDevice->Release();
    d3d->Release();

    DestroyWindow(
        dummyWindow
    );

    UnregisterClassA(
        windowClass.lpszClassName,
        windowClass.hInstance
    );

    return
        resetStatus == MH_OK &&
        endSceneStatus == MH_OK;
}
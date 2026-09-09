#include <Windows.h>
#include <d3d9.h>

#include "MinHook.h"

#include "imgui.h"
#include "imgui_impl_dx9.h"
#include "imgui_impl_win32.h"

#include "core/input.h"
#include "features/fov.h"
#include "ui/menu.h"

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

static EndSceneFn g_originalEndScene = nullptr;

static HWND g_gameWindow = nullptr;
static WNDPROC g_originalWndProc = nullptr;

bool g_imguiInitialized = false;
bool g_menuOpen = true;

float g_fovValue = 65.0f;

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
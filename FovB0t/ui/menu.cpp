#include "../pch.h"

#include "imgui.h"

#include "menu.h"
#include "../features/fov.h"
#include "../features/fps.h"

extern bool g_menuOpen;
extern float g_fovValue;

static float g_minusHoldTime = 0.0f;
static float g_minusRepeatTimer = 0.0f;

static float g_plusHoldTime = 0.0f;
static float g_plusRepeatTimer = 0.0f;

static int g_fpsInputValue = 85;
static bool g_fpsInputInitialized = false;

static bool RepeatButton(
    const char* label,
    float& holdTime,
    float& repeatTimer
)
{
    const bool clicked =
        ImGui::Button(
            label,
            ImVec2(
                40.0f,
                0.0f
            )
        );

    if (!ImGui::IsItemActive())
    {
        holdTime = 0.0f;
        repeatTimer = 0.0f;

        return clicked;
    }

    const float deltaTime =
        ImGui::GetIO().DeltaTime;

    holdTime +=
        deltaTime;

    if (holdTime < 0.35f)
    {
        return clicked;
    }

    float repeatInterval =
        0.10f;

    if (holdTime >= 2.0f)
    {
        repeatInterval =
            0.025f;
    }
    else if (holdTime >= 1.0f)
    {
        repeatInterval =
            0.05f;
    }

    repeatTimer +=
        deltaTime;

    if (repeatTimer >= repeatInterval)
    {
        repeatTimer = 0.0f;

        return true;
    }

    return clicked;
}

void RenderMenu()
{
    if (!g_menuOpen)
    {
        return;
    }

    ImGui::SetNextWindowSize(
        ImVec2(
            500.0f,
            180.0f
        ),
        ImGuiCond_FirstUseEver
    );

    ImGui::Begin(
        "FovB0t | By: 0183",
        nullptr,
        ImGuiWindowFlags_NoCollapse
    );

    ImGui::Text(
        "Field of View"
    );

    ImGui::SameLine();

    ImGui::SetNextItemWidth(
        220.0f
    );

    if (ImGui::SliderFloat(
        "##FOV",
        &g_fovValue,
        40.0f,
        120.0f,
        "%.1f"
    ))
    {
        SetFov(
            g_fovValue
        );
    }

    ImGui::SameLine();

    if (ImGui::Button(
        "Reset##FOV"
    ))
    {
        ResetFov();

        g_fovValue =
            65.0f;
    }

    ImGui::Spacing();

    const int fpsLimit =
        GetFpsLimit();

    if (!g_fpsInputInitialized)
    {
        g_fpsInputValue =
            fpsLimit;

        g_fpsInputInitialized =
            true;
    }

    if (!ImGui::IsAnyItemActive())
    {
        g_fpsInputValue =
            fpsLimit;
    }

    ImGui::Text(
        "FPS Limit"
    );

    ImGui::SameLine();

    if (RepeatButton(
        "-",
        g_minusHoldTime,
        g_minusRepeatTimer
    ))
    {
        SetFpsLimit(
            fpsLimit - 1
        );

        g_fpsInputValue =
            GetFpsLimit();
    }

    ImGui::SameLine();

    ImGui::SetNextItemWidth(
        80.0f
    );

    if (ImGui::InputInt(
        "##FPS",
        &g_fpsInputValue,
        0,
        0,
        ImGuiInputTextFlags_EnterReturnsTrue
    ))
    {
        SetFpsLimit(
            g_fpsInputValue
        );

        g_fpsInputValue =
            GetFpsLimit();
    }

    ImGui::SameLine();

    if (RepeatButton(
        "+",
        g_plusHoldTime,
        g_plusRepeatTimer
    ))
    {
        SetFpsLimit(
            fpsLimit + 1
        );

        g_fpsInputValue =
            GetFpsLimit();
    }

    ImGui::SameLine();

    if (ImGui::Button(
        "Reset##FPS"
    ))
    {
        ResetFpsLimit();

        g_fpsInputValue =
            GetFpsLimit();
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    ImGui::TextDisabled(
        "INSERT | Toggle menu"
    );

    ImGui::End();
}
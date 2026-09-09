#include "../pch.h"

#include "imgui.h"

#include "menu.h"
#include "../features/fov.h"

extern bool g_menuOpen;
extern float g_fovValue;

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
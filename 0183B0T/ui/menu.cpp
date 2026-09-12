#include "../pch.h"

#include "imgui.h"

#include "menu.h"
#include "../features/fov.h"
#include "../features/fps.h"
#include "../features/players.h"

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
    SetFov(
        g_fovValue
    );

    EnforceFpsLimit();

    if (!g_menuOpen)
    {
        return;
    }

    ImGui::SetNextWindowSize(
        ImVec2(
            760.0f,
            500.0f
        ),
        ImGuiCond_FirstUseEver
    );

    ImGui::Begin(
        "0183B0T | By: 0183",
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

    ImGui::Text(
        "Players"
    );

    const std::vector<PlayerInfo> players =
        GetPlayers();

    int activePlayerCount =
        0;

    for (const PlayerInfo& player : players)
    {
        if (player.occupied)
        {
            ++activePlayerCount;
        }
    }

    ImGui::Text(
        "Active players: %d / 18",
        activePlayerCount
    );

    ImGui::Spacing();

    if (ImGui::BeginTable(
        "PlayersTable",
        4,
        ImGuiTableFlags_Borders |
        ImGuiTableFlags_RowBg |
        ImGuiTableFlags_SizingStretchProp
    ))
    {
        ImGui::TableSetupColumn(
            "Slot",
            ImGuiTableColumnFlags_WidthFixed,
            50.0f
        );

        ImGui::TableSetupColumn(
            "Name"
        );

        ImGui::TableSetupColumn(
            "IP"
        );

        ImGui::TableSetupColumn(
            "Steam ID"
        );

        ImGui::TableHeadersRow();

        for (const PlayerInfo& player : players)
        {
            if (!player.occupied)
            {
                continue;
            }

            ImGui::TableNextRow();

            ImGui::TableSetColumnIndex(
                0
            );

            ImGui::Text(
                "%d",
                player.slot
            );

            ImGui::TableSetColumnIndex(
                1
            );

            ImGui::TextUnformatted(
                player.name.c_str()
            );

            ImGui::TableSetColumnIndex(
                2
            );

            ImGui::TextUnformatted(
                player.ip.c_str()
            );

            ImGui::TableSetColumnIndex(
                3
            );

            if (player.steamId == 0)
            {
                ImGui::TextUnformatted(
                    "-"
                );
            }
            else
            {
                ImGui::Text(
                    "%llu",
                    static_cast<unsigned long long>(
                        player.steamId
                        )
                );
            }
        }

        ImGui::EndTable();
    }

    if (activePlayerCount == 0)
    {
        ImGui::TextDisabled(
            "No active players"
        );
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    ImGui::TextDisabled(
        "INSERT | Toggle menu"
    );

    ImGui::End();
}
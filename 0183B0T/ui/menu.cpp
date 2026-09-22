#include "../pch.h"

#include "imgui.h"

#include "menu.h"

#include "../features/fov.h"
#include "../features/fps.h"
#include "../features/players.h"
#include "../features/chams.h"

#include <Windows.h>

#include <cstdint>
#include <cstdio>
#include <string>

extern bool g_menuOpen;
extern float g_fovValue;

static float g_minusHoldTime = 0.0f;
static float g_minusRepeatTimer = 0.0f;

static float g_plusHoldTime = 0.0f;
static float g_plusRepeatTimer = 0.0f;

static int g_fpsInputValue = 85;
static bool g_fpsInputInitialized = false;


// =============================================================
// Game addresses
// =============================================================

// sub_1402D4DC0
//
// Proven behavior:
//
// if (Steam is initialized)
// {
//     SteamFriends()->ActivateGameOverlayToUser(
//         "steamid",
//         steamId
//     );
// }
//
// Image base:
//     0x140000000
//
// Function:
//     0x1402D4DC0
//
// RVA:
//     0x2D4DC0
//
static constexpr std::uintptr_t kOpenSteamProfileRva =
0x2D4DC0;


// =============================================================
// Repeat button
// =============================================================

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


// =============================================================
// Steam profile
// =============================================================

static void OpenSteamProfile(
    const std::uint64_t steamId
)
{
    if (steamId == 0)
    {
        return;
    }

    const HMODULE gameModule =
        GetModuleHandleW(
            nullptr
        );

    if (gameModule == nullptr)
    {
        return;
    }

    const std::uintptr_t gameBase =
        reinterpret_cast<std::uintptr_t>(
            gameModule
            );

    using OpenSteamProfileFn =
        void(__fastcall*)(
            std::uint64_t steamId
            );

    const auto openSteamProfile =
        reinterpret_cast<OpenSteamProfileFn>(
            gameBase +
            kOpenSteamProfileRva
            );

    openSteamProfile(
        steamId
    );
}


// =============================================================
// Player context menu
// =============================================================

static void OpenPlayerContextMenuOnRightClick()
{
    if (
        ImGui::IsItemHovered() &&
        ImGui::IsMouseReleased(
            ImGuiMouseButton_Right
        )
        )
    {
        ImGui::OpenPopup(
            "PlayerContextMenu"
        );
    }
}


static void RenderPlayerContextMenu(
    const PlayerInfo& player
)
{
    if (!ImGui::BeginPopup(
        "PlayerContextMenu"
    ))
    {
        return;
    }


    // ---------------------------------------------------------
    // Copy IP
    // ---------------------------------------------------------

    const bool hasIp =
        !player.ip.empty() &&
        player.ip != "-";

    if (
        ImGui::MenuItem(
            "Copy IP",
            nullptr,
            false,
            hasIp
        )
        )
    {
        ImGui::SetClipboardText(
            player.ip.c_str()
        );
    }


    // ---------------------------------------------------------
    // Copy Steam ID
    // ---------------------------------------------------------

    if (
        ImGui::MenuItem(
            "Copy Steam ID",
            nullptr,
            false,
            player.steamId != 0
        )
        )
    {
        char steamIdBuffer[32]{};

        std::snprintf(
            steamIdBuffer,
            sizeof(steamIdBuffer),
            "%llu",
            static_cast<unsigned long long>(
                player.steamId
                )
        );

        ImGui::SetClipboardText(
            steamIdBuffer
        );
    }


    ImGui::Separator();


    // ---------------------------------------------------------
    // Steam profile
    // ---------------------------------------------------------

    if (
        ImGui::MenuItem(
            "Open Steam Profile",
            nullptr,
            false,
            player.steamId != 0
        )
        )
    {
        OpenSteamProfile(
            player.steamId
        );
    }


    ImGui::EndPopup();
}


// =============================================================
// Player name + badges
// =============================================================

static void RenderPlayerName(
    const PlayerInfo& player
)
{
    ImGui::TextUnformatted(
        player.name.c_str()
    );


    // ---------------------------------------------------------
    // You
    // ---------------------------------------------------------

    if (player.isLocal)
    {
        ImGui::SameLine(
            0.0f,
            5.0f
        );

        ImGui::TextColored(
            ImVec4(
                0.30f,
                1.00f,
                0.30f,
                1.00f
            ),
            "[You]"
        );
    }


    // ---------------------------------------------------------
    // Host
    // ---------------------------------------------------------

    if (player.isHost)
    {
        ImGui::SameLine(
            0.0f,
            5.0f
        );

        ImGui::TextColored(
            ImVec4(
                0.30f,
                1.00f,
                1.00f,
                1.00f
            ),
            "[HOST]"
        );
    }


    // ---------------------------------------------------------
    // Friend
    // ---------------------------------------------------------

    if (player.isFriend)
    {
        ImGui::SameLine(
            0.0f,
            5.0f
        );

        ImGui::TextColored(
            ImVec4(
                1.00f,
                0.30f,
                1.00f,
                1.00f
            ),
            "[FRIEND]"
        );
    }
}


// =============================================================
// General tab
// =============================================================

static void RenderGeneralTab()
{
    // ---------------------------------------------------------
    // FOV
    // ---------------------------------------------------------

    ImGui::Text(
        "Field of View"
    );

    ImGui::Spacing();

    ImGui::SetNextItemWidth(
        300.0f
    );

    if (
        ImGui::SliderFloat(
            "##FOV",
            &g_fovValue,
            40.0f,
            120.0f,
            "%.1f"
        )
        )
    {
        SetFov(
            g_fovValue
        );
    }

    ImGui::SameLine();

    if (
        ImGui::Button(
            "Reset##FOV"
        )
        )
    {
        ResetFov();

        g_fovValue =
            65.0f;
    }


    // ---------------------------------------------------------
    // FPS
    // ---------------------------------------------------------

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    ImGui::Text(
        "FPS Limit"
    );

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

    if (
        RepeatButton(
            "-",
            g_minusHoldTime,
            g_minusRepeatTimer
        )
        )
    {
        SetFpsLimit(
            fpsLimit - 1
        );

        g_fpsInputValue =
            GetFpsLimit();
    }

    ImGui::SameLine();

    ImGui::SetNextItemWidth(
        100.0f
    );

    if (
        ImGui::InputInt(
            "##FPS",
            &g_fpsInputValue,
            0,
            0,
            ImGuiInputTextFlags_EnterReturnsTrue
        )
        )
    {
        SetFpsLimit(
            g_fpsInputValue
        );

        g_fpsInputValue =
            GetFpsLimit();
    }

    ImGui::SameLine();

    if (
        RepeatButton(
            "+",
            g_plusHoldTime,
            g_plusRepeatTimer
        )
        )
    {
        SetFpsLimit(
            fpsLimit + 1
        );

        g_fpsInputValue =
            GetFpsLimit();
    }

    ImGui::SameLine();

    if (
        ImGui::Button(
            "Reset##FPS"
        )
        )
    {
        ResetFpsLimit();

        g_fpsInputValue =
            GetFpsLimit();
    }
}


// =============================================================
// Chams tab
// =============================================================

static void RenderChamsTab()
{
    ChamsSettings& chams =
        GetChamsSettings();


    ImGui::Text(
        "Chams"
    );

    ImGui::Spacing();


    // ---------------------------------------------------------
    // Enabled
    // ---------------------------------------------------------

    ImGui::Checkbox(
        "Enabled##Chams",
        &chams.enabled
    );


    // ---------------------------------------------------------
    // Target
    // ---------------------------------------------------------

    ImGui::Spacing();

    ImGui::Text(
        "Target"
    );

    ImGui::SameLine();

    int target =
        static_cast<int>(
            chams.target
            );

    ImGui::SetNextItemWidth(
        150.0f
    );

    if (
        ImGui::Combo(
            "##TargetChams",
            &target,
            "All\0Enemies\0Friendlies\0"
        )
        )
    {
        chams.target =
            static_cast<ChamsTarget>(
                target
                );
    }


    // ---------------------------------------------------------
    // Options
    // ---------------------------------------------------------

    ImGui::Spacing();

    ImGui::Checkbox(
        "Wall Hack##Chams",
        &chams.wallHack
    );

    ImGui::SameLine();

    ImGui::Checkbox(
        "Dead bodies##Chams",
        &chams.deadBodies
    );


    // ---------------------------------------------------------
    // Colors
    // ---------------------------------------------------------

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    ImGui::Text(
        "Colors"
    );

    ImGui::Spacing();

    float hiddenColor[4] =
    {
        chams.hiddenColor.r,
        chams.hiddenColor.g,
        chams.hiddenColor.b,
        chams.hiddenColor.a
    };

    float visibleColor[4] =
    {
        chams.visibleColor.r,
        chams.visibleColor.g,
        chams.visibleColor.b,
        chams.visibleColor.a
    };

    if (chams.wallHack)
    {
        if (
            ImGui::ColorEdit4(
                "Hidden##Chams",
                hiddenColor,
                ImGuiColorEditFlags_NoInputs
            )
            )
        {
            chams.hiddenColor =
            {
                hiddenColor[0],
                hiddenColor[1],
                hiddenColor[2],
                hiddenColor[3]
            };
        }
    }

    if (
        ImGui::ColorEdit4(
            "Visible##Chams",
            visibleColor,
            ImGuiColorEditFlags_NoInputs
        )
        )
    {
        chams.visibleColor =
        {
            visibleColor[0],
            visibleColor[1],
            visibleColor[2],
            visibleColor[3]
        };
    }
}


// =============================================================
// Players tab
// =============================================================

static void RenderPlayersTab()
{
    const std::vector<PlayerInfo> players =
        GetPlayers();

    int activePlayerCount = 0;

    for (const PlayerInfo& player : players)
    {
        if (player.occupied)
        {
            ++activePlayerCount;
        }
    }


    // ---------------------------------------------------------
    // Header
    // ---------------------------------------------------------

    ImGui::Text(
        "Active players: %d / 18",
        activePlayerCount
    );

    ImGui::SameLine();

    ImGui::TextDisabled(
        "| Right-click a player for actions"
    );

    ImGui::Spacing();


    // ---------------------------------------------------------
    // Table
    // ---------------------------------------------------------

    if (
        ImGui::BeginTable(
            "PlayersTable",
            4,
            ImGuiTableFlags_Borders |
            ImGuiTableFlags_RowBg |
            ImGuiTableFlags_SizingStretchProp
        )
        )
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

            ImGui::PushID(
                player.slot
            );

            ImGui::TableNextRow();


            // -------------------------------------------------
            // Relative team color
            // -------------------------------------------------

            if (
                player.clientIndex >= 0 &&
                player.team != 0
                )
            {
                const ImU32 rowColor =
                    player.isFriendly
                    ? IM_COL32(
                        40,
                        120,
                        55,
                        35
                    )
                    : IM_COL32(
                        150,
                        45,
                        45,
                        35
                    );

                ImGui::TableSetBgColor(
                    ImGuiTableBgTarget_RowBg0,
                    rowColor
                );
            }


            // -------------------------------------------------
            // Slot
            // -------------------------------------------------

            ImGui::TableSetColumnIndex(
                0
            );

            ImGui::Text(
                "%d",
                player.slot
            );

            OpenPlayerContextMenuOnRightClick();


            // -------------------------------------------------
            // Name + badges
            // -------------------------------------------------

            ImGui::TableSetColumnIndex(
                1
            );

            RenderPlayerName(
                player
            );

            // The last rendered item is the last badge when
            // present, otherwise the name itself.
            OpenPlayerContextMenuOnRightClick();


            // -------------------------------------------------
            // IP
            // -------------------------------------------------

            ImGui::TableSetColumnIndex(
                2
            );

            ImGui::TextUnformatted(
                player.ip.c_str()
            );

            OpenPlayerContextMenuOnRightClick();


            // -------------------------------------------------
            // Steam ID
            // -------------------------------------------------

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

            OpenPlayerContextMenuOnRightClick();


            // -------------------------------------------------
            // One context popup per player
            // -------------------------------------------------

            RenderPlayerContextMenu(
                player
            );

            ImGui::PopID();
        }


        ImGui::EndTable();
    }


    if (activePlayerCount == 0)
    {
        ImGui::Spacing();

        ImGui::TextDisabled(
            "No active players"
        );
    }
}


// =============================================================
// Main menu
// =============================================================

void RenderMenu()
{
    // These remain active independently of the selected tab.
    SetFov(
        g_fovValue
    );

    EnforceFpsLimit();


    if (!g_menuOpen)
    {
        return;
    }


    // ---------------------------------------------------------
    // Window
    // ---------------------------------------------------------

    ImGui::SetNextWindowSize(
        ImVec2(
            800.0f,
            500.0f
        ),
        ImGuiCond_FirstUseEver
    );

    ImGui::Begin(
        "0183B0T | By: 0183",
        nullptr,
        ImGuiWindowFlags_NoCollapse
    );


    // ---------------------------------------------------------
    // Tabs
    // ---------------------------------------------------------

    if (
        ImGui::BeginTabBar(
            "MainTabs"
        )
        )
    {
        // -----------------------------------------------------
        // General
        // -----------------------------------------------------

        if (
            ImGui::BeginTabItem(
                "General"
            )
            )
        {
            ImGui::Spacing();

            RenderGeneralTab();

            ImGui::EndTabItem();
        }


        // -----------------------------------------------------
        // Chams
        // -----------------------------------------------------

        if (
            ImGui::BeginTabItem(
                "Chams"
            )
            )
        {
            ImGui::Spacing();

            RenderChamsTab();

            ImGui::EndTabItem();
        }


        // -----------------------------------------------------
        // Players
        // -----------------------------------------------------

        if (
            ImGui::BeginTabItem(
                "Players"
            )
            )
        {
            ImGui::Spacing();

            RenderPlayersTab();

            ImGui::EndTabItem();
        }


        ImGui::EndTabBar();
    }


    // ---------------------------------------------------------
    // Footer
    // ---------------------------------------------------------

    ImGui::SetCursorPosY(
        ImGui::GetWindowHeight() - 30.0f
    );

    ImGui::Separator();

    ImGui::TextDisabled(
        "INSERT | Toggle menu"
    );


    ImGui::End();
}
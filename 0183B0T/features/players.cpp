#include "../pch.h"
#include "players.h"

#include <Windows.h>

#include <cstdint>
#include <cstdio>
#include <cstring>

namespace
{
    constexpr std::uintptr_t kActiveMembersRva = 0x1607078;
    constexpr std::uintptr_t kPlayerStride = 0x148;

    constexpr std::uintptr_t kNameOffset = 0x2C;
    constexpr std::uintptr_t kIpv4Offset = 0xE6;
    constexpr std::uintptr_t kSteamIdOffset = 0x110;

    constexpr int kMaxPlayers = 18;

    std::string FormatIpv4(const std::uint8_t* bytes)
    {
        if (bytes[0] == 0 &&
            bytes[1] == 0 &&
            bytes[2] == 0 &&
            bytes[3] == 0)
        {
            return "-";
        }

        char buffer[16]{};

        std::snprintf(
            buffer,
            sizeof(buffer),
            "%u.%u.%u.%u",
            static_cast<unsigned int>(bytes[0]),
            static_cast<unsigned int>(bytes[1]),
            static_cast<unsigned int>(bytes[2]),
            static_cast<unsigned int>(bytes[3])
        );

        return buffer;
    }
}

std::vector<PlayerInfo> GetPlayers()
{
    std::vector<PlayerInfo> players;

    const auto moduleBase =
        reinterpret_cast<std::uintptr_t>(
            GetModuleHandleW(L"iw5mp.exe")
            );

    if (moduleBase == 0)
    {
        return players;
    }

    const std::uintptr_t membersBase =
        moduleBase + kActiveMembersRva;

    players.reserve(
        kMaxPlayers
    );

    for (int slot = 0; slot < kMaxPlayers; ++slot)
    {
        const std::uintptr_t member =
            membersBase +
            (static_cast<std::uintptr_t>(slot) * kPlayerStride);

        const char* rawName =
            reinterpret_cast<const char*>(
                member + kNameOffset
                );

        char nameBuffer[33]{};

        std::memcpy(
            nameBuffer,
            rawName,
            32
        );

        nameBuffer[32] =
            '\0';

        PlayerInfo player{};

        player.slot =
            slot;

        player.occupied =
            nameBuffer[0] != '\0';

        if (!player.occupied)
        {
            player.name =
                "<empty>";

            player.ip =
                "-";

            player.steamId =
                0;

            players.push_back(
                player
            );

            continue;
        }

        player.name =
            nameBuffer;

        player.steamId =
            *reinterpret_cast<const std::uint64_t*>(
                member + kSteamIdOffset
                );

        const auto* ipv4 =
            reinterpret_cast<const std::uint8_t*>(
                member + kIpv4Offset
                );

        player.ip =
            FormatIpv4(
                ipv4
            );

        players.push_back(
            player
        );
    }

    return players;
}
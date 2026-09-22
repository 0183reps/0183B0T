#include "../pch.h"
#include "players.h"

#include <Windows.h>

#include <cstdint>
#include <cstdio>
#include <cstring>

namespace
{
    constexpr int kMaxPlayers = 18;


    // =========================================================
    // Existing player/member table
    // =========================================================

    constexpr std::uintptr_t kActiveMembersRva = 0x1607078;
    constexpr std::uintptr_t kPlayerStride = 0x148;

    constexpr std::uintptr_t kNameOffset = 0x2C;
    constexpr std::uintptr_t kIpv4Offset = 0xE6;
    constexpr std::uintptr_t kSteamIdOffset = 0x110;


    // =========================================================
    // Entity data
    // =========================================================

    constexpr std::uintptr_t kEntityArrayRva = 0x6C43C0;
    constexpr std::uintptr_t kEntityStride = 0x210;

    constexpr std::uintptr_t kEntityIndexOffset = 0xD8;
    constexpr std::uintptr_t kEntityTypeOffset = 0xDC;
    constexpr std::uintptr_t kEntityClientIndexOffset = 0x168;

    constexpr int kLivePlayerEntityType = 1;


    // =========================================================
    // Local client / team
    // =========================================================

    constexpr std::uintptr_t kLocalClientIndexRva = 0x5A6220;

    constexpr std::uintptr_t kTeamArrayRva = 0x6B73D4;
    constexpr std::uintptr_t kTeamStride = 1400;


    // =========================================================
    // gameSession
    //
    // qword_142CBD2D0 is the gameSession object.
    //
    // member:
    //   session + 0x68 + clientIndex * 0x40 = registered
    //   session + 0x70 + clientIndex * 0x40 = identifier
    // =========================================================

    constexpr std::uintptr_t kGameSessionRva = 0x2CBD2D0;

    constexpr std::uintptr_t kSessionMemberBaseOffset = 0x68;
    constexpr std::uintptr_t kSessionMemberStride = 0x40;

    constexpr std::uintptr_t kSessionRegisteredOffset = 0x00;
    constexpr std::uintptr_t kSessionIdentifierOffset = 0x08;


    // =========================================================
    // Host
    //
    // sub_1400E9490(clientNum)
    // =========================================================

    constexpr std::uintptr_t kIsHostRva = 0xE9490;


    // =========================================================
    // SteamFriends013
    //
    // +0x28 = GetFriendRelationship
    // vtable index = 0x28 / 8 = 5
    //
    // k_EFriendRelationshipFriend = 3
    // =========================================================

    constexpr std::size_t kGetFriendRelationshipIndex = 5;
    constexpr int kFriendRelationshipFriend = 3;


    // =========================================================
    // IP
    // =========================================================

    std::string FormatIpv4(
        const std::uint8_t* bytes
    )
    {
        if (
            bytes[0] == 0 &&
            bytes[1] == 0 &&
            bytes[2] == 0 &&
            bytes[3] == 0
            )
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


    // =========================================================
    // Team
    // =========================================================

    int GetTeam(
        const std::uintptr_t moduleBase,
        const int clientIndex
    )
    {
        if (
            clientIndex < 0 ||
            clientIndex >= kMaxPlayers
            )
        {
            return 0;
        }

        return *reinterpret_cast<const int*>(
            moduleBase +
            kTeamArrayRva +
            (
                static_cast<std::uintptr_t>(clientIndex) *
                kTeamStride
                )
            );
    }


    // =========================================================
    // Player slot -> entity -> clientIndex
    //
    // Runtime verified:
    // slot 0 -> client 0
    // slot 1 -> client 1
    // slot 4 -> client 4
    // etc.
    // =========================================================

    int FindClientIndexForSlot(
        const std::uintptr_t moduleBase,
        const int slot
    )
    {
        const std::uintptr_t entityArray =
            moduleBase + kEntityArrayRva;

        for (
            int entityNumber = 0;
            entityNumber < kMaxPlayers;
            ++entityNumber
            )
        {
            const std::uintptr_t entity =
                entityArray +
                (
                    static_cast<std::uintptr_t>(entityNumber) *
                    kEntityStride
                    );

            const int entityType =
                *reinterpret_cast<const int*>(
                    entity + kEntityTypeOffset
                    );

            if (entityType != kLivePlayerEntityType)
            {
                continue;
            }

            const int entityIndex =
                *reinterpret_cast<const int*>(
                    entity + kEntityIndexOffset
                    );

            if (entityIndex != slot)
            {
                continue;
            }

            const int clientIndex =
                *reinterpret_cast<const int*>(
                    entity + kEntityClientIndexOffset
                    );

            if (
                clientIndex < 0 ||
                clientIndex >= kMaxPlayers
                )
            {
                return -1;
            }

            return clientIndex;
        }

        return -1;
    }


    // =========================================================
    // gameSession member
    // =========================================================

    std::uintptr_t GetSessionMemberAddress(
        const std::uintptr_t moduleBase,
        const int clientIndex
    )
    {
        if (
            clientIndex < 0 ||
            clientIndex >= kMaxPlayers
            )
        {
            return 0;
        }

        const std::uintptr_t gameSession =
            moduleBase + kGameSessionRva;

        return
            gameSession +
            kSessionMemberBaseOffset +
            (
                static_cast<std::uintptr_t>(clientIndex) *
                kSessionMemberStride
                );
    }


    bool IsSessionMemberRegistered(
        const std::uintptr_t moduleBase,
        const int clientIndex
    )
    {
        const std::uintptr_t member =
            GetSessionMemberAddress(
                moduleBase,
                clientIndex
            );

        if (member == 0)
        {
            return false;
        }

        return
            *reinterpret_cast<const std::uint8_t*>(
                member + kSessionRegisteredOffset
                ) != 0;
    }


    std::uint64_t GetSessionIdentifier(
        const std::uintptr_t moduleBase,
        const int clientIndex
    )
    {
        const std::uintptr_t member =
            GetSessionMemberAddress(
                moduleBase,
                clientIndex
            );

        if (member == 0)
        {
            return 0;
        }

        if (!IsSessionMemberRegistered(
            moduleBase,
            clientIndex
        ))
        {
            return 0;
        }

        return
            *reinterpret_cast<const std::uint64_t*>(
                member + kSessionIdentifierOffset
                );
    }


    // =========================================================
    // Host
    // =========================================================

    bool IsClientHost(
        const std::uintptr_t moduleBase,
        const int clientIndex
    )
    {
        if (
            clientIndex < 0 ||
            clientIndex >= kMaxPlayers
            )
        {
            return false;
        }

        if (!IsSessionMemberRegistered(
            moduleBase,
            clientIndex
        ))
        {
            return false;
        }

        using IsHostFn =
            int(__fastcall*)(
                unsigned int clientNum
                );

        const auto isHost =
            reinterpret_cast<IsHostFn>(
                moduleBase + kIsHostRva
                );

        return
            isHost(
                static_cast<unsigned int>(clientIndex)
            ) != 0;
    }


    // =========================================================
    // SteamFriends
    // =========================================================

    void* GetSteamFriendsInterface()
    {
        HMODULE steamApi =
            GetModuleHandleW(
                L"steam_api64.dll"
            );

        if (steamApi == nullptr)
        {
            return nullptr;
        }

        using SteamFriendsFn =
            void* (__cdecl*)();

        const auto steamFriendsFn =
            reinterpret_cast<SteamFriendsFn>(
                GetProcAddress(
                    steamApi,
                    "SteamFriends"
                )
                );

        if (steamFriendsFn == nullptr)
        {
            return nullptr;
        }

        return steamFriendsFn();
    }


    bool IsSteamFriend(
        void* steamFriends,
        const std::uint64_t steamId
    )
    {
        if (
            steamFriends == nullptr ||
            steamId == 0
            )
        {
            return false;
        }

        void** vtable =
            *reinterpret_cast<void***>(
                steamFriends
                );

        if (vtable == nullptr)
        {
            return false;
        }

        using GetFriendRelationshipFn =
            int(__fastcall*)(
                void* steamFriends,
                std::uint64_t steamId
                );

        const auto getFriendRelationship =
            reinterpret_cast<GetFriendRelationshipFn>(
                vtable[kGetFriendRelationshipIndex]
                );

        if (getFriendRelationship == nullptr)
        {
            return false;
        }

        const int relationship =
            getFriendRelationship(
                steamFriends,
                steamId
            );

        return
            relationship ==
            kFriendRelationshipFriend;
    }
}


// =============================================================
// GetPlayers
// =============================================================

std::vector<PlayerInfo> GetPlayers()
{
    std::vector<PlayerInfo> players;

    const auto moduleBase =
        reinterpret_cast<std::uintptr_t>(
            GetModuleHandleW(
                L"iw5mp.exe"
            )
            );

    if (moduleBase == 0)
    {
        return players;
    }

    const std::uintptr_t membersBase =
        moduleBase + kActiveMembersRva;


    // ---------------------------------------------------------
    // Local player
    // ---------------------------------------------------------

    const int localClientIndex =
        *reinterpret_cast<const int*>(
            moduleBase + kLocalClientIndexRva
            );

    const int localTeam =
        GetTeam(
            moduleBase,
            localClientIndex
        );


    // ---------------------------------------------------------
    // Steam
    // ---------------------------------------------------------

    void* steamFriends =
        GetSteamFriendsInterface();


    players.reserve(
        kMaxPlayers
    );


    // ---------------------------------------------------------
    // Players
    // ---------------------------------------------------------

    for (
        int slot = 0;
        slot < kMaxPlayers;
        ++slot
        )
    {
        const std::uintptr_t member =
            membersBase +
            (
                static_cast<std::uintptr_t>(slot) *
                kPlayerStride
                );

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

        nameBuffer[32] = '\0';


        PlayerInfo player{};

        player.slot = slot;

        player.clientIndex = -1;
        player.team = 0;

        player.isLocal = false;
        player.isFriendly = false;
        player.isHost = false;
        player.isFriend = false;

        player.occupied =
            nameBuffer[0] != '\0';


        // -----------------------------------------------------
        // Empty slot
        // -----------------------------------------------------

        if (!player.occupied)
        {
            player.name = "<empty>";
            player.ip = "-";
            player.steamId = 0;

            players.push_back(
                player
            );

            continue;
        }


        // -----------------------------------------------------
        // Existing member data
        // -----------------------------------------------------

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


        // -----------------------------------------------------
        // Entity -> client
        // -----------------------------------------------------

        player.clientIndex =
            FindClientIndexForSlot(
                moduleBase,
                slot
            );

        if (player.clientIndex < 0)
        {
            players.push_back(
                player
            );

            continue;
        }


        // -----------------------------------------------------
        // Team
        // -----------------------------------------------------

        player.team =
            GetTeam(
                moduleBase,
                player.clientIndex
            );


        // -----------------------------------------------------
        // You
        // -----------------------------------------------------

        player.isLocal =
            player.clientIndex ==
            localClientIndex;


        // -----------------------------------------------------
        // Friendly / enemy
        // -----------------------------------------------------

        player.isFriendly =
            localTeam != 0 &&
            player.team != 0 &&
            player.team == localTeam;


        // -----------------------------------------------------
        // Host
        // -----------------------------------------------------

        player.isHost =
            IsClientHost(
                moduleBase,
                player.clientIndex
            );


        // -----------------------------------------------------
        // Friend
        //
        // Use the proven session identifier rather than assuming
        // the old member-table Steam ID is the SteamFriends ID.
        // -----------------------------------------------------

        const std::uint64_t sessionSteamId =
            GetSessionIdentifier(
                moduleBase,
                player.clientIndex
            );

        player.isFriend =
            IsSteamFriend(
                steamFriends,
                sessionSteamId
            );


        players.push_back(
            player
        );
    }

    return players;
}
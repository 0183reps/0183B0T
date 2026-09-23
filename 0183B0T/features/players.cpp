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
    // Local Steam identity
    // =========================================================

    constexpr std::uintptr_t kGetLocalSteamIdRva = 0x2D3FB0;


    // =========================================================
    // gameSession
    //
    // qword_142CBD2D0
    //
    // member:
    //   session + 0x68 + index * 0x40 = registered
    //   session + 0x70 + index * 0x40 = identifier
    // =========================================================

    constexpr std::uintptr_t kGameSessionRva = 0x2CBD2D0;

    constexpr std::uintptr_t kSessionMemberBaseOffset = 0x68;
    constexpr std::uintptr_t kSessionMemberStride = 0x40;

    constexpr std::uintptr_t kSessionRegisteredOffset = 0x00;
    constexpr std::uintptr_t kSessionIdentifierOffset = 0x08;


    // =========================================================
    // Session lookup / Host
    //
    // sub_140260750(&gameSession, identifier)
    //   -> exact session member index
    //
    // sub_1400E9490(sessionIndex)
    //   -> game's own host check
    //
    // player +0x110 is proven to contain the same identifier
    // accepted by sub_140260750.
    // =========================================================

    constexpr std::uintptr_t kFindSessionIndexRva = 0x260750;
    constexpr std::uintptr_t kIsHostRva = 0xE9490;


    // =========================================================
    // TEMP host debugging
    //
    // sub_1400E9490 searches this 0x148 table by identifier,
    // then compares the resulting index against:
    //
    // dword_14160FDDC
    // =========================================================

    constexpr std::uintptr_t kOnlineMembersRva = 0x160E018;
    constexpr std::uintptr_t kOnlineMemberStride = 0x148;
    constexpr std::uintptr_t kOnlineIdentifierOffset = 0x110;

    constexpr std::uintptr_t kHostOnlineIndexRva = 0x160FDDC;


    // =========================================================
    // SteamFriends013
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
    // Local Steam identity
    // =========================================================

    std::uint64_t GetLocalSteamId(
        const std::uintptr_t moduleBase
    )
    {
        if (moduleBase == 0)
        {
            return 0;
        }

        using GetLocalSteamIdFn =
            std::uint64_t(__fastcall*)();

        const auto getLocalSteamId =
            reinterpret_cast<GetLocalSteamIdFn>(
                moduleBase + kGetLocalSteamIdRva
                );

        return getLocalSteamId();
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
    // Player slot -> entity -> gameplay clientIndex
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
        const int sessionIndex
    )
    {
        if (
            sessionIndex < 0 ||
            sessionIndex >= kMaxPlayers
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
                static_cast<std::uintptr_t>(sessionIndex) *
                kSessionMemberStride
                );
    }


    bool IsSessionMemberRegistered(
        const std::uintptr_t moduleBase,
        const int sessionIndex
    )
    {
        const std::uintptr_t member =
            GetSessionMemberAddress(
                moduleBase,
                sessionIndex
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
        const int sessionIndex
    )
    {
        const std::uintptr_t member =
            GetSessionMemberAddress(
                moduleBase,
                sessionIndex
            );

        if (member == 0)
        {
            return 0;
        }

        if (!IsSessionMemberRegistered(
            moduleBase,
            sessionIndex
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
    // Player identifier -> gameSession member index
    //
    // sub_140260750(&gameSession, identifier)
    //
    // Returns 0..17 when found.
    // Returns 0xFFFFFFFF when not found.
    // =========================================================

    int FindSessionIndexByIdentifier(
        const std::uintptr_t moduleBase,
        const std::uint64_t identifier
    )
    {
        if (
            moduleBase == 0 ||
            identifier == 0
            )
        {
            return -1;
        }

        using FindSessionIndexFn =
            unsigned int(__fastcall*)(
                std::uintptr_t gameSession,
                std::uint64_t identifier
                );

        const auto findSessionIndex =
            reinterpret_cast<FindSessionIndexFn>(
                moduleBase + kFindSessionIndexRva
                );

        const unsigned int sessionIndex =
            findSessionIndex(
                moduleBase + kGameSessionRva,
                identifier
            );

        if (
            sessionIndex >=
            static_cast<unsigned int>(kMaxPlayers)
            )
        {
            return -1;
        }

        return static_cast<int>(
            sessionIndex
            );
    }


    // =========================================================
    // TEMP DEBUG:
    // Identifier -> online 0x148 table index
    //
    // This mirrors the lookup performed inside sub_1400E9490.
    // =========================================================

    int FindOnlineIndexByIdentifier(
        const std::uintptr_t moduleBase,
        const std::uint64_t identifier
    )
    {
        if (
            moduleBase == 0 ||
            identifier == 0
            )
        {
            return -1;
        }

        const std::uintptr_t onlineMembers =
            moduleBase + kOnlineMembersRva;

        for (
            int index = 0;
            index < kMaxPlayers;
            ++index
            )
        {
            const std::uintptr_t member =
                onlineMembers +
                (
                    static_cast<std::uintptr_t>(index) *
                    kOnlineMemberStride
                    );

            const std::uint8_t status =
                *reinterpret_cast<const std::uint8_t*>(
                    member
                    );

            // First validity condition from sub_1400E9490:
            //
            // test byte ptr [entry], 0FDh
            //
            // The identifier comparison is reached when this
            // test is non-zero.

            if ((status & 0xFD) == 0)
            {
                continue;
            }

            const std::uint64_t memberIdentifier =
                *reinterpret_cast<const std::uint64_t*>(
                    member + kOnlineIdentifierOffset
                    );

            if (memberIdentifier == identifier)
            {
                return index;
            }
        }

        return -1;
    }


    // =========================================================
    // Host
    // =========================================================

    bool IsSessionClientHost(
        const std::uintptr_t moduleBase,
        const int sessionIndex
    )
    {
        if (
            sessionIndex < 0 ||
            sessionIndex >= kMaxPlayers
            )
        {
            return false;
        }

        if (!IsSessionMemberRegistered(
            moduleBase,
            sessionIndex
        ))
        {
            return false;
        }

        using IsHostFn =
            int(__fastcall*)(
                unsigned int sessionClientNum
                );

        const auto isHost =
            reinterpret_cast<IsHostFn>(
                moduleBase + kIsHostRva
                );

        return
            isHost(
                static_cast<unsigned int>(sessionIndex)
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
    // Local gameplay state
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
    // Local identity
    // ---------------------------------------------------------

    const std::uint64_t localSteamId =
        GetLocalSteamId(
            moduleBase
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
        // Existing player-table data
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
        // Entity -> gameplay client
        // -----------------------------------------------------

        player.clientIndex =
            FindClientIndexForSlot(
                moduleBase,
                slot
            );


        // -----------------------------------------------------
        // Existing session identity
        //
        // Kept unchanged for [You] / [FRIEND].
        // -----------------------------------------------------

        const std::uint64_t sessionSteamId =
            GetSessionIdentifier(
                moduleBase,
                slot
            );


        // -----------------------------------------------------
        // You
        // -----------------------------------------------------

        player.isLocal =
            localSteamId != 0 &&
            sessionSteamId != 0 &&
            sessionSteamId == localSteamId;


        // -----------------------------------------------------
        // Host + temporary debugging
        //
        // S = gameSession index resolved from player +0x110
        // O = online 0x148 table index
        // H = dword_14160FDDC
        // R = result returned by sub_1400E9490
        // -----------------------------------------------------

        const int hostSessionIndex =
            FindSessionIndexByIdentifier(
                moduleBase,
                player.steamId
            );

        const int onlineIndex =
            FindOnlineIndexByIdentifier(
                moduleBase,
                player.steamId
            );

        const int hostOnlineIndex =
            *reinterpret_cast<const int*>(
                moduleBase + kHostOnlineIndexRva
                );

        player.isHost =
            hostSessionIndex >= 0 &&
            IsSessionClientHost(
                moduleBase,
                hostSessionIndex
            );


        // -----------------------------------------------------
        // TEMPORARY visible host diagnostics
        // -----------------------------------------------------

        char hostDebug[128]{};

        std::snprintf(
            hostDebug,
            sizeof(hostDebug),
            " [DBG S=%d O=%d H=%d R=%d]",
            hostSessionIndex,
            onlineIndex,
            hostOnlineIndex,
            player.isHost ? 1 : 0
        );

        player.name += hostDebug;


        // -----------------------------------------------------
        // Friend
        // -----------------------------------------------------

        player.isFriend =
            IsSteamFriend(
                steamFriends,
                sessionSteamId
            );


        // -----------------------------------------------------
        // No gameplay entity yet
        // -----------------------------------------------------

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
        // Friendly / enemy
        // -----------------------------------------------------

        player.isFriendly =
            localTeam != 0 &&
            player.team != 0 &&
            player.team == localTeam;


        players.push_back(
            player
        );
    }


    return players;
}
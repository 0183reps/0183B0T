#pragma once

#include <cstdint>
#include <string>
#include <vector>

struct PlayerInfo
{
    int slot;
    std::string name;
    std::string ip;
    std::uint64_t steamId;
    bool occupied;

    int clientIndex;
    int team;

    bool isLocal;
    bool isFriendly;
    bool isHost;
    bool isFriend;
};

std::vector<PlayerInfo> GetPlayers();
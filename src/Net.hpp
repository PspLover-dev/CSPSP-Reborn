#pragma once

#include "Types.hpp"

#include <string>
#include <vector>

enum class NetRole { Offline, Host, Client };

enum class NetMsg : uint8_t {
    Join = 1,
    Welcome = 2,
    Lobby = 3,
    Start = 4,
    Input = 5,
    Snapshot = 6,
    Leave = 7,
    TeamPick = 8
};

struct NetPeer {
    unsigned char mac[6]{};
    char name[12]{};
    uint8_t id = 0;
    uint8_t team = 0;
    bool connected = false;
};

struct NetInput {
    uint8_t id = 0;
    int8_t ax = 0;
    int8_t ay = 0;
    uint8_t buttons = 0;
    uint8_t weapon = 0;
    uint16_t seq = 0;
};

struct NetActorSnap {
    uint8_t id = 0;
    uint8_t team = 0;
    uint8_t hp = 0;
    uint8_t weapon = 0;
    uint16_t x = 0;
    uint16_t y = 0;
    int16_t angle = 0;
    uint8_t flags = 0;
};

struct NetSnapshot {
    uint16_t tick = 0;
    uint8_t count = 0;
    uint8_t wave = 0;
    uint8_t extra = 0;
    uint8_t teamCount = 2;
    uint8_t baseHp[kMaxTeams] = {255, 255, 255, 255, 255, 255};
    NetActorSnap actors[kMaxPlayers]{};
};

class NetSession {
public:
    ~NetSession() { shutdown(); }

    bool init();
    void shutdown();

    bool host(const std::string& partyName);
    bool join(const std::string& partyName);
    bool scan(std::vector<std::string>& parties);
    void leave();

    void pump();
    void sendInput(const NetInput& in);
    void sendStart(uint16_t mapIndex, uint8_t mode, uint8_t difficulty, uint8_t teamCount = 2);
    void sendLobbyInfo(uint16_t mapIndex, uint8_t mode, uint8_t difficulty, uint8_t teamCount = 2);
    void sendTeamPick(uint8_t team);
    void setLocalTeam(uint8_t team);
    void sendSnapshot(const NetSnapshot& snap);
    bool takeSnapshot(NetSnapshot& snap);
    bool takeInput(NetInput& in);
    bool takeStart(uint16_t& mapIndex, uint8_t& mode, uint8_t& difficulty);
    bool takeWelcome(uint8_t& myId, uint8_t& team, uint16_t& mapIndex, uint8_t& mode, uint8_t& difficulty);

    NetRole role() const { return role_; }
    int peerCount() const;
    const NetPeer* peers() const { return peers_; }
    const std::string& lastError() const { return error_; }
    const std::string& partyName() const { return party_; }
    bool connected() const { return connected_; }
    uint8_t localId() const { return localId_; }
    uint8_t gameMode() const { return gameMode_; }
    uint8_t gameDifficulty() const { return gameDifficulty_; }
    uint8_t teamCount() const { return teamCount_; }
    uint16_t lobbyMap() const { return lobbyMap_; }

    static constexpr uint8_t kBtnFire = 1;
    static constexpr uint8_t kBtnReload = 2;
    static constexpr uint8_t kBtnWeapon = 4;

private:
    bool waitState(int want, int timeoutMs);
    bool openPdp();
    void sendTo(const unsigned char* mac, const void* data, int len);
    void broadcast(const void* data, int len);
    int findPeerByMac(const unsigned char* mac) const;
    int allocPeer(const unsigned char* mac, const char* name, uint8_t team);

    NetRole role_ = NetRole::Offline;
    bool inited_ = false;
    bool connected_ = false;
    int pdp_ = -1;
    unsigned char mac_[8]{};
    unsigned char broadcastMac_[6]{0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    NetPeer peers_[kMaxPlayers]{};
    uint8_t localId_ = 0;
    std::string party_;
    std::string error_;
    NetSnapshot pendingSnap_{};
    bool hasSnap_ = false;
    NetInput pendingInputs_[8]{};
    int inputCount_ = 0;
    bool hasStart_ = false;
    uint16_t startMap_ = 0;
    uint8_t startMode_ = 0;
    uint8_t startDiff_ = 1;
    bool hasWelcome_ = false;
    uint8_t welcomeId_ = 0;
    uint8_t welcomeTeam_ = 0;
    uint16_t welcomeMap_ = 0;
    uint8_t gameMode_ = 0;
    uint8_t gameDifficulty_ = 1;
    uint8_t teamCount_ = 2;
    uint16_t lobbyMap_ = 0;
    uint16_t port_ = 0x309;
};

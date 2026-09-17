#include "Net.hpp"

#include <algorithm>
#include <cstdio>
#include <cstring>

#ifdef PSP
#include <pspkernel.h>
#include <pspnet.h>
#include <pspnet_adhoc.h>
#include <pspnet_adhocctl.h>
#include <psputility.h>
#include <psputility_netmodules.h>
#include <pspwlan.h>
#include <SDL.h>
#endif

bool NetSession::init() {
#ifdef PSP
    if (inited_) {
        return true;
    }
    if (sceWlanGetSwitchState() == 0) {
        error_ = "WLAN switch is off";
        return false;
    }
    sceUtilityLoadNetModule(PSP_NET_MODULE_COMMON);
    sceUtilityLoadNetModule(PSP_NET_MODULE_ADHOC);
    int err = sceNetInit(128 * 1024, 42, 4 * 1024, 42, 4 * 1024);
    if (err < 0) {
        error_ = "sceNetInit failed";
        return false;
    }
    err = sceNetAdhocInit();
    if (err < 0) {
        error_ = "sceNetAdhocInit failed";
        return false;
    }
    productStruct product{};
    product.unknown = 0;
    std::memcpy(product.product, "CSPSP001", 9);
    err = sceNetAdhocctlInit(0x2000, 0x30, &product);
    if (err < 0) {
        error_ = "sceNetAdhocctlInit failed";
        return false;
    }
    sceWlanGetEtherAddr(mac_);
    inited_ = true;
    return true;
#else
    error_ = "Adhoc is PSP only (use Solo / Editor on PC)";
    return false;
#endif
}

void NetSession::shutdown() {
    leave();
#ifdef PSP
    if (!inited_) {
        return;
    }
    sceNetAdhocctlTerm();
    sceNetAdhocTerm();
    sceNetTerm();
    sceUtilityUnloadNetModule(PSP_NET_MODULE_ADHOC);
    sceUtilityUnloadNetModule(PSP_NET_MODULE_COMMON);
    inited_ = false;
#endif
}

bool NetSession::waitState(int want, int timeoutMs) {
#ifdef PSP
    const Uint32 start = SDL_GetTicks();
    while (SDL_GetTicks() - start < static_cast<Uint32>(timeoutMs)) {
        int state = 0;
        sceNetAdhocctlGetState(&state);
        if (state == want) {
            return true;
        }
        SDL_Delay(16);
    }
    return false;
#else
    (void)want;
    (void)timeoutMs;
    return false;
#endif
}

bool NetSession::openPdp() {
#ifdef PSP
    pdp_ = sceNetAdhocPdpCreate(mac_, port_, 0x1000, 0);
    if (pdp_ < 0) {
        error_ = "PDP create failed";
        return false;
    }
    return true;
#else
    return false;
#endif
}

bool NetSession::host(const std::string& partyName) {
    if (!init()) {
        return false;
    }
#ifdef PSP
    party_ = partyName.substr(0, 8);
    char name[9]{};
    std::snprintf(name, sizeof(name), "%s", party_.c_str());
    if (sceNetAdhocctlCreate(name) < 0) {
        error_ = "Create party failed";
        return false;
    }
    if (!waitState(1, 8000)) {
        error_ = "Host timeout";
        return false;
    }
    if (!openPdp()) {
        return false;
    }
    role_ = NetRole::Host;
    connected_ = true;
    localId_ = 0;
    peers_[0].connected = true;
    peers_[0].id = 0;
    peers_[0].team = 0;
    std::memcpy(peers_[0].mac, mac_, 6);
    std::snprintf(peers_[0].name, sizeof(peers_[0].name), "HOST");
    return true;
#else
    (void)partyName;
    return false;
#endif
}

bool NetSession::join(const std::string& partyName) {
    if (!init()) {
        return false;
    }
#ifdef PSP
    party_ = partyName.substr(0, 8);
    char name[9]{};
    std::snprintf(name, sizeof(name), "%s", party_.c_str());
    if (sceNetAdhocctlConnect(name) < 0) {
        error_ = "Join failed";
        return false;
    }
    if (!waitState(1, 8000)) {
        error_ = "Join timeout";
        return false;
    }
    if (!openPdp()) {
        return false;
    }
    role_ = NetRole::Client;
    connected_ = true;
    uint8_t pkt[32]{};
    pkt[0] = static_cast<uint8_t>(NetMsg::Join);
    std::memcpy(pkt + 4, "PLAYER", 6);
    broadcast(pkt, 16);
    return true;
#else
    (void)partyName;
    return false;
#endif
}

bool NetSession::scan(std::vector<std::string>& parties) {
    parties.clear();
    if (!init()) {
        return false;
    }
#ifdef PSP
    if (sceNetAdhocctlScan() < 0) {
        error_ = "Scan failed";
        return false;
    }
    waitState(1, 3000);
    int length = 0;
    sceNetAdhocctlGetScanInfo(&length, nullptr);
    if (length <= 0) {
        return true;
    }
    std::vector<char> buf(static_cast<size_t>(length));
    if (sceNetAdhocctlGetScanInfo(&length, buf.data()) < 0) {
        return false;
    }
    auto* info = reinterpret_cast<SceNetAdhocctlScanInfo*>(buf.data());
    while (info) {
        char n[9]{};
        std::memcpy(n, info->name, 8);
        if (n[0]) {
            parties.emplace_back(n);
        }
        info = info->next;
    }
    return true;
#else
    return false;
#endif
}

void NetSession::leave() {
#ifdef PSP
    if (pdp_ >= 0) {
        sceNetAdhocPdpDelete(pdp_, 0);
        pdp_ = -1;
    }
    if (connected_) {
        sceNetAdhocctlDisconnect();
    }
#endif
    connected_ = false;
    role_ = NetRole::Offline;
    for (auto& p : peers_) {
        p = NetPeer{};
    }
}

int NetSession::peerCount() const {
    int n = 0;
    for (const auto& p : peers_) {
        if (p.connected) {
            ++n;
        }
    }
    return n;
}

void NetSession::sendTo(const unsigned char* mac, const void* data, int len) {
#ifdef PSP
    if (pdp_ < 0) {
        return;
    }
    sceNetAdhocPdpSend(pdp_, const_cast<unsigned char*>(mac), port_, const_cast<void*>(data),
                       static_cast<unsigned int>(len), 0, 1);
#else
    (void)mac;
    (void)data;
    (void)len;
#endif
}

void NetSession::broadcast(const void* data, int len) {
    sendTo(broadcastMac_, data, len);
}

int NetSession::findPeerByMac(const unsigned char* mac) const {
    for (int i = 0; i < kMaxPlayers; ++i) {
        if (peers_[i].connected && std::memcmp(peers_[i].mac, mac, 6) == 0) {
            return i;
        }
    }
    return -1;
}

int NetSession::allocPeer(const unsigned char* mac, const char* name, uint8_t team) {
    for (int i = 1; i < kMaxPlayers; ++i) {
        if (!peers_[i].connected) {
            peers_[i].connected = true;
            peers_[i].id = static_cast<uint8_t>(i);
            peers_[i].team = team;
            std::memcpy(peers_[i].mac, mac, 6);
            std::snprintf(peers_[i].name, sizeof(peers_[i].name), "%s", name);
            return i;
        }
    }
    return -1;
}

void NetSession::sendInput(const NetInput& in) {
    uint8_t pkt[16]{};
    pkt[0] = static_cast<uint8_t>(NetMsg::Input);
    pkt[1] = in.id;
    pkt[2] = static_cast<uint8_t>(in.seq & 0xFF);
    pkt[3] = static_cast<uint8_t>(in.seq >> 8);
    pkt[4] = static_cast<uint8_t>(in.ax);
    pkt[5] = static_cast<uint8_t>(in.ay);
    pkt[6] = in.buttons;
    pkt[7] = in.weapon;
    if (role_ == NetRole::Host) {
        broadcast(pkt, 8);
    } else {
        broadcast(pkt, 8);
    }
}

void NetSession::setLocalTeam(uint8_t team) {
    if (localId_ < kMaxPlayers) {
        peers_[localId_].team = team % static_cast<uint8_t>(kMaxTeams);
    }
}

void NetSession::sendTeamPick(uint8_t team) {
    uint8_t pkt[4]{};
    pkt[0] = static_cast<uint8_t>(NetMsg::TeamPick);
    pkt[1] = team % static_cast<uint8_t>(kMaxTeams);
    if (role_ == NetRole::Host) {
        setLocalTeam(pkt[1]);
        return;
    }
    broadcast(pkt, 4);
}

void NetSession::sendSnapshot(const NetSnapshot& snap) {
    uint8_t pkt[13 + kMaxPlayers * 11]{};
    pkt[0] = static_cast<uint8_t>(NetMsg::Snapshot);
    pkt[1] = snap.count;
    pkt[2] = static_cast<uint8_t>(snap.tick & 0xFF);
    pkt[3] = static_cast<uint8_t>(snap.tick >> 8);
    pkt[4] = snap.wave;
    pkt[5] = snap.extra;
    pkt[6] = snap.teamCount;
    for (int i = 0; i < kMaxTeams; ++i) {
        pkt[7 + i] = snap.baseHp[i];
    }
    uint8_t* p = pkt + 13;
    const int count = std::min(static_cast<int>(snap.count), kMaxPlayers);
    for (int i = 0; i < count; ++i) {
        const auto& a = snap.actors[i];
        p[0] = a.id;
        p[1] = a.team;
        p[2] = a.hp;
        p[3] = a.weapon;
        p[4] = static_cast<uint8_t>(a.x & 0xFF);
        p[5] = static_cast<uint8_t>(a.x >> 8);
        p[6] = static_cast<uint8_t>(a.y & 0xFF);
        p[7] = static_cast<uint8_t>(a.y >> 8);
        p[8] = static_cast<uint8_t>(a.angle & 0xFF);
        p[9] = static_cast<uint8_t>(a.angle >> 8);
        p[10] = a.flags;
        p += 11;
    }
    broadcast(pkt, 13 + count * 11);
}

void NetSession::sendStart(uint16_t mapIndex, uint8_t mode, uint8_t difficulty, uint8_t teamCount) {
    uint8_t pkt[8]{};
    pkt[0] = static_cast<uint8_t>(NetMsg::Start);
    pkt[2] = static_cast<uint8_t>(mapIndex & 0xFF);
    pkt[3] = static_cast<uint8_t>(mapIndex >> 8);
    pkt[4] = static_cast<uint8_t>(peerCount());
    pkt[5] = mode;
    pkt[6] = difficulty;
    pkt[7] = std::max<uint8_t>(2, std::min<uint8_t>(kMaxTeams, teamCount));
    gameMode_ = mode;
    gameDifficulty_ = difficulty;
    teamCount_ = pkt[7];
    lobbyMap_ = mapIndex;
    broadcast(pkt, 8);
}

void NetSession::sendLobbyInfo(uint16_t mapIndex, uint8_t mode, uint8_t difficulty, uint8_t teamCount) {
    uint8_t pkt[8]{};
    pkt[0] = static_cast<uint8_t>(NetMsg::Lobby);
    pkt[1] = mode;
    pkt[2] = static_cast<uint8_t>(mapIndex & 0xFF);
    pkt[3] = static_cast<uint8_t>(mapIndex >> 8);
    pkt[4] = difficulty;
    pkt[5] = std::max<uint8_t>(2, std::min<uint8_t>(kMaxTeams, teamCount));
    gameMode_ = mode;
    gameDifficulty_ = difficulty;
    teamCount_ = pkt[5];
    lobbyMap_ = mapIndex;
    broadcast(pkt, 8);
}

bool NetSession::takeSnapshot(NetSnapshot& snap) {
    if (!hasSnap_) {
        return false;
    }
    snap = pendingSnap_;
    hasSnap_ = false;
    return true;
}

bool NetSession::takeInput(NetInput& in) {
    if (inputCount_ <= 0) {
        return false;
    }
    in = pendingInputs_[0];
    for (int i = 1; i < inputCount_; ++i) {
        pendingInputs_[i - 1] = pendingInputs_[i];
    }
    --inputCount_;
    return true;
}

bool NetSession::takeStart(uint16_t& mapIndex, uint8_t& mode, uint8_t& difficulty) {
    if (!hasStart_) {
        return false;
    }
    mapIndex = startMap_;
    mode = startMode_;
    difficulty = startDiff_;
    hasStart_ = false;
    return true;
}

bool NetSession::takeWelcome(uint8_t& myId, uint8_t& team, uint16_t& mapIndex, uint8_t& mode, uint8_t& difficulty) {
    if (!hasWelcome_) {
        return false;
    }
    myId = welcomeId_;
    team = welcomeTeam_;
    mapIndex = welcomeMap_;
    mode = gameMode_;
    difficulty = gameDifficulty_;
    hasWelcome_ = false;
    return true;
}

void NetSession::pump() {
#ifdef PSP
    if (pdp_ < 0) {
        return;
    }
    for (int n = 0; n < 8; ++n) {
        unsigned char src[6];
        unsigned short port = 0;
        uint8_t buf[1024];
        int len = static_cast<int>(sizeof(buf));
        const int rec = sceNetAdhocPdpRecv(pdp_, src, &port, buf, &len, 0, 1);
        if (rec < 0 || len <= 0) {
            break;
        }
        const auto type = static_cast<NetMsg>(buf[0]);
        if (type == NetMsg::Join && role_ == NetRole::Host) {
            char nick[12]{};
            std::memcpy(nick, buf + 4, 8);
            uint8_t team = static_cast<uint8_t>(peerCount() % std::max<int>(2, teamCount_));
            if (len > 2 && buf[2] == 1 && buf[1] < kMaxTeams) {
                team = buf[1];
            }
            const int id = allocPeer(src, nick[0] ? nick : "PLAYER", team);
            if (id < 0) {
                continue;
            }
            uint8_t welcome[16]{};
            welcome[0] = static_cast<uint8_t>(NetMsg::Welcome);
            welcome[1] = static_cast<uint8_t>(id);
            welcome[2] = team;
            welcome[3] = static_cast<uint8_t>(lobbyMap_ & 0xFF);
            welcome[4] = static_cast<uint8_t>(lobbyMap_ >> 8);
            welcome[5] = gameMode_;
            welcome[6] = gameDifficulty_;
            welcome[7] = teamCount_;
            sendTo(src, welcome, 8);
            uint8_t lobby[8]{};
            lobby[0] = static_cast<uint8_t>(NetMsg::Lobby);
            lobby[1] = gameMode_;
            lobby[2] = static_cast<uint8_t>(lobbyMap_ & 0xFF);
            lobby[3] = static_cast<uint8_t>(lobbyMap_ >> 8);
            lobby[4] = gameDifficulty_;
            lobby[5] = teamCount_;
            broadcast(lobby, 8);
        } else if (type == NetMsg::Welcome && role_ == NetRole::Client) {
            hasWelcome_ = true;
            welcomeId_ = buf[1];
            welcomeTeam_ = buf[2];
            welcomeMap_ = static_cast<uint16_t>(buf[3] | (buf[4] << 8));
            gameMode_ = buf[5];
            gameDifficulty_ = buf[6];
            if (len > 7 && buf[7] >= 2) {
                teamCount_ = std::min<uint8_t>(kMaxTeams, buf[7]);
            }
            lobbyMap_ = welcomeMap_;
            localId_ = welcomeId_;
        } else if (type == NetMsg::Lobby && role_ == NetRole::Client) {
            gameMode_ = buf[1];
            lobbyMap_ = static_cast<uint16_t>(buf[2] | (buf[3] << 8));
            gameDifficulty_ = buf[4];
            if (len > 5 && buf[5] >= 2) {
                teamCount_ = std::min<uint8_t>(kMaxTeams, buf[5]);
            }
        } else if (type == NetMsg::Start) {
            hasStart_ = true;
            startMap_ = static_cast<uint16_t>(buf[2] | (buf[3] << 8));
            startMode_ = buf[5];
            startDiff_ = buf[6];
            gameMode_ = startMode_;
            gameDifficulty_ = startDiff_;
            if (len > 7 && buf[7] >= 2) {
                teamCount_ = std::min<uint8_t>(kMaxTeams, buf[7]);
            }
        } else if (type == NetMsg::TeamPick && role_ == NetRole::Host) {
            const int id = findPeerByMac(src);
            if (id >= 0 && len > 1) {
                peers_[id].team = buf[1] % static_cast<uint8_t>(kMaxTeams);
            }
        } else if (type == NetMsg::Input && inputCount_ < 8) {
            NetInput in{};
            in.id = buf[1];
            in.seq = static_cast<uint16_t>(buf[2] | (buf[3] << 8));
            in.ax = static_cast<int8_t>(buf[4]);
            in.ay = static_cast<int8_t>(buf[5]);
            in.buttons = buf[6];
            in.weapon = buf[7];
            pendingInputs_[inputCount_++] = in;
        } else if (type == NetMsg::Snapshot) {
            pendingSnap_.tick = static_cast<uint16_t>(buf[2] | (buf[3] << 8));
            pendingSnap_.count = buf[1];
            pendingSnap_.wave = buf[4];
            pendingSnap_.extra = buf[5];
            pendingSnap_.teamCount = 2;
            for (int i = 0; i < kMaxTeams; ++i) {
                pendingSnap_.baseHp[i] = 255;
            }
            const int count = std::min(static_cast<int>(pendingSnap_.count), kMaxPlayers);
            const uint8_t* p = buf + 6;
            if (len >= 13) {
                pendingSnap_.teamCount = std::max<uint8_t>(2, buf[6]);
                for (int i = 0; i < kMaxTeams; ++i) {
                    pendingSnap_.baseHp[i] = buf[7 + i];
                }
                p = buf + 13;
            } else if (len >= 8) {
                pendingSnap_.baseHp[0] = buf[6];
                pendingSnap_.baseHp[1] = buf[7];
                p = buf + 8;
            }
            for (int i = 0; i < count; ++i) {
                auto& a = pendingSnap_.actors[i];
                a.id = p[0];
                a.team = p[1];
                a.hp = p[2];
                a.weapon = p[3];
                a.x = static_cast<uint16_t>(p[4] | (p[5] << 8));
                a.y = static_cast<uint16_t>(p[6] | (p[7] << 8));
                a.angle = static_cast<int16_t>(p[8] | (p[9] << 8));
                a.flags = p[10];
                p += 11;
            }
            hasSnap_ = true;
        }
    }

    if (role_ == NetRole::Host) {
        uint8_t pkt[4 + kMaxPlayers * 11]{};
        pkt[0] = static_cast<uint8_t>(NetMsg::Snapshot);
        // filled by World via sendSnapshot? keep pump receive-only for host snaps
        (void)pkt;
    }
#else
#endif
}

#pragma once

#include "../../vendor/enet/enet.h"
#include <net/CNetServer.h>
#include <map>

class CNetServerPeerManager {
public:
    struct SPeerRecord
    {
        NetServerPlayerID id;
        unsigned short    usBitStreamVersion = 0;
        SFixedString<32>  strSerial;
        SFixedString<64>  strExtra;
        SFixedString<32>  strVersion;
    };

    CNetServerPeerManager() = default;
    ~CNetServerPeerManager() = default;

    NetServerPlayerID Register(ENetPeer* pPeer);

    void Unregister(ENetPeer* pPeer);

    void Clear();

    ENetPeer* Find(const NetServerPlayerID& id) const;

    static SPeerRecord* GetRecord(ENetPeer* pPeer);
    SPeerRecord*         FindRecord(const NetServerPlayerID& id) const;

    template <typename Fn>
    void ForEachPeer(Fn fn) const
    {
        for (const auto& pair : m_PeerById)
            fn(pair.second);
    }

    size_t GetPeerCount() const { return m_PeerById.size(); }

private:
    static NetServerPlayerID MakePlayerId(const ENetPeer* pPeer);

    std::map<NetServerPlayerID, ENetPeer*> m_PeerById;
};

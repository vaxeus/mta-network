#include "StdInc.h"
#include "CNetServerPeerManager.h"

NetServerPlayerID CNetServerPeerManager::MakePlayerId(const ENetPeer* pPeer)
{
    return NetServerPlayerID(pPeer->address.host, pPeer->address.port);
}

NetServerPlayerID CNetServerPeerManager::Register(ENetPeer* pPeer)
{
    SPeerRecord* pRecord = new SPeerRecord();
    pRecord->id = MakePlayerId(pPeer);

    pPeer->data = pRecord;
    m_PeerById[pRecord->id] = pPeer;
    return pRecord->id;
}

void CNetServerPeerManager::Unregister(ENetPeer* pPeer)
{
    if (pPeer == nullptr)
        return;

    SPeerRecord* pRecord = GetRecord(pPeer);
    if (pRecord == nullptr)
        return;

    m_PeerById.erase(pRecord->id);
    delete pRecord;
    pPeer->data = nullptr;
}

void CNetServerPeerManager::Clear()
{
    for (auto& pair : m_PeerById)
        delete GetRecord(pair.second);
    m_PeerById.clear();
}

ENetPeer* CNetServerPeerManager::Find(const NetServerPlayerID& id) const
{
    auto it = m_PeerById.find(id);
    return it != m_PeerById.end() ? it->second : nullptr;
}

CNetServerPeerManager::SPeerRecord* CNetServerPeerManager::GetRecord(ENetPeer* pPeer)
{
    return pPeer != nullptr ? static_cast<SPeerRecord*>(pPeer->data) : nullptr;
}

CNetServerPeerManager::SPeerRecord* CNetServerPeerManager::FindRecord(const NetServerPlayerID& id) const
{
    return GetRecord(Find(id));
}

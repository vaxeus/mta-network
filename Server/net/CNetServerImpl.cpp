#include "StdInc.h"
#include "CNetServerImpl.h"
#include "net/Packets.h"

static void WriteServerNetworkLog(const char* szFormat, ...)
{
    char    buffer[1024];
    va_list vlist;
    va_start(vlist, szFormat);
    VSNPRINTF(buffer, sizeof(buffer), szFormat, vlist);
    va_end(vlist);

    SharedUtil::MakeSureDirExists("logs/network.log");
    SString strMessage("%s - [DEBUG] %s", *SharedUtil::GetLocalTimeString(), buffer);
    SharedUtil::FileAppend("logs/network.log", strMessage + "\n");
}

CNetServerImpl::CNetServerImpl()
    : m_pHost(nullptr),
    m_pfnPacketHandler(nullptr),
    m_usMaxConnections(0),
    m_uiUpdateCycleDatagrams(0),
    m_uiUpdateCycleMessages(0),
    m_bNetSimEnabled(false),
    m_iNetSimPacketLoss(0),
    m_iNetSimExtraPing(0),
    m_iNetSimExtraPingVariance(0),
    m_iNetSimKBPSLimit(0)
{
    memset(m_PacketStats, 0, sizeof(m_PacketStats));
    enet_initialize();
}

CNetServerImpl::~CNetServerImpl()
{
    StopNetwork();

    for (auto& pair : m_DownloadManagers)
        delete pair.second;
    m_DownloadManagers.clear();

    enet_deinitialize();
}

bool CNetServerImpl::StartNetwork(const char* szIP, unsigned short usServerPort, unsigned int uiAllowedPlayers, const char* szServerName)
{
    StopNetwork();

    ENetAddress address;
    address.port = usServerPort;
    if (szIP != nullptr && szIP[0] != '\0')
    {
        if (enet_address_set_host(&address, szIP) != 0)
            return false;
    }
    else
    {
        address.host = ENET_HOST_ANY;
    }

    m_pHost = enet_host_create(&address, uiAllowedPlayers, NET_CHANNEL_COUNT, 0, 0);
    if (m_pHost == nullptr)
        return false;

    m_usMaxConnections = static_cast<unsigned short>(uiAllowedPlayers);

    if (m_iNetSimKBPSLimit > 0)
        enet_host_bandwidth_limit(m_pHost, 0, static_cast<enet_uint32>(m_iNetSimKBPSLimit) * 1024u);

    return true;
}

void CNetServerImpl::StopNetwork()
{
    for (SDelayedPacket& delayed : m_DelayedPackets)
        enet_packet_destroy(delayed.pPacket);
    m_DelayedPackets.clear();

    if (m_pHost != nullptr)
    {
        enet_host_destroy(m_pHost);
        m_pHost = nullptr;
    }

    m_PeerManager.Clear();
}

void CNetServerImpl::HandleConnect(ENetPeer* pPeer)
{
    const enet_uint32 h = pPeer->address.host;
    WriteServerNetworkLog("CNetServerImpl: ENet CONNECT event from %u.%u.%u.%u:%u", h & 0xFF, (h >> 8) & 0xFF, (h >> 16) & 0xFF, (h >> 24) & 0xFF, pPeer->address.port);

    if (m_usMaxConnections != 0 && m_PeerManager.GetPeerCount() >= m_usMaxConnections)
    {
        WriteServerNetworkLog("CNetServerImpl: rejecting connect, at max connections (%u/%u)", m_PeerManager.GetPeerCount(), m_usMaxConnections);
        enet_peer_disconnect_now(pPeer, 0);
        return;
    }

    NetServerPlayerID id = m_PeerManager.Register(pPeer);

    if (m_pfnPacketHandler != nullptr)
    {
        CNetBitStream* pJoinBitStream = new CNetBitStream();
        WriteServerNetworkLog("CNetServerImpl: dispatching synthetic PACKET_ID_PLAYER_JOIN to packet handler");
        bool bHandled = m_pfnPacketHandler(PACKET_ID_PLAYER_JOIN, id, pJoinBitStream, nullptr);
        pJoinBitStream->Release();
        WriteServerNetworkLog("CNetServerImpl: PACKET_ID_PLAYER_JOIN handler returned %s", bHandled ? "true" : "false");
    }
    else
    {
        WriteServerNetworkLog("CNetServerImpl: no packet handler registered, PACKET_ID_PLAYER_JOIN dropped");
    }
}

void CNetServerImpl::HandleReceive(const ENetEvent& event)
{
    CNetServerPeerManager::SPeerRecord* pRecord = CNetServerPeerManager::GetRecord(event.peer);
    if (pRecord == nullptr)
        return;

    const unsigned char  ucPacketID = ExtractNetPacketId(event.packet);
    const unsigned char* pPayload = GetNetPacketPayload(event.packet);
    const size_t          uiPayloadLength = GetNetPacketPayloadLength(event.packet);

    m_PacketStats[CNetServer::STATS_INCOMING_TRAFFIC][ucPacketID].iCount++;
    m_PacketStats[CNetServer::STATS_INCOMING_TRAFFIC][ucPacketID].iTotalBytes += static_cast<int>(uiPayloadLength);
    m_uiUpdateCycleMessages++;

    if (m_pfnPacketHandler != nullptr)
    {
        CNetBitStream* pBitStream = new CNetBitStream(pPayload, uiPayloadLength, pRecord->usBitStreamVersion);
        SNetExtraInfo* pExtraInfo = new SNetExtraInfo();
        pExtraInfo->m_bHasPing = true;
        pExtraInfo->m_uiPing = event.peer->roundTripTime;
        m_pfnPacketHandler(ucPacketID, pRecord->id, pBitStream, pExtraInfo);
        pExtraInfo->Release();
        pBitStream->Release();
    }
}

void CNetServerImpl::HandleDisconnect(ENetPeer* pPeer, enet_uint32 uiEventData)
{
    WriteServerNetworkLog("CNetServerImpl: ENet DISCONNECT event (data=%u)", uiEventData);

    CNetServerPeerManager::SPeerRecord* pRecord = CNetServerPeerManager::GetRecord(pPeer);
    if (pRecord != nullptr && m_pfnPacketHandler != nullptr)
    {
        CNetBitStream* pBitStream = new CNetBitStream();
        if (uiEventData == 0)
        {
            WriteServerNetworkLog("CNetServerImpl: dispatching synthetic PACKET_ID_PLAYER_TIMEOUT to packet handler");
            m_pfnPacketHandler(PACKET_ID_PLAYER_TIMEOUT, pRecord->id, pBitStream, nullptr);
        }
        else
        {
            WriteServerNetworkLog("CNetServerImpl: dispatching synthetic PACKET_ID_PLAYER_QUIT to packet handler");
            m_pfnPacketHandler(PACKET_ID_PLAYER_QUIT, pRecord->id, pBitStream, nullptr);
        }
        pBitStream->Release();
    }

    m_PeerManager.Unregister(pPeer);
}

void CNetServerImpl::DoPulse()
{
    if (m_pHost == nullptr)
        return;

    FlushDelayedPackets();

    ENetEvent event;
    while (m_pHost != nullptr && enet_host_service(m_pHost, &event, 0) > 0)
    {
        m_uiUpdateCycleDatagrams++;

        switch (event.type)
        {
            case ENET_EVENT_TYPE_CONNECT:
                HandleConnect(event.peer);
                break;

            case ENET_EVENT_TYPE_RECEIVE:
                HandleReceive(event);
                enet_packet_destroy(event.packet);
                break;

            case ENET_EVENT_TYPE_DISCONNECT:
                HandleDisconnect(event.peer, event.data);
                break;

            default:
                break;
        }
    }
}

void CNetServerImpl::RegisterPacketHandler(PPACKETHANDLER pfnPacketHandler)
{
    m_pfnPacketHandler = pfnPacketHandler;
}

bool CNetServerImpl::GetNetworkStatistics(NetStatistics* pDest, const NetServerPlayerID& PlayerID)
{
    if (pDest == nullptr)
        return false;

    memset(pDest, 0, sizeof(NetStatistics));

    ENetPeer* pPeer = m_PeerManager.Find(PlayerID);
    if (pPeer == nullptr)
        return false;

    pDest->bytesReceived = pPeer->incomingDataTotal;
    pDest->bytesSent = pPeer->outgoingDataTotal;
    pDest->packetsReceived = m_pHost != nullptr ? m_pHost->totalReceivedPackets : 0;
    pDest->packetsSent = m_pHost != nullptr ? m_pHost->totalSentPackets : 0;
    pDest->packetlossTotal = static_cast<float>(pPeer->packetLoss) * 100.0f / ENET_PEER_PACKET_LOSS_SCALE;
    pDest->packetlossLastSecond = pDest->packetlossTotal;

    return true;
}

const SPacketStat* CNetServerImpl::GetPacketStats()
{
    return &m_PacketStats[0][0];
}

bool CNetServerImpl::GetBandwidthStatistics(SBandwidthStatistics* pDest)
{
    if (pDest == nullptr)
        return false;

    memset(pDest, 0, sizeof(SBandwidthStatistics));
    if (m_pHost == nullptr)
        return false;

    m_PeerManager.ForEachPeer(
        [&](ENetPeer* pPeer)
        {
            pDest->llOutgoingUDPByteCount += pPeer->outgoingDataTotal;
            pDest->llIncomingUDPByteCount += pPeer->incomingDataTotal;
        });

    pDest->llOutgoingUDPPacketCount = m_pHost->totalSentPackets;
    pDest->llIncomingUDPPacketCount = m_pHost->totalReceivedPackets;
    return true;
}

bool CNetServerImpl::GetNetPerformanceStatistics(SNetPerformanceStatistics* pDest, bool bResetCounters)
{
    if (pDest == nullptr)
        return false;

    memset(pDest, 0, sizeof(SNetPerformanceStatistics));
    pDest->uiUpdateCycleDatagramsMax = m_uiUpdateCycleDatagrams;
    pDest->fUpdateCycleDatagramsAvg = static_cast<float>(m_uiUpdateCycleDatagrams);
    pDest->uiUpdateCycleMessagesMax = m_uiUpdateCycleMessages;
    pDest->fUpdateCycleMessagesAvg = static_cast<float>(m_uiUpdateCycleMessages);

    if (bResetCounters)
    {
        m_uiUpdateCycleDatagrams = 0;
        m_uiUpdateCycleMessages = 0;
    }
    return true;
}

void CNetServerImpl::GetPingStatus(SFixedString<32>* pstrStatus)
{
    if (pstrStatus != nullptr)
        *pstrStatus = "";
}

bool CNetServerImpl::GetSyncThreadStatistics(SSyncThreadStatistics* pDest, bool bResetCounters)
{
    if (pDest != nullptr)
        memset(pDest, 0, sizeof(SSyncThreadStatistics));
    return false;
}

NetBitStreamInterface* CNetServerImpl::AllocateNetServerBitStream(unsigned short usBitStreamVersion, const void* pData, uint uiDataSize, bool bCopyData)
{
    if (pData != nullptr && uiDataSize > 0)
        return new CNetBitStream(pData, uiDataSize, usBitStreamVersion);
    return new CNetBitStream(usBitStreamVersion);
}

void CNetServerImpl::DeallocateNetServerBitStream(NetBitStreamInterface* bitStream)
{
    if (bitStream != nullptr)
        bitStream->Release();
}

bool CNetServerImpl::SendPacket(unsigned char ucPacketID, const NetServerPlayerID& playerID, NetBitStreamInterface* bitStream, bool bBroadcast, NetServerPacketPriority packetPriority, NetServerPacketReliability packetReliability, ePacketOrdering packetOrdering)
{
    if (m_pHost == nullptr)
    {
        WriteServerNetworkLog("CNetServerImpl::SendPacket: id=%u failed, host not started", ucPacketID);
        return false;
    }

    const SNetChannelAndFlags channelAndFlags = GetNetChannelAndFlags(packetReliability, packetOrdering);

    const unsigned char* pData = bitStream != nullptr ? bitStream->GetData() : nullptr;
    const size_t          uiLength = bitStream != nullptr ? static_cast<size_t>(bitStream->GetNumberOfBytesUsed()) : 0;

    ENetPeer* pPeer = nullptr;
    if (!bBroadcast)
    {
        pPeer = m_PeerManager.Find(playerID);
        if (pPeer == nullptr)
        {
            WriteServerNetworkLog("CNetServerImpl::SendPacket: id=%u failed, unknown playerID (peer not found)", ucPacketID);
            return false;
        }
    }

    ENetPacket* pPacket = nullptr;
    if (ucPacketID == PACKET_ID_PLAYER_LIST)
    {
        pPacket = enet_packet_create(nullptr, uiLength + 5, channelAndFlags.flags);
        if (pPacket != nullptr)
        {
            pPacket->data[0] = ucPacketID;
            memset(pPacket->data + 1, 0, 4);
            if (uiLength > 0 && pData != nullptr)
                memcpy(pPacket->data + 5, pData, uiLength);
        }
    }
    else
    {
        pPacket = BuildNetPacket(ucPacketID, pData, uiLength, channelAndFlags.flags);
    }
    
    if (pPacket == nullptr)
    {
        WriteServerNetworkLog("CNetServerImpl::SendPacket: id=%u failed, BuildNetPacket returned null", ucPacketID);
        return false;
    }

    const bool bReliable = (pPacket->flags & ENET_PACKET_FLAG_RELIABLE) != 0;

    if (m_bNetSimEnabled && m_iNetSimPacketLoss > 0 && !bReliable && (rand() % 100) < m_iNetSimPacketLoss)
    {
        enet_packet_destroy(pPacket);
        return true;
    }

    int iExtraPing = 0;
    if (m_bNetSimEnabled)
    {
        iExtraPing = m_iNetSimExtraPing;
        if (m_iNetSimExtraPingVariance > 0)
            iExtraPing += rand() % (m_iNetSimExtraPingVariance + 1);
    }

    if (iExtraPing > 0)
    {
        SDelayedPacket delayed;
        delayed.uiSendTime = enet_time_get() + static_cast<enet_uint32>(iExtraPing);
        delayed.bBroadcast = bBroadcast;
        delayed.playerID = playerID;
        delayed.ucPacketID = ucPacketID;
        delayed.ucChannel = channelAndFlags.channel;
        delayed.pPacket = pPacket;
        m_DelayedPackets.push_back(delayed);
        return true;
    }

    if (bBroadcast)
    {
        enet_host_broadcast(m_pHost, channelAndFlags.channel, pPacket);
        if (ucPacketID <= PACKET_ID_MOD_NAME)
            WriteServerNetworkLog("CNetServerImpl::SendPacket: id=%u broadcast on channel=%u len=%u", ucPacketID, channelAndFlags.channel, uiLength);
    }
    else
    {
        if (enet_peer_send(pPeer, channelAndFlags.channel, pPacket) != 0)
        {
            WriteServerNetworkLog("CNetServerImpl::SendPacket: id=%u failed, enet_peer_send() returned nonzero (peer state=%d)", ucPacketID, pPeer->state);
            enet_packet_destroy(pPacket);
            return false;
        }

        if (ucPacketID <= PACKET_ID_MOD_NAME)
            WriteServerNetworkLog("CNetServerImpl::SendPacket: id=%u sent to peer on channel=%u len=%u (peer state=%d)", ucPacketID, channelAndFlags.channel, uiLength, pPeer->state);
    }

    m_PacketStats[CNetServer::STATS_OUTGOING_TRAFFIC][ucPacketID].iCount++;
    m_PacketStats[CNetServer::STATS_OUTGOING_TRAFFIC][ucPacketID].iTotalBytes += static_cast<int>(uiLength);

    return true;
}

void CNetServerImpl::FlushDelayedPackets()
{
    if (m_DelayedPackets.empty())
        return;

    enet_uint32 uiNow = enet_time_get();

    while (!m_DelayedPackets.empty())
    {
        SDelayedPacket& front = m_DelayedPackets.front();
        if (uiNow < front.uiSendTime)
            break;

        SDelayedPacket delayed = front;
        m_DelayedPackets.pop_front();

        if (m_pHost == nullptr)
        {
            enet_packet_destroy(delayed.pPacket);
            continue;
        }

        const size_t uiLength = delayed.pPacket->dataLength;

        if (delayed.bBroadcast)
        {
            enet_host_broadcast(m_pHost, delayed.ucChannel, delayed.pPacket);
        }
        else
        {
            ENetPeer* pPeer = m_PeerManager.Find(delayed.playerID);
            if (pPeer == nullptr || enet_peer_send(pPeer, delayed.ucChannel, delayed.pPacket) != 0)
            {
                enet_packet_destroy(delayed.pPacket);
                continue;
            }
        }

        m_PacketStats[CNetServer::STATS_OUTGOING_TRAFFIC][delayed.ucPacketID].iCount++;
        m_PacketStats[CNetServer::STATS_OUTGOING_TRAFFIC][delayed.ucPacketID].iTotalBytes += static_cast<int>(uiLength);
    }
}

void CNetServerImpl::GetPlayerIP(const NetServerPlayerID& playerID, char strIP[22], unsigned short* usPort)
{
    if (strIP != nullptr)
        strIP[0] = '\0';
    if (usPort != nullptr)
        *usPort = 0;

    ENetPeer* pPeer = m_PeerManager.Find(playerID);
    if (pPeer == nullptr)
        return;

    const enet_uint32 h = pPeer->address.host;
    if (strIP != nullptr)
        snprintf(strIP, 22, "%u.%u.%u.%u", h & 0xFF, (h >> 8) & 0xFF, (h >> 16) & 0xFF, (h >> 24) & 0xFF);
    if (usPort != nullptr)
        *usPort = pPeer->address.port;
}

void CNetServerImpl::Kick(const NetServerPlayerID& PlayerID)
{
    ENetPeer* pPeer = m_PeerManager.Find(PlayerID);
    if (pPeer != nullptr)
        enet_peer_disconnect(pPeer, 0);
}

void CNetServerImpl::SetPassword(const char* szPassword)
{
    m_strPassword = szPassword != nullptr ? szPassword : "";
}

void CNetServerImpl::SetMaximumIncomingConnections(unsigned short numberAllowed)
{
    m_usMaxConnections = numberAllowed;
}

CNetHTTPDownloadManagerInterface* CNetServerImpl::GetHTTPDownloadManager(EDownloadModeType iMode)
{
    auto it = m_DownloadManagers.find(iMode);
    if (it != m_DownloadManagers.end())
        return it->second;

    CNetHTTPDownloadManager* pManager = new CNetHTTPDownloadManager();
    m_DownloadManagers[iMode] = pManager;
    return pManager;
}

void CNetServerImpl::SetClientBitStreamVersion(const NetServerPlayerID& PlayerID, unsigned short usBitStreamVersion)
{
    CNetServerPeerManager::SPeerRecord* pRecord = m_PeerManager.FindRecord(PlayerID);
    if (pRecord != nullptr) pRecord->usBitStreamVersion = usBitStreamVersion;
}

void CNetServerImpl::ClearClientBitStreamVersion(const NetServerPlayerID& PlayerID)
{
    CNetServerPeerManager::SPeerRecord* pRecord = m_PeerManager.FindRecord(PlayerID);
    if (pRecord != nullptr) pRecord->usBitStreamVersion = 0;
}

void CNetServerImpl::SetChecks(const char* szDisableComboACMap, const char* szDisableACMap, const char* szEnableSDMap, int iEnableClientChecks, bool bHideAC, const char* szImgMods)
{
}

unsigned int CNetServerImpl::GetPendingPacketCount()
{
    return static_cast<unsigned int>(m_DelayedPackets.size());
}

void CNetServerImpl::GetNetRoute(SFixedString<32>* pstrRoute)
{
    if (pstrRoute != nullptr)
        *pstrRoute = "";
}

bool CNetServerImpl::InitServerId(const char* szPath)
{
    if (szPath == nullptr || szPath[0] == '\0')
        return false;

    SString strExisting;
    if (SharedUtil::FileLoad(szPath, strExisting) && strExisting.length() >= 32)
    {
        m_strServerId = strExisting.SubStr(0, 32);
        return true;
    }

    static const char szHexChars[] = "0123456789ABCDEF";
    char               buffer[33];
    for (int i = 0; i < 32; i++)
        buffer[i] = szHexChars[rand() % 16];
    buffer[32] = '\0';
    m_strServerId = buffer;

    SharedUtil::MakeSureDirExists(szPath);
    return SharedUtil::FileSave(szPath, m_strServerId);
}

void CNetServerImpl::ResendModPackets(const NetServerPlayerID& playerID)
{
}

void CNetServerImpl::ResendACPackets(const NetServerPlayerID& playerID)
{
}

void CNetServerImpl::GetClientSerialAndVersion(const NetServerPlayerID& playerID, SFixedString<32>& strSerial, SFixedString<64>& strExtra, SFixedString<32>& strVersion)
{
    CNetServerPeerManager::SPeerRecord* pRecord = m_PeerManager.FindRecord(playerID);
    if (pRecord != nullptr)
    {
        strSerial = pRecord->strSerial;
        strExtra = pRecord->strExtra;
        strVersion = pRecord->strVersion;
    }
    else
    {
        strSerial = "";
        strExtra = "";
        strVersion = "";
    }
}

void CNetServerImpl::SetNetOptions(const SNetOptions& options)
{
    if (options.netSim.bValid)
    {
        m_iNetSimPacketLoss = Clamp(0, options.netSim.iPacketLoss, 100);
        m_iNetSimExtraPing = options.netSim.iExtraPing < 0 ? 0 : options.netSim.iExtraPing;
        m_iNetSimExtraPingVariance = options.netSim.iExtraPingVariance < 0 ? 0 : options.netSim.iExtraPingVariance;
        m_iNetSimKBPSLimit = options.netSim.iKBPSLimit < 0 ? 0 : options.netSim.iKBPSLimit;
        m_bNetSimEnabled = m_iNetSimPacketLoss > 0 || m_iNetSimExtraPing > 0 || m_iNetSimExtraPingVariance > 0 || m_iNetSimKBPSLimit > 0;

        if (m_pHost != nullptr)
        {
            enet_uint32 uiOutgoing = m_iNetSimKBPSLimit > 0 ? static_cast<enet_uint32>(m_iNetSimKBPSLimit) * 1024u : 0u;
            enet_host_bandwidth_limit(m_pHost, 0, uiOutgoing);
        }
    }
}

bool CNetServerImpl::IsValidSocket(const NetServerPlayerID& playerID)
{
    ENetPeer* pPeer = m_PeerManager.Find(playerID);
    if (pPeer == nullptr)
        return false;

    return pPeer->state == ENET_PEER_STATE_CONNECTED || pPeer->state == ENET_PEER_STATE_DISCONNECT_LATER;
}

void CNetServerImpl::GenerateRandomData(void* pOutData, uint uiLength)
{
    unsigned char* p = static_cast<unsigned char*>(pOutData);
    for (uint i = 0; i < uiLength; i++)
        p[i] = static_cast<unsigned char>(rand() & 0xFF);
}

void CNetServerImpl::SetMinClientRequirement(const char* szVersion)
{
    m_strMinClientRequirement = szVersion != nullptr ? szVersion : "";
}

bool CNetServerImpl::ValidateHttpCacheFileName(const char* szFilename)
{
    if (szFilename == nullptr || szFilename[0] == '\0')
        return false;

    if (strstr(szFilename, "..") != nullptr)
        return false;

    return true;
}

bool CNetServerImpl::DeobfuscateScript(const char* cpInBuffer, uint uiInSize, const char** pcpOutBuffer, uint* puiOutSize, const char* szScriptName)
{
    if (pcpOutBuffer != nullptr)
        *pcpOutBuffer = cpInBuffer;
    if (puiOutSize != nullptr)
        *puiOutSize = uiInSize;
    return true;
}

bool CNetServerImpl::GetScriptInfo(const char* cpInBuffer, uint uiInSize, SScriptInfo* pOutInfo)
{
    return false;
}

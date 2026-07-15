#pragma once

#include "CNetServerPeerManager.h"
#include "../../Shared/net/CNetHTTPDownloadManager.h"
#include <net/CNetServer.h>
#include <map>
#include <deque>

class CNetServerImpl final : public CNetServer {
public:
    CNetServerImpl();
    ~CNetServerImpl() override;

    bool StartNetwork(const char* szIP, unsigned short usServerPort, unsigned int uiAllowedPlayers, const char* szServerName) override;
    void StopNetwork() override;

    void DoPulse() override;

    void RegisterPacketHandler(PPACKETHANDLER pfnPacketHandler) override;

    bool               GetNetworkStatistics(NetStatistics* pDest, const NetServerPlayerID& PlayerID) override;
    const SPacketStat* GetPacketStats() override;
    bool               GetBandwidthStatistics(SBandwidthStatistics* pDest) override;
    bool               GetNetPerformanceStatistics(SNetPerformanceStatistics* pDest, bool bResetCounters) override;
    void               GetPingStatus(SFixedString<32>* pstrStatus) override;
    bool               GetSyncThreadStatistics(SSyncThreadStatistics* pDest, bool bResetCounters) override;

    NetBitStreamInterface* AllocateNetServerBitStream(unsigned short usBitStreamVersion, const void* pData = nullptr, uint uiDataSize = 0, bool bCopyData = false) override;
    void                    DeallocateNetServerBitStream(NetBitStreamInterface* bitStream) override;
    bool                    SendPacket(unsigned char ucPacketID, const NetServerPlayerID& playerID, NetBitStreamInterface* bitStream, bool bBroadcast, NetServerPacketPriority packetPriority, NetServerPacketReliability packetReliability, ePacketOrdering packetOrdering = PACKET_ORDERING_DEFAULT) override;

    void GetPlayerIP(const NetServerPlayerID& playerID, char strIP[22], unsigned short* usPort) override;

    void Kick(const NetServerPlayerID& PlayerID) override;

    void SetPassword(const char* szPassword) override;

    void SetMaximumIncomingConnections(unsigned short numberAllowed) override;

    CNetHTTPDownloadManagerInterface* GetHTTPDownloadManager(EDownloadModeType iMode) override;

    void SetClientBitStreamVersion(const NetServerPlayerID& PlayerID, unsigned short usBitStreamVersion) override;
    void ClearClientBitStreamVersion(const NetServerPlayerID& PlayerID) override;

    void SetChecks(const char* szDisableComboACMap, const char* szDisableACMap, const char* szEnableSDMap, int iEnableClientChecks, bool bHideAC, const char* szImgMods) override;

    unsigned int GetPendingPacketCount() override;
    void         GetNetRoute(SFixedString<32>* pstrRoute) override;

    bool InitServerId(const char* szPath) override;
    void ResendModPackets(const NetServerPlayerID& playerID) override;
    void ResendACPackets(const NetServerPlayerID& playerID) override;

    void GetClientSerialAndVersion(const NetServerPlayerID& playerID, SFixedString<32>& strSerial, SFixedString<64>& strExtra,
                                    SFixedString<32>& strVersion) override;
    void SetNetOptions(const SNetOptions& options) override;
    void GenerateRandomData(void* pOutData, uint uiLength) override;

    bool IsValidSocket(const NetServerPlayerID& playerID) override;

    void SetMinClientRequirement(const char* szVersion) override;

    bool ValidateHttpCacheFileName(const char* szFilename) override;

    bool DeobfuscateScript(const char* cpInBuffer, uint uiInSize, const char** pcpOutBuffer, uint* puiOutSize, const char* szScriptName) override;
    bool GetScriptInfo(const char* cpInBuffer, uint uiInSize, SScriptInfo* pOutInfo) override;

private:
    void HandleConnect(ENetPeer* pPeer);
    void HandleReceive(const ENetEvent& event);
    void HandleDisconnect(ENetPeer* pPeer, enet_uint32 uiEventData);
    void FlushDelayedPackets();

    ENetHost*             m_pHost;
    CNetServerPeerManager m_PeerManager;
    PPACKETHANDLER        m_pfnPacketHandler;

    SString        m_strPassword;
    unsigned short m_usMaxConnections;
    SString        m_strMinClientRequirement;
    SString        m_strServerId;

    SPacketStat m_PacketStats[2][256];

    uint m_uiUpdateCycleDatagrams;
    uint m_uiUpdateCycleMessages;

    bool m_bNetSimEnabled;
    int  m_iNetSimPacketLoss;
    int  m_iNetSimExtraPing;
    int  m_iNetSimExtraPingVariance;
    int  m_iNetSimKBPSLimit;

    struct SDelayedPacket
    {
        enet_uint32       uiSendTime;
        bool              bBroadcast;
        NetServerPlayerID playerID;
        unsigned char     ucPacketID;
        unsigned char     ucChannel;
        ENetPacket*       pPacket;
    };
    std::deque<SDelayedPacket> m_DelayedPackets;

    std::map<EDownloadModeType, CNetHTTPDownloadManager*> m_DownloadManagers;
};

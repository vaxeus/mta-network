#pragma once

#include <net/CNet.h>
#include "../../Shared/net/NetChannelMap.h"
#include "../../Shared/net/CNetHTTPDownloadManager.h"
#include <map>
#include <deque>
#include <cstdio>

class CCore;

class CNetBinaryFile final : public CBinaryFileInterface {
public:
    CNetBinaryFile() = default;
    ~CNetBinaryFile() override { FClose(); }

    bool FOpen(const char* szFilename, const char* szMode, bool bValidate) override;
    void FClose() override;
    bool FEof() override;
    void FFlush() override;
    int  FTell() override;
    void FSeek(int iOffset, int iOrigin) override;
    int  FRead(void* pData, uint uiSize) override;
    int  FWrite(const void* pData, uint uiSize) override;

private:
    FILE* m_pFile = nullptr;
};

class CNetClient final : public CNet {
public:
    explicit CNetClient(CCore* pCore);
    ~CNetClient() override;

    bool StartNetwork(const char* szServerHost, unsigned short usServerPort, bool bPacketTag = false) override;
    void StopNetwork() override;

    void SetFakeLag(unsigned short usPacketLoss, unsigned short usMinExtraPing, unsigned short usExtraPingVariance, int iKBPSLimit) override;

    bool IsReady() override;
    bool IsConnected() override;

    void DoPulse() override;
    void Shutdown() override;

    void RegisterPacketHandler(PPACKETHANDLER pfnPacketHandler) override;

    NetBitStreamInterface* AllocateNetBitStream() override;
    void                   DeallocateNetBitStream(NetBitStreamInterface* bitStream) override;
    bool                   SendPacket(unsigned char ucPacketID, NetBitStreamInterface* bitStream, NetPacketPriority packetPriority, NetPacketReliability packetReliability, ePacketOrdering packetOrdering = PACKET_ORDERING_DEFAULT) override;

    void        SetClientPort(unsigned short usClientPort) override;
    const char* GetConnectedServer(bool bIncludePort = false) override;

    bool               GetNetworkStatistics(NetStatistics* pDest) override;
    const SPacketStat* GetPacketStats() override;

    int           GetPing() override;
    unsigned long GetTime() override;

    const char* GetLocalIP() override;
    void        GetSerial(char* szSerial, size_t maxLength) override;

    unsigned char GetConnectionError() override;
    void          SetConnectionError(unsigned char ucConnectionError) override;

    void Reset() override;

    CNetHTTPDownloadManagerInterface* GetHTTPDownloadManager(EDownloadModeType iMode) override;

    void           SetServerBitStreamVersion(unsigned short usServerBitStreamVersion) override;
    unsigned short GetServerBitStreamVersion() override;

    void           GetStatus(char* szStatus, size_t maxLength) override;
    unsigned short GetNetRev() override;
    unsigned short GetNetRel() override;

    const char* GetNextBuffer() override;
    const char* GetDiagnosticStatus() override;
    void        UpdatePingStatus(const char* status, size_t statusLength, ushort& usDataRef, bool& isVerified) override;

    bool VerifySignature(const char* pData, unsigned long ulSize) override;

    void ResetStub(DWORD dwType, ...) override;
    void ResetStub(DWORD dwType, va_list) override;

    const char* GetCurrentServerId(bool bPreviousVer) override;
    bool        CheckFile(const char* szType, const char* szFilename, const void* pData = nullptr, size_t sizeData = 0) override;

    uint GetExtendedErrorCode() override;
    void SetTimeoutTime(uint uiTimeoutTime) override;

    bool                  ValidateBinaryFileName(const char* szFilename) override;
    CBinaryFileInterface* AllocateBinaryFile() override;
    bool                  EncryptDumpfile(const char* szClearPathFilename, const char* szEncryptedPathFilename) override;
    bool DeobfuscateScript(const char* cpInBuffer, uint uiInSize, const char** pcpOutBuffer, uint* puiOutSize, const char* szScriptName) override;
    void PostCrash() override;
    int  SendTo(SOCKET s, const char* buf, int len, int flags, const struct sockaddr* to, int tolen) override;

private:
    bool CreateHost();
    void DestroyHost();
    void HandleReceive(const ENetEvent& event);
    void FlushDelayedPackets();

    CCore* m_pCore;

    ENetHost* m_pHost;
    ENetPeer* m_pPeer;

    PPACKETHANDLER m_pfnPacketHandler;

    bool           m_bReady;
    bool           m_bConnected;
    unsigned char  m_ucConnectionError;
    unsigned short m_usClientPort;
    unsigned short m_usServerBitStreamVersion;
    uint           m_uiTimeoutTime;
    uint           m_uiExtendedErrorCode;

    SString        m_strConnectedHost;
    unsigned short m_usConnectedPort;
    SString        m_strConnectedServerBuffer;
    SString        m_strLocalIP;
    SString        m_strStatus;
    SString        m_strDiagnosticStatus;
    SString        m_strCurrentServerId;
    SString        m_strSerial;

    SPacketStat m_PacketStats[2][256];

    unsigned short m_usFakeLagPacketLoss;
    unsigned short m_usFakeLagExtraPing;
    unsigned short m_usFakeLagExtraPingVariance;
    int            m_iFakeLagKBPSLimit;

    struct SDelayedPacket
    {
        enet_uint32   uiSendTime;
        unsigned char ucPacketID;
        unsigned char ucChannel;
        ENetPacket*   pPacket;
    };
    std::deque<SDelayedPacket> m_DelayedPackets;

    std::map<EDownloadModeType, CNetHTTPDownloadManager*> m_DownloadManagers;
};

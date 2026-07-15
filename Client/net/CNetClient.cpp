#include "StdInc.h"
#include "CNetClient.h"
#include "../../Shared/net/CNetBitStream.h"
#include <cstring>
#include <cstdio>

bool CNetBinaryFile::FOpen(const char* szFilename, const char* szMode, bool bValidate)
{
    FClose();
    m_pFile = fopen(szFilename, szMode);
    return m_pFile != nullptr;
}

void CNetBinaryFile::FClose()
{
    if (m_pFile != nullptr)
    {
        fclose(m_pFile);
        m_pFile = nullptr;
    }
}

bool CNetBinaryFile::FEof()
{
    return m_pFile == nullptr || feof(m_pFile) != 0;
}

void CNetBinaryFile::FFlush()
{
    if (m_pFile != nullptr)
        fflush(m_pFile);
}

int CNetBinaryFile::FTell()
{
    return m_pFile != nullptr ? static_cast<int>(ftell(m_pFile)) : -1;
}

void CNetBinaryFile::FSeek(int iOffset, int iOrigin)
{
    if (m_pFile != nullptr)
        fseek(m_pFile, iOffset, iOrigin);
}

int CNetBinaryFile::FRead(void* pData, uint uiSize)
{
    return m_pFile != nullptr ? static_cast<int>(fread(pData, 1, uiSize, m_pFile)) : 0;
}

int CNetBinaryFile::FWrite(const void* pData, uint uiSize)
{
    return m_pFile != nullptr ? static_cast<int>(fwrite(pData, 1, uiSize, m_pFile)) : 0;
}

CNetClient::CNetClient(CCore* pCore)
    : m_pCore(pCore),
      m_pHost(nullptr),
      m_pPeer(nullptr),
      m_pfnPacketHandler(nullptr),
      m_bReady(false),
      m_bConnected(false),
      m_ucConnectionError(0),
      m_usClientPort(NET_CLIENT_PORT),
      m_usServerBitStreamVersion(0),
      m_uiTimeoutTime(NET_DISCONNECT_DELAY * 1000),
      m_uiExtendedErrorCode(0),
      m_usConnectedPort(0),
      m_usFakeLagPacketLoss(0),
      m_usFakeLagExtraPing(0),
      m_usFakeLagExtraPingVariance(0),
      m_iFakeLagKBPSLimit(0)
{
    memset(m_PacketStats, 0, sizeof(m_PacketStats));
    enet_initialize();
    CreateHost();
}

CNetClient::~CNetClient()
{
    Shutdown();
    enet_deinitialize();
}

void CNetClient::DestroyHost()
{
    for (SDelayedPacket& delayed : m_DelayedPackets)
        enet_packet_destroy(delayed.pPacket);

    m_DelayedPackets.clear();

    if (m_pHost != nullptr)
    {
        if (m_pPeer != nullptr)
        {
            enet_peer_disconnect_now(m_pPeer, 1);
            m_pPeer = nullptr;
        }
        enet_host_destroy(m_pHost);
        m_pHost = nullptr;
    }
    m_bReady = false;
    m_bConnected = false;
}

bool CNetClient::CreateHost()
{
    ENetAddress clientAddress;
    clientAddress.host = ENET_HOST_ANY;
    clientAddress.port = m_usClientPort;

    m_pHost = enet_host_create(m_usClientPort != 0 ? &clientAddress : nullptr, 1, NET_CHANNEL_COUNT, 0, 0);
    if (m_pHost != nullptr && m_iFakeLagKBPSLimit > 0)
        enet_host_bandwidth_limit(m_pHost, 0, static_cast<enet_uint32>(m_iFakeLagKBPSLimit) * 1024u);
    return m_pHost != nullptr;
}

bool CNetClient::StartNetwork(const char* szServerHost, unsigned short usServerPort, bool bPacketTag)
{
    DestroyHost();

    if (!CreateHost())
    {
        WriteNetworkDebugEvent("CNetClient: enet_host_create failed in StartNetwork");
        m_ucConnectionError = 1;
        return false;
    }

    ENetAddress serverAddress;
    if (enet_address_set_host(&serverAddress, szServerHost) != 0)
    {
        WriteNetworkDebugEvent(SString("CNetClient: enet_address_set_host failed to resolve '%s'", szServerHost));
        DestroyHost();
        m_ucConnectionError = 2;
        return false;
    }

    serverAddress.port = usServerPort;

    m_pPeer = enet_host_connect(m_pHost, &serverAddress, NET_CHANNEL_COUNT, 0);
    if (m_pPeer == nullptr)
    {
        WriteNetworkDebugEvent(SString("CNetClient: enet_host_connect(%s:%u) failed", szServerHost, usServerPort));
        DestroyHost();
        m_ucConnectionError = 3;
        return false;
    }

    enet_peer_timeout(m_pPeer, 0, m_uiTimeoutTime, m_uiTimeoutTime * 2);

    m_strConnectedHost = szServerHost;
    m_usConnectedPort = usServerPort;
    m_ucConnectionError = 0;

    WriteNetworkDebugEvent(SString("CNetClient: ENet peer connect requested to %s:%u", szServerHost, usServerPort));

    return true;
}

void CNetClient::StopNetwork()
{
    DestroyHost();
}

void CNetClient::SetFakeLag(unsigned short usPacketLoss, unsigned short usMinExtraPing, unsigned short usExtraPingVariance, int iKBPSLimit)
{
    m_usFakeLagPacketLoss = usPacketLoss > 100 ? 100 : usPacketLoss;
    m_usFakeLagExtraPing = usMinExtraPing;
    m_usFakeLagExtraPingVariance = usExtraPingVariance;
    m_iFakeLagKBPSLimit = iKBPSLimit < 0 ? 0 : iKBPSLimit;

    if (m_pHost != nullptr)
    {
        enet_uint32 uiOutgoing = m_iFakeLagKBPSLimit > 0 ? static_cast<enet_uint32>(m_iFakeLagKBPSLimit) * 1024u : 0u;
        enet_host_bandwidth_limit(m_pHost, 0, uiOutgoing);
    }
}

bool CNetClient::IsReady()
{
    return m_pHost != nullptr;
}

bool CNetClient::IsConnected()
{
    return m_bConnected;
}

void CNetClient::HandleReceive(const ENetEvent& event)
{
    unsigned char         ucPacketID = ExtractNetPacketId(event.packet);
    const unsigned char*  pPayload = GetNetPacketPayload(event.packet);
    size_t                uiPayloadLength = GetNetPacketPayloadLength(event.packet);

    m_PacketStats[CNet::STATS_INCOMING_TRAFFIC][ucPacketID].iCount++;
    m_PacketStats[CNet::STATS_INCOMING_TRAFFIC][ucPacketID].iTotalBytes += static_cast<int>(uiPayloadLength);

    if (m_pfnPacketHandler != nullptr)
    {
        CNetBitStream* pBitStream = new CNetBitStream(pPayload, uiPayloadLength, m_usServerBitStreamVersion);
        m_pfnPacketHandler(ucPacketID, *pBitStream);
        pBitStream->Release();
    }
}

void CNetClient::DoPulse()
{
    for (auto& pair : m_DownloadManagers)
        pair.second->ProcessQueuedFiles();

    if (m_pHost == nullptr)
        return;

    FlushDelayedPackets();

    static ENetHost* s_pLastLoggedHost = nullptr;
    static ENetPeer* s_pLastLoggedPeer = nullptr;
    static int       s_iLastLoggedPeerState = -2;
    const int        iPeerState = m_pPeer != nullptr ? static_cast<int>(m_pPeer->state) : -1;
    if (m_pHost != s_pLastLoggedHost || m_pPeer != s_pLastLoggedPeer || iPeerState != s_iLastLoggedPeerState)
    {
        WriteNetworkDebugEvent(SString("CNetClient::DoPulse: host=%p peer=%p peerState=%d channelCount=%u peerCount=%u", m_pHost, m_pPeer, iPeerState, m_pPeer != nullptr ? static_cast<unsigned>(m_pPeer->channelCount) : 0u, static_cast<unsigned>(m_pHost->peerCount)));
        s_pLastLoggedHost = m_pHost;
        s_pLastLoggedPeer = m_pPeer;
        s_iLastLoggedPeerState = iPeerState;
    }

    ENetEvent event;
    while (m_pHost != nullptr && enet_host_service(m_pHost, &event, 0) > 0)
    {
        switch (event.type)
        {
            case ENET_EVENT_TYPE_CONNECT:
                m_bReady = true;
                m_bConnected = true;
                WriteNetworkDebugEvent(SString("CNetClient: ENet low-level handshake completed with %s:%u", m_strConnectedHost.c_str(), m_usConnectedPort));
                break;

            case ENET_EVENT_TYPE_RECEIVE:
                HandleReceive(event);
                enet_packet_destroy(event.packet);
                break;

            case ENET_EVENT_TYPE_DISCONNECT:
                WriteNetworkDebugEvent(SString("CNetClient: ENet peer disconnected (data=%u)", event.data));
                m_bConnected = false;
                m_pPeer = nullptr;
                break;

            default:
                break;
        }
    }
}

void CNetClient::Shutdown()
{
    DestroyHost();

    for (auto& pair : m_DownloadManagers)
        delete pair.second;
    m_DownloadManagers.clear();
}

void CNetClient::RegisterPacketHandler(PPACKETHANDLER pfnPacketHandler)
{
    m_pfnPacketHandler = pfnPacketHandler;
}

NetBitStreamInterface* CNetClient::AllocateNetBitStream()
{
    return new CNetBitStream(m_usServerBitStreamVersion);
}

void CNetClient::DeallocateNetBitStream(NetBitStreamInterface* bitStream)
{
    if (bitStream != nullptr)
        bitStream->Release();
}

bool CNetClient::SendPacket(unsigned char ucPacketID, NetBitStreamInterface* bitStream, NetPacketPriority packetPriority, NetPacketReliability packetReliability, ePacketOrdering packetOrdering)
{
    if (m_pPeer == nullptr)
        return false;

    SNetChannelAndFlags channelAndFlags = GetNetChannelAndFlags(packetReliability, packetOrdering);

    const unsigned char* pData = bitStream != nullptr ? bitStream->GetData() : nullptr;
    size_t                uiLength = bitStream != nullptr ? static_cast<size_t>(bitStream->GetNumberOfBytesUsed()) : 0;

    ENetPacket* pPacket = BuildNetPacket(ucPacketID, pData, uiLength, channelAndFlags.flags);
    if (pPacket == nullptr)
        return false;

    const bool bReliable = (pPacket->flags & ENET_PACKET_FLAG_RELIABLE) != 0;

    if (m_usFakeLagPacketLoss > 0 && !bReliable && (rand() % 100) < m_usFakeLagPacketLoss)
    {
        enet_packet_destroy(pPacket);
        return true;
    }

    unsigned int uiExtraPing = m_usFakeLagExtraPing;
    if (m_usFakeLagExtraPingVariance > 0)
        uiExtraPing += rand() % (m_usFakeLagExtraPingVariance + 1);

    if (uiExtraPing > 0)
    {
        SDelayedPacket delayed;
        delayed.uiSendTime = enet_time_get() + uiExtraPing;
        delayed.ucPacketID = ucPacketID;
        delayed.ucChannel = channelAndFlags.channel;
        delayed.pPacket = pPacket;
        m_DelayedPackets.push_back(delayed);
        return true;
    }

    if (enet_peer_send(m_pPeer, channelAndFlags.channel, pPacket) != 0)
    {
        enet_packet_destroy(pPacket);
        return false;
    }

    m_PacketStats[CNet::STATS_OUTGOING_TRAFFIC][ucPacketID].iCount++;
    m_PacketStats[CNet::STATS_OUTGOING_TRAFFIC][ucPacketID].iTotalBytes += static_cast<int>(uiLength);

    return true;
}

void CNetClient::FlushDelayedPackets()
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

        if (m_pPeer == nullptr)
        {
            enet_packet_destroy(delayed.pPacket);
            continue;
        }

        if (enet_peer_send(m_pPeer, delayed.ucChannel, delayed.pPacket) == 0)
        {
            m_PacketStats[CNet::STATS_OUTGOING_TRAFFIC][delayed.ucPacketID].iCount++;
            m_PacketStats[CNet::STATS_OUTGOING_TRAFFIC][delayed.ucPacketID].iTotalBytes += static_cast<int>(delayed.pPacket->dataLength);
        }
        else
        {
            enet_packet_destroy(delayed.pPacket);
        }
    }
}

void CNetClient::SetClientPort(unsigned short usClientPort)
{
    m_usClientPort = usClientPort;
}

const char* CNetClient::GetConnectedServer(bool bIncludePort)
{
    if (bIncludePort)
        m_strConnectedServerBuffer = SString("%s:%u", *m_strConnectedHost, m_usConnectedPort);
    else
        m_strConnectedServerBuffer = m_strConnectedHost;
    return *m_strConnectedServerBuffer;
}

bool CNetClient::GetNetworkStatistics(NetStatistics* pDest)
{
    if (pDest == nullptr)
        return false;

    memset(pDest, 0, sizeof(NetStatistics));

    if (m_pPeer == nullptr)
        return false;

    pDest->bytesReceived = m_pPeer->incomingDataTotal;
    pDest->bytesSent = m_pPeer->outgoingDataTotal;
    pDest->packetsReceived = m_pHost != nullptr ? m_pHost->totalReceivedPackets : 0;
    pDest->packetsSent = m_pHost != nullptr ? m_pHost->totalSentPackets : 0;
    pDest->packetlossTotal = static_cast<float>(m_pPeer->packetLoss) * 100.0f / ENET_PEER_PACKET_LOSS_SCALE;
    pDest->packetlossLastSecond = pDest->packetlossTotal;

    return true;
}

const SPacketStat* CNetClient::GetPacketStats()
{
    return &m_PacketStats[0][0];
}

int CNetClient::GetPing()
{
    return m_pPeer != nullptr ? static_cast<int>(m_pPeer->roundTripTime) : 0;
}

unsigned long CNetClient::GetTime()
{
    return static_cast<unsigned long>(enet_time_get());
}

const char* CNetClient::GetLocalIP()
{
    if (m_pHost != nullptr)
    {
        enet_uint32 h = m_pHost->address.host;
        m_strLocalIP = SString("%u.%u.%u.%u", h & 0xFF, (h >> 8) & 0xFF, (h >> 16) & 0xFF, (h >> 24) & 0xFF);
    }
    else
    {
        m_strLocalIP = "0.0.0.0";
    }
    return *m_strLocalIP;
}

void CNetClient::GetSerial(char* szSerial, size_t maxLength)
{
    if (szSerial == nullptr || maxLength == 0)
        return;

    if (m_strSerial.empty())
    {
        SString strPath = CalcMTASAPath(PathJoin("mta", "serial.dat"));
        SString strLoaded;
        bool     bValid = SharedUtil::FileLoad(strPath, strLoaded) && strLoaded.length() == 32;
        if (bValid)
        {
            for (char c : strLoaded)
            {
                if (!isxdigit(static_cast<unsigned char>(c)))
                {
                    bValid = false;
                    break;
                }
            }
        }

        if (bValid)
        {
            m_strSerial = strLoaded.ToUpper();
        }
        else
        {
            static const char szHexChars[] = "0123456789ABCDEF";
            char               buffer[33];
            for (int i = 0; i < 32; i++)
                buffer[i] = szHexChars[rand() % 16];
            buffer[32] = '\0';
            m_strSerial = buffer;

            SharedUtil::MakeSureDirExists(strPath);
            SharedUtil::FileSave(strPath, m_strSerial);
        }
    }

#ifdef WIN32
    strncpy_s(szSerial, maxLength, m_strSerial.c_str(), _TRUNCATE);
#else
    strncpy(szSerial, m_strSerial.c_str(), maxLength - 1);
    szSerial[maxLength - 1] = '\0';
#endif
}

unsigned char CNetClient::GetConnectionError()
{
    return m_ucConnectionError;
}

void CNetClient::SetConnectionError(unsigned char ucConnectionError)
{
    m_ucConnectionError = ucConnectionError;
}

void CNetClient::Reset()
{
    m_ucConnectionError = 0;
    m_bConnected = false;
    m_bReady = false;
    m_usServerBitStreamVersion = 0;
    memset(m_PacketStats, 0, sizeof(m_PacketStats));

    for (auto& pair : m_DownloadManagers)
    {
        if (pair.first > EDownloadMode::CORE_LAST)
            pair.second->Reset();
    }
}

CNetHTTPDownloadManagerInterface* CNetClient::GetHTTPDownloadManager(EDownloadModeType iMode)
{
    auto it = m_DownloadManagers.find(iMode);
    if (it != m_DownloadManagers.end())
        return it->second;

    CNetHTTPDownloadManager* pManager = new CNetHTTPDownloadManager();
    m_DownloadManagers[iMode] = pManager;
    return pManager;
}

void CNetClient::SetServerBitStreamVersion(unsigned short usServerBitStreamVersion)
{
    m_usServerBitStreamVersion = usServerBitStreamVersion;
}

unsigned short CNetClient::GetServerBitStreamVersion()
{
    return m_usServerBitStreamVersion;
}

void CNetClient::GetStatus(char* szStatus, size_t maxLength)
{
    if (szStatus == nullptr || maxLength == 0)
        return;

    const char* szText = m_bConnected ? "Connected" : (m_pHost != nullptr ? "Connecting" : "Disconnected");

#ifdef WIN32
    strncpy_s(szStatus, maxLength, szText, _TRUNCATE);
#else
    strncpy(szStatus, szText, maxLength - 1);
    szStatus[maxLength - 1] = '\0';
#endif
}

unsigned short CNetClient::GetNetRev()
{
    return 0;
}

unsigned short CNetClient::GetNetRel()
{
    return 0;
}

const char* CNetClient::GetNextBuffer()
{
    return "";
}

const char* CNetClient::GetDiagnosticStatus()
{
    return *m_strDiagnosticStatus;
}

void CNetClient::UpdatePingStatus(const char* status, size_t statusLength, ushort& usDataRef, bool& isVerified)
{
    isVerified = true;
}

bool CNetClient::VerifySignature(const char* pData, unsigned long ulSize)
{
    return true;
}

void CNetClient::ResetStub(DWORD dwType, ...)
{
}

void CNetClient::ResetStub(DWORD dwType, va_list)
{
}

const char* CNetClient::GetCurrentServerId(bool bPreviousVer)
{
    return *m_strCurrentServerId;
}

bool CNetClient::CheckFile(const char* szType, const char* szFilename, const void* pData, size_t sizeData)
{
    return true;
}

uint CNetClient::GetExtendedErrorCode()
{
    return m_uiExtendedErrorCode;
}

void CNetClient::SetTimeoutTime(uint uiTimeoutTime)
{
    m_uiTimeoutTime = uiTimeoutTime;
    if (m_pPeer != nullptr)
        enet_peer_timeout(m_pPeer, 0, m_uiTimeoutTime, m_uiTimeoutTime * 2);
}

bool CNetClient::ValidateBinaryFileName(const char* szFilename)
{
    return szFilename != nullptr && szFilename[0] != '\0';
}

CBinaryFileInterface* CNetClient::AllocateBinaryFile()
{
    return new CNetBinaryFile();
}

bool CNetClient::EncryptDumpfile(const char* szClearPathFilename, const char* szEncryptedPathFilename)
{
    return false;
}

bool CNetClient::DeobfuscateScript(const char* cpInBuffer, uint uiInSize, const char** pcpOutBuffer, uint* puiOutSize, const char* szScriptName)
{
    if (pcpOutBuffer != nullptr)
        *pcpOutBuffer = cpInBuffer;
    if (puiOutSize != nullptr)
        *puiOutSize = uiInSize;
    return true;
}

void CNetClient::PostCrash()
{
}

int CNetClient::SendTo(SOCKET s, const char* buf, int len, int flags, const struct sockaddr* to, int tolen)
{
    return sendto(s, buf, len, flags, to, tolen);
}

#pragma once

#include "../../vendor/enet/enet.h"
#include <cstddef>
#include <cstring>

enum ENetChannelId : enet_uint8
{
    NET_CHANNEL_UNRELIABLE = 0,

    NET_CHANNEL_DEFAULT_UNRELIABLE_SEQUENCED,
    NET_CHANNEL_DEFAULT_RELIABLE_ORDERED,
    NET_CHANNEL_DEFAULT_RELIABLE_SEQUENCED,

    NET_CHANNEL_CHAT_UNRELIABLE_SEQUENCED,
    NET_CHANNEL_CHAT_RELIABLE_ORDERED,
    NET_CHANNEL_CHAT_RELIABLE_SEQUENCED,

    NET_CHANNEL_DATA_TRANSFER_UNRELIABLE_SEQUENCED,
    NET_CHANNEL_DATA_TRANSFER_RELIABLE_ORDERED,
    NET_CHANNEL_DATA_TRANSFER_RELIABLE_SEQUENCED,

    NET_CHANNEL_VOICE_UNRELIABLE_SEQUENCED,
    NET_CHANNEL_VOICE_RELIABLE_ORDERED,
    NET_CHANNEL_VOICE_RELIABLE_SEQUENCED,

    NET_CHANNEL_COUNT
};

struct SNetChannelAndFlags
{
    enet_uint8  channel;
    enet_uint32 flags;
};

inline SNetChannelAndFlags GetNetChannelAndFlags(int iReliability, int iOrdering)
{
    if (iReliability == 0)
        return {NET_CHANNEL_UNRELIABLE, ENET_PACKET_FLAG_UNSEQUENCED};

    enet_uint8 baseChannel;
    switch (iOrdering)
    {
        case 1:
            baseChannel = NET_CHANNEL_CHAT_UNRELIABLE_SEQUENCED;
            break;
        case 2:
            baseChannel = NET_CHANNEL_DATA_TRANSFER_UNRELIABLE_SEQUENCED;
            break;
        case 3:
            baseChannel = NET_CHANNEL_VOICE_UNRELIABLE_SEQUENCED;
            break;
        default:
            baseChannel = NET_CHANNEL_DEFAULT_UNRELIABLE_SEQUENCED;
            break;
    }

    if (iReliability == 1)
        return {baseChannel, 0};

    const bool       bSequenced = (iReliability == 4);
    const enet_uint8 channel = bSequenced ? static_cast<enet_uint8>(baseChannel + 2) : static_cast<enet_uint8>(baseChannel + 1);
    return {channel, ENET_PACKET_FLAG_RELIABLE};
}

inline ENetPacket* BuildNetPacket(unsigned char ucPacketID, const unsigned char* pData, size_t uiLength, enet_uint32 flags)
{
    ENetPacket* pPacket = enet_packet_create(nullptr, uiLength + 1, flags);
    if (pPacket == nullptr)
        return nullptr;

    pPacket->data[0] = ucPacketID;
    if (uiLength > 0 && pData != nullptr)
        std::memcpy(pPacket->data + 1, pData, uiLength);
    return pPacket;
}

inline unsigned char ExtractNetPacketId(const ENetPacket* pPacket)
{
    return (pPacket != nullptr && pPacket->dataLength > 0) ? pPacket->data[0] : 0;
}

inline const unsigned char* GetNetPacketPayload(const ENetPacket* pPacket)
{
    return (pPacket != nullptr && pPacket->dataLength > 1) ? pPacket->data + 1 : nullptr;
}

inline size_t GetNetPacketPayloadLength(const ENetPacket* pPacket)
{
    return (pPacket != nullptr && pPacket->dataLength > 0) ? pPacket->dataLength - 1 : 0;
}

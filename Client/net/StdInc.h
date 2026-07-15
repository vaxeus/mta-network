#pragma once

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winsock2.h>

#define MTA_CLIENT
#include "SharedUtil.h"

#include <cstdio>
#include <cstring>
#include <map>
#include <vector>

#include "../../vendor/enet/enet.h"
#include "../../Shared/net/NetChannelMap.h"
#include "../../Shared/net/CNetBitStream.h"
#include "../../Shared/net/CNetHTTPDownloadManager.h"

#include <net/CNet.h>

#include "CNetClient.h"

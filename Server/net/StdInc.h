#pragma once

#ifdef WIN32
    #define WIN32_LEAN_AND_MEAN
    #include <windows.h>
#else
    #include <limits.h>
#endif

#include <cstdio>
#include <cstring>
#include <map>

#include "SharedUtil.h"

#include "../../vendor/enet/enet.h"
#include "../../Shared/net/NetChannelMap.h"
#include "../../Shared/net/CNetBitStream.h"
#include "../../Shared/net/CNetHTTPDownloadManager.h"

#include <net/CNetServer.h>

#include "CNetServerPeerManager.h"
#include "CNetServerImpl.h"

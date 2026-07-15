#include "StdInc.h"
#include "../../Shared/sdk/version.h"
#define ALLOC_STATS_MODULE_NAME "net"
#include "SharedUtil.hpp"

CNetServerImpl* g_pNetServerImpl = nullptr;

MTAEXPORT CNetServer* InitNetServerInterface()
{
    if (g_pNetServerImpl == nullptr)
        g_pNetServerImpl = new CNetServerImpl();

    return g_pNetServerImpl;
}

MTAEXPORT void ReleaseNetServerInterface()
{
    if (g_pNetServerImpl != nullptr)
    {
        delete g_pNetServerImpl;
        g_pNetServerImpl = nullptr;
    }
}

MTAEXPORT unsigned long CheckCompatibility(unsigned long ulVersion, unsigned long* pulVersion)
{
    if (ulVersion == 1)
    {
        if (pulVersion != nullptr)
            *pulVersion = MTA_DM_SERVER_NET_MODULE_VERSION;
        return 1;
    }

    return (ulVersion == MTA_DM_SERVER_NET_MODULE_VERSION) ? 1 : 0;
}

#ifdef WIN32

int WINAPI DllMain(HINSTANCE hModule, DWORD dwReason, PVOID pvNothing)
{
    if (dwReason == DLL_PROCESS_DETACH)
    {
        ReleaseNetServerInterface();
    }

    return TRUE;
}

#else

void __attribute__((destructor)) ReleaseNetServerInterface(void);

#endif

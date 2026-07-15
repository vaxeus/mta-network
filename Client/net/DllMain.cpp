#include "StdInc.h"
#include "../../Shared/sdk/version.h"
#define ALLOC_STATS_MODULE_NAME "netc"
#include "SharedUtil.hpp"

CNetClient* g_pNetClient = nullptr;

MTAEXPORT CNet* InitNetInterface(CCore* pCore)
{
    if (g_pNetClient == nullptr)
        g_pNetClient = new CNetClient(pCore);

    return g_pNetClient;
}

MTAEXPORT void InitNetRev(const char* szRegistryPath, const char* szCommonDataDir, const char* szProductVersion)
{
}

MTAEXPORT bool CheckService(unsigned int uiStage)
{
    return true;
}

MTAEXPORT unsigned long CheckCompatibility(unsigned long ulVersion, unsigned long* pulVersion)
{
    if (ulVersion == 1)
    {
        if (pulVersion != nullptr)
            *pulVersion = MTA_DM_CLIENT_NET_MODULE_VERSION;
        return 1;
    }

    return (ulVersion == MTA_DM_CLIENT_NET_MODULE_VERSION) ? 1 : 0;
}

int WINAPI DllMain(HINSTANCE hModule, DWORD dwReason, PVOID pvNothing)
{
    if (dwReason == DLL_PROCESS_DETACH && g_pNetClient != nullptr)
    {
        delete g_pNetClient;
        g_pNetClient = nullptr;
    }

    return TRUE;
}

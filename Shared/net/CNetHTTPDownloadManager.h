#pragma once

#include "../sdk/net/CNetHTTPDownloadManagerInterface.h"
#include "../../vendor/curl/include/curl/curl.h"
#include <cstdio>
#include <map>
#include <vector>

class CNetHTTPDownloadManager final : public CNetHTTPDownloadManagerInterface {
public:
    CNetHTTPDownloadManager();
    ~CNetHTTPDownloadManager() override;

    uint        GetDownloadSizeNow() override;
    void        ResetDownloadSize() override;
    const char* GetError() override;
    bool        ProcessQueuedFiles() override;
    bool        QueueFile(const char* szURL, const char* szOutputFile, void* objectPtr = NULL, PFN_DOWNLOAD_FINISHED_CALLBACK pfnDownloadFinishedCallback = NULL, const SHttpRequestOptionsTx&   options = SHttpRequestOptionsTx()) override;
    void        SetMaxConnections(int iMaxConnections) override;
    void        Reset() override;
    bool        CancelDownload(void* objectPtr, PFN_DOWNLOAD_FINISHED_CALLBACK pfnDownloadFinishedCallback) override;
    bool        GetDownloadStatus(void* objectPtr, PFN_DOWNLOAD_FINISHED_CALLBACK pfnDownloadFinishedCallback, SDownloadStatus& outDownloadStatus) override;

private:
    struct SDownloadItem
    {
        CURL*                           pEasy = nullptr;
        void*                           objectPtr = nullptr;
        PFN_DOWNLOAD_FINISHED_CALLBACK  pfnCallback = nullptr;
        SString                         strUrl;
        SString                         strOutputFile;
        SHttpRequestOptions             options;
        FILE*                           pFile = nullptr;
        std::vector<unsigned char>      memoryBuffer;
        std::string                     headerText;
        SDownloadStatus                 status;
        curl_slist*                     pHeaderList = nullptr;
        curl_mime*                      pMime = nullptr;
        bool                            bAddedToMultiHandle = false;
    };

    void        PromoteQueuedItems();
    void        ApplyOptions(SDownloadItem* pItem);
    void        FinishItem(SDownloadItem* pItem, bool bSuccess, int iErrorCode);
    void        DestroyItem(SDownloadItem* pItem);
    SDownloadItem* FindItem(void* objectPtr, PFN_DOWNLOAD_FINISHED_CALLBACK pfnCallback);

    static size_t WriteCallback(char* ptr, size_t size, size_t nmemb, void* userdata);
    static size_t HeaderCallback(char* ptr, size_t size, size_t nmemb, void* userdata);

    CURLM*                       m_pMultiHandle;
    std::vector<SDownloadItem*>  m_QueuedItems;
    std::map<CURL*, SDownloadItem*> m_ActiveItems;
    int                          m_iMaxConnections;
    uint                         m_uiTotalDownloadedBytes;
    SString                      m_strLastError;
};

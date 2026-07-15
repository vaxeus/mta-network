#include "CNetHTTPDownloadManager.h"
#include <algorithm>
#include <cstdlib>
#include <cstring>

#ifdef WIN32
    #define NET_STRNICMP _strnicmp
#else
    #define NET_STRNICMP strncasecmp
#endif

CNetHTTPDownloadManager::CNetHTTPDownloadManager() : m_pMultiHandle(curl_multi_init()), m_iMaxConnections(8), m_uiTotalDownloadedBytes(0) {}

CNetHTTPDownloadManager::~CNetHTTPDownloadManager()
{
    Reset();
    if (m_pMultiHandle != nullptr)
        curl_multi_cleanup(m_pMultiHandle);
}

uint CNetHTTPDownloadManager::GetDownloadSizeNow()
{
    uint uiTotal = 0;
    for (const auto& pair : m_ActiveItems)
        uiTotal += pair.second->status.uiBytesReceived;
    return uiTotal;
}

void CNetHTTPDownloadManager::ResetDownloadSize()
{
    m_uiTotalDownloadedBytes = 0;
}

const char* CNetHTTPDownloadManager::GetError()
{
    return m_strLastError.c_str();
}

size_t CNetHTTPDownloadManager::WriteCallback(char* ptr, size_t size, size_t nmemb, void* userdata)
{
    SDownloadItem* pItem = reinterpret_cast<SDownloadItem*>(userdata);
    size_t         uiBytes = size * nmemb;

    if (pItem->pFile != nullptr)
        std::fwrite(ptr, 1, uiBytes, pItem->pFile);
    else
        pItem->memoryBuffer.insert(pItem->memoryBuffer.end(), ptr, ptr + uiBytes);

    pItem->status.uiBytesReceived += static_cast<uint>(uiBytes);
    return uiBytes;
}

size_t CNetHTTPDownloadManager::HeaderCallback(char* ptr, size_t size, size_t nmemb, void* userdata)
{
    SDownloadItem* pItem = reinterpret_cast<SDownloadItem*>(userdata);
    size_t         uiBytes = size * nmemb;
    pItem->headerText.append(ptr, uiBytes);

    if (uiBytes > 15 && NET_STRNICMP(ptr, "Content-Length:", 15) == 0)
    {
        long lLength = std::atol(ptr + 15);
        if (lLength > 0)
            pItem->status.uiContentLength = static_cast<uint>(lLength);
    }
    return uiBytes;
}

void CNetHTTPDownloadManager::ApplyOptions(SDownloadItem* pItem)
{
    CURL*                pEasy = pItem->pEasy;
    const SHttpRequestOptions& options = pItem->options;

    curl_easy_setopt(pEasy, CURLOPT_URL, pItem->strUrl.c_str());
    curl_easy_setopt(pEasy, CURLOPT_WRITEFUNCTION, &CNetHTTPDownloadManager::WriteCallback);
    curl_easy_setopt(pEasy, CURLOPT_WRITEDATA, pItem);
    curl_easy_setopt(pEasy, CURLOPT_HEADERFUNCTION, &CNetHTTPDownloadManager::HeaderCallback);
    curl_easy_setopt(pEasy, CURLOPT_HEADERDATA, pItem);
    curl_easy_setopt(pEasy, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(pEasy, CURLOPT_MAXREDIRS, static_cast<long>(options.uiMaxRedirects));
    curl_easy_setopt(pEasy, CURLOPT_CONNECTTIMEOUT_MS, static_cast<long>(options.uiConnectTimeoutMs));
    curl_easy_setopt(pEasy, CURLOPT_FAILONERROR, options.bIsLegacy ? 1L : 0L);
    curl_easy_setopt(pEasy, CURLOPT_NOSIGNAL, 1L);

    if (!options.bIsLocal)
    {
        curl_easy_setopt(pEasy, CURLOPT_LOW_SPEED_LIMIT, 10L);
        curl_easy_setopt(pEasy, CURLOPT_LOW_SPEED_TIME, static_cast<long>(options.uiConnectTimeoutMs / 1000));
    }

    if (!options.strRequestMethod.empty())
        curl_easy_setopt(pEasy, CURLOPT_CUSTOMREQUEST, options.strRequestMethod.c_str());

    if (!options.strUsername.empty())
    {
        curl_easy_setopt(pEasy, CURLOPT_USERNAME, options.strUsername.c_str());
        curl_easy_setopt(pEasy, CURLOPT_PASSWORD, options.strPassword.c_str());
    }

    if (!options.formFields.empty())
    {
        pItem->pMime = curl_mime_init(pEasy);
        for (const auto& field : options.formFields)
        {
            curl_mimepart* pPart = curl_mime_addpart(pItem->pMime);
            curl_mime_name(pPart, field.first.c_str());
            curl_mime_data(pPart, field.second.c_str(), field.second.length());
        }
        curl_easy_setopt(pEasy, CURLOPT_MIMEPOST, pItem->pMime);
    }
    else if (!options.strPostData.empty())
    {
        if (options.bPostBinary)
        {
            curl_easy_setopt(pEasy, CURLOPT_POSTFIELDS, options.strPostData.data());
            curl_easy_setopt(pEasy, CURLOPT_POSTFIELDSIZE, static_cast<long>(options.strPostData.length()));
        }
        else
        {
            curl_easy_setopt(pEasy, CURLOPT_POSTFIELDS, options.strPostData.c_str());
        }
    }

    if (!options.requestHeaders.empty())
    {
        for (const auto& header : options.requestHeaders)
        {
            SString strLine("%s: %s", header.first.c_str(), header.second.c_str());
            pItem->pHeaderList = curl_slist_append(pItem->pHeaderList, strLine.c_str());
        }
        curl_easy_setopt(pEasy, CURLOPT_HTTPHEADER, pItem->pHeaderList);
    }

    if (options.bResumeFile && pItem->pFile != nullptr)
    {
        long lExistingSize = static_cast<long>(ftell(pItem->pFile));
        if (lExistingSize > 0)
            curl_easy_setopt(pEasy, CURLOPT_RESUME_FROM_LARGE, static_cast<curl_off_t>(lExistingSize));
    }
}

bool CNetHTTPDownloadManager::QueueFile(const char* szURL, const char* szOutputFile, void* objectPtr, PFN_DOWNLOAD_FINISHED_CALLBACK pfnDownloadFinishedCallback, const SHttpRequestOptionsTx& optionsTx)
{
    SDownloadItem* pItem = new SDownloadItem();
    pItem->strUrl = szURL;
    pItem->strOutputFile = szOutputFile ? szOutputFile : "";
    pItem->objectPtr = objectPtr;
    pItem->pfnCallback = pfnDownloadFinishedCallback;
    pItem->options = SHttpRequestOptions(optionsTx);

    if (!pItem->strOutputFile.empty())
    {
        pItem->pFile = fopen(pItem->strOutputFile.c_str(), pItem->options.bResumeFile ? "ab" : "wb");
        if (pItem->pFile == nullptr)
        {
            m_strLastError = SString("Could not open output file '%s' for writing", pItem->strOutputFile.c_str());
            delete pItem;
            return false;
        }
    }

    pItem->pEasy = curl_easy_init();
    if (pItem->pEasy == nullptr)
    {
        m_strLastError = "curl_easy_init failed";
        DestroyItem(pItem);
        return false;
    }

    ApplyOptions(pItem);
    m_QueuedItems.push_back(pItem);
    return true;
}

void CNetHTTPDownloadManager::PromoteQueuedItems()
{
    while (!m_QueuedItems.empty() && static_cast<int>(m_ActiveItems.size()) < m_iMaxConnections)
    {
        SDownloadItem* pItem = m_QueuedItems.front();
        m_QueuedItems.erase(m_QueuedItems.begin());

        curl_multi_add_handle(m_pMultiHandle, pItem->pEasy);
        pItem->bAddedToMultiHandle = true;
        pItem->status.uiAttemptNumber = 1;
        m_ActiveItems[pItem->pEasy] = pItem;
    }
}

void CNetHTTPDownloadManager::FinishItem(SDownloadItem* pItem, bool bSuccess, int iErrorCode)
{
    SHttpDownloadResult result{};
    result.pObj = pItem->objectPtr;
    result.bSuccess = bSuccess;
    result.iErrorCode = iErrorCode;
    result.uiAttemptNumber = pItem->status.uiAttemptNumber;
    result.uiContentLength = pItem->status.uiContentLength;
    result.szHeaders = pItem->headerText.c_str();

    if (bSuccess && pItem->pFile == nullptr)
    {
        result.pData = pItem->memoryBuffer.empty() ? "" : reinterpret_cast<const char*>(pItem->memoryBuffer.data());
        result.dataSize = pItem->memoryBuffer.size();
    }
    else
    {
        result.pData = nullptr;
        result.dataSize = 0;
    }

    if (pItem->pfnCallback != nullptr)
        pItem->pfnCallback(result);

    DestroyItem(pItem);
}

void CNetHTTPDownloadManager::DestroyItem(SDownloadItem* pItem)
{
    if (pItem->bAddedToMultiHandle && m_pMultiHandle != nullptr)
        curl_multi_remove_handle(m_pMultiHandle, pItem->pEasy);
    if (pItem->pEasy != nullptr)
        curl_easy_cleanup(pItem->pEasy);
    if (pItem->pHeaderList != nullptr)
        curl_slist_free_all(pItem->pHeaderList);
    if (pItem->pMime != nullptr)
        curl_mime_free(pItem->pMime);
    if (pItem->pFile != nullptr)
        fclose(pItem->pFile);
    delete pItem;
}

bool CNetHTTPDownloadManager::ProcessQueuedFiles()
{
    PromoteQueuedItems();

    if (m_pMultiHandle == nullptr)
        return true;

    int iStillRunning = 0;
    curl_multi_perform(m_pMultiHandle, &iStillRunning);

    int         iMessagesLeft = 0;
    CURLMsg*    pMsg = nullptr;
    while ((pMsg = curl_multi_info_read(m_pMultiHandle, &iMessagesLeft)) != nullptr)
    {
        if (pMsg->msg != CURLMSG_DONE)
            continue;

        auto it = m_ActiveItems.find(pMsg->easy_handle);
        if (it == m_ActiveItems.end())
            continue;

        SDownloadItem* pItem = it->second;
        m_ActiveItems.erase(it);

        long lResponseCode = 0;
        curl_easy_getinfo(pMsg->easy_handle, CURLINFO_RESPONSE_CODE, &lResponseCode);

        bool bSuccess = (pMsg->data.result == CURLE_OK);
        int  iErrorCode = bSuccess || lResponseCode >= 400 ? static_cast<int>(lResponseCode) : static_cast<int>(pMsg->data.result);
        if (!bSuccess)
            m_strLastError = curl_easy_strerror(pMsg->data.result);
        if (bSuccess && lResponseCode >= 400 && lResponseCode <= 599)
            bSuccess = !pItem->options.bIsLegacy;

        m_uiTotalDownloadedBytes += pItem->status.uiBytesReceived;
        FinishItem(pItem, bSuccess, iErrorCode);
    }

    PromoteQueuedItems();
    return m_QueuedItems.empty() && m_ActiveItems.empty();
}

void CNetHTTPDownloadManager::SetMaxConnections(int iMaxConnections)
{
    m_iMaxConnections = iMaxConnections > 0 ? iMaxConnections : 1;
}

void CNetHTTPDownloadManager::Reset()
{
    for (SDownloadItem* pItem : m_QueuedItems)
        DestroyItem(pItem);
    m_QueuedItems.clear();

    for (auto& pair : m_ActiveItems)
        DestroyItem(pair.second);
    m_ActiveItems.clear();

    m_uiTotalDownloadedBytes = 0;
    m_strLastError = "";
}

CNetHTTPDownloadManager::SDownloadItem* CNetHTTPDownloadManager::FindItem(void* objectPtr, PFN_DOWNLOAD_FINISHED_CALLBACK pfnCallback)
{
    for (SDownloadItem* pItem : m_QueuedItems)
    {
        if (pItem->objectPtr == objectPtr && pItem->pfnCallback == pfnCallback)
            return pItem;
    }
    for (auto& pair : m_ActiveItems)
    {
        if (pair.second->objectPtr == objectPtr && pair.second->pfnCallback == pfnCallback)
            return pair.second;
    }
    return nullptr;
}

bool CNetHTTPDownloadManager::CancelDownload(void* objectPtr, PFN_DOWNLOAD_FINISHED_CALLBACK pfnDownloadFinishedCallback)
{
    SDownloadItem* pItem = FindItem(objectPtr, pfnDownloadFinishedCallback);
    if (pItem == nullptr)
        return false;

    if (pItem->bAddedToMultiHandle)
        m_ActiveItems.erase(pItem->pEasy);
    else
        m_QueuedItems.erase(std::find(m_QueuedItems.begin(), m_QueuedItems.end(), pItem));

    DestroyItem(pItem);
    return true;
}

bool CNetHTTPDownloadManager::GetDownloadStatus(void* objectPtr, PFN_DOWNLOAD_FINISHED_CALLBACK pfnDownloadFinishedCallback, SDownloadStatus& outDownloadStatus)
{
    SDownloadItem* pItem = FindItem(objectPtr, pfnDownloadFinishedCallback);
    if (pItem == nullptr)
        return false;

    outDownloadStatus = pItem->status;
    return true;
}

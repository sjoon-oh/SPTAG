// Author: Sukjoon Oh (sjoon@kaist.ac.kr)
// 

#include <utility>
#include <memory>
#include <iostream>

// #include <sys/syscall.h>
#include <linux/aio_abi.h>

#include "inc/Helper/DiskIO.h"
#include "inc/Extension/CacheLfuMt.hh"

using namespace SPTAG::EXT;


CacheLfuSpannMt::CacheLfuSpannMt(const size_t p_capacity, bool p_enableLock) noexcept
    : m_cacheCapacity(p_capacity), m_enableLock(p_enableLock), m_minFrequency(0)
{
    m_stats.resetAll();
}


CacheLfuSpannMt::~CacheLfuSpannMt() noexcept
{

}


bool
CacheLfuSpannMt::setItemCached(uintptr_t p_key, uint8_t* p_object, size_t p_size) noexcept
{
    size_t traceHandle = 0;
    if (m_enableLock)
        traceHandle = m_itemLock.getRefreshLock();
    
    if (isItemCached(p_key))
    {
        if (m_enableLock)
            m_itemLock.releaseRefreshLock(traceHandle);

        return false;
    }

    // 
    // Evict items until we have enough space for the new item
    size_t evictCount = 0;
    while (m_stats.getCurrentSize() + p_size > m_cacheCapacity)
    {
        uintptr_t evictKey = m_freqLruList[m_minFrequency].front();
        m_freqLruList[m_minFrequency].pop_front();  // Pop least recently used

        if (m_freqLruList.empty())
            m_freqLruList.erase(m_minFrequency);

        CacheItem<uintptr_t>& lruItem = (m_cachedItems[evictKey].m_item);
        uint64_t decrSize = lruItem.getSize();

        m_cachedItems.erase(evictKey);      
        m_posInFreqList.erase(evictKey);

        evictCount++;

        m_stats.decrCurrentSize(decrSize);
    }

    m_stats.incrEvictCount(evictCount);

    m_cachedItems.insert(
        std::make_pair(
            p_key, 
            CacheItemWrapper(CacheItem<uintptr_t>(p_key, p_object, p_size))
        )
    );
    
    m_minFrequency = 1;
    m_freqLruList[m_minFrequency].push_back(p_key);
    m_posInFreqList[p_key] = --m_freqLruList[m_minFrequency].end();

    if (m_enableLock)
        m_itemLock.releaseRefreshLock(traceHandle);

    return true;
}


SPTAG::EXT::CacheItem<uintptr_t>* 
CacheLfuSpannMt::getCachedItem(uintptr_t p_key) noexcept
{
    size_t traceHandle = 0;
    if (m_enableLock)
        traceHandle = m_itemLock.getSearchLock();

    if (!isItemCached(p_key))
    {
        m_stats.incrMissCount(1);
        if (m_enableLock)
            m_itemLock.releaseSearchLock(traceHandle);

        return nullptr; // Item not found
    }

    m_stats.incrHitCount(1);

    auto& item = m_cachedItems[p_key];
    size_t prevItemFrequency = item.m_frequency;
    
    item.m_frequency += 1;

    m_freqLruList[prevItemFrequency].erase(m_posInFreqList[p_key]);   // Erase from the LRU list
    if (m_freqLruList[prevItemFrequency].empty())
    {
        m_freqLruList.erase(prevItemFrequency);
        if (m_minFrequency == prevItemFrequency)
            m_minFrequency += 1;
    }

    m_freqLruList[item.m_frequency].push_back(p_key);
    m_posInFreqList[p_key] = --m_freqLruList[item.m_frequency].end(); // Update iterator

    if (m_enableLock)
        m_itemLock.releaseSearchLock(traceHandle);

    return &(item.m_item); // Return the item
}


const bool
CacheLfuSpannMt::isItemCached(uintptr_t p_key) noexcept
{
    return (m_cachedItems.find(p_key) != m_cachedItems.end());
}


SPTAG::EXT::CacheStatMt*
CacheLfuSpannMt::getCacheStat() noexcept
{
    return &(m_stats);
}


std::vector<CacheStatWeak>&
CacheLfuSpannMt::getCacheStatTrace() noexcept
{
    return m_statTrace;
}


SPTAG::EXT::SpinlockWithStat&
CacheLfuSpannMt::getSpinlockWithStat() noexcept
{
    return m_itemLock;
}


void
CacheLfuSpannMt::recordStatTrace() noexcept
{
    // uint64_t chkpHitCount = m_stats.getHitCount();
    // uint64_t chkpMissCount = m_stats.getMissCount();
    // uint64_t chkpEvictCount = m_stats.getEvictCount();
    // uint64_t chkpCurrentSize = m_stats.getCurrentSize();

    // CacheStatWeak recordInst(
    //     chkpHitCount, chkpMissCount, chkpEvictCount, chkpCurrentSize
    // );
    
    // m_statTrace.push_back(recordInst);
}


void
CacheLfuSpannMt::resetStat() noexcept
{
    m_stats.resetAll();
}


void
CacheLfuSpannMt::setDelayedToCache(
        size_t p_delayedNumToCache,
        std::vector<bool> p_delayedToCache,
        void* p_requests,
        int p_tid
    ) noexcept
{
    m_delayedToCacheArr[p_tid].m_delayedNumToCache = p_delayedNumToCache;
    m_delayedToCacheArr[p_tid].m_delayedToCache.reset(
        new std::vector<bool>(std::move(p_delayedToCache)));
    m_delayedToCacheArr[p_tid].m_requests = p_requests;
}


struct ListInfo
{
    std::size_t listTotalBytes = 0;
    int listEleCount = 0;
    std::uint16_t listPageCount = 0;
    std::uint64_t listOffset = 0;
    std::uint16_t pageOffset = 0;
};


void
CacheLfuSpannMt::refreshCacheBulkSingle(int p_tid) noexcept
{
    size_t traceHandle = 0;
    if (m_enableLock)
        traceHandle = m_itemLock.getRefreshLock();

    size_t localHits = 0;
    size_t totalSize = 0;
    struct PendingInfo& frontDelayed = m_delayedToCacheArr[p_tid];

    SPTAG::Helper::AsyncReadRequest* requests = 
        (SPTAG::Helper::AsyncReadRequest*)frontDelayed.m_requests;

    for (int i = 0; i < frontDelayed.m_delayedNumToCache; i++)
    {
        if ((*(frontDelayed.m_delayedToCache))[i] == false)
            localHits++;

        else
        {
            ListInfo* listInfo = (ListInfo*)(requests[i].m_payload);
            totalSize += requests[i].m_readSize;
        }
    }

    // 
    // Evict items until we have enough space for the new item
    size_t evictCount = 0;
    while (m_stats.getCurrentSize() + totalSize > m_cacheCapacity)
    {
        uintptr_t evictKey = m_freqLruList[m_minFrequency].front();
        m_freqLruList[m_minFrequency].pop_front();  // Pop least recently used

        if (m_freqLruList.empty())
            m_freqLruList.erase(m_minFrequency);

        CacheItem<uintptr_t>& lruItem = (m_cachedItems[evictKey].m_item);
        uint64_t decrSize = lruItem.getSize();

        m_cachedItems.erase(evictKey);      
        m_posInFreqList.erase(evictKey);

        evictCount++;

        m_stats.decrCurrentSize(decrSize);        
    }

    m_stats.incrEvictCount(evictCount);

    for (int i = 0; i < frontDelayed.m_delayedNumToCache; i++)
    {
        if ((*(frontDelayed.m_delayedToCache))[i] == false)
            ;
        else
        {   
            ListInfo* listInfo = (ListInfo*)(requests[i].m_payload);
            uintptr_t key = static_cast<uintptr_t>(requests[i].m_offset) + listInfo->pageOffset;
            
            // m_cachedItems[key] = std::pair<uintptr_t, CacheItemWrapper>(
            //     key, CacheItemWrapper(CacheItem<uintptr_t>(key, reinterpret_cast<uint8_t*>(requests[i].m_buffer), requests[i].m_readSize))
            // );

            // m_cachedItems[p_key] = std::pair<uintptr_t, CacheItemWrapper>(
            //     p_key, CacheItemWrapper(CacheItem<uintptr_t>(p_key, p_object, p_size))
            // );   

            m_cachedItems.insert(
                std::make_pair(
                    key, 
                    CacheItemWrapper(CacheItem<uintptr_t>(key, reinterpret_cast<uint8_t*>(requests[i].m_buffer), requests[i].m_readSize))
                )
            );
            
            m_minFrequency = 1;
            m_freqLruList[m_minFrequency].push_back(key);
            m_posInFreqList[key] = --m_freqLruList[m_minFrequency].end();

            m_stats.incrCurrentSize(requests[i].m_readSize);
        }
    }
    
    
    // std::cout << "cursz: " << m_stats.getCurrentSize() << std::endl;

    double localHitRatio = (localHits * 1.0) / frontDelayed.m_delayedToCache->size();

    // std::cout << "local count: " <<  localHits << ", over " << frontDelayed.m_delayedToCache->size() << ", local HR: "<< localHitRatio << "\n";

    m_statTrace.emplace_back(
        m_stats.getHitCount(), m_stats.getMissCount(), m_stats.getEvictCount(), m_stats.getCurrentSize(), localHitRatio
    );
    
    if (m_enableLock)
        m_itemLock.releaseRefreshLock(traceHandle);
}





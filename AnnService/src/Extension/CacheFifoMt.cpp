#pragma once
// Author: Sukjoon Oh (sjoon@kaist.ac.kr)
// 

#include <utility>
#include <memory>
#include <iostream>

// #include <sys/syscall.h>
#include <linux/aio_abi.h>

#include "inc/Helper/DiskIO.h"
#include "inc/Extension/CacheFifoMt.hh"

using namespace SPTAG::EXT;


CacheFifoSpannMt::CacheFifoSpannMt(const size_t p_capacity, bool p_enableLock) noexcept
    : m_cacheCapacity(p_capacity), m_enableLock(p_enableLock)
{
    m_stats.resetAll();
}


CacheFifoSpannMt::~CacheFifoSpannMt() noexcept
{

}


bool
CacheFifoSpannMt::setItemCached(uintptr_t p_key, uint8_t* p_object, size_t p_size) noexcept
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
    while (m_stats.getCurrentSize() + p_size > m_cacheCapacity)
    {
        CacheItem<uintptr_t>* lruItem = &(m_usageList.back());
        uint64_t decrSize = lruItem->getSize();
        
        m_cachedItems.erase(lruItem->getKey());
        m_usageList.pop_back();
        m_stats.incrEvictCount(1);

        m_stats.decrCurrentSize(decrSize);
    }

    m_usageList.emplace_front(p_key, p_object, p_size);

    m_cachedItems[p_key] = m_usageList.begin();
    m_stats.incrCurrentSize(p_size);

    if (m_enableLock)
        m_itemLock.releaseRefreshLock(traceHandle);

    return true;
}


SPTAG::EXT::CacheItem<uintptr_t>* 
CacheFifoSpannMt::getCachedItem(uintptr_t p_key) noexcept
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

    if (m_enableLock)
        m_itemLock.releaseSearchLock(traceHandle);

    return &(*(m_cachedItems[p_key])); // Return the item
}


const bool
CacheFifoSpannMt::isItemCached(uintptr_t p_key) noexcept
{
    return (m_cachedItems.find(p_key) != m_cachedItems.end());
}


SPTAG::EXT::CacheStatMt*
CacheFifoSpannMt::getCacheStat() noexcept
{
    return &(m_stats);
}


std::vector<CacheStatWeak>&
CacheFifoSpannMt::getCacheStatTrace() noexcept
{
    return m_statTrace;
}


SPTAG::EXT::SpinlockWithStat&
CacheFifoSpannMt::getSpinlockWithStat() noexcept
{
    return m_itemLock;
}


void
CacheFifoSpannMt::recordStatTrace() noexcept
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
CacheFifoSpannMt::resetStat() noexcept
{
    m_stats.resetAll();
    
    // 
}


void
CacheFifoSpannMt::setDelayedToCache(
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
CacheFifoSpannMt::refreshCacheBulkSingle(int p_tid) noexcept
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

    size_t evictCount = 0;
    while ((m_stats.getCurrentSize() + totalSize) > m_cacheCapacity)
    {
        CacheItem<uintptr_t>* lruItem = &(m_usageList.back());
        uint64_t decrSize = lruItem->getSize();
        
        m_cachedItems.erase(lruItem->getKey());
        m_usageList.pop_back();

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
            
            m_usageList.emplace_front(key, reinterpret_cast<uint8_t*>(requests[i].m_buffer), requests[i].m_readSize);
            m_cachedItems[key] = m_usageList.begin();

            m_stats.incrCurrentSize(requests[i].m_readSize);
        }
    }

    // std::cout << "cursz: " << m_stats.getCurrentSize() << std::endl;

    double localHitRatio = (localHits * 1.0) / frontDelayed.m_delayedToCache->size();

    m_statTrace.emplace_back(
        m_stats.getHitCount(), m_stats.getMissCount(), m_stats.getEvictCount(), m_stats.getCurrentSize(), localHitRatio
    );
    
    if (m_enableLock)
        m_itemLock.releaseRefreshLock(traceHandle);
}





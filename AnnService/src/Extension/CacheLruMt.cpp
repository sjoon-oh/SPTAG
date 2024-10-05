// Author: Sukjoon Oh (sjoon@kaist.ac.kr)
// 

#include <utility>
#include <memory>
#include <iostream>

// #include <sys/syscall.h>
#include <linux/aio_abi.h>

#include "inc/Helper/DiskIO.h"

#include "inc/Extension/CacheLruWeak.hh"
#include "inc/Extension/CacheLruMt.hh"

using namespace SPTAG::EXT;


CacheStatMt::CacheStatMt() noexcept
    : m_hitCount(0), m_missCount(0), m_evictCount(0), m_currentSize(0)
{

}


CacheStatMt::~CacheStatMt() noexcept
{

}


uint64_t
CacheStatMt::getHitCount() const noexcept
{
    return m_hitCount.load();
}


uint64_t
CacheStatMt::getMissCount() const noexcept
{
    return m_missCount.load();
}


uint64_t
CacheStatMt::getEvictCount() const noexcept
{
    return m_evictCount.load();
}


uint64_t
CacheStatMt::getCurrentSize() const noexcept
{
    return m_currentSize.load();
}


void
CacheStatMt::incrHitCount(uint64_t p_val) noexcept
{
    m_hitCount.fetch_add(p_val);
}


void
CacheStatMt::incrMissCount(uint64_t p_val) noexcept
{
    m_missCount.fetch_add(p_val);
}


void
CacheStatMt::incrEvictCount(uint64_t p_val) noexcept
{
    m_evictCount.fetch_add(p_val);
}


void
CacheStatMt::incrCurrentSize(uint64_t p_val) noexcept
{
    m_currentSize.fetch_add(p_val);
}


void
CacheStatMt::decrCurrentSize(uint64_t p_val) noexcept
{
    m_currentSize.fetch_sub(p_val);
}


void
CacheStatMt::resetAll() noexcept
{
    m_hitCount.store(0);
    m_missCount.store(0);
    m_evictCount.store(0);
}


CacheLruSpannMt::CacheLruSpannMt(const size_t p_capacity) noexcept
    : m_cacheCapacity(p_capacity)
{
    m_stats.resetAll();
}


CacheLruSpannMt::~CacheLruSpannMt() noexcept
{

}


bool
CacheLruSpannMt::setItemCached(uintptr_t p_key, uint8_t* p_object, size_t p_size) noexcept
{
    size_t traceHandle = m_itemLock.getLock();
    if (isItemCached(p_key))
    {
        m_itemLock.releaseLock(traceHandle);
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

    m_itemLock.releaseLock(traceHandle);
    return true;
}


SPTAG::EXT::CacheItem<uintptr_t>* 
SPTAG::EXT::CacheLruSpannMt::getCachedItem(uintptr_t p_key) noexcept
{
    size_t traceHandle = m_itemLock.getLock();

    if (!isItemCached(p_key))
    {
        m_stats.incrMissCount(1);
        m_itemLock.releaseLock(traceHandle);
        return nullptr; // Item not found
    }

    // Move accessed item to front of usage list
    auto it = m_cachedItems[p_key];
    CacheItem<uintptr_t> itemToMove = std::move(*it); // Move the item out

    m_usageList.erase(it); // Remove from current position
    m_usageList.push_front(std::move(itemToMove)); // Add to front the previous item
    
    m_cachedItems[p_key] = m_usageList.begin(); // Update map
    m_stats.incrHitCount(1);

    m_itemLock.releaseLock(traceHandle);

    return &(*(m_cachedItems[p_key])); // Return the item
}


const bool
SPTAG::EXT::CacheLruSpannMt::isItemCached(uintptr_t p_key) noexcept
{
    return (m_cachedItems.find(p_key) != m_cachedItems.end());
}


SPTAG::EXT::CacheStatMt*
SPTAG::EXT::CacheLruSpannMt::getCacheStat() noexcept
{
    return &(m_stats);
}


std::vector<CacheStatWeak>&
SPTAG::EXT::CacheLruSpannMt::getCacheStatTrace() noexcept
{
    return m_statTrace;
}


SPTAG::EXT::SpinlockWithStat&
SPTAG::EXT::CacheLruSpannMt::getSpinlockWithStat() noexcept
{
    return m_itemLock;
}


void
SPTAG::EXT::CacheLruSpannMt::recordStatTrace() noexcept
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
SPTAG::EXT::CacheLruSpannMt::resetStat() noexcept
{
    m_stats.resetAll();
}


void
CacheLruSpannMt::setDelayedToCache(
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
CacheLruSpannMt::refreshCacheBulkSingle(int p_tid) noexcept
{
    size_t traceHandle = m_itemLock.getLock();

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
        // m_stats.incrEvictCount(1);

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
    
    m_itemLock.releaseLock(traceHandle);
}





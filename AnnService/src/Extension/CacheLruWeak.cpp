// Author: Sukjoon Oh (sjoon@kaist.ac.kr)
// 

#include <utility>
#include <memory>
#include <iostream>

// #include <sys/syscall.h>
#include <linux/aio_abi.h>

#include "inc/Extension/CacheLruWeak.hh"
#include "inc/Helper/DiskIO.h"

using namespace SPTAG::EXT;


CacheStatWeak::CacheStatWeak() noexcept
    : m_hitCount(0), m_missCount(0), m_evictCount(0), m_currentSize(0)
{

}

CacheStatWeak::CacheStatWeak(uint64_t p_hitCount, uint64_t p_missCount, uint64_t p_evictCount, uint64_t p_currentSize, double p_localHitRatio) noexcept
    : m_hitCount(p_hitCount), m_missCount(p_missCount), m_evictCount(p_evictCount), m_currentSize(p_currentSize), m_localHitRatio(p_localHitRatio)
{

}


CacheStatWeak::~CacheStatWeak() noexcept
{

}


uint64_t
CacheStatWeak::getHitCount() const noexcept
{
    return m_hitCount;
}


uint64_t
CacheStatWeak::getMissCount() const noexcept
{
    return m_missCount;
}


uint64_t
CacheStatWeak::getEvictCount() const noexcept
{
    return m_evictCount;
}


uint64_t
CacheStatWeak::getCurrentSize() const noexcept
{
    return m_currentSize;
}


double
CacheStatWeak::getLocalHitRatio() const noexcept
{
    return m_localHitRatio;
}


void
CacheStatWeak::incrHitCount(uint64_t p_val) noexcept
{
    m_hitCount += p_val;
}


void
CacheStatWeak::incrMissCount(uint64_t p_val) noexcept
{
    m_missCount += p_val;
}


void
CacheStatWeak::incrEvictCount(uint64_t p_val) noexcept
{
    m_evictCount += p_val;
}


void
CacheStatWeak::setCurrentSize(uint64_t p_val) noexcept
{
    m_currentSize = p_val;
}


void
CacheStatWeak::resetAll() noexcept
{
    m_hitCount = 0;
    m_missCount = 0;
    m_evictCount = 0;
}


CacheLruSpannSt::CacheLruSpannSt(const size_t p_size) noexcept
    : CacheLru<uintptr_t>(p_size), m_delayedNumToCache(0)
{
    m_delayPending = false;
}


CacheLruSpannSt::~CacheLruSpannSt() noexcept
{

}


void
CacheLruSpannSt::setDelayedToCache(
        size_t p_delayedNumToCache,
        std::vector<bool> p_delayedToCache,
        void* p_requests
    ) noexcept
{
    m_delayPending = true;
    m_delayedNumToCache = p_delayedNumToCache;
    m_delayedToCache.reset(new std::vector<bool>(std::move(p_delayedToCache)));

    // AsyncReadRequest* requests = p_requests;
    m_requests = p_requests;
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
CacheLruSpannSt::refreshCache() noexcept
{
    if (m_delayPending == true)
    {
        SPTAG::Helper::AsyncReadRequest* requests = (SPTAG::Helper::AsyncReadRequest*)m_requests;

        for (int i = 0; i < m_delayedNumToCache; i++)
        {
            if ((*m_delayedToCache)[i] == false)
            {
                ;
            }
            else
            {
                ListInfo* listInfo = (ListInfo*)(requests[i].m_payload);
                uintptr_t key = static_cast<uintptr_t>(requests[i].m_offset) + listInfo->pageOffset;

                bool res = setItemCached(
                    key, 
                    reinterpret_cast<uint8_t*>(requests[i].m_buffer),
                    requests[i].m_readSize);
            }
        }
    }
    
    m_delayPending = false;
}


void 
CacheLruSpannSt::refreshCacheBulk() noexcept
{
    if (m_delayPending == true)
    {   
        size_t totalSize = 0;
        SPTAG::Helper::AsyncReadRequest* requests = (SPTAG::Helper::AsyncReadRequest*)m_requests;

        for (int i = 0; i < m_delayedNumToCache; i++)
        {
            if ((*m_delayedToCache)[i] == false)
                ;
            else
            {   
                ListInfo* listInfo = (ListInfo*)(requests[i].m_payload);
                totalSize += requests[i].m_readSize;
            }
        }

        while (m_currentSize + totalSize > m_cacheCapacity)
        {
            CacheItem<uintptr_t>* lruItem = &(m_usageList.back());
            
            m_currentSize -= lruItem->getSize(); // Reduce current size by the evicted item's size
            m_cachedItems.erase(lruItem->getKey());
            m_usageList.pop_back();
            m_stats.incrEvictCount(1);
        }

        for (int i = 0; i < m_delayedNumToCache; i++)
        {
            if ((*m_delayedToCache)[i] == false)
                ;
            else
            {   
                ListInfo* listInfo = (ListInfo*)(requests[i].m_payload);
                uintptr_t key = static_cast<uintptr_t>(requests[i].m_offset) + listInfo->pageOffset;
                
                m_usageList.emplace_front(key, reinterpret_cast<uint8_t*>(requests[i].m_buffer), requests[i].m_readSize);

                m_cachedItems[key] = m_usageList.begin();
                m_currentSize += requests[i].m_readSize; // Update the current size

                m_stats.setCurrentSize(m_currentSize);
            }
        }
    }
    
    m_delayPending = false;
}
// Author: Sukjoon Oh (sjoon@kaist.ac.kr)
// 

#include <utility>
#include <memory>
#include <iostream>

// #include <sys/syscall.h>
#include <linux/aio_abi.h>

#include "inc/Extension/CacheLru.hh"
#include "inc/Helper/DiskIO.h"

using namespace SPTAG::EXT;


CacheStats::CacheStats()
    : m_hitCount(0), m_missCount(0), m_evictCount(0)
{

}


CacheStats::~CacheStats()
{

}


uint64_t
CacheStats::getHitCount() const 
{
    return m_hitCount;
}


uint64_t
CacheStats::getMissCount() const 
{
    return m_missCount;
}


uint64_t
CacheStats::getEvictCount() const
{
    return m_evictCount;
}


uint64_t
CacheStats::getCurrentSize() const
{
    return m_currentSize;
}


void
CacheStats::incrHitCount(uint64_t p_val)
{
    m_hitCount += p_val;
}


void
CacheStats::incrMissCount(uint64_t p_val)
{
    m_missCount += p_val;
}


void
CacheStats::incrEvictCount(uint64_t p_val)
{
    m_evictCount += p_val;
}


void
CacheStats::setCurrentSize(uint64_t p_val)
{
    m_currentSize = p_val;
}


CacheLruSPANN::CacheLruSPANN(const size_t p_size)
    : CacheLru<uintptr_t>(p_size), m_delayedNumToCache(0)
{
    m_delayPending = false;
}


CacheLruSPANN::~CacheLruSPANN()
{

}


void
CacheLruSPANN::setDelayedToCache(
        size_t p_delayedNumToCache,
        std::vector<bool> p_delayedToCache,
        void* p_requests
    )
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
CacheLruSPANN::refreshCache()
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










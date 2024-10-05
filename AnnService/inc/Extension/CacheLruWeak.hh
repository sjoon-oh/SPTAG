#pragma once
// Author: Sukjoon Oh (sjoon@kaist.ac.kr)
// 

#ifndef _SPTAG_EXT_CACHE_LRUWEAK_H
#define _SPTAG_EXT_CACHE_LRUWEAK_H

#include <chrono>
#include <cstdint>
#include <cstring>
#include <cassert>

#include <memory>
#include <utility>

#include <unordered_map>
#include <list>
#include <vector>

#include <iostream>

// #include "inc/Extension/Locks.hh"


namespace SPTAG {
    namespace EXT {
        
        // 
        // Statistics
        class CacheStatWeak
        {
        protected:
            uint64_t m_hitCount;
            uint64_t m_missCount;
            uint64_t m_evictCount;

            size_t m_currentSize;

            double m_localHitRatio;

        public:
            CacheStatWeak() noexcept;
            CacheStatWeak(uint64_t, uint64_t, uint64_t, uint64_t, double) noexcept;
            virtual ~CacheStatWeak() noexcept;

            uint64_t getHitCount() const noexcept;
            uint64_t getMissCount() const noexcept;
            uint64_t getEvictCount() const noexcept;
            uint64_t getCurrentSize() const noexcept;
            double getLocalHitRatio() const noexcept;

            void incrHitCount(uint64_t) noexcept;
            void incrMissCount(uint64_t) noexcept;
            void incrEvictCount(uint64_t) noexcept;

            void setCurrentSize(uint64_t) noexcept;

            void resetAll() noexcept;
        };


        // 
        // 
        template <class K>
        class CacheItem
        {
        private:
            K m_key;
            uint8_t* m_buffer;
            size_t m_size;

        public:
            CacheItem(K, uint8_t*, size_t) noexcept;
            CacheItem(CacheItem&&) noexcept;
            virtual ~CacheItem() noexcept;

            uint8_t* getItem() noexcept;

            K getKey() const noexcept;
            const size_t getSize() const noexcept;
        };

        
        // 
        // Simple Hash-based LRU cache
        template <class K>
        class CacheLru
        {
        protected:
            CacheStatWeak m_stats;
            const size_t m_cacheCapacity;
            size_t m_currentSize;

            std::vector<CacheStatWeak> m_statTrace;
            std::vector<double> m_deltaHitRatioTrace;
            
            std::vector<double> m_latencyGet;
            std::vector<double> m_latencySet;

            std::unordered_map<K, typename std::list<CacheItem<K>>::iterator> m_cachedItems;
            std::list<CacheItem<K>> m_usageList;    // For tracking LRU order

#ifdef _TRACK_EVICT_
            
#endif
        public:
            CacheLru(const size_t) noexcept;
            virtual ~CacheLru() noexcept;
            
            bool setItemCached(K, uint8_t*, size_t) noexcept;
            CacheItem<K>* getCachedItem(K) noexcept;
            inline const bool isItemCached(K) noexcept;

            CacheStatWeak getCacheStat() const noexcept;
            std::vector<CacheStatWeak>& getCacheStatTrace() noexcept;

            void recordStatTrace() noexcept;
            void resetStat() noexcept;
            void resetStatTrace() noexcept;

            void recordLatencyGet(double) noexcept;
            void recordLatencySet(double) noexcept;

            void resetLatencyGet() noexcept;
            void resetLatencySet() noexcept;

            std::vector<double>& getLatencyGet() noexcept;
            std::vector<double>& getLatencySet() noexcept;

            std::vector<double>& getDeltaHitRatioTrace() noexcept;
        };


        class CacheLruSpannSt : public CacheLru<uintptr_t>
        {
        protected:
            bool m_delayPending;
            size_t m_delayedNumToCache;
            std::unique_ptr<std::vector<bool>> m_delayedToCache;

            void* m_requests;          
            
        public:
            CacheLruSpannSt(const size_t) noexcept;
            virtual ~CacheLruSpannSt() noexcept;

            void setDelayedToCache(size_t, std::vector<bool>, void*) noexcept;
            
            void refreshCache() noexcept;
            void refreshCacheBulk() noexcept;
        };

    }
}


// Cache Item
template <class K>
SPTAG::EXT::CacheItem<K>::CacheItem(K p_key, uint8_t* p_object, size_t p_size) noexcept
    : m_size(p_size), m_key(p_key)
{
    m_buffer = new uint8_t[m_size];
    std::memcpy(m_buffer, p_object, m_size); // copy
}


template <class K>
SPTAG::EXT::CacheItem<K>::CacheItem(SPTAG::EXT::CacheItem<K>&& p_other) noexcept
    : m_size(p_other.m_size), m_key(p_other.m_key), m_buffer(p_other.m_buffer)
{
    p_other.m_buffer = nullptr;
}


template <class K>
SPTAG::EXT::CacheItem<K>::~CacheItem() noexcept
{
    if (m_buffer != nullptr)
        delete[] m_buffer;
}


template <class K>
uint8_t*
SPTAG::EXT::CacheItem<K>::getItem() noexcept
{
    return m_buffer;
}


template <class K>
K
SPTAG::EXT::CacheItem<K>::getKey() const noexcept
{
    return m_key;
}


// const size_t getSize() const;
template <class K>
const size_t
SPTAG::EXT::CacheItem<K>::getSize() const noexcept
{
    return m_size;
}


template <class K>
SPTAG::EXT::CacheLru<K>::CacheLru(const size_t p_capacity) noexcept
    : m_cacheCapacity(p_capacity), m_currentSize(0)
{
    m_stats.setCurrentSize(m_currentSize);
    m_deltaHitRatioTrace.push_back(0);
}


template <class K>
SPTAG::EXT::CacheLru<K>::~CacheLru() noexcept
{

}


template <class K>
bool
SPTAG::EXT::CacheLru<K>::setItemCached(K p_key, uint8_t* p_object, size_t p_size) noexcept
{
    if (isItemCached(p_key))
    {
        return false; // Item already cached.
    }
    
    // 
    // Evict items until we have enough space for the new item
    while (m_currentSize + p_size > m_cacheCapacity)
    {
        CacheItem<K>* lruItem = &(m_usageList.back());
        
        m_currentSize -= lruItem->getSize(); // Reduce current size by the evicted item's size
        m_cachedItems.erase(lruItem->getKey());
        m_usageList.pop_back();
        m_stats.incrEvictCount(1);
    }

    m_usageList.emplace_front(p_key, p_object, p_size);

    m_cachedItems[p_key] = m_usageList.begin();
    m_currentSize += p_size; // Update the current size

    m_stats.setCurrentSize(m_currentSize);

    return true;
}


template <class K>
SPTAG::EXT::CacheItem<K>* 
SPTAG::EXT::CacheLru<K>::getCachedItem(K p_key) noexcept
{
    if (!isItemCached(p_key))
    {
        m_stats.incrMissCount(1);
        return nullptr; // Item not found
    }

    // Move accessed item to front of usage list
    auto it = m_cachedItems[p_key];
    CacheItem<K> itemToMove = std::move(*it); // Move the item out

    m_usageList.erase(it); // Remove from current position
    m_usageList.push_front(std::move(itemToMove)); // Add to front the previous item
    
    m_cachedItems[p_key] = m_usageList.begin(); // Update map
    m_stats.incrHitCount(1);

    return &(*(m_cachedItems[p_key])); // Return the item
}


template <class K>
const bool
SPTAG::EXT::CacheLru<K>::isItemCached(K p_key) noexcept
{
    return (m_cachedItems.find(p_key) != m_cachedItems.end());
}


template <class K>
SPTAG::EXT::CacheStatWeak
SPTAG::EXT::CacheLru<K>::getCacheStat() const noexcept
{
    return m_stats;
}


template <class K>
std::vector<SPTAG::EXT::CacheStatWeak>&
SPTAG::EXT::CacheLru<K>::getCacheStatTrace() noexcept
{
    return m_statTrace;
}


template <class K>
void
SPTAG::EXT::CacheLru<K>::recordStatTrace() noexcept
{
    if (m_statTrace.size() != 0)
    {
        CacheStatWeak& prevStat = m_statTrace.back();
        
        double deltaHits = static_cast<double>(m_stats.getHitCount()) - prevStat.getHitCount();
        double deltaMisses = static_cast<double>(m_stats.getMissCount()) - prevStat.getMissCount();

        double deltaHitRatio = deltaHits / (deltaHits + deltaMisses);

        m_deltaHitRatioTrace.push_back(deltaHitRatio);
    }
    
    m_statTrace.push_back(this->m_stats);
}


template <class K>
void
SPTAG::EXT::CacheLru<K>::resetStat() noexcept
{
    m_stats.resetAll();
}


template <class K>
void
SPTAG::EXT::CacheLru<K>::resetStatTrace() noexcept
{
    m_statTrace.clear();
}


template <class K>
void
SPTAG::EXT::CacheLru<K>::recordLatencyGet(double p_record) noexcept
{
    m_latencyGet.push_back(p_record);
}


template <class K>
void
SPTAG::EXT::CacheLru<K>::recordLatencySet(double p_record) noexcept
{
    m_latencySet.push_back(p_record);
}


template <class K>
void
SPTAG::EXT::CacheLru<K>::resetLatencyGet() noexcept
{
    m_latencyGet.clear();
}


template <class K>
void
SPTAG::EXT::CacheLru<K>::resetLatencySet() noexcept
{
    m_latencySet.clear();
}


template <class K>
std::vector<double>&
SPTAG::EXT::CacheLru<K>::getLatencyGet() noexcept
{
    return m_latencyGet;
}


template <class K>
std::vector<double>&
SPTAG::EXT::CacheLru<K>::getLatencySet() noexcept
{
    return m_latencySet;
}


template <class K>
std::vector<double>&
SPTAG::EXT::CacheLru<K>::getDeltaHitRatioTrace() noexcept
{
    return m_deltaHitRatioTrace;
}

#endif


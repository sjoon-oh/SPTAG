#pragma once
// Author: Sukjoon Oh (sjoon@kaist.ac.kr)
// 

#ifndef _SPTAG_EXTENSION_H
#define _SPTAG_EXTENSION_H

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


namespace SPTAG {
    namespace EXT {
        
        // 
        // Statistics
        class CacheStats
        {
        protected:
            uint64_t m_hitCount;
            uint64_t m_missCount;
            uint64_t m_evictCount;

            size_t m_currentSize;

        public:
            CacheStats();
            virtual ~CacheStats();

            uint64_t getHitCount() const;
            uint64_t getMissCount() const;
            uint64_t getEvictCount() const;
            uint64_t getCurrentSize() const;

            void incrHitCount(uint64_t);
            void incrMissCount(uint64_t);
            void incrEvictCount(uint64_t);

            void setCurrentSize(uint64_t);
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
            CacheItem(K, uint8_t*, size_t);
            CacheItem(CacheItem&&) noexcept;
            virtual ~CacheItem();

            uint8_t* getItem();

            K getKey() const;
            const size_t getSize() const;
        };

        
        // 
        // Simple Hash-based LRU cache
        template <class K>
        class CacheLru
        {
        protected:
            CacheStats m_stats;
            const size_t m_cacheCapacity;
            size_t m_currentSize;

            std::vector<CacheStats> m_statTrace;

            std::unordered_map<K, typename std::list<CacheItem<K>>::iterator> m_cachedItems;
            std::list<CacheItem<K>> m_usageList;    // For tracking LRU order

#ifdef _TRACK_EVICT_
            
#endif
        public:
            CacheLru(const size_t);
            virtual ~CacheLru();
            
            bool setItemCached(K, uint8_t*, size_t);
            CacheItem<K>* getCachedItem(K);
            inline const bool isItemCached(K);

            CacheStats getCacheStat() const;
            std::vector<CacheStats>& getCacheStatTrace();

            void recordStatTrace();
        };


        class CacheLruSPANN : public CacheLru<uintptr_t>
        {
        private:
            bool m_delayPending;
            size_t m_delayedNumToCache;
            std::unique_ptr<std::vector<bool>> m_delayedToCache;

            void* m_requests;
            
            
        public:
            CacheLruSPANN(const size_t);
            virtual ~CacheLruSPANN();

            void setDelayedToCache(size_t, std::vector<bool>, void*);
            
            void refreshCache();
        };
    }
}


// Cache Item
template <class K>
SPTAG::EXT::CacheItem<K>::CacheItem(K p_key, uint8_t* p_object, size_t p_size)
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
SPTAG::EXT::CacheItem<K>::~CacheItem()
{
    if (m_buffer != nullptr)
        delete[] m_buffer;
}


template <class K>
uint8_t*
SPTAG::EXT::CacheItem<K>::getItem()
{
    return m_buffer;
}


template <class K>
K
SPTAG::EXT::CacheItem<K>::getKey() const
{
    return m_key;
}


// const size_t getSize() const;
template <class K>
const size_t
SPTAG::EXT::CacheItem<K>::getSize() const
{
    return m_size;
}


template <class K>
SPTAG::EXT::CacheLru<K>::CacheLru(const size_t p_capacity) 
    : m_cacheCapacity(p_capacity), m_currentSize(0)
{
    m_stats.setCurrentSize(m_currentSize);
}


template <class K>
SPTAG::EXT::CacheLru<K>::~CacheLru()
{

}


template <class K>
bool
SPTAG::EXT::CacheLru<K>::setItemCached(K p_key, uint8_t* p_object, size_t p_size)
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
SPTAG::EXT::CacheLru<K>::getCachedItem(K p_key)
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
SPTAG::EXT::CacheLru<K>::isItemCached(K p_key)
{
    return (m_cachedItems.find(p_key) != m_cachedItems.end());
}


template <class K>
SPTAG::EXT::CacheStats
SPTAG::EXT::CacheLru<K>::getCacheStat() const
{
    return m_stats;
}


template <class K>
std::vector<SPTAG::EXT::CacheStats>&
SPTAG::EXT::CacheLru<K>::getCacheStatTrace()
{
    return m_statTrace;
}


template <class K>
void
SPTAG::EXT::CacheLru<K>::recordStatTrace()
{
    m_statTrace.push_back(this->m_stats);
}



#endif


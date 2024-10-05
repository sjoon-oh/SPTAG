#pragma once
// Author: Sukjoon Oh (sjoon@kaist.ac.kr)
// 

#ifndef _SPTAG_EXT_CACHE_LRUMT_H
#define _SPTAG_EXT_CACHE_LRUMT_H

#include <chrono>
#include <cstdint>
#include <cstring>
#include <cassert>

#include <atomic>
#include <memory>
#include <utility>

#include <unordered_map>
#include <list>
#include <vector>
#include <queue>
#include <array>

#include <iostream>

#include "inc/Extension/Locks.hh"
#include "inc/Extension/CacheLruWeak.hh"

namespace SPTAG {
    namespace EXT {

        #define MAX_NTHREADS    8192

        class CacheStatMt
        {
        protected:
            std::atomic_ulong m_hitCount;
            std::atomic_ulong m_missCount;
            std::atomic_ulong m_evictCount;

            std::atomic_ulong m_currentSize;


        public:
            CacheStatMt() noexcept;
            virtual ~CacheStatMt() noexcept;

            uint64_t getHitCount() const noexcept;
            uint64_t getMissCount() const noexcept;
            uint64_t getEvictCount() const noexcept;
            uint64_t getCurrentSize() const noexcept;

            void incrHitCount(uint64_t) noexcept;
            void incrMissCount(uint64_t) noexcept;
            void incrEvictCount(uint64_t) noexcept;

            void incrCurrentSize(uint64_t) noexcept;
            void decrCurrentSize(uint64_t) noexcept;

            void resetAll() noexcept;
        };


        // 
        // Simple Hash-based LRU cache
        class CacheLruSpannMt
        {
        protected:
            const size_t m_cacheCapacity;
            
            CacheStatMt m_stats;
            std::vector<CacheStatWeak> m_statTrace;


            SpinlockWithStat m_itemLock;
            
            std::unordered_map<uintptr_t, typename std::list<CacheItem<uintptr_t>>::iterator> m_cachedItems;
            std::list<CacheItem<uintptr_t>> m_usageList;    // For tracking LRU order

            // Delay
            struct PendingInfo
            {
                size_t m_delayedNumToCache;
                std::unique_ptr<std::vector<bool>> m_delayedToCache;
                void* m_requests;

                PendingInfo()
                    : m_delayedNumToCache(0), m_requests(nullptr)
                {

                }

                PendingInfo(size_t p_delayedNumToCache, std::vector<bool> p_delayedToCache, void* p_requests)
                    : m_delayedNumToCache(p_delayedNumToCache), m_requests(p_requests)
                {
                    m_delayedToCache.reset(new std::vector<bool>(std::move(p_delayedToCache)));
                }
            };
            
            std::array<struct PendingInfo, MAX_NTHREADS> m_delayedToCacheArr;


        public:
            CacheLruSpannMt(const size_t) noexcept;
            virtual ~CacheLruSpannMt() noexcept;
            
            bool setItemCached(uintptr_t, uint8_t*, size_t) noexcept;
            CacheItem<uintptr_t>* getCachedItem(uintptr_t) noexcept;
            inline const bool isItemCached(uintptr_t) noexcept;

            CacheStatMt* getCacheStat() noexcept;
            std::vector<CacheStatWeak>& getCacheStatTrace() noexcept;
            SpinlockWithStat& getSpinlockWithStat() noexcept;

            void recordStatTrace() noexcept;
            void resetStat() noexcept;

            void setDelayedToCache(size_t, std::vector<bool>, void*, int) noexcept;
            
            void refreshCacheBulkSingle(int) noexcept;
        };
    }
}


#endif
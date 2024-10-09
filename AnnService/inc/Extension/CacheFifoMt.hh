#pragma once
// Author: Sukjoon Oh (sjoon@kaist.ac.kr)
// 

#ifndef _SPTAG_EXT_CACHE_FIFOMT_H
#define _SPTAG_EXT_CACHE_FIFOMT_H

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
#include "inc/Extension/CacheLruMt.hh"

namespace SPTAG {
    namespace EXT {

        class CacheFifoSpannMt
        {
        protected:
            const size_t m_cacheCapacity;

            CacheStatMt m_stats;
            std::vector<CacheStatWeak> m_statTrace;

            bool m_enableLock;
            SpinlockWithStat m_itemLock;

            std::unordered_map<uintptr_t, typename std::list<CacheItem<uintptr_t>>::iterator> m_cachedItems;
            std::list<CacheItem<uintptr_t>> m_usageList;    // For tracking LRU order

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
            CacheFifoSpannMt(const size_t, bool) noexcept;
            virtual ~CacheFifoSpannMt() noexcept;

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
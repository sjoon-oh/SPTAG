#pragma once
// Author: Sukjoon Oh (sjoon@kaist.ac.kr)
// 

#ifndef _SPTAG_EXT_CACHE_LFU2_H
#define _SPTAG_EXT_CACHE_LFU2_H

#include "inc/Extension/Locks.hh"
#include "inc/Extension/CacheLruWeak.hh"
#include "inc/Extension/CacheLruMt.hh"
#include "inc/Extension/CacheCores.hh"


namespace SPTAG 
{
    namespace EXT
    {
        class CacheLfu2
        {
        protected:
            const size_t m_capacity;
            
            std::shared_ptr<DATA_CACHED_T> m_cachedShared;
            std::unique_ptr<CacheLfuCore> m_lfuCache;

            // Stats
            CacheStatMt m_stats;
            std::vector<CacheStatWeak> m_statTrace;

            bool m_enableLock;
            SpinlockWithStat m_itemLock;

            struct DelayedUpdates
            {
                size_t m_numToCache;
                std::unique_ptr<std::vector<bool>> m_toCache;
                void* m_requests;

                DelayedUpdates()
                    : m_numToCache(0), m_requests(nullptr)
                {

                }

                DelayedUpdates(size_t p_numToCache, std::vector<bool> p_toCache, void* p_requests)
                    : m_numToCache(p_numToCache), m_requests(p_requests)
                {
                    m_toCache.reset(new std::vector<bool>(std::move(p_toCache)));
                }
            };

            struct DelayedUpdates m_delayedUpdates;

        public:
            CacheLfu2(const size_t p_capacity, bool p_enableLock = false) noexcept
                : m_capacity(p_capacity), m_cachedShared(new DATA_CACHED_T())
            {
                m_lfuCache.reset(new CacheLfuCore(size_t(m_capacity), m_cachedShared));
                m_stats.resetAll();
            }

            virtual ~CacheLfu2() noexcept
            {

            }

            inline bool isCached(key_t p_key) noexcept
            {
                return ((*m_cachedShared).find(p_key) != (*m_cachedShared).end());
            }

            CacheItem<key_t>* getCachedItem(key_t p_key) noexcept
            {
                if (!isCached(p_key))
                {
                    m_stats.incrMissCount(1);
                    return nullptr; // Item not found
                }

                m_stats.incrHitCount(1);

                auto& item = (*m_cachedShared)[p_key];
                m_lfuCache->updateGetHit(item);

                return reinterpret_cast<CacheItem<key_t>*>(&(item.m_item));
            }

            void setDelayedToCache(size_t p_numToCache, std::vector<bool> p_toCache, void* p_requests, int p_tid = 0) noexcept
            {
                m_delayedUpdates.m_numToCache = p_numToCache;
                m_delayedUpdates.m_toCache.reset(new std::vector<bool>(std::move(p_toCache)));
                m_delayedUpdates.m_requests = p_requests;
            }

            void refreshCacheBulkSingle(int p_pid) noexcept
            {                  
                size_t localHits = 0;
                size_t totalSize = 0;

                struct DelayedUpdates& delayedUpdates = m_delayedUpdates;

                struct ListInfo
                {
                    std::size_t     listTotalBytes;
                    int             listEleCount;
                    std::uint16_t   listPageCount;
                    std::uint64_t   listOffset;
                    std::uint16_t   pageOffset;
                };

                SPTAG::Helper::AsyncReadRequest* requests = 
                    (SPTAG::Helper::AsyncReadRequest*)delayedUpdates.m_requests;

                // std::cout << "Num to cache: " << delayedUpdates.m_numToCache << std::endl;

                std::vector<int> indexHit;
                std::vector<int> indexMiss;

                for (int i = 0; i < delayedUpdates.m_numToCache; i++)
                {
                    if ((*(delayedUpdates.m_toCache))[i] == false)
                    {
                        indexHit.emplace_back(i);
                        localHits++;
                    }
                    else
                    {
                        indexMiss.emplace_back(i);
                        totalSize += (requests[i].m_readSize);
                    }
                }

                m_lfuCache->evictOverflows(totalSize);

                for (auto& index: indexMiss)
                {
                    ListInfo* listInfo = (ListInfo*)(requests[index].m_payload);
                    key_t key = static_cast<key_t>(requests[index].m_offset) + listInfo->pageOffset;

                    m_lfuCache->insertItem(
                        key, reinterpret_cast<buf_t*>(requests[index].m_buffer), requests[index].m_readSize
                    );
                }

                m_statTrace.emplace_back(
                    m_stats.getHitCount(), 
                    m_stats.getMissCount(), 
                    m_stats.getEvictCount(), 
                    m_lfuCache->getCurrentSize(), 
                    (localHits * 1.0) / delayedUpdates.m_toCache->size()
                );
            }

            // -- Handles--
            void recordStatTrace() noexcept
            {

            }

            inline CacheStatMt* getCacheStat() noexcept
            {
                return &(m_stats);
            }

            inline std::vector<CacheStatWeak>& getCacheStatTrace() noexcept
            {
                return m_statTrace;
            }

            inline SpinlockWithStat& getSpinlockWithStat() noexcept
            {
                return m_itemLock;
            }

            inline void resetStat() noexcept
            {
                m_stats.resetAll();
            }
        };
    }
}

#endif
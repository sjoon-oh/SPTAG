// Author: Sukjoon Oh (sjoon@kaist.ac.kr)
// 

#ifndef _SPTAG_EXT_CACHE_CORRLFU_H
#define _SPTAG_EXT_CACHE_CORRLFU_H

#include <chrono>
#include <cstdint>
#include <cstring>
#include <cassert>

#include <atomic>
#include <memory>
#include <utility>

#include <map>
#include <unordered_map>
#include <list>
#include <vector>
#include <queue>
#include <array>

#include <iostream>

#include "inc/Helper/DiskIO.h"

#include "inc/Extension/Locks.hh"
#include "inc/Extension/CacheLruWeak.hh"
#include "inc/Extension/CacheLruMt.hh"
#include "inc/Extension/CacheLfuMt.hh"

namespace SPTAG {
    namespace EXT {

#define CACHE_CORRLFU_MAX_LEVEL 4
#define CACHE_PROMOTION_THRESH  4
#define MAX_NTHREADS            8192

        using key_t = uintptr_t;
        using buf_t = uint8_t;

        class BufferItem
        {
        private:
            // Main 
            key_t   m_key;
            buf_t*  m_buffer;
            size_t  m_size;

            uint8_t m_level;

        public:
            BufferItem() noexcept
            : m_size(0), m_key(0)
            {
                m_buffer = nullptr;
            }

            BufferItem(key_t p_key, buf_t* p_object, size_t p_size, uint8_t p_level = 0) noexcept
                : m_size(p_size), m_key(p_key), m_level(p_level)
            {
                m_buffer = new buf_t[m_size];
                std::memcpy(m_buffer, p_object, m_size); // copy
            }

            BufferItem(BufferItem&& p_other, uint8_t* p_newLevel = nullptr) noexcept
                : m_size(p_other.m_size), m_key(p_other.m_key), m_buffer(p_other.m_buffer), m_level(p_other.m_level)
            {
                p_other.m_buffer = nullptr;
                if (p_newLevel != nullptr)
                    m_level = *p_newLevel;
            }

            virtual ~BufferItem() noexcept = default;

            inline void setLevel(uint8_t p_level) noexcept
            {
                m_level = p_level;
            }

            inline buf_t* getItem() noexcept
            {
                return m_buffer;
            }

            inline key_t getKey() const noexcept
            {
                return m_key;
            }

            inline const size_t getSize() const noexcept
            {
                return m_size;
            }

            inline const uint8_t getLevel() const noexcept
            {
                return m_level;
            }
        };

        struct BufferItemWrapper
        {
            BufferItem  m_item;
            size_t      m_freq;

            BufferItemWrapper() = default;
            BufferItemWrapper(BufferItem&& p_item)
                : m_item(std::move(p_item)), m_freq(1)
            {

            }
        };


        class CacheLruCore
        {
        protected:
            // 
            // Core
            const size_t    m_capacity;
            size_t          m_currentSize;

            size_t          m_minFreq;

#define MAP_T           std::unordered_map
#define LIST_T          std::list
#define VEC_T           std::vector

#define DATA_CACHED_T       MAP_T<key_t, struct BufferItemWrapper>
#define META_FREQLRU_T      std::map<key_t, LIST_T<key_t>>
#define META_FREQLRU_POS_T  MAP_T<key_t, LIST_T<key_t>::iterator>

            std::shared_ptr<DATA_CACHED_T> m_cached;
            std::unique_ptr<META_FREQLRU_T> m_freqLruList;
            std::unique_ptr<META_FREQLRU_POS_T> m_posInFreqList;

            // Stats
            uint64_t        m_hitCounts;
            uint64_t        m_missCounts;
            uint64_t        m_evictCounts;
            VEC_T<double>   m_hrHistory;

        public:
            CacheLruCore(const size_t p_capacity, std::shared_ptr<DATA_CACHED_T> p_cached) noexcept
                : m_capacity(p_capacity), m_currentSize(0), m_minFreq(0)
            {
                m_freqLruList.reset(new META_FREQLRU_T);
                m_posInFreqList.reset(new META_FREQLRU_POS_T);

                m_cached = p_cached;
            }

            virtual ~CacheLruCore() noexcept
            {

            }

            inline bool isCached(key_t p_key) noexcept
            {
                return ((*m_cached).find(p_key) != (*m_cached).end());
            }

            inline void updateGetHit(struct BufferItemWrapper& p_bufItem)
            {
                m_hitCounts++;

                size_t prevFreq = p_bufItem.m_freq;
                key_t key = p_bufItem.m_item.getKey();

                p_bufItem.m_freq += 1;

                (*m_freqLruList)[prevFreq].erase((*m_posInFreqList)[key]);   // Erase from the LRU list

                if ((*m_freqLruList)[prevFreq].empty())
                {
                    int numErased = (*m_freqLruList).erase(prevFreq);
                    if (m_minFreq == prevFreq)
                        m_minFreq += 1;
                }

                (*m_freqLruList)[p_bufItem.m_freq].push_back(key);
                (*m_posInFreqList)[key] = --(*m_freqLruList)[p_bufItem.m_freq].end(); // Update iterator
            }

            BufferItem* getItemAndUpdate(key_t p_key) noexcept
            {
                if (!isCached(p_key))
                {
                    m_missCounts++;
                    return nullptr; // Item not found
                }

                auto& bufItem = (*m_cached)[p_key];
                updateGetHit(bufItem);

                return &(bufItem.m_item);
            }

            BufferItem* getItemWithoutUpdate(key_t p_key) noexcept
            {
                if (!isCached(p_key))
                    return nullptr; // Item not found

                auto& bufItem = (*m_cached)[p_key];
                return &(bufItem.m_item);
            }

            // 
            // Only erases from the current tracking list
            inline void eraseItemFromLists(key_t p_evictKey) noexcept
            {
                auto& lruItem = ((*m_cached)[p_evictKey].m_item);
                uint64_t decrSize = lruItem.getSize();

                (*m_posInFreqList).erase(p_evictKey);

                m_currentSize = m_currentSize - decrSize;
            }

            size_t evictOverflows() noexcept
            {
                size_t localEvictCounts = 0;
                while (m_currentSize > m_capacity)
                {
                    
                    // if ((*m_freqLruList)[m_minFreq].size() == 0)
                    // {
                    //     m_minFreq++;
                    //     continue;
                    // }

                    key_t evictKey = (*m_freqLruList)[m_minFreq].front();
                    (*m_freqLruList)[m_minFreq].pop_front();                // Pop least recently used

                    // If it is the last element in the m_minFreq list, 
                    // need to update the m_minFreq to next one.    
                    if ((*m_freqLruList)[m_minFreq].empty())
                    {
                        m_freqLruList->erase(m_minFreq);
                        key_t nextMinKey = (*(m_freqLruList->upper_bound(m_minFreq))).second.front();

                        // std::cout << m_minFreq << ", " << nextMinKey << std::endl;

                        m_minFreq = nextMinKey;
                    }

                    eraseItemFromLists(evictKey);
                    m_cached->erase(evictKey);

                    m_evictCounts++;
                    localEvictCounts++;
                }

                return localEvictCounts;
            }

            inline void insertItemToLists(key_t p_key, size_t p_size) noexcept
            {
                m_minFreq = 1;

                (*m_freqLruList)[m_minFreq].push_back(p_key);
                (*m_posInFreqList)[p_key] = --(*m_freqLruList)[m_minFreq].end();

                 m_currentSize += p_size;
            }

            void insertItem(key_t p_key, buf_t* p_buffer, size_t p_size, uint8_t p_level = 0) noexcept
            {
                m_cached->insert(
                    std::make_pair(p_key, BufferItemWrapper(BufferItem(p_key, p_buffer, p_size, p_level)))
                );

                // m_minFreq = 1;

                // (*m_freqLruList)[m_minFreq].push_back(p_key);
                // (*m_posInFreqList)[p_key] = --(*m_freqLruList)[m_minFreq].end();

                // m_currentSize += p_size;

                insertItemToLists(p_key, p_size);
            }
            
            void recordHr(uint64_t p_hitCounts = 0, uint64_t p_missCounts = 0) noexcept
            {
                if ((p_hitCounts + p_missCounts) == 0)
                    m_hrHistory.emplace_back(m_hitCounts * 1.0 / (m_hitCounts + m_missCounts));
                
                else
                    m_hrHistory.emplace_back(p_hitCounts * 1.0 / (p_hitCounts + p_missCounts));
            }

            inline void resetCounts() noexcept
            {
                m_hitCounts = m_missCounts = m_evictCounts = 0;
            }

            inline void resetHrHistory() noexcept
            {
                m_hrHistory.clear();
            }

            inline size_t getCurrentSize() noexcept
            {
                return m_currentSize;
            }

            inline VEC_T<double>& getHrHistory() noexcept
            {
                return m_hrHistory;
            }
        };

        
        class CacheCorrLfu
        {
        protected:
            const size_t m_capacity;
            
            std::shared_ptr<DATA_CACHED_T> m_cachedShared;
            std::array<std::unique_ptr<CacheLruCore>, CACHE_CORRLFU_MAX_LEVEL> m_leveledLru;

            bool m_enableLock;
            SpinlockWithStat m_itemLock;

            // Stats
            CacheStatMt m_stats;
            std::vector<CacheStatWeak> m_statTrace;

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
            CacheCorrLfu(const size_t p_capacity, bool p_enableLock = false) noexcept
                : m_capacity(p_capacity), m_cachedShared(new DATA_CACHED_T())
            {
                double leveledSize = (p_capacity * 0.60);
                m_leveledLru[0].reset(new CacheLruCore(size_t(leveledSize), m_cachedShared)); // Set base size
                
                leveledSize = (p_capacity * 0.20);
                m_leveledLru[1].reset(new CacheLruCore(size_t(leveledSize), m_cachedShared)); // Set second level size
                
                int leftLevels = CACHE_CORRLFU_MAX_LEVEL - 2;
                for (int lev = 2; lev < CACHE_CORRLFU_MAX_LEVEL; lev++)
                {
                    m_leveledLru[lev].reset(
                        new CacheLruCore(size_t((p_capacity * 0.20) / leftLevels), m_cachedShared));
                }

                m_enableLock = p_enableLock;
                m_stats.resetAll();
            }

            virtual ~CacheCorrLfu() noexcept
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
                uint8_t residentLevel = item.m_item.getLevel();

                m_leveledLru[residentLevel]->updateGetHit(item);

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

                size_t evictCounts = 0;
                if (delayedUpdates.m_numToCache > CACHE_PROMOTION_THRESH)
                {
                    for (int i = 0; i < delayedUpdates.m_numToCache; i++)
                    {
                        if ((*(delayedUpdates.m_toCache))[i] == false)
                        {
                            // 
                            // Case when hit. 
                            ListInfo* listInfo = (ListInfo*)(requests[i].m_payload);
                            key_t key = static_cast<key_t>(requests[i].m_offset) + listInfo->pageOffset;

                            BufferItem& bufItem = ((*m_cachedShared)[key]).m_item;
                            uint8_t bufLevel = bufItem.getLevel();

                            if (bufLevel != (CACHE_CORRLFU_MAX_LEVEL - 1))
                            {
                                // Move to the upper level
                                m_leveledLru[bufLevel]->eraseItemFromLists(key);
                                m_leveledLru[bufLevel + 1]->insertItemToLists(key, bufItem.getSize());

                                bufItem.setLevel(bufLevel + 1);

                                // Remove if the upper level is full
                                // evictCounts += (m_leveledLru[bufLevel + 1]->evictOverflows());
                            }

                            localHits++;
                        }

                        else
                        {

                        }
                    }

                    // 
                    // Deal overflows of upper level
                    for (int i = 1; i < CACHE_CORRLFU_MAX_LEVEL; i++)
                    {
                        evictCounts += (m_leveledLru[i]->evictOverflows());
                    }
                }

                // 
                // Always inserted to the base level.
                for (int i = 0; i < delayedUpdates.m_numToCache; i++)
                {
                    if ((*(delayedUpdates.m_toCache))[i] == true)
                    {
                        ListInfo* listInfo = (ListInfo*)(requests[i].m_payload);
                        key_t key = static_cast<key_t>(requests[i].m_offset) + listInfo->pageOffset;

                        m_leveledLru[0]->insertItem(
                            key, reinterpret_cast<buf_t*>(requests[i].m_buffer), requests[i].m_readSize
                        );
                    }
                }

                // std::cout << "Shared size: " << m_cachedShared->size() << std::endl;

                // for (auto& lruElem: m_leveledLru)
                //     std::cout << lruElem->getCurrentSize() << "\t";
                // std::cout << std::endl;

                evictCounts += (m_leveledLru[0]->evictOverflows());
                m_stats.incrEvictCount(evictCounts);

                size_t currentTotalSize = 0;
                for (auto& lruElem: m_leveledLru)
                {
                    currentTotalSize += (lruElem->getCurrentSize());
                }

                m_statTrace.emplace_back(
                    m_stats.getHitCount(), 
                    m_stats.getMissCount(), 
                    m_stats.getEvictCount(), 
                    currentTotalSize, 
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
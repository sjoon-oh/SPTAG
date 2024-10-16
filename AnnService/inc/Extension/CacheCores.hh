#pragma once
// Author: Sukjoon Oh (sjoon@kaist.ac.kr)
// 

#ifndef _SPTAG_EXT_CACHE_CORES_H
#define _SPTAG_EXT_CACHE_CORES_H

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



namespace SPTAG 
{
    namespace EXT 
    {

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

            void setLevel(uint8_t p_level) noexcept
            {
                m_level = p_level;
            }

            buf_t* getItem() noexcept
            {
                return m_buffer;
            }

            key_t getKey() const noexcept
            {
                return m_key;
            }

            const size_t getSize() const noexcept
            {
                return m_size;
            }

            const uint8_t getLevel() const noexcept
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


#define MAP_T           std::unordered_map
#define LIST_T          std::list
#define VEC_T           std::vector

#define DATA_CACHED_T       MAP_T<key_t, struct BufferItemWrapper>

        class CacheCore
        {
        protected:
            size_t          m_capacity;
            size_t          m_currentSize;

            std::shared_ptr<DATA_CACHED_T> m_cached;

            // Stats
            uint64_t        m_hitCounts;
            uint64_t        m_missCounts;
            uint64_t        m_evictCounts;
            VEC_T<double>   m_hrHistory;

        public:
            CacheCore(const size_t p_capacity, std::shared_ptr<DATA_CACHED_T> p_cached) noexcept
                : m_capacity(p_capacity), m_currentSize(0)
            {
                m_cached = p_cached;
            }

            virtual ~CacheCore() noexcept
            {

            }

            bool isCached(key_t p_key) noexcept
            {
                return ((*m_cached).find(p_key) != (*m_cached).end());
            }

            virtual void updateGetHit(struct BufferItemWrapper&) noexcept = 0;

            virtual void eraseItemFromLists(key_t) noexcept = 0;
            virtual size_t evictOverflows(size_t = 0) noexcept = 0;

            virtual void insertItemToLists(key_t, size_t) noexcept = 0;
            virtual void insertItem(key_t p_key, buf_t* p_buffer, size_t p_size) noexcept = 0;

            void recordHr(uint64_t p_hitCounts = 0, uint64_t p_missCounts = 0) noexcept
            {
                if ((p_hitCounts + p_missCounts) == 0)
                    m_hrHistory.emplace_back(m_hitCounts * 1.0 / (m_hitCounts + m_missCounts));
                
                else
                    m_hrHistory.emplace_back(p_hitCounts * 1.0 / (p_hitCounts + p_missCounts));
            }

            void resetCounts() noexcept
            {
                m_hitCounts = m_missCounts = m_evictCounts = 0;
            }

            void resetHrHistory() noexcept
            {
                m_hrHistory.clear();
            }

            size_t getCurrentSize() noexcept
            {
                return m_currentSize;
            }

            VEC_T<double>& getHrHistory() noexcept
            {
                return m_hrHistory;
            }
        };


        class CacheFifoCore : public CacheCore
        {
        protected:
            LIST_T<key_t> m_recencyList;    // For tracking LRU order
                                            // Inserted at the front, poped at the back
            MAP_T<key_t, typename LIST_T<key_t>::iterator> m_posRecencyList;

        public:
            CacheFifoCore(const size_t p_capacity, std::shared_ptr<DATA_CACHED_T> p_cached) noexcept
                : CacheCore(p_capacity, p_cached)
            {

            }

            virtual ~CacheFifoCore() noexcept
            {

            }
            
            void updateGetHit(struct BufferItemWrapper& p_bufItem) noexcept
            {
                m_hitCounts++;
                key_t key = p_bufItem.m_item.getKey();

                p_bufItem.m_freq += 1;
            }

            // 
            // Only erases from the current tracking list
            void eraseItemFromLists(key_t p_evictKey) noexcept
            {
                auto& lruItem = ((*m_cached)[p_evictKey].m_item);
                uint64_t decrSize = lruItem.getSize();

                auto recencyListPos = m_posRecencyList[p_evictKey];
                
                m_recencyList.erase(recencyListPos);
                m_posRecencyList.erase(p_evictKey);

                m_currentSize = m_currentSize - decrSize;
            }

            size_t evictOverflows(size_t p_newSize = 0) noexcept
            {
                size_t localEvictCounts = 0;
                while ((m_currentSize + p_newSize) > m_capacity)
                {
                    key_t oldestKey = m_recencyList.back();

                    eraseItemFromLists(oldestKey);
                    m_cached->erase(oldestKey);

                    m_evictCounts++;
                    localEvictCounts++;
                }

                return localEvictCounts;
            }

            void insertItemToLists(key_t p_key, size_t p_size) noexcept
            {
                m_recencyList.emplace_front(p_key);
                m_posRecencyList[p_key] = m_recencyList.begin();

                m_currentSize = m_currentSize + p_size;
            }

            void insertItem(key_t p_key, buf_t* p_buffer, size_t p_size) noexcept
            {
                m_cached->insert(
                    std::make_pair(p_key, BufferItemWrapper(BufferItem(p_key, p_buffer, p_size)))
                );

                insertItemToLists(p_key, p_size);
            }
        };



        class CacheLruCore : public CacheCore
        {
        protected:
            LIST_T<key_t> m_recencyList;    // For tracking LRU order
            MAP_T<key_t, typename LIST_T<key_t>::iterator> m_posRecencyList;

        public:
            CacheLruCore(const size_t p_capacity, std::shared_ptr<DATA_CACHED_T> p_cached) noexcept
                : CacheCore(p_capacity, p_cached)
            {

            }

            virtual ~CacheLruCore() noexcept
            {

            }
            
            void updateGetHit(struct BufferItemWrapper& p_bufItem) noexcept
            {

                m_hitCounts++;
                key_t key = p_bufItem.m_item.getKey();

                p_bufItem.m_freq += 1;

                auto recencyListPos = m_posRecencyList[key];
                m_recencyList.erase(recencyListPos);

                m_recencyList.push_front(key);
                m_posRecencyList[key] = m_recencyList.begin();
            }

            // 
            // Only erases from the current tracking list
            void eraseItemFromLists(key_t p_evictKey) noexcept
            {
                auto& lruItem = ((*m_cached)[p_evictKey].m_item);
                uint64_t decrSize = lruItem.getSize();

                auto recencyListPos = m_posRecencyList[p_evictKey];
                m_recencyList.erase(recencyListPos);
                m_posRecencyList.erase(p_evictKey);

                m_currentSize = m_currentSize - decrSize;
            }

            size_t evictOverflows(size_t p_newSize = 0) noexcept
            {
                size_t localEvictCounts = 0;
                while ((m_currentSize + p_newSize) > m_capacity)
                {
                    key_t leastRUsedKey = m_recencyList.back();

                    eraseItemFromLists(leastRUsedKey);
                    m_cached->erase(leastRUsedKey);

                    m_evictCounts++;
                    localEvictCounts++;
                }

                return localEvictCounts;
            }

            void insertItemToLists(key_t p_key, size_t p_size) noexcept
            {
                m_recencyList.emplace_front(p_key);
                m_posRecencyList[p_key] = m_recencyList.begin();

                m_currentSize = m_currentSize + p_size;
            }

            void insertItem(key_t p_key, buf_t* p_buffer, size_t p_size) noexcept
            {
                m_cached->insert(
                    std::make_pair(p_key, BufferItemWrapper(BufferItem(p_key, p_buffer, p_size, 0)))
                );

                insertItemToLists(p_key, p_size);
            }
        };


        class CacheLfuCore : public CacheCore
        {
        protected:
            size_t          m_minFreq;


#define META_FREQLRU_T      std::map<key_t, LIST_T<key_t>>
#define META_FREQLRU_POS_T  MAP_T<key_t, LIST_T<key_t>::iterator>

            std::unique_ptr<META_FREQLRU_T> m_freqList;
            std::unique_ptr<META_FREQLRU_POS_T> m_posFreqList;

        public:
            CacheLfuCore(const size_t p_capacity, std::shared_ptr<DATA_CACHED_T> p_cached) noexcept
                : CacheCore(p_capacity, p_cached), m_minFreq(0)
            {
                m_freqList.reset(new META_FREQLRU_T);
                m_posFreqList.reset(new META_FREQLRU_POS_T);

                m_cached = p_cached;
            }

            virtual ~CacheLfuCore() noexcept
            {

            }

            void updateGetHit(struct BufferItemWrapper& p_bufItem) noexcept
            {
                m_hitCounts++;

                size_t prevFreq = p_bufItem.m_freq;
                key_t key = p_bufItem.m_item.getKey();

                p_bufItem.m_freq += 1;

                (*m_freqList)[prevFreq].erase((*m_posFreqList)[key]);   // Erase from the LRU list

                if ((*m_freqList)[prevFreq].empty())
                {
                    int numErased = (*m_freqList).erase(prevFreq);
                    if (m_minFreq == prevFreq)
                        m_minFreq += 1;
                }

                (*m_freqList)[p_bufItem.m_freq].push_back(key);
                (*m_posFreqList)[key] = --(*m_freqList)[p_bufItem.m_freq].end(); // Update iterator
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
            void eraseItemFromLists(key_t p_evictKey) noexcept
            {
                auto& lruItem = ((*m_cached)[p_evictKey].m_item);
                uint64_t decrSize = lruItem.getSize();

                (*m_posFreqList).erase(p_evictKey);

                m_currentSize = m_currentSize - decrSize;
            }

            size_t evictOverflows(size_t p_newSize = 0) noexcept
            {
                size_t localEvictCounts = 0;
                while ((m_currentSize + p_newSize) > m_capacity)
                {
                    
                    key_t evictKey = (*m_freqList)[m_minFreq].front();
                    (*m_freqList)[m_minFreq].pop_front();                // Pop least recently used

                    // If it is the last element in the m_minFreq list, 
                    // need to update the m_minFreq to next one.    
                    if ((*m_freqList)[m_minFreq].empty())
                    {
                        m_freqList->erase(m_minFreq);
                        key_t nextMinKey = (*(m_freqList->upper_bound(m_minFreq))).second.front();

                        m_minFreq = nextMinKey;
                    }

                    eraseItemFromLists(evictKey);
                    m_cached->erase(evictKey);

                    m_evictCounts++;
                    localEvictCounts++;
                }

                return localEvictCounts;
            }

            void insertItemToLists(key_t p_key, size_t p_size) noexcept
            {
                m_minFreq = 1;

                (*m_freqList)[m_minFreq].push_back(p_key);
                (*m_posFreqList)[p_key] = --(*m_freqList)[m_minFreq].end();

                 m_currentSize += p_size;
            }

            void insertItem(key_t p_key, buf_t* p_buffer, size_t p_size) noexcept
            {
                m_cached->insert(
                    std::make_pair(p_key, BufferItemWrapper(BufferItem(p_key, p_buffer, p_size, 0)))
                );

                insertItemToLists(p_key, p_size);
            }
        };

        class CacheLfuResizableCore : public CacheLfuCore
        {
        protected:

        public:
            CacheLfuResizableCore(const size_t p_capacity, std::shared_ptr<DATA_CACHED_T> p_cached) noexcept
                : CacheLfuCore(p_capacity, p_cached)
            {

            }

            virtual ~CacheLfuResizableCore() noexcept
            {

            }

            void setCapacity(size_t p_newCapacity) noexcept
            {
                p_newCapacity;
            }
        };
    }
}

#endif
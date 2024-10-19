
#pragma once
// Author: Sukjoon Oh (sjoon@kaist.ac.kr)
// 

#ifndef _SPTAG_EXT_READBULK_H
#define _SPTAG_EXT_READBULK_H

#include <cassert>
#include <cstdlib>
#include <cstdarg>

#include <map>
#include <list>
#include <unordered_map>

#include <cstdio>

#include "inc/Core/SearchQuery.h"
// #include "inc/Extension/CacheCores.hh"

// #define DEBUG
#ifdef BATCH_READ_CACHE_DEBUG
    #define PRINT__     debug_print
#else
    #define PRINT__     debug_print_off
#endif


namespace SPTAG 
{
    namespace EXT 
    {
        void debug_print(const char *format, ...);
        void debug_print_off(const char *format, ...);

        using distance_t    = float;
        using key_t         = uintptr_t;

        class PostingList
        {
        private:
            // Main 
            key_t   m_offset;
            uint8_t* m_buffer;
            size_t  m_size;

        public:
            PostingList() noexcept
                : m_size(0), m_offset(0)
            {
                m_buffer = nullptr;
            }
            
            PostingList(key_t p_offset, uint8_t* p_object, size_t p_size) noexcept
                : m_offset(p_offset), m_size(p_size)
            {
                // m_buffer = new uint8_t[m_size];
                m_buffer = (uint8_t*)aligned_alloc(4096, m_size);
                std::memcpy(m_buffer, p_object, m_size); // copy
            }

            PostingList(PostingList&& p_other) noexcept
                : m_size(p_other.m_size), m_offset(p_other.m_offset), m_buffer(p_other.m_buffer)
            {
                p_other.m_buffer = nullptr;
            }

            virtual ~PostingList() noexcept
            {
                if (m_buffer != nullptr)
                    free(m_buffer);
            }

            key_t getOffset() noexcept
            {
                return m_offset;
            }

            uint8_t* getBuffer() noexcept
            {
                return m_buffer;
            }         

            const size_t getSize() noexcept
            {
                return m_size;
            }
        };

        struct PostingListWrapper
        {
            PostingList m_item;
            size_t      m_freq;

            PostingListWrapper() = default;
            PostingListWrapper(PostingList&& p_item)
                : m_item(std::move(p_item)), m_freq(1)
            {

            }
        };
        
        class ReadBatch
        {
        protected:
            key_t m_batchId;
            size_t m_batchSize;

            std::map<key_t, size_t> m_keys;

        public:
            ReadBatch() noexcept
                : m_batchId(0), m_batchSize(0)
            {
                m_keys.clear();
            }

            ReadBatch(const key_t p_batchId) noexcept
                : m_batchId(p_batchId), m_batchSize(0)
            {
                m_keys.clear();
            }

            virtual ~ReadBatch() noexcept
            {

            }

            bool isSet(const key_t p_key)
            {
                return (m_keys.find(p_key) != m_keys.end());
            }

            void addKey(const key_t p_key, size_t p_size) noexcept
            {
                m_keys.insert(std::make_pair(p_key, p_size));
                m_batchSize += p_size;
            }

            void removeKey(const key_t p_key) noexcept
            {   
                size_t size = m_keys[p_key];
                m_keys.erase(p_key);
                m_batchSize -= size;
            }

            inline const size_t getBatchSize() noexcept
            {
                return m_batchSize;
            }

            inline const key_t getbatchId() const 
            {
                return m_batchId;
            }

            inline std::map<key_t, size_t>& getKeys()
            {
                return m_keys;
            }
        };


        template <typename T>
        class GeneralCache
        {
        protected:
            size_t          m_capacity;
            size_t          m_currentSize;

            std::shared_ptr<std::unordered_map<key_t, T>> m_cached;

            // Stats
            uint64_t        m_hitCounts;
            uint64_t        m_missCounts;
            uint64_t        m_evictCounts;
        
        public:
            GeneralCache(
                const size_t p_capacity, 
                std::shared_ptr<std::unordered_map<key_t, T>> p_cached) noexcept
                : m_capacity(p_capacity), m_currentSize(0)
            {
                m_cached = p_cached;
            }

            virtual ~GeneralCache() noexcept
            {

            }

            bool isCached(key_t p_key) noexcept
            {
                return ((*m_cached).find(p_key) != (*m_cached).end());
            }

            void resetCounts() noexcept
            {
                m_hitCounts = m_missCounts = m_evictCounts = 0;
            }

            size_t getCurrentSize() noexcept
            {
                return m_currentSize;
            }            
        };


        class PostingListLfuCache : public GeneralCache<PostingListWrapper>
        {
        protected:
            size_t          m_minFreq;
            
            std::unique_ptr<std::map<key_t, std::list<key_t>>> m_freqList;
            std::unique_ptr<std::unordered_map<key_t, std::list<key_t>::iterator>> m_posFreqList;

        public:
            PostingListLfuCache(
                    const size_t p_capacity, 
                    std::shared_ptr<std::unordered_map<key_t, PostingListWrapper>> p_cached) noexcept
                : GeneralCache(p_capacity, p_cached), m_minFreq(0)
            {
                m_freqList.reset(new std::map<key_t, std::list<key_t>>());
                m_posFreqList.reset(new std::unordered_map<key_t, std::list<key_t>::iterator>());

                m_cached = p_cached;
            }

            virtual ~PostingListLfuCache() noexcept
            {

            }

            void updateGetHit(struct PostingListWrapper& p_bufItem) noexcept
            {
                m_hitCounts++;

                size_t prevFreq = p_bufItem.m_freq;
                key_t key = p_bufItem.m_item.getOffset();

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

            void updateGetHit(key_t p_key) noexcept
            {

                PRINT__(" > Update Hit LFU: %ld\n", p_key);

                m_hitCounts++;
                
                struct PostingListWrapper& bufItem = (*m_cached)[p_key];

                size_t prevFreq = bufItem.m_freq;
                key_t key = p_key;

                (*m_freqList)[prevFreq].erase((*m_posFreqList)[key]);   // Erase from the LRU list

                if ((*m_freqList)[prevFreq].empty())
                {
                    int numErased = (*m_freqList).erase(prevFreq);
                    if (m_minFreq == prevFreq)
                        m_minFreq += 1;
                }

                (*m_freqList)[bufItem.m_freq].push_back(key);
                (*m_posFreqList)[key] = --(*m_freqList)[bufItem.m_freq].end(); // Update iterator
            }

            PostingList* getItemAndUpdate(key_t p_key) noexcept
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

            PostingList* getItemWithoutUpdate(key_t p_key) noexcept
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

                        auto nextMinKeyItor = m_freqList->upper_bound(m_minFreq);
                        key_t nextMinKey = (*nextMinKeyItor).first;

                        m_minFreq = nextMinKey;                         
                    }

                    PRINT__(" > Evicted from LFU: %ld\n", evictKey);

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

            void insertItem(key_t p_key, uint8_t* p_buffer, size_t p_size) noexcept
            {
                m_cached->insert(
                    std::make_pair(p_key, PostingListWrapper(PostingList(p_key, p_buffer, p_size)))
                );

                insertItemToLists(p_key, p_size);
            }
        };


        class ReadBatchFifoCache : public GeneralCache<ReadBatch>
        {
        protected:
            std::list<key_t> m_recencyList;    // For tracking LRU order
                                            // Inserted at the front, poped at the back
            std::unordered_map<key_t, typename std::list<key_t>::iterator> m_posRecencyList;

        public:
            ReadBatchFifoCache(
                    const size_t p_capacity, 
                    std::shared_ptr<std::unordered_map<key_t, ReadBatch>> p_cached) noexcept
                : GeneralCache(p_capacity, p_cached)
            {

            }

            virtual ~ReadBatchFifoCache() noexcept
            {

            }
            
            void updateGetHit(ReadBatch& p_bufItem) noexcept
            {
                
            }

            // 
            // Only erases from the current tracking list
            void eraseItemFromLists(key_t p_evictKey) noexcept
            {
                auto& oldestItem = (*m_cached)[p_evictKey];
                uint64_t decrSize = oldestItem.getBatchSize();

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

            size_t evictOverflowsOnce(std::vector<key_t>& p_evictedKeys) noexcept
            {
                size_t evictedBatchSize = 0;
                // while ((m_currentSize + p_newSize) > m_capacity)
                // {
                if ((m_currentSize + evictedBatchSize) > m_capacity)
                {
                    key_t oldestKey = m_recencyList.back();

                    evictedBatchSize = (*m_cached)[oldestKey].getBatchSize();

                    // Get keys from ReadBatch
                    for (auto& postingListKey: (*m_cached)[oldestKey].getKeys())
                    {
                        p_evictedKeys.emplace_back(postingListKey.first);
                    }

                    eraseItemFromLists(oldestKey);
                    m_cached->erase(oldestKey);

                    m_evictCounts++;
                }

                return evictedBatchSize;
            }

            void insertItemToLists(key_t p_key, size_t p_size) noexcept
            {
                m_recencyList.emplace_front(p_key);
                m_posRecencyList[p_key] = m_recencyList.begin();

                m_currentSize = m_currentSize + p_size;
            }

            void insertItem(key_t p_key, ReadBatch& p_readBatch) noexcept
            {
                size_t size = p_readBatch.getBatchSize();
                m_cached->insert(
                    std::make_pair(p_key, std::move(p_readBatch))
                );

                insertItemToLists(p_key, size);
            }

            void removeKeyFromReadBatch(key_t p_key)
            {
                auto& batch = ((*m_cached)[p_key]);
                batch.removeKey(p_key);
            }
        };




        using batch_pos_t   = typename std::list<ReadBatch>::iterator;

        class ReadBatchCache
        {
        protected:
            size_t m_nextBatchId;

            std::shared_ptr<std::unordered_map<key_t, PostingListWrapper>> m_postingListCached;    // All cached posting lists are here.
            std::shared_ptr<std::unordered_map<key_t, ReadBatch>> m_readBatchCached;        // Manages all read batch infos.

            // Key : offset of the Posting Lists (key), value : Batch ID
            //  Conversion
            std::unordered_map<key_t, key_t> m_postingListToBatchId;
            std::unordered_map<key_t, bool> m_batchIdInUse;

            const size_t m_lfuCapacity;
            const size_t m_fifoCapacity;

            std::unique_ptr<PostingListLfuCache> m_postingListLfuCache;
            std::unique_ptr<ReadBatchFifoCache> m_readBatchFifoCache;

            enum { UNCACHED = 0, BATCH_CACHED, LFU_CACHED };
            struct DelayedUpdates
            {
                key_t m_offset;                
                int m_status;

                DelayedUpdates(key_t p_offset, int p_status) : m_offset(p_offset), m_status(p_status)
                {

                }
            };

            void* m_delayedRequests;
            std::vector<DelayedUpdates> m_delayedUpdates;

            // Stats
            struct CacheStatus {
                size_t m_hitCounts;
                size_t m_missCounts;
                size_t m_reuseCounts;

                CacheStatus() : m_hitCounts(0), m_missCounts(0), m_reuseCounts(0)
                {
                    
                }

                CacheStatus(size_t p_hitCounts, size_t p_missCounts, size_t p_reuseCounts)
                    : m_hitCounts(p_hitCounts), m_missCounts(p_missCounts), m_reuseCounts(p_reuseCounts)
                {
                    
                }
            };

            struct CacheStatus m_statusCurr;
            struct CacheStatus m_statusBatch;

            std::vector<struct CacheStatus> m_statusBatchHistory;
            std::vector<double> m_statusLatencyGet;
            std::vector<double> m_statusLatencyBatchRead;

        public:
            ReadBatchCache(const size_t p_lfuCapacity, const size_t p_fifoCapacity) noexcept
                : m_lfuCapacity(p_lfuCapacity), m_fifoCapacity(p_fifoCapacity), m_nextBatchId(0),
                    m_postingListCached(new std::unordered_map<key_t, PostingListWrapper>()),
                    m_readBatchCached(new std::unordered_map<key_t, ReadBatch>())
            {
                m_postingListLfuCache.reset(new PostingListLfuCache(size_t(m_lfuCapacity), m_postingListCached));
                m_readBatchFifoCache.reset(new ReadBatchFifoCache(size_t(m_fifoCapacity), m_readBatchCached));

                m_statusCurr.m_hitCounts = 0;
                m_statusCurr.m_missCounts = 0;
                m_statusCurr.m_reuseCounts = 0;

                m_statusBatch.m_hitCounts = 0;
                m_statusBatch.m_missCounts = 0;
                m_statusBatch.m_reuseCounts = 0;

                m_delayedRequests = nullptr;
            }

            virtual ~ReadBatchCache() noexcept
            {

            }

            bool isCachedLfu(const key_t p_key)
            {
                return ((*m_postingListCached).find(p_key) != (*m_postingListCached).end());
            }

            bool isCachedBatch(const key_t p_key)
            {
                return (m_postingListToBatchId.find(p_key) != m_postingListToBatchId.end());
            }

            bool isBatchIdInUse(const key_t p_key)
            {
                if (m_batchIdInUse.find(p_key) != m_batchIdInUse.end())
                {
                    // Case when Batch ID exists in the cache.
                    return true;
                }

                return false;
            }

            const key_t allocateNewBatchId()
            {
                key_t nextCandidate = m_nextBatchId;
                if (isBatchIdInUse(nextCandidate))
                {
                    // Case when the new batch id is in use
                    m_nextBatchId++;
                    return allocateNewBatchId();
                }

                else
                {
                    m_batchIdInUse[nextCandidate] = true;    
                    return nextCandidate;
                }
            }

            void deallocateBatchId(const key_t p_key)
            {
                m_batchIdInUse.erase(p_key);
            }


            // 
            // Calls at every single posting list
            size_t getCachedReadBatch(key_t p_key, std::vector<PostingList*>& p_observedBatch)
            {
                key_t batchId = 0;

                // If it is already cached (both LFU and in the batch)
                if (isCachedLfu(p_key)) 
                {       

                    if (isCachedBatch(p_key))
                    {
                        batchId = m_postingListToBatchId[p_key];
                        ReadBatch& readBatch = (*m_readBatchCached)[batchId];

                        PRINT__(" > Preparing to return batch ID: %ld, (Size: %ld)\n", batchId, readBatch.getKeys().size());
#ifdef BATCH_READ_CACHE_DEBUG
                        for (auto& elem: readBatch.getKeys())
                        {
                            PRINT__("%ld, ", elem.first);
                        }
#endif
                        PRINT__("\n");

                        // If the target key, place it at the front
                        // p_observedBatch.emplace_front(
                        //     &((*m_postingListCached)[p_key].m_item)
                        // );

                        for (auto& item: readBatch.getKeys())
                        {
                            // Skip if the target key, since it will be always in front of the list.
                            if (item.first != p_key)
                            {
                                p_observedBatch.emplace_back(
                                    &((*m_postingListCached)[item.first].m_item)
                                );

                            }
                        }

                        p_observedBatch.emplace_back(
                            &((*m_postingListCached)[p_key].m_item)
                        );

                        PRINT__(" > Returning this... (Batch size: %ld)\n", readBatch.getBatchSize());
#ifdef BATCH_READ_CACHE_DEBUG
                        for (auto& elem: p_observedBatch)
                        {
                            PRINT__("%ld(%d), ", elem->getOffset(), isCachedBatch(elem->getOffset()));
                        }
#endif
                        PRINT__("\n");
                        
                        m_delayedUpdates.emplace_back(p_key, BATCH_CACHED);
                        return readBatch.getKeys().size();
                    }

                    // Case when it is in the LRU.
                    else 
                    {
                        // p_observedBatch.emplace_front(&((*m_postingListCached)[p_key].m_item));
                        p_observedBatch.emplace_back(&((*m_postingListCached)[p_key].m_item));

                        PRINT__(" > Returning this at LFU...\n");
#ifdef BATCH_READ_CACHE_DEBUG
                        for (auto& elem: p_observedBatch)
                        {
                            PRINT__("%ld(%d), ", elem->getOffset(), isCachedBatch(elem->getOffset()));
                        }
#endif
                        PRINT__("\n");

                        m_delayedUpdates.emplace_back(p_key, LFU_CACHED);
                        return size_t(1);
                    }
                }

                // Delay them.
                else
                {
                    m_delayedUpdates.emplace_back(p_key, UNCACHED);
                    return size_t(0);
                }
            }

            void setRequests(void* p_requests)
            {
                m_delayedRequests = p_requests;
            }

            void processDelayedUpdates()
            {
                key_t newBatchId = allocateNewBatchId();
                
                size_t localHits = 0;

                size_t uncachedCounts = 0;
                size_t batchHitCounts = 0;
                size_t lfuHitCounts = 0;

                ReadBatch readBatch(newBatchId);

                bool isBatchFormed = false;

                PRINT__(" ------ delayed process start -----\n");
                PRINT__(" > Status: ");

#ifdef BATCH_READ_CACHE_DEBUG
                for (auto& elem: m_delayedUpdates)
                    PRINT__("%ld, ", elem.m_status);
#endif

                PRINT__("\n");

                int index = 0;
                for (auto& item: m_delayedUpdates)
                {
                    key_t key = item.m_offset;
                    PRINT__("index: %d", index);
                    
                    switch (item.m_status)
                    {
                        case UNCACHED: // Case 0
                        {
                            // Make it cache, record it as a new read-batch.
                            //  May be recorded as the uncached ones, but it might have formed a batch in the previous loop.
                            struct ListInfo
                            {
                                std::size_t     listTotalBytes = 0;
                                int             listEleCount = 0;
                                std::uint16_t   listPageCount = 0;
                                std::uint64_t   listOffset = 0;
                                std::uint16_t   pageOffset = 0;
                            };

                            SPTAG::Helper::AsyncReadRequest* requests = 
                                (SPTAG::Helper::AsyncReadRequest*)m_delayedRequests;

                            // 1. Record to global cache.
                            m_postingListCached->insert(
                                std::make_pair(
                                    key, 
                                    PostingListWrapper(PostingList(
                                        key, 
                                        reinterpret_cast<uint8_t*>(requests[index].m_buffer), 
                                        requests[index].m_readSize)))
                            );

                            // 2. Give it a new batch ID
                            m_postingListToBatchId[key] = newBatchId;

                            PRINT__(" > Adding %ld to batch ID(%ld)\n", key, newBatchId);

                            uncachedCounts++;
                            readBatch.addKey(key, requests[index].m_readSize);

                            isBatchFormed = true;
                            break;
                        }

                        case BATCH_CACHED: // Case 1
                        {
                            // Case when hit while in read-batch (FIFO).
                            //  Move it to the upper LFU cache the posting list. 
                            key_t existingBatchId = m_postingListToBatchId[key];

                            m_postingListToBatchId.erase(key);              // 1. First, erase the ID from the mapping (offset-batchid)
                            ((*m_readBatchCached)[existingBatchId]).removeKey(key);     // 2. Remove from the FIFO (of exisitng batch)

                            PRINT__(" > Moving %ld from batch %ld: is map has the key(%ld), Left posting lists(%ld)\n", 
                                key, existingBatchId, isCachedBatch(key), ((*m_readBatchCached)[existingBatchId]).getKeys().size()); 

                            // 3. If that batch is zero, reqmove from the cached one.
                            if (((*m_readBatchCached)[existingBatchId]).getBatchSize() == 0)
                            {
                                m_readBatchFifoCache->eraseItemFromLists(existingBatchId);
                                m_readBatchCached->erase(existingBatchId);

                                PRINT__(" > Removed batch ID %ld\n", existingBatchId); 
                            }

                            // 4. Move the item to LRU list
                            struct PostingListWrapper& item = (*m_postingListCached)[key];
                            size_t incrSize = item.m_item.getSize();

                            m_postingListLfuCache->evictOverflows(incrSize);
                            m_postingListLfuCache->insertItemToLists(key, incrSize);

                            localHits++;
                            batchHitCounts++;

                            break;
                        }

                        case LFU_CACHED: // Case 2
                        {
                            m_postingListLfuCache->updateGetHit(key);

                            lfuHitCounts++;
                            localHits++;
                            break;
                        }

                        default:
                        {
                            assert(false); // Something is wrong.
                            break;
                        }
                    }

                    
                    index++;
                }

                if (isBatchFormed == false)
                {
                    deallocateBatchId(newBatchId);
                }

                if (uncachedCounts > 0)
                {
                    m_nextBatchId++;

                    std::vector<key_t> evictedKeys;
                    m_readBatchFifoCache->insertItem(newBatchId, readBatch);

                    while (m_readBatchFifoCache->evictOverflowsOnce(evictedKeys) != 0)
                    {   
                        // Not now.
                        // Write to a file here,

                    }

                    for (auto& evictedPostingListKey: evictedKeys)
                    {
                        PRINT__(" > Evicted from FIFO: %ld\n", evictedPostingListKey);
                        m_postingListToBatchId.erase(evictedPostingListKey);
                        m_postingListCached->erase(evictedPostingListKey);
                    }

                    // evictedKeys.clear();
                }


                m_delayedUpdates.clear();
                m_delayedRequests = nullptr;

                m_statusBatch.m_hitCounts = localHits;
                m_statusBatch.m_missCounts = uncachedCounts;

                PRINT__("\n");
            }

            inline void recordReuseCounts(size_t p_reuseCounts) noexcept
            {
                m_statusBatch.m_reuseCounts = p_reuseCounts;
            }

            inline void recordCurrentStat() noexcept
            {
                
                m_statusBatchHistory.emplace_back(
                    m_statusBatch.m_hitCounts, 
                    m_statusBatch.m_missCounts,
                    m_statusBatch.m_reuseCounts - m_statusBatch.m_hitCounts
                );

                m_statusBatch.m_hitCounts = 0;
                m_statusBatch.m_missCounts = 0;
                m_statusBatch.m_reuseCounts = 0;
            }

            inline void recordLatencyGet(double p_record) noexcept
            {
                m_statusLatencyGet.push_back(p_record);
            }

            inline void recordLatencyTotalBatchRead(double p_record) noexcept
            {
                m_statusLatencyBatchRead.push_back(p_record);
            }

            inline void exportStatToFile(std::string p_filename) noexcept
            {
                std::ofstream fileCacheTrace(p_filename);

                if (!fileCacheTrace)
                    return;

                else
                {
                    int globalIndex = 0;
                    int index = 0;
                    for (auto& stat: m_statusBatchHistory)
                    {
                        double totalGetLatencyMs = 0;
                        size_t readBatch = stat.m_hitCounts + stat.m_missCounts;
                        for (int i = index; i < (index + readBatch); i++)
                            totalGetLatencyMs += m_statusLatencyGet[i];
                        
                        index += int(readBatch);

                        fileCacheTrace  << stat.m_hitCounts << "\t"
                                        << stat.m_missCounts << "\t"
                                        << stat.m_reuseCounts << "\t"
                                        << totalGetLatencyMs << "\t"
                                        << m_statusLatencyBatchRead[globalIndex] << "\n";
                        
                        globalIndex++;
                    }

                    fileCacheTrace << std::endl;
                    fileCacheTrace.close();
                }
            }
        };
    }
}

#endif
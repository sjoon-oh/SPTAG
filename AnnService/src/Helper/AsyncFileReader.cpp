// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#include "inc/Helper/AsyncFileReader.h"

// Author: Sukjoon Oh (sjoon@kaist.ac.kr), added
#include <cassert>
#include <memory>
#include <utility>
#include <iostream>

#define _CACHE_ENABLED_
#if defined (_CACHE_ENABLED_)

#include "inc/Extension/CacheLruWeak.hh"
#include "inc/Extension/CacheLruMt.hh"
#include "inc/Extension/CacheFifoMt.hh"
#include "inc/Extension/CacheLfuMt.hh"
#include "inc/Extension/CacheLfu2.hh"
#include "inc/Extension/Cache2Q.hh"

#include "inc/Extension/ReadBatchCache.hh"

#define _CACHE_BATCH_READ_
#if defined (_CACHE_FIFO_)
std::unique_ptr<SPTAG::EXT::CacheFifoSpannMt> globalCache;
#elif defined (_CACHE_LFU_)
std::unique_ptr<SPTAG::EXT::CacheLfu2> globalCache;
#elif defined (_CACHE_LRU_)
std::unique_ptr<SPTAG::EXT::CacheLruSpannMt> globalCache;
#elif defined (_CACHE_2Q_)
std::unique_ptr<SPTAG::EXT::Cache2Q> globalCache;
#elif defined (_CACHE_BATCH_READ_)
std::unique_ptr<SPTAG::EXT::ReadBatchCache> globalCache;
#endif
#endif

namespace SPTAG {
    namespace Helper {
#ifndef _MSC_VER
        void SetThreadAffinity(int threadID, std::thread& thread, NumaStrategy socketStrategy, OrderStrategy idStrategy)
        {
#ifdef NUMA
            int numGroups = numa_num_task_nodes();
            int numCpus = numa_num_task_cpus() / numGroups;

            int group = threadID / numCpus;
            int cpuid = threadID % numCpus;
            if (socketStrategy == NumaStrategy::SCATTER) {
                group = threadID % numGroups;
                cpuid = (threadID / numGroups) % numCpus;
            }

            struct bitmask* cpumask = numa_allocate_cpumask();
            if (!numa_node_to_cpus(group, cpumask)) {
                unsigned int nodecpu = 0;
                for (unsigned int i = 0; i < cpumask->size; i++) {
                    if (numa_bitmask_isbitset(cpumask, i)) {
                        if (cpuid == nodecpu) {
                            cpu_set_t cpuset;
                            CPU_ZERO(&cpuset);
                            CPU_SET(i, &cpuset);
                            int rc = pthread_setaffinity_np(thread.native_handle(), sizeof(cpu_set_t), &cpuset);
                            if (rc != 0) {
                                SPTAGLIB_LOG(Helper::LogLevel::LL_Error, "Error calling pthread_setaffinity_np for thread %d: %d\n", threadID, rc);
                            }
                            break;
                        }
                        nodecpu++;
                    }
                }
            }
#else
            cpu_set_t cpuset;
            CPU_ZERO(&cpuset);
            CPU_SET(threadID, &cpuset);
            int rc = pthread_setaffinity_np(thread.native_handle(), sizeof(cpu_set_t), &cpuset);
            if (rc != 0) {
                SPTAGLIB_LOG(Helper::LogLevel::LL_Error, "Error calling pthread_setaffinity_np for thread %d: %d\n", threadID, rc);
            }
#endif
        }

        struct timespec AIOTimeout {0, 30000};
        void BatchReadFileAsync(std::vector<std::shared_ptr<Helper::DiskIO>>& handlers, AsyncReadRequest* readRequests, int num, int p_tid)
        {
            std::vector<struct iocb> myiocbs(num);
            std::vector<std::vector<struct iocb*>> iocbs(handlers.size());
            std::vector<int> submitted(handlers.size(), 0);
            std::vector<int> done(handlers.size(), 0);
            int totalToSubmit = 0, channel = 0;

#define _CACHE_ENABLED_
#if defined (_CACHE_ENABLED_)
            
            // Author: Sukjoon Oh (sjoon@kaist.ac.kr), added
            // bool* requestToCache;
            // requestToCache = new bool[handlers.size()];
            // std::memset(requestToCache, true, sizeof(bool) * handlers.size());

            // assert(requestToCache != nullptr);
#ifndef _CACHE_BATCH_READ_
            std::vector<bool> requestToCache(num, true);
#else
            size_t reuseCount = 0;
            size_t passThroughCount = 0;

            size_t diskFetchCount = 0;

            std::vector<EXT::PostingList*> observedBatch;
            std::vector<bool> callbackDelayed(num, true);
#endif

            std::chrono::steady_clock::time_point timeStart, timeStartBatchRead;
            std::chrono::steady_clock::time_point timeEnd, timeEndBatchRead;

            #define getElapsedMsIndependent(start, end) \
                ((std::chrono::duration_cast<std::chrono::microseconds>(end - start).count() * 1.0) / 1000.000)

            timeStartBatchRead = std::chrono::steady_clock::now();
            
#endif

            memset(myiocbs.data(), 0, num * sizeof(struct iocb));
            for (int i = 0; i < num; i++) {
                AsyncReadRequest* readRequest = &(readRequests[i]);

#if defined (_CACHE_ENABLED_)

                struct ListInfo
                {
                    std::size_t listTotalBytes = 0;
                    int listEleCount = 0;
                    std::uint16_t listPageCount = 0;
                    std::uint64_t listOffset = 0;
                    std::uint16_t pageOffset = 0;
                };

                timeStart = std::chrono::steady_clock::now();

                // Author: Sukjoon Oh (sjoon@kaist.ac.kr), added
                //  Note: Check the cache first.
                ListInfo* listInfo = (ListInfo*)(readRequest->m_payload);
                uintptr_t key = static_cast<uintptr_t>(readRequest->m_offset) + listInfo->pageOffset;
                
#ifndef _CACHE_BATCH_READ_    
                EXT::CacheItem<uintptr_t>* fetchedItem = globalCache->getCachedItem(key); 
                    // Key is the offset.      
                if (fetchedItem != nullptr)
                {
                    requestToCache[i] = false;   // Skip if fetched.
                    std::memcpy(
                        reinterpret_cast<uint8_t*>(readRequest->m_buffer), fetchedItem->getItem(), readRequest->m_readSize);

                    continue;
                }
#else
                int substituteIndex = num - int(reuseCount);

                // printf("Now iterating: %d/%d, num(%d), current observed left(%ld), looking for(%ld)", i, substituteIndex, num, observedBatch.size(), key);
                if (i > (substituteIndex))
                {

                    // printf("Original disk load: %ld, trying to overwrite size: %ld\n", readRequest->m_readSize, observedBatch.front()->getSize());

                    char* destBuffer = reinterpret_cast<char*>(observedBatch.back()->getBuffer());
                    
                    // printf(", substituted (Found: %ld) at destination(0x%p)\n", observedBatch.front()->getOffset(), destBuffer);
                    
                    // Case when prefetched are reused.
                    std::memcpy(
                        reinterpret_cast<uint8_t*>(readRequest->m_buffer), 
                        destBuffer, 
                        observedBatch.back()->getSize());

                    // substitutedBuffers[i] = readRequest->m_buffer;
                    // readRequest->m_buffer = destBuffer;

                    observedBatch.pop_back();

                    timeEnd = std::chrono::steady_clock::now();
                    globalCache->recordLatencyGet(getElapsedMsIndependent(timeStart, timeEnd));
                    
                    continue;
                }

                // printf("\n");

                size_t fetchedBatchSize = 0;
                fetchedBatchSize = globalCache->getCachedReadBatch(key, observedBatch);

                if (fetchedBatchSize > 0)
                {
                    // This is the case when single posting list is fetched. 
                    // The posting list is in the LFU.

                    // printf("Fetched batch size (%ld)\n", fetchedBatchSize);
                    char* destBuffer = reinterpret_cast<char*>(observedBatch.back()->getBuffer());

                    // Return to request.
                    std::memcpy(reinterpret_cast<uint8_t*>(readRequest->m_buffer), destBuffer, observedBatch.back()->getSize());
                    // substitutedBuffers[i] = readRequest->m_buffer;
                    // readRequest->m_buffer = destBuffer;

                    observedBatch.pop_back();

                    reuseCount = observedBatch.size();
                    passThroughCount++;

                    timeEnd = std::chrono::steady_clock::now();
                    globalCache->recordLatencyGet(getElapsedMsIndependent(timeStart, timeEnd));

                    continue;
                }
                else
                    callbackDelayed[i] = false; // Case when none found. Skip.
                
                diskFetchCount++;

                timeEnd = std::chrono::steady_clock::now();
                globalCache->recordLatencyGet(getElapsedMsIndependent(timeStart, timeEnd));
#endif


#endif

                channel = readRequest->m_status & 0xffff;
                int fileid = (readRequest->m_status >> 16);

                struct iocb* myiocb = &(myiocbs[totalToSubmit++]);
                myiocb->aio_data = reinterpret_cast<uintptr_t>(readRequest);
                myiocb->aio_lio_opcode = IOCB_CMD_PREAD;
                myiocb->aio_fildes = ((AsyncFileIO*)(handlers[fileid].get()))->GetFileHandler();
                myiocb->aio_buf = (std::uint64_t)(readRequest->m_buffer);
                myiocb->aio_nbytes = readRequest->m_readSize;
                myiocb->aio_offset = static_cast<std::int64_t>(readRequest->m_offset);

                iocbs[fileid].emplace_back(myiocb);
            }
            std::vector<struct io_event> events(totalToSubmit);
            int totalDone = 0, totalSubmitted = 0, totalQueued = 0;
            while (totalDone < totalToSubmit) {
                if (totalSubmitted < totalToSubmit) {
                    for (int i = 0; i < handlers.size(); i++) {
                        if (submitted[i] < iocbs[i].size()) {
                            AsyncFileIO* handler = (AsyncFileIO*)(handlers[i].get());
                            int s = syscall(__NR_io_submit, handler->GetIOCP(channel), iocbs[i].size() - submitted[i], iocbs[i].data() + submitted[i]);
                            if (s > 0) {
                                submitted[i] += s;
                                totalSubmitted += s;
                            }
                            else {
                                SPTAGLIB_LOG(Helper::LogLevel::LL_Error, "fid:%d channel %d, to submit:%d, submitted:%s\n", i, channel, iocbs[i].size() - submitted[i], strerror(-s));
                            }
                        }
                    }
                }

                for (int i = totalQueued; i < totalDone; i++) {
                    AsyncReadRequest* req = reinterpret_cast<AsyncReadRequest*>((events[i].data));
                    if (nullptr != req)
                    {
                        req->m_callback(true);
                    }
                }
                totalQueued = totalDone;

                for (int i = 0; i < handlers.size(); i++) {
                    if (done[i] < submitted[i]) {
                        int wait = submitted[i] - done[i];
                        AsyncFileIO* handler = (AsyncFileIO*)(handlers[i].get());
                        auto d = syscall(__NR_io_getevents, handler->GetIOCP(channel), wait, wait, events.data() + totalDone, &AIOTimeout);
                        done[i] += d;
                        totalDone += d;
                    }
                }
            }

            for (int i = totalQueued; i < totalDone; i++) {
                AsyncReadRequest* req = reinterpret_cast<AsyncReadRequest*>((events[i].data));
                if (nullptr != req)
                {
                    req->m_callback(true);
                }
            }

            // Author: Sukjoon Oh (sjoon@kaist.ac.kr), added
            //  Note: 
            // 

#if defined (_CACHE_ENABLED_)            
#ifndef _CACHE_BATCH_READ_
            for (int i = 0; i < num; i++)
            {
                if (requestToCache[i] == false)
                {
                    readRequests[i].m_callback(true);
                }
            }
            globalCache->setDelayedToCache(num, std::move(requestToCache), readRequests, p_tid);

#else
            for (int i = 0; i < num; i++)
            {
                if (callbackDelayed[i] == true)
                {
                    readRequests[i].m_callback(true);
                }
            }

            timeEndBatchRead = std::chrono::steady_clock::now();
            globalCache->recordLatencyTotalBatchRead(getElapsedMsIndependent(timeStartBatchRead, timeEndBatchRead));

            globalCache->setRequests(readRequests);
            globalCache->recordReuseCounts(num - diskFetchCount);

            // std::cout << "BatchRead done. -- Disk Fetch Count: " << diskFetchCount << ", Passthroughs: " << passThroughCount << "\n" << std::endl;

            // observedBatch.clear();
#endif
            // auto cacheStat = globalCache->getCacheStat();
            // SPTAGLIB_LOG(Helper::LogLevel::LL_Info, 
            //     "Cache Status - Hit:%ld, Miss: %ld, Evict: %ld, Size: %ld\n", 
            //     cacheStat.getHitCount(), cacheStat.getMissCount(), cacheStat.getEvictCount(), cacheStat.getCurrentSize());

            // globalCache->recordStatTrace();

#endif

        }
#else
        ULONGLONG GetCpuMasks(WORD group, DWORD numCpus)
        {
            ULONGLONG masks = 0, mask = 1;
            for (DWORD i = 0; i < numCpus; ++i)
            {
                masks |= mask;
                mask <<= 1;
            }

            return masks;
        }

        void SetThreadAffinity(int threadID, std::thread& thread, NumaStrategy socketStrategy, OrderStrategy idStrategy)
        {
            WORD numGroups = GetActiveProcessorGroupCount();
            DWORD numCpus = GetActiveProcessorCount(0);

            GROUP_AFFINITY ga;
            memset(&ga, 0, sizeof(ga));
            PROCESSOR_NUMBER pn;
            memset(&pn, 0, sizeof(pn));

            WORD group = (WORD)(threadID / numCpus);
            pn.Number = (BYTE)(threadID % numCpus);
            if (socketStrategy == NumaStrategy::SCATTER) {
                group = (WORD)(threadID % numGroups);
                pn.Number = (BYTE)((threadID / numGroups) % numCpus);
            }

            ga.Group = group;
            ga.Mask = GetCpuMasks(group, numCpus);
            BOOL res = SetThreadGroupAffinity(GetCurrentThread(), &ga, NULL);
            if (!res)
            {
                SPTAGLIB_LOG(Helper::LogLevel::LL_Error, "Failed SetThreadGroupAffinity for group %d and mask %I64x for thread %d.\n", ga.Group, ga.Mask, threadID);
                return;
            }
            pn.Group = group;
            if (idStrategy == OrderStrategy::DESC) {
                pn.Number = (BYTE)(numCpus - 1 - pn.Number);
            }
            res = SetThreadIdealProcessorEx(GetCurrentThread(), &pn, NULL);
            if (!res)
            {
                SPTAGLIB_LOG(Helper::LogLevel::LL_Error, "Unable to set ideal processor for thread %d.\n", threadID);
                return;
            }

            //SPTAGLIB_LOG(Helper::LogLevel::LL_Info, "numGroup:%d numCPUs:%d threadID:%d group:%d cpuid:%d\n", (int)(numGroups), (int)numCpus, threadID, (int)(group), (int)(pn.Number));
            YieldProcessor();
        }

        void BatchReadFileAsync(std::vector<std::shared_ptr<Helper::DiskIO>>& handlers, AsyncReadRequest* readRequests, int num)
        {
            if (handlers.size() == 1) {
                handlers[0]->BatchReadFile(readRequests, num);
            }
            else {
                int currFileId = 0, currReqStart = 0;
                for (int i = 0; i < num; i++) {
                    AsyncReadRequest* readRequest = &(readRequests[i]);

                    int fileid = (readRequest->m_status >> 16);
                    if (fileid != currFileId) {
                        handlers[currFileId]->BatchReadFile(readRequests + currReqStart, i - currReqStart);
                        currFileId = fileid;
                        currReqStart = i;
                    }
                }
                if (currReqStart < num) {
                    handlers[currFileId]->BatchReadFile(readRequests + currReqStart, num - currReqStart);
                }
            }
        }
#endif
    }
}

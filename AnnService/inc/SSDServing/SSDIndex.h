// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#pragma once
#include <limits>
#include "inc/Core/Common.h"
#include "inc/Core/Common/DistanceUtils.h"
#include "inc/Core/Common/QueryResultSet.h"
#include "inc/Core/SPANN/Index.h"
#include "inc/Core/SPANN/ExtraFullGraphSearcher.h"
#include "inc/Helper/VectorSetReader.h"
#include "inc/Helper/StringConvert.h"
#include "inc/SSDServing/Utils.h"

#include <functional>
#include <map>
#include <cstdio>


#define __TOPKACHE2__
#ifdef __TOPKACHE1__

#include "ResultCache.hh"

extern std::unique_ptr<topkache::ResultCache> topKacheInstance;
SPTAG::BasicVectorSet* queryVectorSet;

std::vector<size_t> hashedQuery;
size_t perVectorDataSize;

#elif defined(__TOPKACHE2__)

#include "ResultCache2.hh"

extern std::unique_ptr<topkache::ResultCache2> topKacheInstance;
SPTAG::BasicVectorSet* queryVectorSet;

std::vector<size_t> hashedQuery;
size_t perVectorDataSize;

#endif


namespace SPTAG {
	namespace SSDServing {
		namespace SSDIndex {

            template <typename ValueType>
            ErrorCode OutputResult(const std::string& p_output, std::vector<QueryResult>& p_results, int p_resultNum)
            {
                if (!p_output.empty())
                {
                    auto ptr = f_createIO();
                    if (ptr == nullptr || !ptr->Initialize(p_output.c_str(), std::ios::binary | std::ios::out)) {
                        SPTAGLIB_LOG(Helper::LogLevel::LL_Error, "Failed create file: %s\n", p_output.c_str());
                        return ErrorCode::FailedCreateFile;
                    }
                    int32_t i32Val = static_cast<int32_t>(p_results.size());
                    if (ptr->WriteBinary(sizeof(i32Val), reinterpret_cast<char*>(&i32Val)) != sizeof(i32Val)) {
                        SPTAGLIB_LOG(Helper::LogLevel::LL_Error, "Fail to write result file!\n");
                        return ErrorCode::DiskIOFail;
                    }
                    i32Val = p_resultNum;
                    if (ptr->WriteBinary(sizeof(i32Val), reinterpret_cast<char*>(&i32Val)) != sizeof(i32Val)) {
                        SPTAGLIB_LOG(Helper::LogLevel::LL_Error, "Fail to write result file!\n");
                        return ErrorCode::DiskIOFail;
                    }

                    float fVal = 0;
                    for (size_t i = 0; i < p_results.size(); ++i)
                    {
                        for (int j = 0; j < p_resultNum; ++j)
                        {
                            i32Val = p_results[i].GetResult(j)->VID;
                            if (ptr->WriteBinary(sizeof(i32Val), reinterpret_cast<char*>(&i32Val)) != sizeof(i32Val)) {
                                SPTAGLIB_LOG(Helper::LogLevel::LL_Error, "Fail to write result file!\n");
                                return ErrorCode::DiskIOFail;
                            }

                            fVal = p_results[i].GetResult(j)->Dist;
                            if (ptr->WriteBinary(sizeof(fVal), reinterpret_cast<char*>(&fVal)) != sizeof(fVal)) {
                                SPTAGLIB_LOG(Helper::LogLevel::LL_Error, "Fail to write result file!\n");
                                return ErrorCode::DiskIOFail;
                            }
                        }
                    }
                }
                return ErrorCode::Success;
            }

            template<typename T, typename V>
            void PrintPercentiles(const std::vector<V>& p_values, std::function<T(const V&)> p_get, const char* p_format)
            {
                double sum = 0;
                std::vector<T> collects;
                collects.reserve(p_values.size());
                for (const auto& v : p_values)
                {
                    T tmp = p_get(v);

                    if (tmp == 0)
                    {
                        continue;
                    }

                    sum += tmp;
                    collects.push_back(tmp);
                }

                std::sort(collects.begin(), collects.end());

                SPTAGLIB_LOG(Helper::LogLevel::LL_Info, "Avg\t50tiles\t90tiles\t95tiles\t99tiles\t99.9tiles\tMax\n");

                std::string formatStr("%.3lf");
                for (int i = 1; i < 7; ++i)
                {
                    formatStr += '\t';
                    formatStr += p_format;
                }

                formatStr += '\n';

                SPTAGLIB_LOG(Helper::LogLevel::LL_Info,
                    formatStr.c_str(),
                    sum / collects.size(),
                    collects[static_cast<size_t>(collects.size() * 0.50)],
                    collects[static_cast<size_t>(collects.size() * 0.90)],
                    collects[static_cast<size_t>(collects.size() * 0.95)],
                    collects[static_cast<size_t>(collects.size() * 0.99)],
                    collects[static_cast<size_t>(collects.size() * 0.999)],
                    collects[static_cast<size_t>(collects.size() - 1)]);
            }


            template <typename ValueType>
            void SearchSequential(SPANN::Index<ValueType>* p_index,
                int p_numThreads,
                std::vector<QueryResult>& p_results,
                std::vector<SPANN::SearchStats>& p_stats,
                int p_maxQueryCount, int p_internalResultNum)
            {
                int numQueries = min(static_cast<int>(p_results.size()), p_maxQueryCount);

                std::atomic_size_t queriesSent(0);

                std::vector<std::thread> threads;
                threads.reserve(p_numThreads);
                SPTAGLIB_LOG(Helper::LogLevel::LL_Info, "Searching: numThread: %d, numQueries: %d.\n", p_numThreads, numQueries);

#ifdef __TOPKACHE1__

                struct QueryResultTopKacheForm {
                    std::int32_t vid;
                    float dist;
                    // char vector_data[240];
                };

                // Prepare data
                size_t vector_list_size = p_internalResultNum;
                struct QueryResultTopKacheForm** vector_data_list = new struct QueryResultTopKacheForm*[vector_list_size];


                for (int i = 0; i < vector_list_size; i++)
                {
                    vector_data_list[i] = (new struct QueryResultTopKacheForm);
                    std::memset(vector_data_list[i], 0, sizeof(struct QueryResultTopKacheForm));
                }

                topkache::Vector** vectors = new topkache::Vector*[vector_list_size];
                for (int i = 0; i < p_internalResultNum; i++)
                {
                    vectors[i] = new topkache::Vector();        // New vector
                    vectors[i]->setVectorData(
                        (topkache::vector_data_t*)vector_data_list[i]);     // Set vector data
                }

                std::uint32_t hit_counts = 0;

#elif defined(__TOPKACHE2__)

                struct QueryResultTopKacheForm {
                    std::int32_t vid;
                    float dist;
                    // char vector_data[240];
                };

                // Prepare data
                size_t vector_list_size = p_internalResultNum;
                struct QueryResultTopKacheForm** vector_data_list = new struct QueryResultTopKacheForm*[vector_list_size];


                for (int i = 0; i < vector_list_size; i++)
                {
                    vector_data_list[i] = (new struct QueryResultTopKacheForm);
                    std::memset(vector_data_list[i], 0, sizeof(struct QueryResultTopKacheForm));
                }

                topkache::Vector2** vectors = new topkache::Vector2*[vector_list_size];
                for (int i = 0; i < p_internalResultNum; i++)
                {
                    vectors[i] = new topkache::Vector2();        // New vector
                    vectors[i]->setVectorData(
                        (topkache::vector_data_t*)vector_data_list[i]);     // Set vector data
                }

                std::uint32_t hit_counts = 0;

#endif
                Utils::StopW sw;

                for (int i = 0; i < p_numThreads; i++) { threads.emplace_back([&, i]()
                    {
                        NumaStrategy ns = (p_index->GetDiskIndex() != nullptr) ? NumaStrategy::SCATTER : NumaStrategy::LOCAL; // Only for SPANN, we need to avoid IO threads overlap with search threads.
                        Helper::SetThreadAffinity(i, threads[i], ns, OrderStrategy::ASC); 

                        Utils::StopW threadws;
                        size_t index = 0;
                        while (true)
                        {
                            index = queriesSent.fetch_add(1);
                            if (index < numQueries)
                            {

#pragma region TOPKACHE
                                double startTime = threadws.getElapsedMs();

// #ifndef __TOPKCACHE__
//                                 if ((index & ((1 << 14) - 1)) == 0)
//                                 {
//                                     SPTAGLIB_LOG(Helper::LogLevel::LL_Info, 
//                                         "Sent %.2lf%%... Internal result num: %ld, Current hit ratio: %.4lf\n", 
//                                             index * 100.0 / numQueries, p_internalResultNum, hit_counts * 1.0 / index);
//                                 }
// #endif
#ifdef __TOPKACHE1__

                                double cacheGetStartTime = threadws.getElapsedMs();

                                QueryResult& queryResult = p_results[index];

                                if ((index & ((1 << 14) - 1)) == 0)
                                {
                                    size_t numSearchedVectors = queryResult.m_resultNum;
                                    SPTAGLIB_LOG(Helper::LogLevel::LL_Info, 
                                        "Sent %.2lf%%... Internal result num: %ld, Current hit ratio: %.4lf\n", 
                                            index * 100.0 / numQueries, p_internalResultNum, hit_counts * 1.0 / index);
                                }

                                // 
                                // First, we need to get the result from the topkache
                                size_t hashed_query_id = hashedQuery[index];
                                topkache::result_cache_entry_t* resultCacheEntry = topKacheInstance->getResultCacheEntryStrict(hashed_query_id);

                                // bool complete_set = true;
                                if (resultCacheEntry != nullptr) 
                                {
                                    hit_counts++;
                                    for (int result_i = 0; result_i < p_internalResultNum; result_i++)
                                    {
                                        struct QueryResultTopKacheForm* vector_data 
                                            = (struct QueryResultTopKacheForm*)(resultCacheEntry->vector_slot_reference_list[result_i]->getVectorData());

                                        queryResult.SetResult(result_i, vector_data->vid, vector_data->dist);
                                    }
                                } 

                                topKacheInstance->releaseReadResultCacheEntryStrict(resultCacheEntry);

                                double cacheGetEndTime = threadws.getElapsedMs();
                                p_stats[index].m_cacheGetLatency = cacheGetEndTime - cacheGetStartTime;
                                
                                if (resultCacheEntry != nullptr)
                                {
                                    p_stats[index].m_cacheInsertLatency = 0;
                                    p_stats[index].m_exLatency = 0;

                                    p_stats[index].m_totalLatency = p_stats[index].m_totalSearchLatency = cacheGetEndTime - startTime;

                                    continue;
                                }

#elif defined(__TOPKACHE2__)

                                double cacheGetStartTime = threadws.getElapsedMs();

                                QueryResult& queryResult = p_results[index];

                                if ((index & ((1 << 14) - 1)) == 0)
                                {
                                    size_t numSearchedVectors = queryResult.m_resultNum;
                                    SPTAGLIB_LOG(Helper::LogLevel::LL_Info, 
                                        "Sent %.2lf%%... Internal result num: %ld, Current hit ratio: %.4lf\n", 
                                            index * 100.0 / numQueries, p_internalResultNum, hit_counts * 1.0 / index);
                                }


                                // 
                                // First, we need to get the result from the topkache
                                size_t hashed_query_id = hashedQuery[index];
                                topkache::result_cache_entry2_t* resultCacheEntry = topKacheInstance->getCacheEntry(hashed_query_id);

                                // bool complete_set = true;
                                if (resultCacheEntry != nullptr) 
                                {
                                    hit_counts++;
                                    for (int result_i = 0; result_i < p_internalResultNum; result_i++)
                                    {
                                        struct QueryResultTopKacheForm* vector_data 
                                            = (struct QueryResultTopKacheForm*)(resultCacheEntry->vector_slot_reference_list[result_i]->getVectorData());

                                        queryResult.SetResult(result_i, vector_data->vid, vector_data->dist);
                                    }
                                } 

                                topKacheInstance->releaseCacheEntry(resultCacheEntry);

                                double cacheGetEndTime = threadws.getElapsedMs();
                                p_stats[index].m_cacheGetLatency = cacheGetEndTime - cacheGetStartTime;
                                
                                if (resultCacheEntry != nullptr)
                                {
                                    p_stats[index].m_cacheInsertLatency = 0;
                                    p_stats[index].m_exLatency = 0;

                                    p_stats[index].m_totalLatency = p_stats[index].m_totalSearchLatency = cacheGetEndTime - startTime;

                                    continue;
                                }


#endif

                                p_index->GetMemoryIndex()->SearchIndex(p_results[index]);
                                double endTime = threadws.getElapsedMs();
                                p_index->SearchDiskIndex(p_results[index], &(p_stats[index]));
                                

#ifdef __TOPKACHE1__
                                double cachePutStartTime = threadws.getElapsedMs();
                                if (resultCacheEntry == nullptr) // Case when not found
                                {
                                    for (int result_i = 0; result_i < vector_list_size; result_i++)
                                    {
                                        vector_data_list[result_i]->vid = queryResult.m_results[result_i].VID;      // Set VID
                                        vector_data_list[result_i]->dist = queryResult.m_results[result_i].Dist;    // Set distance

                                        vectors[result_i]->setVectorId(queryResult.m_results[result_i].VID);
                                        vectors[result_i]->setVectorVersion(0);
                                    }

                                    topkache::result_cache_entry_t* new_entry = topKacheInstance->prepareResultCacheEntry(
                                        hashed_query_id, vector_list_size, vectors
                                    );

                                    topKacheInstance->insertResultCacheEntryNoQueue(hashed_query_id, new_entry);
                                }
                                double cachePutEndTime = threadws.getElapsedMs();

                                p_stats[index].m_cacheInsertLatency = cachePutEndTime - cachePutStartTime;
#elif defined(__TOPKACHE2__)
                                
                                double cachePutStartTime = threadws.getElapsedMs();
                                if (resultCacheEntry == nullptr) // Case when not found
                                {
                                    for (int result_i = 0; result_i < vector_list_size; result_i++)
                                    {
                                        vector_data_list[result_i]->vid = queryResult.m_results[result_i].VID;      // Set VID
                                        vector_data_list[result_i]->dist = queryResult.m_results[result_i].Dist;    // Set distance

                                        vectors[result_i]->setVectorId(queryResult.m_results[result_i].VID);
                                        vectors[result_i]->setVectorVersion(0);
                                    }

                                    topkache::result_cache_entry2_t* new_entry = topKacheInstance->makeCacheEntry(
                                        hashed_query_id, vector_list_size, vectors
                                    );

                                    bool insert_success = topKacheInstance->insertCacheEntry(hashed_query_id, new_entry);
                                }
                                double cachePutEndTime = threadws.getElapsedMs();

                                p_stats[index].m_cacheInsertLatency = cachePutEndTime - cachePutStartTime;

#endif

                                double exEndTime = threadws.getElapsedMs();

                                p_stats[index].m_exLatency = exEndTime - endTime;
                                p_stats[index].m_totalLatency = p_stats[index].m_totalSearchLatency = exEndTime - startTime;
                            }
                            else
                            {
                                return;
                            }
                        }
                    });
                }
                for (auto& thread : threads) { thread.join(); }

                double sendingCost = sw.getElapsedSec();

                SPTAGLIB_LOG(Helper::LogLevel::LL_Info,
                    "Finish sending in %.3lf seconds, actuallQPS is %.2lf, query count %u.\n",
                    sendingCost,
                    numQueries / sendingCost,
                    static_cast<uint32_t>(numQueries));

                for (int i = 0; i < vector_list_size; i++)
                    delete vector_data_list[i];
                delete[] vector_data_list;

                for (int i = 0; i < p_internalResultNum; i++)
                    delete vectors[i];
                delete[] vectors;

                SPTAGLIB_LOG(Helper::LogLevel::LL_Info,
                    "Hits %u/%u\n", hit_counts, numQueries);

                for (int i = 0; i < numQueries; i++) { p_results[i].CleanQuantizedTarget(); }
            }

            template <typename ValueType>
            void Search(SPANN::Index<ValueType>* p_index)
            {
                SPANN::Options& p_opts = *(p_index->GetOptions());
                std::string outputFile = p_opts.m_searchResult;
                std::string truthFile = p_opts.m_truthPath;
                std::string warmupFile = p_opts.m_warmupPath;

                if (p_index->m_pQuantizer)
                {
                   p_index->m_pQuantizer->SetEnableADC(p_opts.m_enableADC);
                }

                if (!p_opts.m_logFile.empty())
                {
                    SetLogger(std::make_shared<Helper::FileLogger>(Helper::LogLevel::LL_Info, p_opts.m_logFile.c_str()));
                }
                int numThreads = p_opts.m_iSSDNumberOfThreads;
                int internalResultNum = p_opts.m_searchInternalResultNum;
                int K = p_opts.m_resultNum;
                int truthK = (p_opts.m_truthResultNum <= 0) ? K : p_opts.m_truthResultNum;

                if (!warmupFile.empty())
                {
                    SPTAGLIB_LOG(Helper::LogLevel::LL_Info, "Start loading warmup query set...\n");
                    std::shared_ptr<Helper::ReaderOptions> queryOptions(new Helper::ReaderOptions(p_opts.m_valueType, p_opts.m_dim, p_opts.m_warmupType, p_opts.m_warmupDelimiter));
                    auto queryReader = Helper::VectorSetReader::CreateInstance(queryOptions);
                    if (ErrorCode::Success != queryReader->LoadFile(p_opts.m_warmupPath))
                    {
                        SPTAGLIB_LOG(Helper::LogLevel::LL_Error, "Failed to read query file.\n");
                        exit(1);
                    }
                    auto warmupQuerySet = queryReader->GetVectorSet();
                    int warmupNumQueries = warmupQuerySet->Count();

                    std::vector<QueryResult> warmupResults(warmupNumQueries, QueryResult(NULL, max(K, internalResultNum), false));
                    std::vector<SPANN::SearchStats> warmpUpStats(warmupNumQueries);
                    for (int i = 0; i < warmupNumQueries; ++i)
                    {
                        (*((COMMON::QueryResultSet<ValueType>*)&warmupResults[i])).SetTarget(reinterpret_cast<ValueType*>(warmupQuerySet->GetVector(i)), p_index->m_pQuantizer);
                        warmupResults[i].Reset();
                    }

                    SPTAGLIB_LOG(Helper::LogLevel::LL_Info, "Start warmup...\n");
                    SearchSequential(p_index, numThreads, warmupResults, warmpUpStats, p_opts.m_queryCountLimit, internalResultNum);
                    SPTAGLIB_LOG(Helper::LogLevel::LL_Info, "\nFinish warmup...\n");
                }

                SPTAGLIB_LOG(Helper::LogLevel::LL_Info, "Start loading QuerySet...\n");
                std::shared_ptr<Helper::ReaderOptions> queryOptions(new Helper::ReaderOptions(p_opts.m_valueType, p_opts.m_dim, p_opts.m_queryType, p_opts.m_queryDelimiter));
                auto queryReader = Helper::VectorSetReader::CreateInstance(queryOptions);
                if (ErrorCode::Success != queryReader->LoadFile(p_opts.m_queryPath))
                {
                    SPTAGLIB_LOG(Helper::LogLevel::LL_Error, "Failed to read query file.\n");
                    exit(1);
                }
                auto querySet = queryReader->GetVectorSet();
                int numQueries = querySet->Count();

                // Register for global vectorSet
                queryVectorSet = (BasicVectorSet*)querySet.get();

                std::vector<QueryResult> results(numQueries, QueryResult(NULL, max(K, internalResultNum), false));
                std::vector<SPANN::SearchStats> stats(numQueries);
                for (int i = 0; i < numQueries; ++i)
                {
                    (*((COMMON::QueryResultSet<ValueType>*)&results[i])).SetTarget(reinterpret_cast<ValueType*>(querySet->GetVector(i)), p_index->m_pQuantizer);
                    results[i].Reset();
                }

                std::int32_t dim = queryVectorSet->m_dimension;
                perVectorDataSize = queryVectorSet->m_perVectorDataSize;

                SPTAGLIB_LOG(Helper::LogLevel::LL_Info, "Vector Dimension:%d, Per-vector Size: %d\n", 
                    dim, perVectorDataSize);

                std::hash<std::string> hasher;
                std::map<size_t, size_t> hashedQueryCount;

                std::map<char*, size_t> duplicateChecker;

                // Checks
                for (int i = 0; i < numQueries; i++)
                {
                    std::string toHashQueryStr = std::string(
                        (char*)queryVectorSet->GetVector(i), perVectorDataSize);

                    size_t hashed = hasher(toHashQueryStr);
                    hashedQuery.push_back(hashed);

                    if (hashedQueryCount.find(hashed) == hashedQueryCount.end())
                    {
                        hashedQueryCount[hashed] = 1;
                    }
                    else
                    {
                        hashedQueryCount[hashed]++;
                    }
                }

                std::vector<size_t> hashedQueryCountVec;

                // Check hash
                // for (auto it = hashedQueryCount.begin(); it != hashedQueryCount.end(); it++)
                // {
                //     hashedQueryCountVec.push_back(it->second);
                // }

                // std::sort(hashedQueryCountVec.begin(), 
                //     hashedQueryCountVec.end()
                //     );        

                // std::reverse(hashedQueryCountVec.begin(), hashedQueryCountVec.end());

                // for (int i = 0; i < numQueries; i++) { 
                //     SPTAGLIB_LOG(Helper::LogLevel::LL_Info, "Num count: %d\n", 
                //         hashedQueryCountVec[i]
                //     );
                //     if (i > 100)
                //         break;
                // }
                
                // SPTAGLIB_LOG(Helper::LogLevel::LL_Info, "Press any key to start...\n");
                // getchar();


                SPTAGLIB_LOG(Helper::LogLevel::LL_Info, "Start ANN Search...\n");

                SearchSequential(p_index, numThreads, results, stats, p_opts.m_queryCountLimit, internalResultNum);

                SPTAGLIB_LOG(Helper::LogLevel::LL_Info, "\nFinish ANN Search...\n");

                std::shared_ptr<VectorSet> vectorSet;

                if (!p_opts.m_vectorPath.empty() && fileexists(p_opts.m_vectorPath.c_str())) {
                    std::shared_ptr<Helper::ReaderOptions> vectorOptions(new Helper::ReaderOptions(p_opts.m_valueType, p_opts.m_dim, p_opts.m_vectorType, p_opts.m_vectorDelimiter));
                    auto vectorReader = Helper::VectorSetReader::CreateInstance(vectorOptions);
                    if (ErrorCode::Success == vectorReader->LoadFile(p_opts.m_vectorPath))
                    {
                        vectorSet = vectorReader->GetVectorSet();
                        if (p_opts.m_distCalcMethod == DistCalcMethod::Cosine) vectorSet->Normalize(numThreads);
                        SPTAGLIB_LOG(Helper::LogLevel::LL_Info, "\nLoad VectorSet(%d,%d).\n", vectorSet->Count(), vectorSet->Dimension());
                    }
                }

                if (p_opts.m_rerank > 0 && vectorSet != nullptr) {
                    SPTAGLIB_LOG(Helper::LogLevel::LL_Info, "\n Begin rerank...\n");
                    for (int i = 0; i < results.size(); i++)
                    {
                        for (int j = 0; j < K; j++)
                        {
                            if (results[i].GetResult(j)->VID < 0) continue;
                            results[i].GetResult(j)->Dist = COMMON::DistanceUtils::ComputeDistance((const ValueType*)querySet->GetVector(i),
                                (const ValueType*)vectorSet->GetVector(results[i].GetResult(j)->VID), querySet->Dimension(), p_opts.m_distCalcMethod);
                        }
                        BasicResult* re = results[i].GetResults();
                        std::sort(re, re + K, COMMON::Compare);
                    }
                    K = p_opts.m_rerank;
                }

                float recall = 0, MRR = 0;
                std::vector<std::set<SizeType>> truth;
                if (!truthFile.empty())
                {
                    SPTAGLIB_LOG(Helper::LogLevel::LL_Info, "Start loading TruthFile...\n");

                    auto ptr = f_createIO();
                    if (ptr == nullptr || !ptr->Initialize(truthFile.c_str(), std::ios::in | std::ios::binary)) {
                        SPTAGLIB_LOG(Helper::LogLevel::LL_Error, "Failed open truth file: %s\n", truthFile.c_str());
                        exit(1);
                    }
                    int originalK = truthK;
                    COMMON::TruthSet::LoadTruth(ptr, truth, numQueries, originalK, truthK, p_opts.m_truthType);
                    char tmp[4];
                    if (ptr->ReadBinary(4, tmp) == 4) {
                        SPTAGLIB_LOG(Helper::LogLevel::LL_Error, "Truth number is larger than query number(%d)!\n", numQueries);
                    }

                    recall = COMMON::TruthSet::CalculateRecall<ValueType>((p_index->GetMemoryIndex()).get(), results, truth, K, truthK, querySet, vectorSet, numQueries, nullptr, false, &MRR);
                    SPTAGLIB_LOG(Helper::LogLevel::LL_Info, "Recall%d@%d: %f MRR@%d: %f\n", truthK, K, recall, K, MRR);
                }

                SPTAGLIB_LOG(Helper::LogLevel::LL_Info, "\nEx Elements Count:\n");
                PrintPercentiles<double, SPANN::SearchStats>(stats,
                    [](const SPANN::SearchStats& ss) -> double
                    {
                        return ss.m_totalListElementsCount;
                    },
                    "%.3lf");

                SPTAGLIB_LOG(Helper::LogLevel::LL_Info, "\nCache Get Latency Distribution:\n");
                PrintPercentiles<double, SPANN::SearchStats>(stats,
                    [](const SPANN::SearchStats& ss) -> double
                    {
                        return ss.m_cacheGetLatency;
                    },
                    "%.3lf");

                SPTAGLIB_LOG(Helper::LogLevel::LL_Info, "\nHead Latency Distribution:\n");
                PrintPercentiles<double, SPANN::SearchStats>(stats,
                    [](const SPANN::SearchStats& ss) -> double
                    {
                        return ss.m_totalSearchLatency - ss.m_exLatency;
                    },
                    "%.3lf");

                SPTAGLIB_LOG(Helper::LogLevel::LL_Info, "\nEx Latency Distribution:\n");
                PrintPercentiles<double, SPANN::SearchStats>(stats,
                    [](const SPANN::SearchStats& ss) -> double
                    {
                        return ss.m_exLatency;
                    },
                    "%.3lf");

                SPTAGLIB_LOG(Helper::LogLevel::LL_Info, "\nCache Put Latency Distribution:\n");
                PrintPercentiles<double, SPANN::SearchStats>(stats,
                    [](const SPANN::SearchStats& ss) -> double
                    {
                        return ss.m_cacheInsertLatency;
                    },
                    "%.3lf");

                SPTAGLIB_LOG(Helper::LogLevel::LL_Info, "\nTotal Latency Distribution:\n");
                PrintPercentiles<double, SPANN::SearchStats>(stats,
                    [](const SPANN::SearchStats& ss) -> double
                    {
                        return ss.m_totalSearchLatency;
                    },
                    "%.3lf");

                SPTAGLIB_LOG(Helper::LogLevel::LL_Info, "\nTotal Disk Page Access Distribution:\n");
                PrintPercentiles<int, SPANN::SearchStats>(stats,
                    [](const SPANN::SearchStats& ss) -> int
                    {
                        return ss.m_diskAccessCount;
                    },
                    "%4d");

                SPTAGLIB_LOG(Helper::LogLevel::LL_Info, "\nTotal Disk IO Distribution:\n");
                PrintPercentiles<int, SPANN::SearchStats>(stats,
                    [](const SPANN::SearchStats& ss) -> int
                    {
                        return ss.m_diskIOCount;
                    },
                    "%4d");

                SPTAGLIB_LOG(Helper::LogLevel::LL_Info, "\n");

                if (!outputFile.empty())
                {
                    SPTAGLIB_LOG(Helper::LogLevel::LL_Info, "Start output to %s\n", outputFile.c_str());
                    OutputResult<ValueType>(outputFile, results, K);
                }

                SPTAGLIB_LOG(Helper::LogLevel::LL_Info,
                    "Recall@%d: %f MRR@%d: %f\n", K, recall, K, MRR);

                SPTAGLIB_LOG(Helper::LogLevel::LL_Info, "\n");

                if (p_opts.m_recall_analysis) {
                    SPTAGLIB_LOG(Helper::LogLevel::LL_Info, "Start recall analysis...\n");

                    std::shared_ptr<VectorIndex> headIndex = p_index->GetMemoryIndex();
                    SizeType sampleSize = numQueries < 100 ? numQueries : 100;
                    SizeType sampleK = headIndex->GetNumSamples() < 1000 ? headIndex->GetNumSamples() : 1000;
                    float sampleE = 1e-6f;

                    std::vector<SizeType> samples(sampleSize, 0);
                    std::vector<float> queryHeadRecalls(sampleSize, 0);
                    std::vector<float> truthRecalls(sampleSize, 0);
                    std::vector<int> shouldSelect(sampleSize, 0);
                    std::vector<int> shouldSelectLong(sampleSize, 0);
                    std::vector<int> nearQueryHeads(sampleSize, 0);
                    std::vector<int> annNotFound(sampleSize, 0);
                    std::vector<int> rngRule(sampleSize, 0);
                    std::vector<int> postingCut(sampleSize, 0);
                    for (int i = 0; i < sampleSize; i++) samples[i] = COMMON::Utils::rand(numQueries);

#pragma omp parallel for schedule(dynamic)
                    for (int i = 0; i < sampleSize; i++)
                    {
                        COMMON::QueryResultSet<ValueType> queryANNHeads((const ValueType*)(querySet->GetVector(samples[i])), max(K, internalResultNum));
                        headIndex->SearchIndex(queryANNHeads);
                        float queryANNHeadsLongestDist = queryANNHeads.GetResult(internalResultNum - 1)->Dist;

                        COMMON::QueryResultSet<ValueType> queryBFHeads((const ValueType*)(querySet->GetVector(samples[i])), max(sampleK, internalResultNum));
                        for (SizeType y = 0; y < headIndex->GetNumSamples(); y++)
                        {
                            float dist = headIndex->ComputeDistance(queryBFHeads.GetQuantizedTarget(), headIndex->GetSample(y));
                            queryBFHeads.AddPoint(y, dist);
                        }
                        queryBFHeads.SortResult();

                        {
                            std::vector<bool> visited(internalResultNum, false);
                            for (SizeType y = 0; y < internalResultNum; y++)
                            {
                                for (SizeType z = 0; z < internalResultNum; z++)
                                {
                                    if (visited[z]) continue;

                                    if (fabs(queryANNHeads.GetResult(z)->Dist - queryBFHeads.GetResult(y)->Dist) < sampleE)
                                    {
                                        queryHeadRecalls[i] += 1;
                                        visited[z] = true;
                                        break;
                                    }
                                }
                            }
                        }

                        std::map<int, std::set<int>> tmpFound; // headID->truths
                        p_index->DebugSearchDiskIndex(queryBFHeads, internalResultNum, sampleK, nullptr, &truth[samples[i]], &tmpFound);

                        for (SizeType z = 0; z < K; z++) {
                            truthRecalls[i] += truth[samples[i]].count(queryBFHeads.GetResult(z)->VID);
                        }

                        for (SizeType z = 0; z < K; z++) {
                            truth[samples[i]].erase(results[samples[i]].GetResult(z)->VID);
                        }

                        for (std::map<int, std::set<int>>::iterator it = tmpFound.begin(); it != tmpFound.end(); it++) {
                            float q2truthposting = headIndex->ComputeDistance(querySet->GetVector(samples[i]), headIndex->GetSample(it->first));
                            for (auto vid : it->second) {
                                if (!truth[samples[i]].count(vid)) continue;

                                if (q2truthposting < queryANNHeadsLongestDist) shouldSelect[i] += 1;
                                else {
                                    shouldSelectLong[i] += 1;

                                    std::set<int> nearQuerySelectedHeads;
                                    float v2vhead = headIndex->ComputeDistance(vectorSet->GetVector(vid), headIndex->GetSample(it->first));
                                    for (SizeType z = 0; z < internalResultNum; z++) {
                                        if (queryANNHeads.GetResult(z)->VID < 0) break;
                                        float v2qhead = headIndex->ComputeDistance(vectorSet->GetVector(vid), headIndex->GetSample(queryANNHeads.GetResult(z)->VID));
                                        if (v2qhead < v2vhead) {
                                            nearQuerySelectedHeads.insert(queryANNHeads.GetResult(z)->VID);
                                        }
                                    }
                                    if (nearQuerySelectedHeads.size() == 0) continue;

                                    nearQueryHeads[i] += 1;

                                    COMMON::QueryResultSet<ValueType> annTruthHead((const ValueType*)(vectorSet->GetVector(vid)), p_opts.m_debugBuildInternalResultNum);
                                    headIndex->SearchIndex(annTruthHead);

                                    bool found = false;
                                    for (SizeType z = 0; z < annTruthHead.GetResultNum(); z++) {
                                        if (nearQuerySelectedHeads.count(annTruthHead.GetResult(z)->VID)) {
                                            found = true;
                                            break;
                                        }
                                    }

                                    if (!found) {
                                        annNotFound[i] += 1;
                                        continue;
                                    }

                                    // RNG rule and posting cut
                                    std::set<int> replicas;
                                    for (SizeType z = 0; z < annTruthHead.GetResultNum() && replicas.size() < p_opts.m_replicaCount; z++) {
                                        BasicResult* item = annTruthHead.GetResult(z);
                                        if (item->VID < 0) break;

                                        bool good = true;
                                        for (auto r : replicas) {
                                            if (p_opts.m_rngFactor * headIndex->ComputeDistance(headIndex->GetSample(r), headIndex->GetSample(item->VID)) < item->Dist) {
                                                good = false;
                                                break;
                                            }
                                        }
                                        if (good) replicas.insert(item->VID);
                                    }

                                    found = false;
                                    for (auto r : nearQuerySelectedHeads) {
                                        if (replicas.count(r)) {
                                            found = true;
                                            break;
                                        }
                                    }

                                    if (found) postingCut[i] += 1;
                                    else rngRule[i] += 1;
                                }
                            }
                        }
                    }
                    float headacc = 0, truthacc = 0, shorter = 0, longer = 0, lost = 0, buildNearQueryHeads = 0, buildAnnNotFound = 0, buildRNGRule = 0, buildPostingCut = 0;
                    for (int i = 0; i < sampleSize; i++) {
                        headacc += queryHeadRecalls[i];
                        truthacc += truthRecalls[i];

                        lost += shouldSelect[i] + shouldSelectLong[i];
                        shorter += shouldSelect[i];
                        longer += shouldSelectLong[i];

                        buildNearQueryHeads += nearQueryHeads[i];
                        buildAnnNotFound += annNotFound[i];
                        buildRNGRule += rngRule[i];
                        buildPostingCut += postingCut[i];
                    }

                    SPTAGLIB_LOG(Helper::LogLevel::LL_Info, "Query head recall @%d:%f.\n", internalResultNum, headacc / sampleSize / internalResultNum);
                    SPTAGLIB_LOG(Helper::LogLevel::LL_Info, "BF top %d postings truth recall @%d:%f.\n", sampleK, truthK, truthacc / sampleSize / truthK);

                    SPTAGLIB_LOG(Helper::LogLevel::LL_Info,
                        "Percent of truths in postings have shorter distance than query selected heads: %f percent\n",
                        shorter / lost * 100);
                    SPTAGLIB_LOG(Helper::LogLevel::LL_Info,
                        "Percent of truths in postings have longer distance than query selected heads: %f percent\n",
                        longer / lost * 100);


                    SPTAGLIB_LOG(Helper::LogLevel::LL_Info,
                        "\tPercent of truths no shorter distance in query selected heads: %f percent\n",
                        (longer - buildNearQueryHeads) / lost * 100);
                    SPTAGLIB_LOG(Helper::LogLevel::LL_Info,
                        "\tPercent of truths exists shorter distance in query selected heads: %f percent\n",
                        buildNearQueryHeads / lost * 100);

                    SPTAGLIB_LOG(Helper::LogLevel::LL_Info,
                        "\t\tRNG rule ANN search loss: %f percent\n", buildAnnNotFound / lost * 100);
                    SPTAGLIB_LOG(Helper::LogLevel::LL_Info,
                        "\t\tPosting cut loss: %f percent\n", buildPostingCut / lost * 100);
                    SPTAGLIB_LOG(Helper::LogLevel::LL_Info,
                        "\t\tRNG rule loss: %f percent\n", buildRNGRule / lost * 100);
                }
            }
		}
	}
}

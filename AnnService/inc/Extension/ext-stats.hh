/*
 * ext-cache.hh
 * Author: Sukjoon Oh (sjoon@kaist.ac.kr)
 */

#ifndef EXT_STATS_H
#define EXT_STATS_H

#include <memory>
#include <vector>
#include <map>

#include <fstream>

namespace extension
{
    namespace stats
    {

        enum class AccessLocation : std::uint8_t
        {
            ACCESS_LOCATION_MEMORY          = 0,
            ACCESS_LOCATION_DISK            = 1
        };

        class ReadBatchStats
        {
        protected:
            // 
            // AccessStat structure
            struct AccessStat
            {
                std::uint64_t               m_address;
                std::uint64_t               m_size;
                AccessLocation              m_location;
            };

            std::vector<std::vector<struct AccessStat>> m_readBatchList;
            std::vector<std::vector<struct AccessStat>> m_readBatchListPage;

        public:
            ReadBatchStats() = default;
            virtual ~ReadBatchStats() = default;

            void makeNewReadBatch() noexcept
            {
                m_readBatchList.emplace_back();
                m_readBatchListPage.emplace_back();
            }

            // void recordReadBatch(std::uint64_t p_address) noexcept
            // {
            //     m_readBatchList.back().emplace_back(
            //         AccessStat{p_address, 0, AccessLocation::ACCESS_LOCATION_DISK}
            //     );
            // }

            void recordReadBatch(std::uint64_t p_address, std::uint64_t p_size = 0) noexcept
            {
                m_readBatchList.back().emplace_back(
                    AccessStat{p_address, p_size, AccessLocation::ACCESS_LOCATION_DISK}
                );
            }

            void recordReadBatchPage(std::uint64_t p_address, std::uint64_t p_size = 0) noexcept
            {
                m_readBatchListPage.back().emplace_back(
                    AccessStat{p_address, p_size, AccessLocation::ACCESS_LOCATION_MEMORY}
                );
            }

            void updateAccessLocation(std::vector<AccessLocation>& p_locationList) noexcept
            {
                if (p_locationList.size() != m_readBatchList.back().size())
                    return;

                for (int i = 0; i < p_locationList.size(); i++)
                {
                    m_readBatchList.back()[i].m_location = p_locationList[i];
                    m_readBatchListPage.back()[i].m_location = p_locationList[i];
                }
            }

            void clearReadBatch() noexcept
            {
                m_readBatchList.clear();
                m_readBatchListPage.clear();
            }

            void dumpAccessDistribution() noexcept
            {
                // Flattened list
                std::vector<std::uint64_t> flattenedList;

                for (int i = 0; i < m_readBatchList.size(); i++)
                    for (int j = 0; j < m_readBatchList[i].size(); j++)
                        flattenedList.emplace_back(
                                m_readBatchList[i][j].m_address
                            );

                // Count the duplicates
                std::map<std::uint64_t, std::uint64_t, std::greater<std::uint64_t>> accessCount;

                for (int i = 0; i < flattenedList.size(); i++)
                {
                    if (accessCount.find(flattenedList[i]) == accessCount.end())
                        accessCount[flattenedList[i]] = 1;

                    else
                        accessCount[flattenedList[i]]++;
                }

                // Export the distribution
                //  First column: Address, Second column: Count
                std::fstream exportFile("access-distribution.csv", std::ios::out);

                if (!exportFile.is_open())
                    return;

                else
                {
                    for (auto& [address, count] : accessCount)
                        exportFile << address << "\t" << count << std::endl;

                    exportFile.close();
                }
            }


            void dumpAccessList(const char* p_filename = "") noexcept
            {
                std::fstream exportFile("access-history.csv", std::ios::out);
                std::fstream exportFilePage("access-history-page.csv", std::ios::out);

                if (!exportFile.is_open())
                    return;

                if (!exportFilePage.is_open())
                    return;

                {
                    for (int i = 0; i < m_readBatchList.size(); i++)
                    {
                        for (int j = 0; j < m_readBatchList[i].size(); j++)
                            exportFile  << m_readBatchList[i][j].m_address << "\t" 
                                        << m_readBatchList[i][j].m_size;
                        
                        exportFile << std::endl;
                    }

                    exportFile.close();

                    for (int i = 0; i < m_readBatchListPage.size(); i++)
                    {
                        for (int j = 0; j < m_readBatchListPage[i].size(); j++)
                            exportFilePage  << m_readBatchListPage[i][j].m_address << "\t" 
                                            << m_readBatchListPage[i][j].m_size;
                        
                        exportFilePage << std::endl;
                    }

                    exportFilePage.close();
                }

                // Second phase, flattened list
                std::fstream exportFileFlattened1("access-history-flattened-1.csv", std::ios::out);
                std::fstream exportFilePageFlattened1("access-history-page-flattened-1.csv", std::ios::out);

                if (!exportFileFlattened1.is_open())
                    return;

                if (!exportFilePageFlattened1.is_open())
                    return;

                for (int i = 0; i < m_readBatchList.size(); i++)
                {
                    for (int j = 0; j < m_readBatchList[i].size(); j++)
                        exportFileFlattened1    << i                                    << "\t"
                                                << m_readBatchList[i][j].m_address      << "\t" 
                                                << m_readBatchList[i][j].m_size         << "\n";
                }

                exportFileFlattened1.close();

                for (int i = 0; i < m_readBatchListPage.size(); i++)
                {
                    for (int j = 0; j < m_readBatchListPage[i].size(); j++)
                        exportFilePageFlattened1    << i                                        << "\t"
                                                    << m_readBatchListPage[i][j].m_address      << "\t" 
                                                    << m_readBatchListPage[i][j].m_size         << "\n";
                }

                exportFilePageFlattened1.close();
            }


            void dumpAccessLocation(const char* p_filename = "access-location.csv") noexcept
            {
                std::fstream exportFile(p_filename, std::ios::out);

                if (!exportFile.is_open())
                    return;

                else
                {
                    for (int i = 0; i < m_readBatchList.size(); i++)
                    {
                        for (int j = 0; j < m_readBatchList[i].size(); j++)
                            exportFile << static_cast<int>(m_readBatchList[i][j].m_location) << "\t";
                        
                        exportFile << std::endl;
                    }

                    exportFile.close();
                }
            }


            void dumpAccessHitRatio(const char* p_filename = "access-perq-hit-ratio.csv") noexcept
            {
                std::fstream exportFile(p_filename, std::ios::out);

                if (!exportFile.is_open())
                    return;

                else
                {
                    std::uint64_t hitCount = 0;
                    std::uint64_t missCount = 0;

                    for (int i = 0; i < m_readBatchList.size(); i++)
                    {
                        hitCount = 0;
                        missCount = 0;

                        for (int j = 0; j < m_readBatchList[i].size(); j++)
                        {
                            if (m_readBatchList[i][j].m_location == AccessLocation::ACCESS_LOCATION_MEMORY)
                                hitCount++;
                            else
                                missCount++;
                        }

                        exportFile  << hitCount << "\t"
                                    << missCount << "\t"
                                    << hitCount * 1.0 / (hitCount + missCount) << std::endl;
                    }                

                    exportFile.close();
                }
            }
        };

        void initStats() noexcept;
        void resetStats() noexcept;
        void exportStats() noexcept;

        // 
        // Get handles.
        ReadBatchStats* getReadBatchStatsHandle() noexcept;
    }
}

#endif // EXT_CACHE_H

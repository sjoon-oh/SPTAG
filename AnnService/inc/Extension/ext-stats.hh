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
                AccessLocation              m_location;
            };

            std::vector<std::vector<struct AccessStat>> m_readBatchList;

        public:
            ReadBatchStats() = default;
            virtual ~ReadBatchStats() = default;

            void makeNewReadBatch() noexcept
            {
                m_readBatchList.emplace_back();
            }

            void recordReadBatch(std::uint64_t p_address) noexcept
            {
                m_readBatchList.back().emplace_back(
                    AccessStat{p_address, AccessLocation::ACCESS_LOCATION_DISK}
                );
            }

            void updateAccessLocation(std::vector<AccessLocation>& p_locationList) noexcept
            {
                if (p_locationList.size() != m_readBatchList.back().size())
                    return;

                for (int i = 0; i < p_locationList.size(); i++)
                    m_readBatchList.back()[i].m_location = p_locationList[i];
            }

            void clearReadBatch() noexcept
            {
                m_readBatchList.clear();
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


            void dumpAccessList(const char* p_filename = "access-history.csv") noexcept
            {
                std::fstream exportFile(p_filename, std::ios::out);

                if (!exportFile.is_open())
                    return;

                else
                {
                    for (int i = 0; i < m_readBatchList.size(); i++)
                    {
                        for (int j = 0; j < m_readBatchList[i].size(); j++)
                            exportFile << m_readBatchList[i][j].m_address << "\t";
                        
                        exportFile << std::endl;
                    }

                    exportFile.close();
                }
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

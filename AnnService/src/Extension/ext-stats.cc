/*
 * ext-stats.cc
 * Author: Sukjoon Oh (sjoon@kaist.ac.kr)
 */

#include "inc/Extension/ext-stats.hh"

namespace extension
{
    namespace stats
    {
        std::unique_ptr<ReadBatchStats> g_accessStats;
            // Get handles
        
        ReadBatchStats* getReadBatchStatsHandle() noexcept
        {
            return g_accessStats.get();
        }
    }
}

void extension::stats::initStats() noexcept
{
    g_accessStats.reset(new ReadBatchStats());
}

void extension::stats::resetStats() noexcept
{
    g_accessStats->clearReadBatch();
    initStats();
}

void extension::stats::exportStats() noexcept
{
    g_accessStats->dumpAccessList();
    g_accessStats->dumpAccessDistribution();
    g_accessStats->dumpAccessLocation();

}


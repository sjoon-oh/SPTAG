/*
 * ext-timer.cc
 * Author: Sukjoon Oh (sjoon@kaist.ac.kr)
 */

#include <vector>
#include <cassert>

#include <fstream>

#include "inc/Extension/ext-timer.hh"
#include "utils/Logger.hh"

namespace extension
{
    std::unique_ptr<
        std::unordered_map<std::string, pduck::utils::TimestampList>> g_timerMap;
}

pduck::utils::TimestampList*
extension::getTimerHandle(std::string p_timerName) noexcept
{
    if (g_timerMap->find(p_timerName) == g_timerMap->end())
        return nullptr;

    else 
        return &(*g_timerMap)[p_timerName];
}

void
extension::initTimers() noexcept
{
    g_timerMap.reset(
        new std::unordered_map<std::string, pduck::utils::TimestampList>());

    assert(g_timerMap != nullptr);
    std::vector<std::string> timerKeys = {
        "cache-get",
        "cache-delay"
    };

    for (const auto& key: timerKeys)
    {
        g_timerMap->insert(
            std::make_pair(key, pduck::utils::TimestampList()));
    }
}

void
extension::exportTimersElapsed(std::string p_timerName) noexcept
{
    //
    pduck::utils::TimestampList* timer = getTimerHandle(p_timerName);
    timer->dumpElapsedTimes((p_timerName + "-elapsed.csv").c_str());
}

void
extension::exportTimersStats(std::string p_timerName) noexcept
{

    // This calculates average, 1%ile, 50%ile, 99%ile, 99.9%ile.
    pduck::utils::TimestampList* timer = getTimerHandle(p_timerName);

    std::vector<double> elapsedTimes = timer->getElapsedTimes();

    // Calculate the average.
    double tsSum = 0.0;
    for (const auto& time: elapsedTimes)
        tsSum += time;

    double average = tsSum / elapsedTimes.size();

    // Calculate the 1%ile.
    std::sort(elapsedTimes.begin(), elapsedTimes.end());

    int idx1    = elapsedTimes.size() * 0.01;
    int idx50   = elapsedTimes.size() * 0.5;
    int idx99   = elapsedTimes.size() * 0.99;
    int idx999  = elapsedTimes.size() * 0.999;

    double p1   = elapsedTimes[idx1];
    double p50  = elapsedTimes[idx50];
    double p99  = elapsedTimes[idx99];
    double p999 = elapsedTimes[idx999];

    std::fstream exportFile((p_timerName + "-stats.csv").c_str(), std::ios::out);
    if (!exportFile.is_open())
        return;
    
    else
    {
        exportFile  <<   average    << "\t"
                    <<   p1         << "\t" 
                    <<   p50        << "\t" 
                    <<   p99        << "\t" 
                    <<   p999       << std::endl;
        
        exportFile.close();
    }
}
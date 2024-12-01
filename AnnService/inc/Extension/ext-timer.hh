/*
 * ext-timer.hh
 * Author: Sukjoon Oh (sjoon@kaist.ac.kr)
 */

#ifndef EXT_TIMER_H
#define EXT_TIMER_H

#include <string>

#include <memory>
#include <unordered_map>

#include "utils/Timer.hh"

namespace extension
{   
    using TimerMapType = std::unordered_map<std::string, pduck::utils::TimestampList>;

    pduck::utils::TimestampList* getTimerHandle(std::string p_timerName) noexcept;

    void initTimers() noexcept;
    
    void exportTimersElapsed(std::string p_timerName) noexcept;
    void exportTimersStats(std::string p_timerName) noexcept;
}



#endif

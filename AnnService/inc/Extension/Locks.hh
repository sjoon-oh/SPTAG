#pragma once
// Author: Sukjoon Oh (sjoon@kaist.ac.kr)
// 

#ifndef _SPTAG_EXTLOCKS_H
#define _SPTAG_EXTLOCKS_H

#include <atomic>
#include <condition_variable>
#include <mutex>

#include <chrono>
#include <array>

#define EXTLOCK_STAT_MAX    100000000

namespace SPTAG {
    namespace EXT {

        class Spinlock 
        {
        protected:
            std::atomic_flag m_lock = ATOMIC_FLAG_INIT;

        public:
            Spinlock() noexcept = default;
            virtual ~Spinlock() noexcept = default;


            inline void getLock() noexcept 
            {
                while (m_lock.test_and_set(std::memory_order_acquire))
                    ;
            };
            

            inline void releaseLock() noexcept
            {
                m_lock.clear(std::memory_order_release);
            };

            Spinlock(const Spinlock&) = delete;
            Spinlock& operator = (const Spinlock&) = delete;
        };


        struct LockStat
        {
            std::chrono::steady_clock::time_point m_requestTp;
            std::chrono::steady_clock::time_point m_getTp;
            std::chrono::steady_clock::time_point m_releaseTp;   
        };


        // 
        class SpinlockWithStat : public Spinlock
        {
        protected:
            std::atomic<size_t> m_statIndex;
            std::array<struct LockStat, EXTLOCK_STAT_MAX> m_stats;

        #define getElapsedUsExt(start, end) \
            (std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count() * 1.0 / 1000.0)


        public:
            SpinlockWithStat() noexcept
            {
                m_statIndex.store(0);
            };
            virtual ~SpinlockWithStat() noexcept = default;


            SpinlockWithStat(const Spinlock&) = delete;
            SpinlockWithStat& operator = (const Spinlock&) = delete;


            inline size_t getLock() noexcept 
            {
                size_t handle = m_statIndex.fetch_add(1);
                if (handle < EXTLOCK_STAT_MAX)
                    m_stats[handle].m_requestTp = std::chrono::steady_clock::now();
                
                Spinlock::getLock();

                if (handle < EXTLOCK_STAT_MAX)
                    m_stats[handle].m_getTp = std::chrono::steady_clock::now();
                
                return handle;
            };


            inline void releaseLock(const size_t p_handle) noexcept
            {
                Spinlock::releaseLock();

                if (p_handle < EXTLOCK_STAT_MAX)
                    m_stats[p_handle].m_releaseTp= std::chrono::steady_clock::now();
            }


            inline std::array<struct LockStat, EXTLOCK_STAT_MAX>& getStats() noexcept
            {
                return m_stats;
            }


            inline size_t getNextIndex() noexcept
            {
                return m_statIndex.load();
            }
        };

    }
}


#endif
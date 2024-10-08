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
            std::atomic<size_t> m_searchStatIndex;
            std::atomic<size_t> m_refreshStatIndex;

            std::array<struct LockStat, EXTLOCK_STAT_MAX> m_searchStat;
            std::array<struct LockStat, EXTLOCK_STAT_MAX> m_refreshStat;

        #define getElapsedUsExt(start, end) \
            (std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count() * 1.0 / 1000.0)


        public:
            SpinlockWithStat() noexcept
            {
                m_searchStatIndex.store(0);
                m_refreshStatIndex.store(0);
            };
            virtual ~SpinlockWithStat() noexcept = default;


            SpinlockWithStat(const Spinlock&) = delete;
            SpinlockWithStat& operator = (const Spinlock&) = delete;


            inline size_t getSearchLock() noexcept
            {
                size_t handle = m_searchStatIndex.fetch_add(1);
                if (handle < EXTLOCK_STAT_MAX)
                    m_searchStat[handle].m_requestTp = std::chrono::steady_clock::now();
                
                Spinlock::getLock();

                if (handle < EXTLOCK_STAT_MAX)
                    m_searchStat[handle].m_getTp = std::chrono::steady_clock::now();
                
                return handle;
            };


            inline size_t getRefreshLock() noexcept
            {
                size_t handle = m_refreshStatIndex.fetch_add(1);
                if (handle < EXTLOCK_STAT_MAX)
                    m_refreshStat[handle].m_requestTp = std::chrono::steady_clock::now();
                
                Spinlock::getLock();

                if (handle < EXTLOCK_STAT_MAX)
                    m_refreshStat[handle].m_getTp = std::chrono::steady_clock::now();
                
                return handle;
            };


            inline void releaseSearchLock(const size_t p_handle) noexcept
            {
                Spinlock::releaseLock();

                if (p_handle < EXTLOCK_STAT_MAX)
                    m_searchStat[p_handle].m_releaseTp= std::chrono::steady_clock::now();
            }


            inline void releaseRefreshLock(const size_t p_handle) noexcept
            {
                Spinlock::releaseLock();

                if (p_handle < EXTLOCK_STAT_MAX)
                    m_refreshStat[p_handle].m_releaseTp= std::chrono::steady_clock::now();
            }


            inline std::array<struct LockStat, EXTLOCK_STAT_MAX>& getSearchStat() noexcept
            {
                return m_searchStat;
            }


            inline std::array<struct LockStat, EXTLOCK_STAT_MAX>& getRefreshStat() noexcept
            {
                return m_refreshStat;
            }


            inline size_t getSearchNextIndex() noexcept
            {
                return m_searchStatIndex.load();
            }


            inline size_t getRefreshNextIndex() noexcept
            {
                return m_refreshStatIndex.load();
            }
        };

    }
}


#endif
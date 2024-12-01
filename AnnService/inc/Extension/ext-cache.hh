/*
 * ext-cache.hh
 * Author: Sukjoon Oh (sjoon@kaist.ac.kr)
 */

#ifndef EXT_CACHE_H
#define EXT_CACHE_H

#include <memory>

#include "cache/CacheBase.hh"
#include "cache/CacheFifo.hh"
#include "cache/CacheLru.hh" 
#include "cache/CacheLfu.hh"

namespace extension
{
    //
    // Wrapper for cache handles.
    //  
    void initCache(const std::string& p_cacheType, const size_t p_cacheSize) noexcept;
    void resetCache(const std::string& p_cacheType, const size_t p_cacheSize) noexcept;

    bool isCacheInit() noexcept;

    pduck::cache::IDelayableCache* getCacheHandle() noexcept;

    // 
    // template namespace holds what other sources need to be aware of.
    namespace templates
    {
        struct ListInfo
        {
            std::size_t         listTotalBytes = 0;
            int                 listEleCount = 0;
            std::uint16_t       listPageCount = 0;
            std::uint64_t       listOffset = 0;
            std::uint16_t       pageOffset = 0;
        };
    }
}

#endif // EXT_CACHE_H

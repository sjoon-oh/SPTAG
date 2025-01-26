/*
 * ext-cache.cc
 * Author: Sukjoon Oh (sjoon@kaist.ac.kr)
 */

#include <memory>
#include <utility>
#include <string>

#include <cassert>

#include "inc/Extension/ext-cache.hh"



namespace extension
{
    std::unique_ptr<pduck::cache::IDelayableCache> g_cacheHandle;
}

void
extension::initCache(const std::string& p_cacheType, const size_t p_cacheSize) noexcept
{
    if (p_cacheType == "FIFO")
    {
        g_cacheHandle.reset(new pduck::cache::weak::FifoCacheFixedBuffer(p_cacheSize));
    }
    else if (p_cacheType == "LRU")
    {
        g_cacheHandle.reset(new pduck::cache::weak::LruCacheFixedBuffer(p_cacheSize));
    }
    else if (p_cacheType == "LFU")
    {
        g_cacheHandle.reset(new pduck::cache::weak::LfuCacheFixedBuffer(p_cacheSize));
    }
    else
    {
        assert(0);
    }
}

void
extension::resetCache(const std::string& p_cacheType, const size_t p_cacheSize) noexcept
{
    g_cacheHandle.reset();
    initCache(p_cacheType, p_cacheSize);
}


bool
extension::isCacheInit() noexcept
{
    return g_cacheHandle != nullptr;
}


pduck::cache::IDelayableCache*
extension::getCacheHandle() noexcept
{
    return g_cacheHandle.get();
}
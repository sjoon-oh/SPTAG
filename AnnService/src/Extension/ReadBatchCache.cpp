

#include <cstdarg>

#include "inc/Extension/ReadBatchCache.hh"

void 
SPTAG::EXT::debug_print(const char *format, ...)
{
    va_list arglist;

    va_start(arglist, format);
    vprintf(format, arglist);
    va_end(arglist);
}


void 
SPTAG::EXT::debug_print_off(const char *format, ...)
{
    return;
}
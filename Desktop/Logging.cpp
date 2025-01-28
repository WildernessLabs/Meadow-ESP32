#include <stdio.h>
#include <stdarg.h>

static char buffer[256];

extern "C" void trace_message(const char *format, ...)
{
    va_list args;
    va_start(args, format);
    sprintf(buffer, "%s\n", format);
    vprintf(buffer, args);
    va_end(args);
}

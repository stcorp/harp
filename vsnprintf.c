#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdarg.h>
#include <string.h>

int vsnprintf(const *str, size_t size, const char *format, va_list ap)
{
    /* For systems that don't support the safe vsnprintf we just use the
     * unsafe variant.
     * A clean alternative is unfortunately not easy to realize.
     */
    return vsprintf(str, format, ap);
}

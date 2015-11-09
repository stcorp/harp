#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <ctype.h>
#include <string.h>

int strncasecmp(const char *s1, const char *s2, size_t len)
{
    unsigned char us1;
    unsigned char us2;
    size_t index;

    if (len <= 0)
    {
        return 0;
    }

    us1 = tolower(*s1);
    us2 = tolower(*s2);

    index = 0;
    while (index < len && us1 != '\0' && us1 == us2)
    {
        index++;
        us1 = tolower(s1[index]);
        us2 = tolower(s2[index]);
    }

    if (index == len)
    {
        return 0;
    }

    return us1 - us2;
}

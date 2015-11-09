#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <ctype.h>
#include <string.h>

int strcasecmp(const char *s1, const char *s2)
{
    unsigned char us1;
    unsigned char us2;
    size_t index;
    
    us1 = tolower(*s1);
    us2 = tolower(*s2);

    index = 0;
    while (us1 != '\0' && us1 == us2)
    {
        index++;
        us1 = tolower(s1[index]);
        us2 = tolower(s2[index]);
    }
    
    return us1 - us2;
}

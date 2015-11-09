#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>
#include <string.h>

char *strdup(const char *str)
{
    char *new_str;
    size_t length;

    if (str == NULL)
    {
        return NULL;
    }
    length = strlen(str);
    new_str = malloc(length + 1);
    if (new_str == NULL)
    {
        return NULL;
    }
    memcpy(new_str, str, length);
    new_str[length] = '\0';

    return new_str;
} 

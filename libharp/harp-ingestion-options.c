/*
 * Copyright (C) 2015-2023 S[&]T, The Netherlands.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include "harp-ingestion.h"

#include <assert.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

static void ingestion_option_delete(harp_ingestion_option *option);

static int ingestion_option_new(const char *name, const char *value, harp_ingestion_option **new_option)
{
    harp_ingestion_option *option;

    assert(name != NULL);
    assert(value != NULL);

    option = (harp_ingestion_option *)malloc(sizeof(harp_ingestion_option));
    if (option == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       sizeof(harp_ingestion_option), __FILE__, __LINE__);
        return -1;
    }

    option->name = NULL;
    option->value = NULL;

    option->name = strdup(name);
    if (option->name == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not duplicate string) (%s:%u)", __FILE__,
                       __LINE__);
        ingestion_option_delete(option);
        return -1;
    }

    option->value = strdup(value);
    if (option->value == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not duplicate string) (%s:%u)", __FILE__,
                       __LINE__);
        ingestion_option_delete(option);
        return -1;
    }

    *new_option = option;
    return 0;
}

static void ingestion_option_delete(harp_ingestion_option *option)
{
    if (option != NULL)
    {
        if (option->name != NULL)
        {
            free(option->name);
        }

        if (option->value != NULL)
        {
            free(option->value);
        }

        free(option);
    }
}

static int ingestion_option_copy(const harp_ingestion_option *other_option, harp_ingestion_option **new_option)
{
    assert(other_option != NULL);
    return ingestion_option_new(other_option->name, other_option->value, new_option);
}

static int ingestion_options_get_option_index(const harp_ingestion_options *options, const char *name)
{
    int i;

    for (i = 0; i < options->num_options; i++)
    {
        if (strcmp(options->option[i]->name, name) == 0)
        {
            return i;
        }
    }

    return -1;
}

static int ingestion_options_add_option(harp_ingestion_options *options, harp_ingestion_option *option)
{
    assert(ingestion_options_get_option_index(options, option->name) == -1);

    if (options->num_options % BLOCK_SIZE == 0)
    {
        harp_ingestion_option **new_option;
        size_t new_size;

        new_size = (options->num_options + BLOCK_SIZE) * sizeof(harp_ingestion_option);
        new_option = (harp_ingestion_option **)realloc(options->option, new_size);
        if (new_option == NULL)
        {
            harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)", new_size,
                           __FILE__, __LINE__);
            return -1;
        }
        options->option = new_option;
    }
    options->option[options->num_options++] = option;

    return 0;
}

static void ingestion_options_replace_option(harp_ingestion_options *options, int index, harp_ingestion_option *option)
{
    assert(index >= 0 && index < options->num_options);
    assert(options->option[index] != option);

    ingestion_option_delete(options->option[index]);
    options->option[index] = option;
}

static int ingestion_options_add_or_replace_option(harp_ingestion_options *options, harp_ingestion_option *option)
{
    int index;

    index = ingestion_options_get_option_index(options, option->name);
    if (index == -1)
    {
        return ingestion_options_add_option(options, option);
    }

    ingestion_options_replace_option(options, index, option);
    return 0;
}

static char *skip_white_space(char *str)
{
    while (*str != '\0' && isspace(*str))
    {
        str++;
    }

    return str;
}

static char *skip_name(char *str)
{
    if (!isalpha(*str))
    {
        return str;
    }

    str++;
    while (*str != '\0' && (*str == '_' || isalnum(*str)))
    {
        str++;
    }

    return str;
}

static char *skip_value(char *str)
{
    while (*str != '\0' && *str != ';' && !isspace(*str))
    {
        str++;
    }

    return str;
}

static int split_option(char *str, char **name, char **value, char **tail)
{
    char *mark;

    str = skip_white_space(str);

    *name = str;
    str = skip_name(str);

    if (str == *name)
    {
        harp_set_error(HARP_ERROR_INGESTION_OPTION_SYNTAX, "syntax error: expected option name");
        return -1;
    }

    mark = str;
    str = skip_white_space(str);

    if (*str != '=')
    {
        harp_set_error(HARP_ERROR_INGESTION_OPTION_SYNTAX, "syntax error: expected '='");
        return -1;
    }

    *mark = '\0';
    str++;

    str = skip_white_space(str);

    *value = str;
    str = skip_value(str);

    if (str == *value)
    {
        harp_set_error(HARP_ERROR_INGESTION_OPTION_SYNTAX, "syntax error: expected option value");
        return -1;
    }

    if (*str != '\0')
    {
        *str = '\0';
        str++;
    }

    *tail = str;
    return 0;
}

static int ingestion_options_set_option_from_string(harp_ingestion_options *options, char *str)
{
    char *name;
    char *value;
    char *tail;

    if (split_option(str, &name, &value, &tail) != 0)
    {
        return -1;
    }

    tail = skip_white_space(tail);
    if (*tail != '\0')
    {
        harp_set_error(HARP_ERROR_INGESTION_OPTION_SYNTAX, "syntax error: trailing characters after option value");
        return -1;
    }

    if (harp_ingestion_options_set_option(options, name, value) != 0)
    {
        return -1;
    }

    return 0;
}

static int ingestion_options_from_string(char *str, harp_ingestion_options **new_options)
{
    harp_ingestion_options *options;

    if (harp_ingestion_options_new(&options) != 0)
    {
        return -1;
    }

    while (*str != '\0')
    {
        char *substr;

        substr = str;
        while (*str != '\0' && *str != ';')
        {
            str++;
        }

        if (*str != '\0')
        {
            *str = '\0';
            str++;
        }

        if (ingestion_options_set_option_from_string(options, substr) != 0)
        {
            return -1;
        }
    }

    *new_options = options;
    return 0;
}

int harp_ingestion_options_new(harp_ingestion_options **new_options)
{
    harp_ingestion_options *options;

    options = (harp_ingestion_options *)malloc(sizeof(harp_ingestion_options));
    if (options == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       sizeof(harp_ingestion_options), __FILE__, __LINE__);
        return -1;
    }

    options->num_options = 0;
    options->option = NULL;

    *new_options = options;
    return 0;
}

int harp_ingestion_options_copy(const harp_ingestion_options *other_options, harp_ingestion_options **new_options)
{
    harp_ingestion_options *options;
    int i;

    assert(other_options != NULL);
    if (harp_ingestion_options_new(&options) != 0)
    {
        return -1;
    }

    for (i = 0; i < other_options->num_options; i++)
    {
        harp_ingestion_option *option;

        if (ingestion_option_copy(other_options->option[i], &option) != 0)
        {
            harp_ingestion_options_delete(options);
            return -1;
        }

        if (ingestion_options_add_option(options, option) != 0)
        {
            ingestion_option_delete(option);
            harp_ingestion_options_delete(options);
            return -1;
        }
    }

    *new_options = options;
    return 0;
}

void harp_ingestion_options_delete(harp_ingestion_options *options)
{
    if (options != NULL)
    {
        if (options->option != NULL)
        {
            int i;

            for (i = 0; i < options->num_options; i++)
            {
                ingestion_option_delete(options->option[i]);
            }
            free(options->option);
        }

        free(options);
    }
}

int harp_ingestion_options_has_option(const harp_ingestion_options *options, const char *name)
{
    return (ingestion_options_get_option_index(options, name) >= 0);
}

int harp_ingestion_options_get_option(const harp_ingestion_options *options, const char *name, const char **value)
{
    int index;

    index = ingestion_options_get_option_index(options, name);
    if (index >= 0)
    {
        *value = options->option[index]->value;
        return 0;
    }

    harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "ingestion option '%s' is not set", name);
    return -1;
}

int harp_ingestion_options_set_option(harp_ingestion_options *options, const char *name, const char *value)
{
    harp_ingestion_option *option;

    if (ingestion_option_new(name, value, &option) != 0)
    {
        return -1;
    }

    if (ingestion_options_add_or_replace_option(options, option) != 0)
    {
        ingestion_option_delete(option);
        return -1;
    }

    return 0;
}

int harp_ingestion_options_remove_option(harp_ingestion_options *options, const char *name)
{
    int index;
    int i;

    index = ingestion_options_get_option_index(options, name);
    if (index == -1)
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "ingestion option %s does not exist", name);
        return -1;
    }

    ingestion_option_delete(options->option[index]);
    for (i = index + 1; i < options->num_options; i++)
    {
        options->option[i - 1] = options->option[i];
    }
    options->num_options--;

    return 0;
}

int harp_ingestion_options_set_option_from_string(harp_ingestion_options *options, const char *str)
{
    char *str_copy;

    str_copy = strdup(str);
    if (str_copy == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not duplicate string) (%s:%u)", __FILE__,
                       __LINE__);
        return -1;
    }

    if (ingestion_options_set_option_from_string(options, str_copy) != 0)
    {
        free(str_copy);
        return -1;
    }
    free(str_copy);

    return 0;
}

int harp_ingestion_options_from_string(const char *str, harp_ingestion_options **new_options)
{
    harp_ingestion_options *options;
    char *str_copy;

    str_copy = strdup(str);
    if (str_copy == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not duplicate string) (%s:%u)", __FILE__,
                       __LINE__);
        return -1;
    }

    if (ingestion_options_from_string(str_copy, &options) != 0)
    {
        free(str_copy);
        return -1;
    }
    free(str_copy);

    *new_options = options;
    return 0;
}

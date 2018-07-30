/*
 * Copyright (C) 2015-2018 S[&]T, The Netherlands.
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "harp.h"

#include <stdlib.h>
#include <string.h>

typedef struct resample_info_struct
{
    harp_collocation_result *collocation_result;
    int perform_nearest_neighbour_x_first;
    char *nearest_neighbour_x_variable_name;
    long nearest_neighbour_x_criterium_index;
    char *nearest_neighbour_y_variable_name;
    long nearest_neighbour_y_criterium_index;
} resample_info;

static void resample_info_delete(resample_info *info)
{
    if (info != NULL)
    {
        if (info->collocation_result != NULL)
        {
            harp_collocation_result_delete(info->collocation_result);
        }
        if (info->nearest_neighbour_x_variable_name != NULL)
        {
            free(info->nearest_neighbour_x_variable_name);
        }
        if (info->nearest_neighbour_y_variable_name != NULL)
        {
            free(info->nearest_neighbour_y_variable_name);
        }
        free(info);
    }
}

static int resample_info_new(resample_info **new_info)
{
    resample_info *info = NULL;

    info = (resample_info *)malloc(sizeof(resample_info));
    if (info == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       sizeof(resample_info), __FILE__, __LINE__);
        return -1;
    }
    info->collocation_result = NULL;
    info->perform_nearest_neighbour_x_first = 0;
    info->nearest_neighbour_x_variable_name = NULL;
    info->nearest_neighbour_x_criterium_index = -1;
    info->nearest_neighbour_y_variable_name = NULL;
    info->nearest_neighbour_y_criterium_index = -1;

    *new_info = info;

    return 0;
}

static int get_criterium_index_for_variable_name(harp_collocation_result *collocation_result, const char *variable_name,
                                                 long *index)
{
    long variable_name_length = (long)strlen(variable_name);
    int i;

    for (i = 0; i < collocation_result->num_differences; i++)
    {
        long difference_name_length = (long)strlen(collocation_result->difference_variable_name[i]);

        if (variable_name_length == difference_name_length)
        {
            if (strcmp(variable_name, collocation_result->difference_variable_name[i]) == 0)
            {
                *index = i;
                return 0;
            }
        }
        else if (variable_name_length < difference_name_length)
        {
            if (strncmp(variable_name, collocation_result->difference_variable_name[i], variable_name_length) == 0 &&
                strcmp(&collocation_result->difference_variable_name[i][variable_name_length], "_absdiff") == 0)
            {
                *index = i;
                return 0;
            }
        }
    }

    harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "collocation result has no difference for '%s'", variable_name);
    return -1;
}

static int resample_info_update(resample_info *info)
{
    if (info->nearest_neighbour_x_variable_name != NULL)
    {
        if (get_criterium_index_for_variable_name(info->collocation_result, info->nearest_neighbour_x_variable_name,
                                                  &info->nearest_neighbour_x_criterium_index) != 0)
        {
            return -1;
        }
    }
    if (info->nearest_neighbour_y_variable_name != NULL)
    {
        if (get_criterium_index_for_variable_name(info->collocation_result, info->nearest_neighbour_y_variable_name,
                                                  &info->nearest_neighbour_y_criterium_index) != 0)
        {
            return -1;
        }
    }

    return 0;
}

int resample_nearest_a(harp_collocation_result *collocation_result, int difference_index)
{
    long i;

    if (harp_collocation_result_sort_by_a(collocation_result) != 0)
    {
        return -1;
    }
    for (i = collocation_result->num_pairs - 1; i > 0; i--)
    {
        if (collocation_result->pair[i]->product_index_a == collocation_result->pair[i - 1]->product_index_a &&
            collocation_result->pair[i]->sample_index_a == collocation_result->pair[i - 1]->sample_index_a)
        {
            if (collocation_result->pair[i]->difference[difference_index] >=
                collocation_result->pair[i - 1]->difference[difference_index])
            {
                if (harp_collocation_result_remove_pair_at_index(collocation_result, i) != 0)
                {
                    return -1;
                }
            }
            else
            {
                if (harp_collocation_result_remove_pair_at_index(collocation_result, i - 1) != 0)
                {
                    return -1;
                }
            }
        }
    }

    return 0;
}

int resample_nearest_b(harp_collocation_result *collocation_result, int difference_index)
{
    long i;

    if (harp_collocation_result_sort_by_b(collocation_result) != 0)
    {
        return -1;
    }
    for (i = collocation_result->num_pairs - 1; i > 0; i--)
    {
        if (collocation_result->pair[i]->product_index_b == collocation_result->pair[i - 1]->product_index_b &&
            collocation_result->pair[i]->sample_index_b == collocation_result->pair[i - 1]->sample_index_b)
        {
            if (collocation_result->pair[i]->difference[difference_index] >=
                collocation_result->pair[i - 1]->difference[difference_index])
            {
                if (harp_collocation_result_remove_pair_at_index(collocation_result, i) != 0)
                {
                    return -1;
                }
            }
            else
            {
                if (harp_collocation_result_remove_pair_at_index(collocation_result, i - 1) != 0)
                {
                    return -1;
                }
            }
        }
    }

    return 0;
}

int resample(int argc, char *argv[])
{
    resample_info *info;
    const char *output;
    long i;

    if (resample_info_new(&info) != 0)
    {
        return -1;
    }
    for (i = 2; i < argc; i++)
    {
        if (strcmp(argv[i], "-nx") == 0 && i + 1 < argc && argv[i + 1][0] != '-')
        {
            if (info->nearest_neighbour_x_variable_name != NULL)
            {
                resample_info_delete(info);
                return 1;
            }
            info->nearest_neighbour_x_variable_name = strdup(argv[i + 1]);

            if (info->nearest_neighbour_x_variable_name == NULL)
            {
                harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not duplicate string) (%s:%u)", __FILE__,
                               __LINE__);
                resample_info_delete(info);
                return -1;
            }
            if (info->nearest_neighbour_y_variable_name == NULL)
            {
                info->perform_nearest_neighbour_x_first = 1;
            }
            i++;
        }
        else if (strcmp(argv[i], "-ny") == 0 && i + 1 < argc && argv[i + 1][0] != '-')
        {
            if (info->nearest_neighbour_y_variable_name != NULL)
            {
                resample_info_delete(info);
                return 1;
            }
            info->nearest_neighbour_y_variable_name = strdup(argv[i + 1]);

            if (info->nearest_neighbour_y_variable_name == NULL)
            {
                harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not duplicate string) (%s:%u)", __FILE__,
                               __LINE__);
                resample_info_delete(info);
                return -1;
            }
            i++;
        }
        else
        {
            if (argv[i][0] == '-' || (i != argc - 1 && i != argc - 2))
            {
                resample_info_delete(info);
                return 1;
            }
            break;
        }
    }

    if (i == argc - 2)
    {
        if (argv[argc - 1][0] == '-')
        {
            resample_info_delete(info);
            return 1;
        }
        output = argv[argc - 1];
    }
    else
    {
        output = argv[i];
    }
    if (harp_collocation_result_read(argv[i], &info->collocation_result) != 0)
    {
        resample_info_delete(info);
        return -1;
    }
    if (resample_info_update(info) != 0)
    {
        return -1;
    }

    for (i = 0; i < 2; i++)
    {
        if ((info->perform_nearest_neighbour_x_first && i == 0) || (!info->perform_nearest_neighbour_x_first && i == 1))
        {
            if (info->nearest_neighbour_x_criterium_index >= 0)
            {
                if (resample_nearest_a(info->collocation_result, info->nearest_neighbour_x_criterium_index) != 0)
                {
                    resample_info_delete(info);
                    return -1;
                }
            }
        }
        else
        {
            if (info->nearest_neighbour_y_criterium_index >= 0)
            {
                if (resample_nearest_b(info->collocation_result, info->nearest_neighbour_y_criterium_index) != 0)
                {
                    resample_info_delete(info);
                    return -1;
                }
            }
        }
    }

    if (harp_collocation_result_sort_by_collocation_index(info->collocation_result) != 0)
    {
        resample_info_delete(info);
        return -1;
    }

    if (harp_collocation_result_write(output, info->collocation_result) != 0)
    {
        resample_info_delete(info);
        return -1;
    }

    resample_info_delete(info);

    return 0;
}

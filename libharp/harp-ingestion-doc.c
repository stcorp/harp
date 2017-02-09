/*
 * Copyright (C) 2015-2017 S[&]T, The Netherlands.
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
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef max
#undef max
#endif
static int max(int a, int b)
{
    return (a > b ? a : b);
}

static int vscprintf(const char *format, va_list ap)
{
#ifdef WIN32
    return _vscprintf(format, ap);
#else
    return vsnprintf(NULL, 0, format, ap);
#endif
}

static int scprintf(const char *format, ...)
{
    va_list ap;
    int length;

    va_start(ap, format);
    length = vscprintf(format, ap);
    va_end(ap);

    assert(length >= 0);

    return length;
}

static void fnputc(long n, char c, FILE *stream)
{
    while (n > 0)
    {
        fputc(c, stream);
        n--;
    }
}

static void print_padded_string(FILE *fout, int column_width, const char *format, ...)
{
    va_list ap;
    int length;

    va_start(ap, format);

    length = vfprintf(fout, format, ap);
    assert(length >= 0);
    if (length < column_width)
    {
        fnputc(column_width - length, ' ', fout);
    }

    va_end(ap);
}

static int product_definition_has_mapping_description(const harp_product_definition *product_definition)
{
    int i;

    if (product_definition->mapping_description != NULL)
    {
        return 1;
    }

    for (i = 0; i < product_definition->num_variable_definitions; i++)
    {
        harp_variable_definition *variable_definition = product_definition->variable_definition[i];

        if (variable_definition->num_mappings > 0 || variable_definition->exclude != NULL)
        {
            return 1;
        }
    }

    return 0;
}

static int generate_product_definition(const char *filename, const harp_ingestion_module *module,
                                       const harp_product_definition *product_definition)
{
    FILE *fout;
    int i;
    int j;

    fout = fopen(filename, "w");
    if (fout == NULL)
    {
        harp_set_error(HARP_ERROR_FILE_OPEN, "could not open file '%s' for writing", filename);
        return -1;
    }

    fputs(":orphan: true\n\n", fout);
    fputs(product_definition->name, fout);
    fputc('\n', fout);
    fnputc(strlen(product_definition->name), '=', fout);
    fnputc(2, '\n', fout);

    fputs("Variables\n", fout);
    fputs("---------\n", fout);
    fprintf(fout, "The table below lists the variables that are present in the HARP product that results from an "
            "ingestion of ``%s`` data.\n\n", product_definition->name);

    fputs(".. csv-table::\n", fout);
    fputs("   :widths: 25 5 15 15 40\n", fout);
    fputs("   :header-rows: 1\n\n", fout);
    fputs("   \"field name\", \"type\", \"dimensions\", \"unit\", \"description\"\n", fout);
    for (i = 0; i < product_definition->num_variable_definitions; i++)
    {
        harp_variable_definition *variable_definition = product_definition->variable_definition[i];

        fnputc(3, ' ', fout);
        fputc('"', fout);
        fprintf(fout, "**%s**", variable_definition->name);
        fputc('"', fout);
        fputs(", ", fout);

        fputc('"', fout);
        fputs(harp_get_data_type_name(variable_definition->data_type), fout);
        fputc('"', fout);
        fputs(", ", fout);

        fputc('"', fout);
        if (variable_definition->num_dimensions > 0)
        {
            fputc('{', fout);
            for (j = 0; j < variable_definition->num_dimensions; j++)
            {
                if (j > 0)
                {
                    fputs(", ", fout);
                }
                if (variable_definition->dimension_type[j] == harp_dimension_independent)
                {
                    fprintf(fout, "%ld", variable_definition->dimension[j]);
                }
                else
                {
                    fprintf(fout, "*%s*", harp_get_dimension_type_name(variable_definition->dimension_type[j]));
                }
            }
            fputc('}', fout);
        }
        fputc('"', fout);
        fputs(", ", fout);

        fputc('"', fout);
        if (variable_definition->unit != NULL && strlen(variable_definition->unit) > 0)
        {
            fputs(variable_definition->unit, fout);
        }
        fputc('"', fout);
        fputs(", ", fout);

        fputc('"', fout);
        if (variable_definition->description != NULL)
        {
            fputs(variable_definition->description, fout);
        }
        fputc('"', fout);
        fputc('\n', fout);
    }

    if (module->num_option_definitions > 0)
    {
        fputc('\n', fout);
        fputs("Ingestion options\n", fout);
        fputs("-----------------\n", fout);
        fprintf(fout, "The table below lists the available ingestion options for ``%s`` products.\n\n", module->name);
        fputs(".. csv-table::\n", fout);
        fputs("   :widths: 15 25 60\n", fout);
        fputs("   :header-rows: 1\n\n", fout);
        fputs("   \"option name\", \"legal values\", \"description\"\n", fout);

        for (j = 0; j < module->num_option_definitions; j++)
        {
            harp_ingestion_option_definition *option_definition = module->option_definition[j];

            fnputc(3, ' ', fout);
            fputc('"', fout);
            fputs(option_definition->name, fout);
            fputc('"', fout);
            fputs(", ", fout);

            fputc('"', fout);
            if (option_definition->num_allowed_values > 0)
            {
                int k;

                for (k = 0; k < option_definition->num_allowed_values; k++)
                {
                    fputs(option_definition->allowed_value[k], fout);
                    if (k + 1 < option_definition->num_allowed_values)
                    {
                        fputs(", ", fout);
                    }
                }
            }
            fputc('"', fout);
            fputs(", ", fout);

            fputc('"', fout);
            if (option_definition->description != NULL)
            {
                fputs(option_definition->description, fout);
            }
            fputc('"', fout);
            fputc('\n', fout);
        }
        fputc('\n', fout);

        if (product_definition->ingestion_option != NULL)
        {
            fprintf(fout, "This definition is only applicable when: %s\n", product_definition->ingestion_option);
        }
    }

    if (product_definition_has_mapping_description(product_definition))
    {
        int column_width[3] = { 0 };
        int span_width;

        fputc('\n', fout);
        fputs("Mapping description\n", fout);
        fputs("-------------------\n", fout);
        fputs("The table below details where and how each variable was retrieved from the input product.\n\n", fout);
        if (product_definition->mapping_description != NULL)
        {
            fprintf(fout, "%s\n\n", product_definition->mapping_description);
        }

        for (i = 0; i < product_definition->num_variable_definitions; i++)
        {
            harp_variable_definition *variable_definition = product_definition->variable_definition[i];

            if (variable_definition->num_mappings == 0 && variable_definition->exclude == NULL)
            {
                continue;
            }

            column_width[0] = max(column_width[0], scprintf("**%s**", variable_definition->name));

            if (variable_definition->exclude != NULL)
            {
                column_width[1] = max(column_width[1], strlen("*available*"));
                column_width[2] = max(column_width[2], strlen("optional"));
            }

            for (j = 0; j < variable_definition->num_mappings; j++)
            {
                harp_mapping_description *mapping = variable_definition->mapping[j];

                if (mapping->ingestion_option != NULL || mapping->condition != NULL)
                {
                    column_width[1] = max(column_width[1], strlen("*condition*"));

                    if (mapping->ingestion_option != NULL && mapping->condition != NULL)
                    {
                        column_width[2] = max(column_width[2], scprintf("%s and %s", mapping->ingestion_option,
                                                                        mapping->condition));
                    }
                    else if (mapping->ingestion_option != NULL)
                    {
                        column_width[2] = max(column_width[2], strlen(mapping->ingestion_option));
                    }
                    else
                    {
                        column_width[2] = max(column_width[2], strlen(mapping->condition));
                    }
                }
                if (mapping->path != NULL)
                {
                    column_width[1] = max(column_width[1], strlen("*path*"));
                    column_width[2] = max(column_width[2], scprintf("**%s**", mapping->path));
                }
                if (mapping->description != NULL)
                {
                    column_width[1] = max(column_width[1], strlen("*description*"));
                    column_width[2] = max(column_width[2], strlen(mapping->description));
                }
            }
        }

        /* Adjust maximum column widths for the header fields. Note that the second header field spans two columns.
         * If the length of the second header field is larger than the maximum widths of the spanned columns combined,
         * the width of the last column is increased such that the combined width matches the length of the second
         * header field.
         */
        column_width[0] = max(column_width[0], strlen("field name"));
        span_width = max(strlen("mapping description"), column_width[1] + 3 + column_width[2]);
        if (span_width > column_width[1] + 3 + column_width[2])
        {
            column_width[2] = span_width - (column_width[1] + 3);
        }

        fputc('+', fout);
        fnputc(column_width[0] + 2, '-', fout);
        fputc('+', fout);
        fnputc(span_width + 2, '-', fout);
        fputc('+', fout);
        fputc('\n', fout);

        fputs("| ", fout);
        print_padded_string(fout, column_width[0], "%s", "field name");
        fputs(" | ", fout);
        print_padded_string(fout, span_width, "%s", "mapping description");
        fputs(" |\n", fout);

        fputc('+', fout);
        fnputc(column_width[0] + 2, '=', fout);
        fputc('+', fout);
        fnputc(column_width[1] + 2, '=', fout);
        fputc('+', fout);
        fnputc(column_width[2] + 2, '=', fout);
        fputc('+', fout);
        fputc('\n', fout);

        for (i = 0; i < product_definition->num_variable_definitions; i++)
        {
            harp_variable_definition *variable_definition = product_definition->variable_definition[i];
            int first_row = 1;

            if (variable_definition->num_mappings == 0 && variable_definition->exclude == NULL)
            {
                continue;
            }

            fputs("| ", fout);
            print_padded_string(fout, column_width[0], "**%s**", variable_definition->name);
            fputs(" | ", fout);

            if (variable_definition->exclude != NULL)
            {
                print_padded_string(fout, column_width[1], "%s", "*available*");
                fputs(" | ", fout);
                print_padded_string(fout, column_width[2], "%s", "optional");
                fputs(" |\n", fout);

                first_row = 0;
            }

            for (j = 0; j < variable_definition->num_mappings; j++)
            {
                harp_mapping_description *mapping = variable_definition->mapping[j];

                if (mapping->ingestion_option != NULL || mapping->condition != NULL)
                {
                    if (!first_row)
                    {
                        fputc('+', fout);
                        fnputc(column_width[0] + 2, ' ', fout);
                        fputc('+', fout);
                        fnputc(column_width[1] + 2, '-', fout);
                        fputc('+', fout);
                        fnputc(column_width[2] + 2, '-', fout);
                        fputc('+', fout);
                        fputc('\n', fout);

                        fputs("| ", fout);
                        fnputc(column_width[0], ' ', fout);
                        fputs(" | ", fout);
                    }
                    else
                    {
                        first_row = 0;
                    }

                    print_padded_string(fout, column_width[1], "%s", "*condition*");
                    fputs(" | ", fout);
                    if (mapping->ingestion_option != NULL && mapping->condition != NULL)
                    {
                        print_padded_string(fout, column_width[2], "%s and %s", mapping->ingestion_option,
                                            mapping->condition);
                    }
                    else if (mapping->ingestion_option != NULL)
                    {
                        print_padded_string(fout, column_width[2], "%s", mapping->ingestion_option);
                    }
                    else
                    {
                        print_padded_string(fout, column_width[2], "%s", mapping->condition);
                    }
                    fputs(" |\n", fout);
                }
                if (mapping->path != NULL)
                {
                    if (!first_row)
                    {
                        fputc('+', fout);
                        fnputc(column_width[0] + 2, ' ', fout);
                        fputc('+', fout);
                        fnputc(column_width[1] + 2, '-', fout);
                        fputc('+', fout);
                        fnputc(column_width[2] + 2, '-', fout);
                        fputc('+', fout);
                        fputc('\n', fout);

                        fputs("| ", fout);
                        fnputc(column_width[0], ' ', fout);
                        fputs(" | ", fout);
                    }
                    else
                    {
                        first_row = 0;
                    }

                    print_padded_string(fout, column_width[1], "%s", "*path*");
                    fputs(" | ", fout);
                    print_padded_string(fout, column_width[2], "**%s**", mapping->path);
                    fputs(" |\n", fout);
                }
                if (mapping->description != NULL)
                {
                    if (!first_row)
                    {
                        fputc('+', fout);
                        fnputc(column_width[0] + 2, ' ', fout);
                        fputc('+', fout);
                        fnputc(column_width[1] + 2, '-', fout);
                        fputc('+', fout);
                        fnputc(column_width[2] + 2, '-', fout);
                        fputc('+', fout);
                        fputc('\n', fout);

                        fputs("| ", fout);
                        fnputc(column_width[0], ' ', fout);
                        fputs(" | ", fout);
                    }
                    else
                    {
                        first_row = 0;
                    }

                    print_padded_string(fout, column_width[1], "%s", "*description*");
                    fputs(" | ", fout);
                    print_padded_string(fout, column_width[2], "%s", mapping->description);
                    fputs(" |\n", fout);
                }
            }

            fputc('+', fout);
            fnputc(column_width[0] + 2, '-', fout);
            fputc('+', fout);
            fnputc(column_width[1] + 2, '-', fout);
            fputc('+', fout);
            fnputc(column_width[2] + 2, '-', fout);
            fputc('+', fout);
            fputc('\n', fout);
        }
    }

    fclose(fout);
    return 0;
}

static int generate_product_group(FILE *fout, const char *product_group, int num_ingestion_modules,
                                  harp_ingestion_module **ingestion_module)
{
    int i;

    fprintf(fout, ".. _%s:\n\n", product_group);
    fprintf(fout, "%s products\n", product_group);
    fnputc(strlen(product_group) + 9, '-', fout);
    fputc('\n', fout);
    fputc('\n', fout);

    fputs(".. csv-table::\n", fout);
    fputs("   :header-rows: 1\n\n", fout);
    fputs("   \"HARP product name\", \"CODA product type\", \"description\"\n", fout);
    for (i = 0; i < num_ingestion_modules; i++)
    {
        const harp_ingestion_module *module = ingestion_module[i];

        fnputc(3, ' ', fout);
        if (module->num_product_definitions == 1 && strcmp(module->product_definition[0]->name, module->name) == 0)
        {
            /* don't print details when we only have one conversion (whose name equals that of the module) */
            fprintf(fout, "\":doc:`%s`\", ", module->product_definition[0]->name);
        }
        else
        {
            fprintf(fout, "\":ref:`%s`\", ", module->name);
        }

        fputc('"', fout);
        if (module->product_class != NULL && module->product_type != NULL)
        {
            fprintf(fout, "%s/%s", module->product_class, module->product_type);
        }
        else if (module->product_class != NULL)
        {
            fputs(module->product_class, fout);
        }
        else if (module->product_type != NULL)
        {
            fputs(module->product_type, fout);
        }
        fputc('"', fout);
        fputs(", ", fout);

        fputc('"', fout);
        if (module->description != NULL)
        {
            fputs(module->description, fout);
        }
        fputc('"', fout);
        fputc('\n', fout);
    }
    fputc('\n', fout);

    for (i = 0; i < num_ingestion_modules; i++)
    {
        const harp_ingestion_module *module = ingestion_module[i];
        int j;

        if (module->num_product_definitions == 1 && strcmp(module->product_definition[0]->name, module->name) == 0)
        {
            /* skip printing details if we already have a direct link to the conversion (see above) */
            continue;
        }

        fprintf(fout, ".. _%s:\n\n", module->name);
        fprintf(fout, "%s\n", module->name);
        fnputc(strlen(module->name), '^', fout);
        fputc('\n', fout);

        if (module->description != NULL)
        {
            fprintf(fout, "%s\n\n", module->description);
        }

        fprintf(fout, "The table below lists the available product conversions for ``%s`` products.\n\n", module->name);
        fputs(".. csv-table::\n", fout);
        fputs("   :header-rows: 1\n\n", fout);
        fputs("   \"name\", \"ingestion option\", \"description\"\n", fout);
        for (j = 0; j < module->num_product_definitions; j++)
        {
            harp_product_definition *product_definition = module->product_definition[j];

            fnputc(3, ' ', fout);
            fprintf(fout, "\":doc:`%s`\"", product_definition->name);
            fputs(", ", fout);

            fputc('"', fout);
            if (product_definition->ingestion_option != NULL)
            {
                fputs(product_definition->ingestion_option, fout);
            }
            fputc('"', fout);
            fputs(", ", fout);

            fputc('"', fout);
            if (product_definition->description != NULL)
            {
                fputs(product_definition->description, fout);
            }
            fputc('"', fout);
            fputc('\n', fout);
        }
        fputc('\n', fout);

        if (module->num_option_definitions > 0)
        {
            fprintf(fout, "The table below lists the available ingestion options for ``%s`` products.\n\n",
                    module->name);
            fputs(".. csv-table::\n", fout);
            fputs("   :widths: 15 25 60\n", fout);
            fputs("   :header-rows: 1\n\n", fout);
            fputs("   \"option name\", \"legal values\", \"description\"\n", fout);

            for (j = 0; j < module->num_option_definitions; j++)
            {
                harp_ingestion_option_definition *option_definition = module->option_definition[j];

                fnputc(3, ' ', fout);
                fputc('"', fout);
                fputs(option_definition->name, fout);
                fputc('"', fout);
                fputs(", ", fout);

                fputc('"', fout);
                if (option_definition->num_allowed_values > 0)
                {
                    int k;

                    for (k = 0; k < option_definition->num_allowed_values; k++)
                    {
                        fputs(option_definition->allowed_value[k], fout);
                        if (k + 1 < option_definition->num_allowed_values)
                        {
                            fputs(", ", fout);
                        }
                    }
                }
                fputc('"', fout);
                fputs(", ", fout);

                fputc('"', fout);
                if (option_definition->description != NULL)
                {
                    fputs(option_definition->description, fout);
                }
                fputc('"', fout);
                fputc('\n', fout);
            }
            fputc('\n', fout);
        }
    }

    return 0;
}

static int compare_by_product_group_and_name(const void *a, const void *b)
{
    harp_ingestion_module *module_a = *(harp_ingestion_module **)a;
    harp_ingestion_module *module_b = *(harp_ingestion_module **)b;
    int result;

    result = strcmp(module_a->product_group, module_b->product_group);
    if (result == 0)
    {
        result = strcmp(module_a->name, module_b->name);
    }
    return result;
}

static int generate_index(const char *filename, int num_ingestion_modules, harp_ingestion_module **ingestion_module)
{
    FILE *fout;
    harp_ingestion_module **sorted_module;
    int i;
    int j;

    fout = fopen(filename, "w");
    if (fout == NULL)
    {
        harp_set_error(HARP_ERROR_FILE_OPEN, "could not open file '%s' for writing", filename);
        return -1;
    }

    fputs("Ingestion definitions\n", fout);
    fputs("=====================\n", fout);
    fputs("HARP can ingest data from various types of products. The list of supported product types is provided below. "
          "HARP will try to automatically determine the product type of each file that you pass to the ingest "
          "function. An error will be raised if the product type of a file cannot be determined.\n\n", fout);
    fputs("For each ingestion, HARP will return a single HARP product. Each variable in a HARP product represents a "
          "specific quantity (e.g. O\\ :sub:`3` number density, cloud fraction, altitude, longitude, latitude, time, "
          "*et cetera*). You can customize which variables you want to include using the ``include()`` and "
          "``exclude()`` operations that can be passed to the ingest function.\n\n", fout);
    fputs("Within a HARP product, dimensions of the same type (*time*, *latitude*, *longitude*, *vertical*, "
          "*spectral*) are linked together. This means that, within an ingested product, variables cannot have "
          "dimensions of the same type with different lengths.\n\n", fout);
    fputs("For each type of product that contains one or more quantities for which dimensions of the same type have "
          "different lengths, the ingestion will be split into multiple *conversions*. Each conversion only contains "
          "quantities for which the length of each type of dimension is the same. When multiple conversions exist for "
          "a product type, HARP will use the first conversion from the list of available conversions by default.\n\n",
          fout);
    fputs("For example, suppose a certain type of product contains both O\\ :sub:`3` and NO\\ :sub:`2` volume mixing "
          "ratios retrieved on different spatial grids. In this case, it is not possible to have a single pair of "
          "*longitude* and *latitude* variables that describes the geolocation information for both retrievals. "
          "Therefore, two different conversions will be made available for this product type, one for the O\\ :sub:`3` "
          "volume mixing ratio, and another for the NO\\ :sub:`2` volume mixing ratio.\n\n", fout);
    fputs("For each product type, *ingestion options* may be available. These options can be used, for example, to "
          "switch between different product conversions (usually when quantities defined on different grids are "
          "present within a single product), or to switch between different variants of a quantity. Ingestion options "
          "should be passed to the ingest function as a semi-colon separated string of ``option_name=value`` pairs. "
          "These options are unrelated to *operations* (filtering, inclusion and exclusion of variables, and adding "
          "derived variables). Ingestion options are only meaningful in the context of an ingestion, while operations "
          "can be applied both on-the-fly during ingestion, as well as to existing HARP products.\n\n", fout);
    fputs("The list below gives an overview of the conversions and ingestion options available for each product type. "
          "For each conversion, there is a separate page that describes the resulting HARP product. This includes a "
          "list of all the variables, the value type, the dimensions, and the unit of each variable, as well as a full "
          "*mapping description* that details where and how HARP retrieved the data from the input product.\n\n", fout);

    /* Copy the ingestion module list to sort it without disturbing the original list. */
    sorted_module = (harp_ingestion_module **)malloc(num_ingestion_modules * sizeof(harp_ingestion_module *));
    if (sorted_module == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       num_ingestion_modules * sizeof(harp_ingestion_module *), __FILE__, __LINE__);
        return -1;
    }
    memcpy(sorted_module, ingestion_module, num_ingestion_modules * sizeof(harp_ingestion_module *));

    /* Sort ingestion module list based on product group name (ascending) and module name within group (ascending) */
    qsort(sorted_module, num_ingestion_modules, sizeof(harp_ingestion_module *), compare_by_product_group_and_name);

    for (i = 0, j = 0; i < num_ingestion_modules; i++)
    {
        if (i + 1 < num_ingestion_modules &&
            strcmp(sorted_module[i]->product_group, sorted_module[i + 1]->product_group) == 0)
        {
            /* The next ingestion module is part of the same product group as the ingestion modules before it. */
            continue;
        }

        if (generate_product_group(fout, sorted_module[j]->product_group, i - j + 1, &sorted_module[j]) != 0)
        {
            free(sorted_module);
            fclose(fout);
            return -1;
        }

        j = i + 1;
    }

    free(sorted_module);
    fclose(fout);

    return 0;
}

/** Generate reStructuredText documentation for all ingestion definitions.
 * \ingroup harp_documentation
 * \param path Path to directory in which the documentation files will be written.
 * \return
 *   \arg \c 0, Success.
 *   \arg \c -1, Error occurred (check #harp_errno).
 */
LIBHARP_API int harp_doc_export_ingestion_definitions(const char *path)
{
    harp_ingestion_module_register *module_register;
    char *filename;
    int i;
    int j;

    if (path == NULL)
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "path is NULL (%s:%u)", __FILE__, __LINE__);
        return -1;
    }

    if (harp_ingestion_init() != 0)
    {
        return -1;
    }

    module_register = harp_ingestion_get_module_register();
    assert(module_register != NULL);

    filename = (char *)malloc(scprintf("%s/index.rst", path) + 1);
    if (filename == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate % lu bytes) (%s:%u)",
                       scprintf("%s/index.rst", path) + 1, __FILE__, __LINE__);
        return -1;
    }

    sprintf(filename, "%s/index.rst", path);
    if (generate_index(filename, module_register->num_ingestion_modules, module_register->ingestion_module) != 0)
    {
        free(filename);
        return -1;
    }
    else
    {
        free(filename);
    }

    for (i = 0; i < module_register->num_ingestion_modules; i++)
    {
        harp_ingestion_module *ingestion_module = module_register->ingestion_module[i];

        for (j = 0; j < ingestion_module->num_product_definitions; j++)
        {
            harp_product_definition *product_definition = ingestion_module->product_definition[j];

            filename = (char *)malloc(scprintf("%s/%s.rst", path, product_definition->name) + 1);
            if (filename == NULL)
            {
                harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate % lu bytes) (%s:%u)",
                               scprintf("%s/%s.rst", path, product_definition->name) + 1, __FILE__, __LINE__);
                return -1;
            }

            sprintf(filename, "%s/%s.rst", path, product_definition->name);
            if (generate_product_definition(filename, ingestion_module, product_definition) != 0)
            {
                free(filename);
                return -1;
            }
            else
            {
                free(filename);
            }
        }
    }

    return 0;
}

/*
 * Copyright (C) 2015 S[&]T, The Netherlands.
 *
 * This file is part of HARP.
 *
 * HARP is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * HARP is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with HARP; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "harp-ingestion.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

static void print_html_string(FILE *f, const char *str, int nbsp_for_empty_string)
{
    if (str != NULL)
    {
        const char *c;

        c = str;
        while (*c != '\0')
        {
            switch (*c)
            {
                case '<':
                    fputs("&lt;", f);
                    break;
                case '>':
                    fputs("&gt;", f);
                    break;
                case '&':
                    fputs("&amp;", f);
                    break;
                case '\n':
                    fputs("<br />", f);
                    break;
                case '\t':
                    /* convert to 2 spaces */
                    fputs("&nbsp;&nbsp;", f);
                    break;
                default:
                    putc(*c, f);
            }
            c++;
        }
    }
    else if (nbsp_for_empty_string)
    {
        fputs("&nbsp;", f);
    }
}

static void print_html_header(FILE *f, const char *title)
{
    fputs("<?xml version=\"1.0\" encoding=\"iso-8859-1\" ?>\n", f);
    fputs("<!DOCTYPE html PUBLIC \"-//W3C//DTD XHTML 1.1//EN\" \"http://www.w3.org/TR/xhtml11/DTD/xhtml11.dtd\">\n", f);
    fputc('\n', f);
    fputs("<html>\n", f);
    fputs("<head>\n", f);
    fprintf(f, "<title>%s</title>\n", title);
    fputs("<link rel=\"stylesheet\" href=\"../css/harp.css\" type=\"text/css\" />\n", f);
    fputs("</head>\n", f);
    fputs("<body>\n\n", f);
    fputs("<div class=\"main\">\n\n", f);
}

static void print_html_footer(FILE *f)
{
    fputc('\n', f);
    fputs("<div class=\"footer\">\n", f);
    fputs("<hr />\n", f);
    fputs("<p>Copyright &copy; 2015 <b>s<span class=\"soft-red\">[</span>&amp;<span class=\"soft-red\">]"
          "</span>t</b>, The Netherlands.</p>\n", f);
    fputs("</div>\n\n", f);
    fputs("</div>\n\n", f);
    fputs("</body>\n", f);
    fputs("</html>\n", f);
}

static int generate_index_html(const char *filename, const harp_ingestion_module_register *module_register)
{
    FILE *f;
    int alt;
    int i;

    f = fopen(filename, "w");
    if (f == NULL)
    {
        harp_set_error(HARP_ERROR_FILE_OPEN, "could not open file '%s' for writing", filename);
        return -1;
    }

    print_html_header(f, "HARP ingestion definition overview");
    fputs("<h1>HARP ingestion definition overview</h1>\n", f);
    fputs("<p>HARP can ingest data from various types of products. The list of supported product types is provided "
          "below. HARP will try to automatically determine the product type of each file that you pass to the ingest "
          "function. An error will be raised if the product type of a file cannot be determined.</p>\n", f);
    fputs("<p>For each ingestion, HARP will return a single HARP product. Each variable in a HARP product represents a "
          "specific quantity (e.g. O<sub>3</sub> number density, cloud fraction, altitude, longitude, latitude, time, "
          "<i>et cetera</i>). You can customize which variables you want to include using the <code>include()</code> "
          "and <code>exclude()</code> actions that can be passed to the ingest function.</p>\n", f);
    fputs("<p>Within a HARP product, dimensions of the same type (<i>time</i>, <i>latitude</i>, <i>longitude</i>, "
          "<i>vertical</i>, <i>spectral</i>) are linked together. This means that, within an ingested product, "
          "variables cannot have dimensions of the same type with different lengths.</p>\n", f);
    fputs("<p>For each type of product that contains one or more quantities for which dimensions of the same type have "
          "different lengths, the ingestion will be split into multiple <i>conversions</i>. Each conversion only "
          "contains quantities for which the length of each type of dimension is the same. When multiple conversions "
          "exist for a product type, HARP will use the first conversion from the list of available conversions by "
          "default.</p>\n", f);
    fputs("<p>For example, suppose a certain type of product contains both O<sub>3</sub> and NO<sub>2</sub> volume "
          "mixing ratios retrieved on different spatial grids. In this case, it is not possible to have a single pair "
          "of <i>longitude</i> and <i>latitude</i> variables that describes the geolocation information for both "
          "retrievals. Therefore, two different conversions will be made available for this product type, one for the "
          "O<sub>3</sub> volume mixing ratio, and another for the NO<sub>2</sub> volume mixing ratio.</p>\n", f);
    fputs("<p>For each product type, <i>ingestion options</i> may be available. These options can be used, for "
          "example, to switch between different product conversions (usually when quantities defined on different "
          "grids are present within a single product), or to switch between different variants of a quantity. "
          "Ingestion options should be passed to the ingest function as a semi-colon separated string of "
          "<code>option_name=value</code> pairs. These options are unrelated to <i>actions</i> (filtering, inclusion "
          "and exclusion of variables, and adding derived variables). Ingestion options are only meaningful in the "
          "context of an ingestion, while actions can be applied both on-the-fly during ingestion, as well as to "
          "existing HARP products.</p>\n", f);
    fputs("<p>The list below gives an overview of the conversions and ingestion options available for each product "
          "type. For each conversion, there is a separate page that describes the resulting HARP product. This "
          "includes a list of all the variables, the value type, the dimensions, and the unit of each variable, as "
          "well as a full <i>mapping description</i> that details where and how HARP retrieved the data from the input "
          "product.</p>\n\n", f);

    fputs("<h2>Overview of supported product types</h2>\n", f);
    fputs("<p>The table below lists the product types that are supported by HARP.</p>\n", f);
    fputs("<table class=\"fancy\" border=\"1\" cellspacing=\"0\" width=\"100%%\">\n", f);
    fputs("<tr><th>HARP&nbsp;product&nbsp;name</th><th>CODA&nbsp;product&nbsp;type</th><th>description</th></tr>\n", f);

    alt = 0;
    for (i = 0; i < module_register->num_ingestion_modules; i++)
    {
        harp_ingestion_module *ingestion_module = module_register->ingestion_module[i];

        fprintf(f, alt ? "<tr class=\"alt\">" : "<tr>");
        fprintf(f, "<td><a href=\"#%s\">%s</a></td><td>", ingestion_module->name, ingestion_module->name);
        print_html_string(f, ingestion_module->product_class, 1);
        if (ingestion_module->product_class != NULL && ingestion_module->product_type != NULL)
        {
            fputc('/', f);
        }
        print_html_string(f, ingestion_module->product_type, 1);
        fputs("</td><td>", f);
        print_html_string(f, ingestion_module->description, 0);
        fputs("</td></tr>\n", f);
        alt = !alt;
    }
    fputs("</table>\n\n", f);

    fputs("<h2>Overview of supported product conversions</h2>\n", f);
    fputs("<p>The tables below lists the available product conversions and ingestion options for each product type "
          "supported by HARP.</p>\n\n", f);
    for (i = 0; i < module_register->num_ingestion_modules; i++)
    {
        harp_ingestion_module *ingestion_module = module_register->ingestion_module[i];
        int j;

        fprintf(f, "<h3 id=\"%s\">%s</h3>\n", ingestion_module->name, ingestion_module->name);
        if (ingestion_module->description != NULL)
        {
            fputs("<p>", f);
            print_html_string(f, ingestion_module->description, 0);
            fputs("</p>\n", f);
        }

        fprintf(f, "<p>The table below lists the available product conversions for <code>%s</code> products.</p>\n",
                ingestion_module->name);
        fputs("<table class=\"fancy\" border=\"1\" cellspacing=\"0\" width=\"100%%\">\n", f);
        fputs("<tr><th>name</th>", f);
        fputs("<th>ingestion&nbsp;option</th>", f);
        fputs("<th>description</th></tr>\n", f);

        alt = 0;
        for (j = 0; j < ingestion_module->num_product_definitions; j++)
        {
            harp_product_definition *product_definition = ingestion_module->product_definition[j];

            fprintf(f, alt ? "<tr class=\"alt\">" : "<tr>");
            fprintf(f, "<td><a href=\"%s.html\">%s</a></td><td>", product_definition->name, product_definition->name);
            print_html_string(f, product_definition->ingestion_option, 0);
            fputs("</td><td>", f);
            print_html_string(f, product_definition->description, 0);
            fputs("</td></tr>\n", f);
            alt = !alt;
        }
        fputs("</table>\n", f);

        alt = 0;
        if (ingestion_module->num_option_definitions > 0)
        {
            fprintf(f, "<p>The table below lists the available ingestion options for <code>%s</code> products.</p>\n",
                    ingestion_module->name);
            fputs("<table class=\"fancy\" border=\"1\" cellspacing=\"0\" width=\"100%%\">\n", f);
            fputs("<tr><th class=\"subhdr\">option&nbsp;name</th><th class=\"subhdr\">legal&nbsp;values</th>"
                  "<th class=\"subhdr\">description</th></tr>\n", f);

            for (j = 0; j < ingestion_module->num_option_definitions; j++)
            {
                harp_ingestion_option_definition *option_definition = ingestion_module->option_definition[j];

                fprintf(f, alt ? "<tr class=\"alt\">" : "<tr>");
                fprintf(f, "<td><code>%s</code></td><td>", option_definition->name);
                if (option_definition->num_allowed_values > 0)
                {
                    int k;

                    for (k = 0; k < option_definition->num_allowed_values; k++)
                    {
                        if (k == 0)
                        {
                            fputs("<b><code>", f);
                            print_html_string(f, option_definition->allowed_value[k], 0);
                            fputs("</code></b>", f);
                        }
                        else
                        {
                            fputs("<code>", f);
                            print_html_string(f, option_definition->allowed_value[k], 0);
                            fputs("</code>", f);
                        }

                        if (k + 1 < option_definition->num_allowed_values)
                        {
                            fputs(", ", f);
                        }
                    }
                }
                fputs("</td><td>", f);
                print_html_string(f, option_definition->description, 0);
                fputs("</td></tr>\n", f);
                alt = !alt;
            }
            fputs("</table>\n", f);
        }

        if (i != module_register->num_ingestion_modules - 1)
        {
            fputc('\n', f);
        }
    }

    print_html_footer(f);

    fclose(f);

    return 0;
}

static int generate_definition_html(const char *filename, const harp_product_definition *product_definition)
{
    FILE *f;
    int alt = 0;
    int i, j;

    f = fopen(filename, "w");
    if (f == NULL)
    {
        harp_set_error(HARP_ERROR_FILE_OPEN, "could not open file '%s' for writing", filename);
        return -1;
    }

    print_html_header(f, product_definition->name);
    fprintf(f, "<h1>%s</h1>\n\n", product_definition->name);
    fputs("<h2>Variables</h2>\n", f);
    fprintf(f, "<p>The table below lists the variables that are present in the HARP product that results from an "
            "ingestion of <code>%s</code> data.\n", product_definition->name);
    fputs("<table class=\"fancy\" border=\"1\" cellspacing=\"0\" width=\"100%%\">\n", f);
    fputs("<tr><th>field&nbsp;name</th><th>type</th><th>dimensions</th><th>unit</th><th>description</th></tr>\n", f);

    for (i = 0; i < product_definition->num_variable_definitions; i++)
    {
        harp_variable_definition *variable_definition = product_definition->variable_definition[i];

        fprintf(f, alt ? "<tr class=\"alt\">" : "<tr>");
        fprintf(f, "<td><b><code>%s</code></b></td>", variable_definition->name);
        switch (variable_definition->data_type)
        {
            case harp_type_int8:
                fputs("<td>int8</td>", f);
                break;
            case harp_type_int16:
                fputs("<td>int16</td>", f);
                break;
            case harp_type_int32:
                fputs("<td>int32</td>", f);
                break;
            case harp_type_float:
                fputs("<td>float</td>", f);
                break;
            case harp_type_double:
                fputs("<td>double</td>", f);
                break;
            case harp_type_string:
                fputs("<td>string</td>", f);
                break;
        }
        if (variable_definition->num_dimensions > 0)
        {
            fputs("<td>{", f);
            for (j = 0; j < variable_definition->num_dimensions; j++)
            {
                if (j > 0)
                {
                    fputs(", ", f);
                }
                if (variable_definition->dimension_type[j] == harp_dimension_independent)
                {
                    fprintf(f, "%ld", variable_definition->dimension[j]);
                }
                else
                {
                    fprintf(f, "<i>%s</i>", harp_get_dimension_type_name(variable_definition->dimension_type[j]));
                }
            }
            fputs("}</td>", f);
        }
        else
        {
            fputs("<td>&nbsp;</td>", f);
        }
        if (variable_definition->unit != NULL && strlen(variable_definition->unit) > 0)
        {
            fprintf(f, "<td>%s</td>", variable_definition->unit);
        }
        else
        {
            fputs("<td>&nbsp;</td>", f);
        }
        fputs("<td>", f);
        print_html_string(f, variable_definition->description, 0);
        fputs("</td></tr>\n", f);
        alt = !alt;
    }
    fputs("</table>\n", f);

    if (product_definition_has_mapping_description(product_definition))
    {
        alt = 0;

        fputc('\n', f);
        fputs("<h2>Mapping description</h2>\n", f);
        fputs("<p>The table below details where and how each variable was retrieved from the input product.</p>\n", f);
        if (product_definition->mapping_description != NULL)
        {
            fputs("<p>", f);
            print_html_string(f, product_definition->mapping_description, 0);
            fputs("</p>\n", f);
        }
        fputs("<table class=\"fancy\" border=\"1\" cellspacing=\"0\" width=\"100%%\">\n", f);
        fputs("<tr><th>field&nbsp;name</th><th colspan=\"2\">mapping description</th></tr>\n", f);
        for (i = 0; i < product_definition->num_variable_definitions; i++)
        {
            harp_variable_definition *variable_definition = product_definition->variable_definition[i];

            if (variable_definition->num_mappings > 0 || variable_definition->exclude != NULL)
            {
                int num_rows = 0;
                int first_row = 1;
                int lalt = alt;

                /* first determine rowspan */
                if (variable_definition->exclude != NULL)
                {
                    num_rows += 1;
                }
                for (j = 0; j < variable_definition->num_mappings; j++)
                {
                    harp_mapping_description *mapping = variable_definition->mapping[j];

                    num_rows += (mapping->ingestion_option != NULL || mapping->condition != NULL);
                    num_rows += (mapping->path != NULL);
                    num_rows += (mapping->description != NULL);
                }

                fprintf(f, alt ? "<tr class=\"alt\">" : "<tr>");
                fprintf(f, "<td rowspan=\"%d\"><b><code>%s</code></b></td>\n", num_rows, variable_definition->name);
                first_row = 1;
                if (variable_definition->exclude != NULL)
                {
                    fputs("<td align=\"right\"><i>available</i></td><td>optional</td></tr>\n", f);
                    first_row = 0;
                    lalt = !lalt;
                }
                for (j = 0; j < variable_definition->num_mappings; j++)
                {
                    harp_mapping_description *mapping = variable_definition->mapping[j];

                    if (mapping->ingestion_option != NULL || mapping->condition != NULL)
                    {
                        if (!first_row)
                        {
                            fprintf(f, lalt ? "<tr class=\"alt\">" : "<tr>");
                        }
                        else
                        {
                            first_row = 0;
                        }
                        fputs("<td align=\"right\"><i>condition</i></td><td>", f);
                        if (mapping->ingestion_option != NULL)
                        {
                            print_html_string(f, mapping->ingestion_option, 0);
                            if (mapping->condition != NULL)
                            {
                                fputs(" and ", f);
                            }
                        }
                        if (mapping->condition != NULL)
                        {
                            print_html_string(f, mapping->condition, 0);
                        }
                        fputs("</td></tr>\n", f);
                    }
                    if (mapping->path != NULL)
                    {
                        if (!first_row)
                        {
                            fprintf(f, lalt ? "<tr class=\"alt\">" : "<tr>");
                        }
                        else
                        {
                            first_row = 0;
                        }
                        fprintf(f, "<td align=\"right\"><i>path</i></td><td><b><code>%s</code></b></td></tr>\n",
                                mapping->path);
                    }
                    if (mapping->description != NULL)
                    {
                        if (!first_row)
                        {
                            fprintf(f, lalt ? "<tr class=\"alt\">" : "<tr>");
                        }
                        else
                        {
                            first_row = 0;
                        }
                        fputs("<td align=\"right\"><i>description</i></td><td>", f);
                        print_html_string(f, mapping->description, 0);
                        fputs("</td></tr>\n", f);
                    }
                    lalt = !lalt;
                }
            }
            alt = !alt;
        }
        fputs("</table>\n", f);
    }

    print_html_footer(f);

    fclose(f);

    return 0;
}

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
        --n;
    }
}

static void print_padded_string(FILE *f, int column_width, const char *format, ...)
{
    va_list ap;
    va_list ap_copy;
    int length;

    va_start(ap, format);
    va_copy(ap_copy, ap);

    length = vscprintf(format, ap);
    assert(length >= 0);

    vfprintf(f, format, ap_copy);
    if (length < column_width)
    {
        fnputc(column_width - length, ' ', f);
    }

    va_end(ap_copy);
    va_end(ap);
}

static int generate_index_rst(const char *filename, const harp_ingestion_module_register *module_register)
{
    FILE *f;
    int i;

    f = fopen(filename, "w");
    if (f == NULL)
    {
        harp_set_error(HARP_ERROR_FILE_OPEN, "could not open file '%s' for writing", filename);
        return -1;
    }

    fputs("Ingestion definitions\n", f);
    fputs("=====================\n", f);
    fputs("HARP can ingest data from various types of products. The list of supported product types is provided below. "
          "HARP will try to automatically determine the product type of each file that you pass to the ingest "
          "function. An error will be raised if the product type of a file cannot be determined.\n\n", f);
    fputs("For each ingestion, HARP will return a single HARP product. Each variable in a HARP product represents a "
          "specific quantity (e.g. O\\ :sub:`3` number density, cloud fraction, altitude, longitude, latitude, time, "
          "*et cetera*). You can customize which variables you want to include using the ``include()`` and "
          "``exclude()`` actions that can be passed to the ingest function.\n\n", f);
    fputs("Within a HARP product, dimensions of the same type (*time*, *latitude*, *longitude*, *vertical*, "
          "*spectral*) are linked together. This means that, within an ingested product, variables cannot have "
          "dimensions of the same type with different lengths.\n\n", f);
    fputs("For each type of product that contains one or more quantities for which dimensions of the same type have "
          "different lengths, the ingestion will be split into multiple *conversions*. Each conversion only contains "
          "quantities for which the length of each type of dimension is the same. When multiple conversions exist for "
          "a product type, HARP will use the first conversion from the list of available conversions by default.\n\n",
          f);
    fputs("For example, suppose a certain type of product contains both O\\ :sub:`3` and NO\\ :sub:`2` volume mixing "
          "ratios retrieved on different spatial grids. In this case, it is not possible to have a single pair of "
          "*longitude* and *latitude* variables that describes the geolocation information for both retrievals. "
          "Therefore, two different conversions will be made available for this product type, one for the O\\ :sub:`3` "
          "volume mixing ratio, and another for the NO\\ :sub:`2` volume mixing ratio.\n\n", f);
    fputs("For each product type, *ingestion options* may be available. These options can be used, for example, to "
          "switch between different product conversions (usually when quantities defined on different grids are "
          "present within a single product), or to switch between different variants of a quantity. Ingestion options "
          "should be passed to the ingest function as a semi-colon separated string of ``option_name=value`` pairs. "
          "These options are unrelated to *actions* (filtering, inclusion and exclusion of variables, and adding "
          "derived variables). Ingestion options are only meaningful in the context of an ingestion, while actions can "
          "be applied both on-the-fly during ingestion, as well as to existing HARP products.\n\n", f);
    fputs("The list below gives an overview of the conversions and ingestion options available for each product type. "
          "For each conversion, there is a separate page that describes the resulting HARP product. This includes a "
          "list of all the variables, the value type, the dimensions, and the unit of each variable, as well as a full "
          "*mapping description* that details where and how HARP retrieved the data from the input product.\n\n", f);

    fputs("Product types\n", f);
    fputs("-------------\n", f);
    fputs("The table below lists the product types that are supported by HARP.\n\n", f);

    fputs(".. csv-table::\n", f);
    fputs("   :header-rows: 1\n\n", f);
    fputs("   \"HARP product name\", \"CODA product type\", \"description\"\n", f);
    for (i = 0; i < module_register->num_ingestion_modules; i++)
    {
        harp_ingestion_module *ingestion_module = module_register->ingestion_module[i];

        fnputc(3, ' ', f);
        fprintf(f, "\":ref:`%s`\", ", ingestion_module->name);

        fputc('"', f);
        if (ingestion_module->product_class != NULL && ingestion_module->product_type != NULL)
        {
            fprintf(f, "%s/%s", ingestion_module->product_class, ingestion_module->product_type);
        }
        else if (ingestion_module->product_class != NULL)
        {
            fputs(ingestion_module->product_class, f);
        }
        else if (ingestion_module->product_type != NULL)
        {
            fputs(ingestion_module->product_type, f);
        }
        fputc('"', f);
        fputs(", ", f);

        fputc('"', f);
        if (ingestion_module->description != NULL)
        {
            fputs(ingestion_module->description, f);
        }
        fputc('"', f);
        fputc('\n', f);
    }
    fputc('\n', f);

    fputs("Product conversions\n", f);
    fputs("-------------------\n", f);
    fputs("The tables below lists the available product conversions and ingestion options for each product type "
          "supported by HARP.\n\n", f);

    for (i = 0; i < module_register->num_ingestion_modules; i++)
    {
        harp_ingestion_module *ingestion_module = module_register->ingestion_module[i];
        int j;

        fprintf(f, ".. _%s:\n\n", ingestion_module->name);
        fprintf(f, "%s\n", ingestion_module->name);
        fnputc(strlen(ingestion_module->name), '^', f);
        fputc('\n', f);

        if (ingestion_module->description != NULL)
        {
            fprintf(f, "%s\n\n", ingestion_module->description);
        }

        fprintf(f, "The table below lists the available product conversions for ``%s`` products.\n\n",
                ingestion_module->name);
        fputs(".. csv-table::\n", f);
        fputs("   :header-rows: 1\n\n", f);
        fputs("   \"name\", \"ingestion option\", \"description\"\n", f);
        for (j = 0; j < ingestion_module->num_product_definitions; j++)
        {
            harp_product_definition *product_definition = ingestion_module->product_definition[j];

            fnputc(3, ' ', f);
            fprintf(f, "\":doc:`%s`\"", product_definition->name);
            fputs(", ", f);

            fputc('"', f);
            if (product_definition->ingestion_option != NULL)
            {
                fputs(product_definition->ingestion_option, f);
            }
            fputc('"', f);
            fputs(", ", f);

            fputc('"', f);
            if (product_definition->description != NULL)
            {
                fputs(product_definition->description, f);
            }
            fputc('"', f);
            fputc('\n', f);
        }
        fputc('\n', f);

        if (ingestion_module->num_option_definitions > 0)
        {
            fprintf(f, "The table below lists the available ingestion options for ``%s`` products.\n\n",
                    ingestion_module->name);
            fputs(".. csv-table::\n", f);
            fputs("   :widths: 15 25 60\n", f);
            fputs("   :header-rows: 1\n\n", f);
            fputs("   \"option name\", \"legal values\", \"description\"\n", f);

            for (j = 0; j < ingestion_module->num_option_definitions; j++)
            {
                harp_ingestion_option_definition *option_definition = ingestion_module->option_definition[j];

                fnputc(3, ' ', f);
                fputc('"', f);
                fputs(option_definition->name, f);
                fputc('"', f);
                fputs(", ", f);

                fputc('"', f);
                if (option_definition->num_allowed_values > 0)
                {
                    int k;

                    for (k = 0; k < option_definition->num_allowed_values; k++)
                    {
                        if (k == 0)
                        {
                            fprintf(f, "**%s**", option_definition->allowed_value[k]);
                        }
                        else
                        {
                            fputs(option_definition->allowed_value[k], f);
                        }

                        if (k + 1 < option_definition->num_allowed_values)
                        {
                            fputs(", ", f);
                        }
                    }
                }
                fputc('"', f);
                fputs(", ", f);

                fputc('"', f);
                if (option_definition->description != NULL)
                {
                    fputs(option_definition->description, f);
                }
                fputc('"', f);
                fputc('\n', f);
            }
            fputc('\n', f);
        }
    }

    fclose(f);
    return 0;
}

static int generate_definition_rst(const char *filename, const harp_product_definition *product_definition)
{
    FILE *f;
    int i;
    int j;

    f = fopen(filename, "w");
    if (f == NULL)
    {
        harp_set_error(HARP_ERROR_FILE_OPEN, "could not open file '%s' for writing", filename);
        return -1;
    }

    fputs(product_definition->name, f);
    fputc('\n', f);
    fnputc(strlen(product_definition->name), '=', f);
    fnputc(2, '\n', f);

    fputs("Variables\n", f);
    fputs("---------\n", f);
    fprintf(f, "The table below lists the variables that are present in the HARP product that results from an "
            "ingestion of ``%s`` data.\n\n", product_definition->name);

    fputs(".. csv-table::\n", f);
    fputs("   :widths: 25 5 15 15 40\n", f);
    fputs("   :header-rows: 1\n\n", f);
    fputs("   \"field name\", \"type\", \"dimensions\", \"unit\", \"description\"\n", f);
    for (i = 0; i < product_definition->num_variable_definitions; i++)
    {
        harp_variable_definition *variable_definition = product_definition->variable_definition[i];

        fnputc(3, ' ', f);
        fputc('"', f);
        fprintf(f, "**%s**", variable_definition->name);
        fputc('"', f);
        fputs(", ", f);

        fputc('"', f);
        fputs(harp_get_data_type_name(variable_definition->data_type), f);
        fputc('"', f);
        fputs(", ", f);

        fputc('"', f);
        if (variable_definition->num_dimensions > 0)
        {
            fputc('{', f);
            for (j = 0; j < variable_definition->num_dimensions; j++)
            {
                if (j > 0)
                {
                    fputs(", ", f);
                }
                if (variable_definition->dimension_type[j] == harp_dimension_independent)
                {
                    fprintf(f, "%ld", variable_definition->dimension[j]);
                }
                else
                {
                    fprintf(f, "*%s*", harp_get_dimension_type_name(variable_definition->dimension_type[j]));
                }
            }
            fputc('}', f);
        }
        fputc('"', f);
        fputs(", ", f);

        fputc('"', f);
        if (variable_definition->unit != NULL && strlen(variable_definition->unit) > 0)
        {
            fputs(variable_definition->unit, f);
        }
        fputc('"', f);
        fputs(", ", f);

        fputc('"', f);
        if (variable_definition->description != NULL)
        {
            fputs(variable_definition->description, f);
        }
        fputc('"', f);
        fputc('\n', f);
    }

    if (product_definition_has_mapping_description(product_definition))
    {
        int column_width[3] = { 0 };
        int span_width;

        fputc('\n', f);
        fputs("Mapping description\n", f);
        fputs("-------------------\n", f);
        fputs("The table below details where and how each variable was retrieved from the input product.\n\n", f);
        if (product_definition->mapping_description != NULL)
        {
            fprintf(f, "%s\n\n", product_definition->mapping_description);
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

        fputc('+', f);
        fnputc(column_width[0] + 2, '-', f);
        fputc('+', f);
        fnputc(span_width + 2, '-', f);
        fputc('+', f);
        fputc('\n', f);

        fputs("| ", f);
        print_padded_string(f, column_width[0], "%s", "field name");
        fputs(" | ", f);
        print_padded_string(f, span_width, "%s", "mapping description");
        fputs(" |\n", f);

        fputc('+', f);
        fnputc(column_width[0] + 2, '=', f);
        fputc('+', f);
        fnputc(column_width[1] + 2, '=', f);
        fputc('+', f);
        fnputc(column_width[2] + 2, '=', f);
        fputc('+', f);
        fputc('\n', f);

        for (i = 0; i < product_definition->num_variable_definitions; i++)
        {
            harp_variable_definition *variable_definition = product_definition->variable_definition[i];
            int first_row = 1;

            if (variable_definition->num_mappings == 0 && variable_definition->exclude == NULL)
            {
                continue;
            }

            fputs("| ", f);
            print_padded_string(f, column_width[0], "**%s**", variable_definition->name);
            fputs(" | ", f);

            if (variable_definition->exclude != NULL)
            {
                print_padded_string(f, column_width[1], "%s", "*available*");
                fputs(" | ", f);
                print_padded_string(f, column_width[2], "%s", "optional");
                fputs(" |\n", f);

                first_row = 0;
            }

            for (j = 0; j < variable_definition->num_mappings; j++)
            {
                harp_mapping_description *mapping = variable_definition->mapping[j];

                if (mapping->ingestion_option != NULL || mapping->condition != NULL)
                {
                    if (!first_row)
                    {
                        fputc('+', f);
                        fnputc(column_width[0] + 2, ' ', f);
                        fputc('+', f);
                        fnputc(column_width[1] + 2, '-', f);
                        fputc('+', f);
                        fnputc(column_width[2] + 2, '-', f);
                        fputc('+', f);
                        fputc('\n', f);

                        fputs("| ", f);
                        fnputc(column_width[0], ' ', f);
                        fputs(" | ", f);
                    }
                    else
                    {
                        first_row = 0;
                    }

                    print_padded_string(f, column_width[1], "%s", "*condition*");
                    fputs(" | ", f);
                    if (mapping->ingestion_option != NULL && mapping->condition != NULL)
                    {
                        print_padded_string(f, column_width[2], "%s and %s", mapping->ingestion_option,
                                            mapping->condition);
                    }
                    else if (mapping->ingestion_option != NULL)
                    {
                        print_padded_string(f, column_width[2], "%s", mapping->ingestion_option);
                    }
                    else
                    {
                        print_padded_string(f, column_width[2], "%s", mapping->condition);
                    }
                    fputs(" |\n", f);
                }
                if (mapping->path != NULL)
                {
                    if (!first_row)
                    {
                        fputc('+', f);
                        fnputc(column_width[0] + 2, ' ', f);
                        fputc('+', f);
                        fnputc(column_width[1] + 2, '-', f);
                        fputc('+', f);
                        fnputc(column_width[2] + 2, '-', f);
                        fputc('+', f);
                        fputc('\n', f);

                        fputs("| ", f);
                        fnputc(column_width[0], ' ', f);
                        fputs(" | ", f);
                    }
                    else
                    {
                        first_row = 0;
                    }

                    print_padded_string(f, column_width[1], "%s", "*path*");
                    fputs(" | ", f);
                    print_padded_string(f, column_width[2], "**%s**", mapping->path);
                    fputs(" |\n", f);
                }
                if (mapping->description != NULL)
                {
                    if (!first_row)
                    {
                        fputc('+', f);
                        fnputc(column_width[0] + 2, ' ', f);
                        fputc('+', f);
                        fnputc(column_width[1] + 2, '-', f);
                        fputc('+', f);
                        fnputc(column_width[2] + 2, '-', f);
                        fputc('+', f);
                        fputc('\n', f);

                        fputs("| ", f);
                        fnputc(column_width[0], ' ', f);
                        fputs(" | ", f);
                    }
                    else
                    {
                        first_row = 0;
                    }

                    print_padded_string(f, column_width[1], "%s", "*description*");
                    fputs(" | ", f);
                    print_padded_string(f, column_width[2], "%s", mapping->description);
                    fputs(" |\n", f);
                }
            }

            fputc('+', f);
            fnputc(column_width[0] + 2, '-', f);
            fputc('+', f);
            fnputc(column_width[1] + 2, '-', f);
            fputc('+', f);
            fnputc(column_width[2] + 2, '-', f);
            fputc('+', f);
            fputc('\n', f);
        }
    }

    fclose(f);
    return 0;
}

static int generate_html(const char *path)
{
    harp_ingestion_module_register *module_register;
    char *filename;
    int i, j;

    if (harp_ingestion_init() != 0)
    {
        return -1;
    }

    module_register = harp_ingestion_get_module_register();
    assert(module_register != NULL);

    filename = (char *)malloc(scprintf("%s/index.html", path) + 1);
    if (filename == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate % lu bytes) (%s:%u)",
                       scprintf("%s/index.html", path) + 1, __FILE__, __LINE__);
        return -1;
    }

    sprintf(filename, "%s/index.html", path);
    if (generate_index_html(filename, module_register) != 0)
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

            filename = (char *)malloc(scprintf("%s/%s.html", path, product_definition->name) + 1);
            if (filename == NULL)
            {
                harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate % lu bytes) (%s:%u)",
                               scprintf("%s/%s.html", path, product_definition->name) + 1, __FILE__, __LINE__);
                return -1;
            }

            sprintf(filename, "%s/%s.html", path, product_definition->name);
            if (generate_definition_html(filename, product_definition) != 0)
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

static int generate_rst(const char *path)
{
    harp_ingestion_module_register *module_register;
    char *filename;
    int i, j;

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
    if (generate_index_rst(filename, module_register) != 0)
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
            if (generate_definition_rst(filename, product_definition) != 0)
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

int harp_doc_export_ingestion_definitions(const char *path, const char *format)
{
    if (path == NULL)
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "path is NULL (%s:%u)", __FILE__, __LINE__);
        return -1;
    }

    if (format == NULL)
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "format is NULL (%s:%u)", __FILE__, __LINE__);
        return -1;
    }

    if (strcmp(format, "html") == 0)
    {
        if (generate_html(path) != 0)
        {
            return -1;
        }
    }
    else if (strcmp(format, "rst") == 0)
    {
        if (generate_rst(path) != 0)
        {
            return -1;
        }
    }
    else
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "unknown output format '%s' (%s:%u)", format, __FILE__, __LINE__);
        return -1;
    }

    return 0;
}

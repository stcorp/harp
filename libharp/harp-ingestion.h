/*
 * Copyright (C) 2015-2016 S[&]T, The Netherlands.
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

#ifndef HARP_INGESTION_H
#define HARP_INGESTION_H

#include "harp-internal.h"
#include "harp-operation.h"
#include "coda.h"

typedef struct harp_ingestion_option_struct
{
    char *name;
    char *value;
} harp_ingestion_option;

typedef struct harp_ingestion_options_struct
{
    int num_options;
    harp_ingestion_option **option;
} harp_ingestion_options;

typedef struct harp_ingestion_option_definition_struct
{
    char *name;
    char *description;
    int num_allowed_values;
    char **allowed_value;
} harp_ingestion_option_definition;

typedef struct harp_mapping_description_struct
{
    char *ingestion_option;
    char *condition;
    char *path;
    char *description;
} harp_mapping_description;

typedef struct harp_variable_definition_struct
{
    char *name;
    harp_data_type data_type;

    int num_dimensions;
    harp_dimension_type dimension_type[HARP_MAX_NUM_DIMS];
    long dimension[HARP_MAX_NUM_DIMS];

    char *description;
    char *unit;
    harp_scalar valid_min;
    harp_scalar valid_max;

    int (*exclude) (void *user_data);
    int (*read_all) (void *user_data, harp_array data);
    int (*read_range) (void *user_data, long index_offset, long index_length, harp_array data);
    int (*get_max_range) (void *user_data);
    int (*read_sample) (void *user_data, long index, harp_array data);

    int num_mappings;
    harp_mapping_description **mapping;
} harp_variable_definition;

typedef struct harp_product_definition_struct
{
    char *name;
    char *description;

    int num_variable_definitions;
    harp_variable_definition **variable_definition;
    struct hashtable_struct *variable_definition_hash_data;

    int (*read_dimensions) (void *user_data, long dimension[HARP_NUM_DIM_TYPES]);

    char *ingestion_option;
    char *mapping_description;
} harp_product_definition;

/* Forward typedef to allow the use of harp_ingestion_module in the declaration of struct harp_ingestion_module_struct
   below.
*/
typedef struct harp_ingestion_module_struct harp_ingestion_module;

struct harp_ingestion_module_struct
{
    char *name;
    char *product_class;
    char *product_type;
    char *product_group;
    char *description;

    int num_product_definitions;
    harp_product_definition **product_definition;

    int num_option_definitions;
    harp_ingestion_option_definition **option_definition;

    int (*verify_product_type_coda) (const harp_ingestion_module *module, coda_product *product);
    int (*verify_product_type_custom) (const harp_ingestion_module *module, const char *filename);
    int (*ingestion_init_coda) (const harp_ingestion_module *module, coda_product *product,
                                const harp_ingestion_options *options, harp_product_definition **definition,
                                void **user_data);
    int (*ingestion_init_custom) (const harp_ingestion_module *module, const char *filename,
                                  const harp_ingestion_options *options, harp_product_definition **definition,
                                  void **user_data);
    void (*ingestion_done) (void *user_data);
};

typedef struct harp_ingestion_module_register_struct
{
    int num_ingestion_modules;
    harp_ingestion_module **ingestion_module;
} harp_ingestion_module_register;

/* Ingestion options. */
int harp_ingestion_options_new(harp_ingestion_options **new_options);
int harp_ingestion_options_copy(const harp_ingestion_options *other_options, harp_ingestion_options **new_options);
void harp_ingestion_options_delete(harp_ingestion_options *options);

int harp_ingestion_options_has_option(const harp_ingestion_options *options, const char *name);
int harp_ingestion_options_get_option(const harp_ingestion_options *options, const char *name, const char **value);
int harp_ingestion_options_set_option(harp_ingestion_options *options, const char *name, const char *value);
int harp_ingestion_options_remove_option(harp_ingestion_options *options, const char *name);

int harp_ingestion_options_from_string(const char *str, harp_ingestion_options **new_options);
int harp_ingestion_options_set_option_from_string(harp_ingestion_options *options, const char *str);

/* Variable definition. */
void harp_variable_definition_add_mapping(harp_variable_definition *variable_definition, const char *ingestion_option,
                                          const char *condition, const char *path, const char *description);
void harp_variable_definition_set_valid_range_int8(harp_variable_definition *variable_definition, int8_t valid_min,
                                                   int8_t valid_max);
void harp_variable_definition_set_valid_range_int16(harp_variable_definition *variable_definition, int16_t valid_min,
                                                    int16_t valid_max);
void harp_variable_definition_set_valid_range_int32(harp_variable_definition *variable_definition, int32_t valid_min,
                                                    int32_t valid_max);
void harp_variable_definition_set_valid_range_float(harp_variable_definition *variable_definition, float valid_min,
                                                    float valid_max);
void harp_variable_definition_set_valid_range_double(harp_variable_definition *variable_definition, double valid_min,
                                                     double valid_max);

int harp_variable_definition_has_dimension_types(const harp_variable_definition *variable_definition,
                                                 int num_dimensions, const harp_dimension_type *dimension_type);
int harp_variable_definition_has_dimension_type(const harp_variable_definition *variable_definition,
                                                harp_dimension_type dimension_type);
int harp_variable_definition_exclude(const harp_variable_definition *variable_definition, void *user_data);

/* Product definition. */
void harp_product_definition_add_mapping(harp_product_definition *product_definition, const char *mapping_description,
                                         const char *ingestion_option);
int harp_product_definition_has_dimension_type(const harp_product_definition *product_definition,
                                               harp_dimension_type dimension_type);
int harp_product_definition_has_variable(const harp_product_definition *product_definition, const char *name);
harp_variable_definition *harp_product_definition_find_variable(const harp_product_definition *product_definition,
                                                                const char *name);
int harp_product_definition_get_variable_index(const harp_product_definition *product_definition, const char *name);

/* Ingestion module. */
int harp_ingestion_module_validate_options(harp_ingestion_module *module, const harp_ingestion_options *options);

/* Module register. */
int harp_ingestion_find_module(const char *filename, harp_ingestion_module **module, coda_product **product);
harp_ingestion_module_register *harp_ingestion_get_module_register(void);

/* Convenience functions. */
harp_ingestion_module *harp_ingestion_register_module_coda
    (const char *name, const char *product_group, const char *product_class, const char *product_type,
     const char *description,
     int (*verify_product_type) (const harp_ingestion_module *module, coda_product *product),
     int (*ingestion_init) (const harp_ingestion_module *module, coda_product *product,
                            const harp_ingestion_options *options, harp_product_definition **definition,
                            void **user_data), void (*ingestion_done) (void *user_data));
harp_ingestion_module *harp_ingestion_register_module_custom
    (const char *name, const char *product_group, const char *description,
     int (*verify_product_type) (const harp_ingestion_module *module, const char *filename),
     int (*ingestion_init) (const harp_ingestion_module *module, const char *filename,
                            const harp_ingestion_options *options, harp_product_definition **definition,
                            void **user_data), void (*ingestion_done) (void *user_data));
harp_ingestion_option_definition *harp_ingestion_register_option(harp_ingestion_module *module, const char *name,
                                                                 const char *description, int num_allowed_values,
                                                                 const char *allowed_value[]);
harp_product_definition *harp_ingestion_register_product(harp_ingestion_module *module, const char *name,
                                                         const char *description,
                                                         int (*read_dimensions) (void *user_data,
                                                                                 long dimension[HARP_NUM_DIM_TYPES]));
harp_variable_definition *harp_ingestion_register_variable_full_read(harp_product_definition *product_definition,
                                                                     const char *name, harp_data_type data_type,
                                                                     int num_dimensions,
                                                                     const harp_dimension_type *dimension_type,
                                                                     const long *dimension, const char *description,
                                                                     const char *unit, int (*exclude) (void *user_data),
                                                                     int (*read_all) (void *user_data,
                                                                                      harp_array data));
harp_variable_definition *harp_ingestion_register_variable_range_read(harp_product_definition *product_definition,
                                                                      const char *name, harp_data_type data_type,
                                                                      int num_dimensions,
                                                                      const harp_dimension_type *dimension_type,
                                                                      const long *dimension, const char *description,
                                                                      const char *unit,
                                                                      int (*exclude) (void *user_data),
                                                                      int (*get_max_range) (void *user_data),
                                                                      int (*read_range) (void *user_data,
                                                                                         long index_offset,
                                                                                         long index_length,
                                                                                         harp_array data));
harp_variable_definition *harp_ingestion_register_variable_sample_read(harp_product_definition *product_definition,
                                                                       const char *name, harp_data_type data_type,
                                                                       int num_dimensions,
                                                                       const harp_dimension_type *dimension_type,
                                                                       const long *dimension, const char *description,
                                                                       const char *unit,
                                                                       int (*exclude) (void *user_data),
                                                                       int (*read_sample) (void *user_data, long index,
                                                                                           harp_array data));

/* Initialization and clean-up. */
int harp_ingestion_init(void);
void harp_ingestion_done(void);

#endif

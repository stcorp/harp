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

#ifndef HARP_INGESTION_H
#define HARP_INGESTION_H

#include "harp-internal.h"
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
    int num_enum_values;
    char **enum_name;

    int (*exclude) (void *user_data);
    int (*read_all) (void *user_data, harp_array data);
    int (*read_range) (void *user_data, long index_offset, long index_length, harp_array data);
    long (*get_max_range) (void *user_data);
    int (*read_block) (void *user_data, long index, harp_array data);

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

    int (*verify_product_type) (const harp_ingestion_module *module, const char *filename);
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
void harp_variable_definition_set_enumeration_values(harp_variable_definition *variable_definition, int num_enum_values,
                                                     const char **enum_name);

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
                                                                      long (*get_max_range) (void *user_data),
                                                                      int (*read_range) (void *user_data,
                                                                                         long index_offset,
                                                                                         long index_length,
                                                                                         harp_array data));
harp_variable_definition *harp_ingestion_register_variable_block_read(harp_product_definition *product_definition,
                                                                      const char *name, harp_data_type data_type,
                                                                      int num_dimensions,
                                                                      const harp_dimension_type *dimension_type,
                                                                      const long *dimension, const char *description,
                                                                      const char *unit,
                                                                      int (*exclude) (void *user_data),
                                                                      int (*read_block) (void *user_data, long index,
                                                                                         harp_array data));

/* Initialization and clean-up. */
int harp_ingestion_init(void);

#endif

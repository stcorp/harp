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
#include "coda.h"
#include "hashtable.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

/* Module register. */
static harp_ingestion_module_register *module_register = NULL;

/* Module initialization functions (forward declarations). */
int harp_ingestion_module_aeolus_l1b_init(void);
int harp_ingestion_module_aeolus_l2a_init(void);
int harp_ingestion_module_aeolus_l2b_init(void);
int harp_ingestion_module_cci_l2_aerosol_init(void);
int harp_ingestion_module_cci_l2_ghg_init(void);
int harp_ingestion_module_cci_l2_o3_lp_init(void);
int harp_ingestion_module_cci_l2_o3_np_init(void);
int harp_ingestion_module_cci_l2_o3_tc_init(void);
int harp_ingestion_module_cci_l3_aerosol_init(void);
int harp_ingestion_module_cci_l3_cloud_init(void);
int harp_ingestion_module_cci_l3_ghg_init(void);
int harp_ingestion_module_cci_l3_o3_lp_init(void);
int harp_ingestion_module_cci_l3_o3_np_init(void);
int harp_ingestion_module_cci_l3_o3_tc_init(void);
int harp_ingestion_module_cci_l4_o3_np_init(void);
int harp_ingestion_module_ecmwf_grib_init(void);
int harp_ingestion_module_geoms_mwr_init(void);
int harp_ingestion_module_geoms_lidar_init(void);
int harp_ingestion_module_geoms_ftir_init(void);
int harp_ingestion_module_geoms_uvvis_doas_init(void);
int harp_ingestion_module_gome_l1_init(void);
int harp_ingestion_module_gome_l2_init(void);
int harp_ingestion_module_gome2_l1_init(void);
int harp_ingestion_module_gome2_l2_init(void);
int harp_ingestion_module_gomos_l1_init(void);
int harp_ingestion_module_gomos_l2_init(void);
int harp_ingestion_module_gosat_fts_l1b_init(void);
int harp_ingestion_module_gosat_fts_l2_init(void);
int harp_ingestion_module_hirdls_l2_init(void);
int harp_ingestion_module_iasi_l1_init(void);
int harp_ingestion_module_iasi_l2_init(void);
int harp_ingestion_module_mipas_l1_init(void);
int harp_ingestion_module_mipas_l2_init(void);
int harp_ingestion_module_mls_l2_init(void);
int harp_ingestion_module_npp_suomi_crimss_l2_init(void);
int harp_ingestion_module_npp_suomi_omps_profiles_l2_init(void);
int harp_ingestion_module_npp_suomi_omps_totals_l2_init(void);
int harp_ingestion_module_npp_suomi_viirs_l2_init(void);
int harp_ingestion_module_omi_l2_init(void);
int harp_ingestion_module_omi_l3_init(void);
int harp_ingestion_module_osiris_l2_init(void);
int harp_ingestion_module_qa4ecv_init(void);
int harp_ingestion_module_sciamachy_l1_init(void);
int harp_ingestion_module_sciamachy_l2_init(void);
int harp_ingestion_module_s5p_l1b_init(void);
int harp_ingestion_module_s5p_l2_init(void);
int harp_ingestion_module_smr_l2_init(void);
int harp_ingestion_module_temis_init(void);
int harp_ingestion_module_tes_l2_init(void);

/* Module initialization functions. */
typedef int (module_init_func_t) (void);

static module_init_func_t *module_init_func[] = {
    harp_ingestion_module_aeolus_l1b_init,
    harp_ingestion_module_aeolus_l2a_init,
    harp_ingestion_module_aeolus_l2b_init,
    harp_ingestion_module_cci_l2_aerosol_init,
    harp_ingestion_module_cci_l2_ghg_init,
    harp_ingestion_module_cci_l2_o3_lp_init,
    harp_ingestion_module_cci_l2_o3_np_init,
    harp_ingestion_module_cci_l2_o3_tc_init,
    harp_ingestion_module_cci_l3_aerosol_init,
    harp_ingestion_module_cci_l3_cloud_init,
    harp_ingestion_module_cci_l3_ghg_init,
    harp_ingestion_module_cci_l3_o3_lp_init,
    harp_ingestion_module_cci_l3_o3_np_init,
    harp_ingestion_module_cci_l3_o3_tc_init,
    harp_ingestion_module_cci_l4_o3_np_init,
    harp_ingestion_module_ecmwf_grib_init,
    harp_ingestion_module_geoms_mwr_init,
    harp_ingestion_module_geoms_lidar_init,
    harp_ingestion_module_geoms_ftir_init,
    harp_ingestion_module_geoms_uvvis_doas_init,
    harp_ingestion_module_gome_l1_init,
    harp_ingestion_module_gome_l2_init,
    harp_ingestion_module_gome2_l1_init,
    harp_ingestion_module_gome2_l2_init,
    harp_ingestion_module_gomos_l1_init,
    harp_ingestion_module_gomos_l2_init,
    harp_ingestion_module_gosat_fts_l1b_init,
    harp_ingestion_module_gosat_fts_l2_init,
    harp_ingestion_module_hirdls_l2_init,
    harp_ingestion_module_iasi_l1_init,
    harp_ingestion_module_iasi_l2_init,
    harp_ingestion_module_mipas_l1_init,
    harp_ingestion_module_mipas_l2_init,
    harp_ingestion_module_mls_l2_init,
    harp_ingestion_module_npp_suomi_crimss_l2_init,
    harp_ingestion_module_npp_suomi_omps_profiles_l2_init,
    harp_ingestion_module_npp_suomi_omps_totals_l2_init,
    harp_ingestion_module_npp_suomi_viirs_l2_init,
    harp_ingestion_module_omi_l2_init,
    harp_ingestion_module_omi_l3_init,
    harp_ingestion_module_osiris_l2_init,
    harp_ingestion_module_qa4ecv_init,
    harp_ingestion_module_sciamachy_l1_init,
    harp_ingestion_module_sciamachy_l2_init,
    harp_ingestion_module_s5p_l1b_init,
    harp_ingestion_module_s5p_l2_init,
    harp_ingestion_module_smr_l2_init,
    harp_ingestion_module_temis_init,
    harp_ingestion_module_tes_l2_init
};

#define NUM_INGESTION_MODULES ((long)(sizeof(module_init_func)/sizeof(module_init_func[0])))

/* Forward declarations. */
static void ingestion_option_definition_delete(harp_ingestion_option_definition *ingestion_option_definition);
static void variable_definition_delete(harp_variable_definition *variable_definition);
static void product_definition_delete(harp_product_definition *product_definition);

static harp_mapping_description *mapping_description_new(const char *ingestion_option, const char *condition,
                                                         const char *path, const char *description)
{
    harp_mapping_description *mapping;

    mapping = malloc(sizeof(harp_mapping_description));
    if (mapping == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       sizeof(harp_mapping_description), __FILE__, __LINE__);
        return NULL;
    }

    mapping->ingestion_option = NULL;
    mapping->condition = NULL;
    mapping->path = NULL;
    mapping->description = NULL;

    if (ingestion_option != NULL)
    {
        mapping->ingestion_option = strdup(ingestion_option);
        assert(mapping->ingestion_option != NULL);
    }
    if (condition != NULL)
    {
        mapping->condition = strdup(condition);
        assert(mapping->condition != NULL);
    }
    if (path != NULL)
    {
        mapping->path = strdup(path);
        assert(mapping->path != NULL);
    }
    if (description != NULL)
    {
        mapping->description = strdup(description);
        assert(mapping->description != NULL);
    }

    return mapping;
}

static void mapping_description_delete(harp_mapping_description *mapping)
{
    if (mapping != NULL)
    {
        if (mapping->ingestion_option != NULL)
        {
            free(mapping->ingestion_option);
        }
        if (mapping->condition != NULL)
        {
            free(mapping->condition);
        }
        if (mapping->path != NULL)
        {
            free(mapping->path);
        }
        if (mapping->description != NULL)
        {
            free(mapping->description);
        }
        free(mapping);
    }
}

static int variable_definition_new(const char *name, harp_data_type data_type, int num_dimensions,
                                   const harp_dimension_type *dimension_type, const long *dimension,
                                   const char *description, const char *unit, int (*exclude) (void *user_data),
                                   int (*read_all) (void *user_data, harp_array data),
                                   int (*read_range) (void *user_data, long index_offset, long index_length,
                                                      harp_array data),
                                   long (*get_max_range) (void *user_data),
                                   int (*read_sample) (void *user_data, long index, harp_array data),
                                   harp_variable_definition **new_variable_definition)
{
    harp_variable_definition *variable_definition;
    int i;

    assert(name != NULL);
    assert(num_dimensions >= 0 && num_dimensions <= HARP_MAX_NUM_DIMS);
    assert(num_dimensions == 0 || dimension_type != NULL);
    assert(unit == NULL || harp_unit_is_valid(unit));
    assert(read_all != NULL || read_range != NULL || read_sample != NULL);

    /* strings can only be read using read_all and read_range when there is no sample dimension */
    assert(read_sample != NULL || data_type != harp_type_string ||
           num_dimensions == 0 || dimension_type[0] != harp_dimension_time);

    /* read_range and get_max_range need to be set or unset both */
    assert((read_range != NULL && get_max_range != NULL) || (read_range == NULL && get_max_range == NULL));

    variable_definition = malloc(sizeof(harp_variable_definition));
    if (variable_definition == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       sizeof(harp_variable_definition), __FILE__, __LINE__);
        return -1;
    }

    variable_definition->name = strdup(name);
    assert(variable_definition->name != NULL);
    variable_definition->data_type = data_type;

    variable_definition->num_dimensions = num_dimensions;
    for (i = 0; i < num_dimensions; i++)
    {
        variable_definition->dimension_type[i] = dimension_type[i];

        if (dimension_type[i] == harp_dimension_independent)
        {
            assert(dimension != NULL && dimension[i] >= 0);
            variable_definition->dimension[i] = dimension[i];
        }
        else
        {
            assert(dimension == NULL || dimension[i] == -1);
            variable_definition->dimension[i] = -1;
        }
    }

    variable_definition->description = NULL;
    if (description != NULL)
    {
        variable_definition->description = strdup(description);
        assert(variable_definition->description != NULL);
    }
    variable_definition->unit = NULL;
    if (unit != NULL)
    {
        variable_definition->unit = strdup(unit);
        assert(variable_definition->unit != NULL);
    }
    if (data_type != harp_type_string)
    {
        variable_definition->valid_min = harp_get_valid_min_for_type(data_type);
        variable_definition->valid_max = harp_get_valid_max_for_type(data_type);
    }

    variable_definition->exclude = exclude;
    variable_definition->read_all = read_all;
    variable_definition->read_range = read_range;
    variable_definition->get_max_range = get_max_range;
    variable_definition->read_sample = read_sample;

    variable_definition->num_mappings = 0;
    variable_definition->mapping = NULL;

    *new_variable_definition = variable_definition;
    return 0;
}

static void variable_definition_delete(harp_variable_definition *variable_definition)
{
    if (variable_definition != NULL)
    {
        free(variable_definition->name);
        if (variable_definition->description != NULL)
        {
            free(variable_definition->description);
        }
        if (variable_definition->unit != NULL)
        {
            free(variable_definition->unit);
        }
        if (variable_definition->mapping != NULL)
        {
            int i;

            for (i = 0; i < variable_definition->num_mappings; i++)
            {
                mapping_description_delete(variable_definition->mapping[i]);
            }
            free(variable_definition->mapping);
        }
        free(variable_definition);
    }
}

static int product_definition_new(const char *name, const char *description,
                                  int (*read_dimensions) (void *user_data, long dimension[HARP_NUM_DIM_TYPES]),
                                  harp_product_definition **new_product_definition)
{
    harp_product_definition *product_definition;

    assert(name != NULL);
    assert(read_dimensions != NULL);

    product_definition = (harp_product_definition *)malloc(sizeof(harp_product_definition));
    if (product_definition == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       sizeof(harp_product_definition), __FILE__, __LINE__);
        return -1;
    }

    product_definition->name = strdup(name);
    assert(product_definition->name != NULL);
    product_definition->description = NULL;
    if (description != NULL)
    {
        product_definition->description = strdup(description);
        assert(product_definition->description != NULL);
    }
    product_definition->num_variable_definitions = 0;
    product_definition->variable_definition = NULL;
    product_definition->variable_definition_hash_data = NULL;
    product_definition->read_dimensions = read_dimensions;
    product_definition->ingestion_option = NULL;
    product_definition->mapping_description = NULL;

    product_definition->variable_definition_hash_data = hashtable_new(1);
    if (product_definition->variable_definition_hash_data == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate hashtable) (%s:%u)", __FILE__,
                       __LINE__);
        product_definition_delete(product_definition);
        return -1;
    }

    *new_product_definition = product_definition;
    return 0;
}

static void product_definition_delete(harp_product_definition *product_definition)
{
    if (product_definition != NULL)
    {
        free(product_definition->name);
        if (product_definition->description != NULL)
        {
            free(product_definition->description);
        }
        hashtable_delete(product_definition->variable_definition_hash_data);
        if (product_definition->variable_definition != NULL)
        {
            int i;

            for (i = 0; i < product_definition->num_variable_definitions; i++)
            {
                variable_definition_delete(product_definition->variable_definition[i]);
            }

            free(product_definition->variable_definition);
        }
        if (product_definition->mapping_description != NULL)
        {
            free(product_definition->mapping_description);
        }
        if (product_definition->ingestion_option != NULL)
        {
            free(product_definition->ingestion_option);
        }

        free(product_definition);
    }
}

static int product_definition_add_variable(harp_product_definition *product_definition,
                                           harp_variable_definition *variable)
{
    assert(product_definition != NULL);
    assert(variable != NULL);
    assert(!harp_product_definition_has_variable(product_definition, variable->name));

    if (product_definition->num_variable_definitions % BLOCK_SIZE == 0)
    {
        harp_variable_definition **new_variable_definition;
        size_t new_size;

        new_size = (product_definition->num_variable_definitions + BLOCK_SIZE) * sizeof(harp_variable_definition *);
        new_variable_definition = (harp_variable_definition **)realloc(product_definition->variable_definition,
                                                                       new_size);
        if (new_variable_definition == NULL)
        {
            harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)", new_size,
                           __FILE__, __LINE__);
            return -1;

        }
        product_definition->variable_definition = new_variable_definition;
    }
    product_definition->variable_definition[product_definition->num_variable_definitions] = variable;
    product_definition->num_variable_definitions++;

    if (hashtable_add_name(product_definition->variable_definition_hash_data, variable->name) != 0)
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "variable '%s' already exists in product conversion definition",
                       variable->name);
        return -1;
    }

    return 0;
}

static int ingestion_option_definition_new(const char *name, const char *description, int num_allowed_values,
                                           const char *allowed_value[],
                                           harp_ingestion_option_definition **new_option_definition)
{
    harp_ingestion_option_definition *option_definition;
    int i;

    assert(name != NULL);
    option_definition = (harp_ingestion_option_definition *)malloc(sizeof(harp_ingestion_option_definition));
    if (option_definition == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       sizeof(harp_ingestion_option_definition), __FILE__, __LINE__);
        return -1;
    }

    option_definition->name = strdup(name);
    option_definition->description = NULL;
    if (description != NULL)
    {
        option_definition->description = strdup(description);
        assert(option_definition->description != NULL);
    }
    option_definition->num_allowed_values = num_allowed_values;
    option_definition->allowed_value = NULL;

    option_definition->allowed_value = malloc(num_allowed_values * sizeof(char *));
    if (option_definition->allowed_value == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       num_allowed_values * sizeof(char *), __FILE__, __LINE__);
        ingestion_option_definition_delete(option_definition);
        return -1;
    }
    for (i = 0; i < num_allowed_values; i++)
    {
        option_definition->allowed_value[i] = strdup(allowed_value[i]);
        assert(option_definition->allowed_value[i] != NULL);
    }

    *new_option_definition = option_definition;
    return 0;
}

static void ingestion_option_definition_delete(harp_ingestion_option_definition *option_definition)
{
    if (option_definition != NULL)
    {
        free(option_definition->name);
        if (option_definition->description != NULL)
        {
            free(option_definition->description);
        }
        if (option_definition->allowed_value != NULL)
        {
            int i;

            for (i = 0; i < option_definition->num_allowed_values; i++)
            {
                if (option_definition->allowed_value[i] != NULL)
                {
                    free(option_definition->allowed_value[i]);
                }
            }

            free(option_definition->allowed_value);
        }
        free(option_definition);
    }
}

static int ingestion_module_new(const char *name, const char *product_group, const char *product_class,
                                const char *product_type, const char *description,
                                int (*ingestion_init_coda) (const harp_ingestion_module *module, coda_product *product,
                                                            const harp_ingestion_options *options,
                                                            harp_product_definition **definition, void **user_data),
                                int (*verify_product_type) (const harp_ingestion_module *module, const char *filename),
                                int (*ingestion_init_custom) (const harp_ingestion_module *module, const char *filename,
                                                              const harp_ingestion_options *options,
                                                              harp_product_definition **definition, void **user_data),
                                void (*ingestion_done) (void *user_data), harp_ingestion_module **new_module)
{
    harp_ingestion_module *module;

    assert(name != NULL);
    assert(product_group != NULL);
    assert((ingestion_init_coda != NULL) != (ingestion_init_custom != NULL));
    assert((ingestion_init_coda != NULL) == (product_class != NULL && product_type != NULL));
    assert((product_class != NULL) == (product_type != NULL));
    assert((ingestion_init_custom != NULL) == (verify_product_type != NULL));
    assert(ingestion_done != NULL);

    module = (harp_ingestion_module *)malloc(sizeof(harp_ingestion_module));
    if (module == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       sizeof(harp_ingestion_module), __FILE__, __LINE__);
        return -1;
    }

    module->name = strdup(name);
    assert(module->name != NULL);
    module->product_group = strdup(product_group);
    assert(module->product_group != NULL);
    module->product_class = NULL;
    if (product_class != NULL)
    {
        module->product_class = strdup(product_class);
        assert(module->product_class != NULL);
    }
    module->product_type = NULL;
    if (product_type != NULL)
    {
        module->product_type = strdup(product_type);
        assert(module->product_type != NULL);
    }
    module->description = NULL;
    if (description != NULL)
    {
        module->description = strdup(description);
        assert(module->description != NULL);
    }
    module->num_product_definitions = 0;
    module->product_definition = NULL;
    module->num_option_definitions = 0;
    module->option_definition = NULL;
    module->ingestion_init_coda = ingestion_init_coda;
    module->verify_product_type = verify_product_type;
    module->ingestion_init_custom = ingestion_init_custom;
    module->ingestion_done = ingestion_done;

    *new_module = module;
    return 0;
}

static void ingestion_module_delete(harp_ingestion_module *module)
{
    if (module != NULL)
    {
        free(module->name);
        if (module->product_group != NULL)
        {
            free(module->product_group);
        }
        if (module->product_class != NULL)
        {
            free(module->product_class);
        }
        if (module->product_type != NULL)
        {
            free(module->product_type);
        }
        if (module->description != NULL)
        {
            free(module->description);
        }
        if (module->product_definition != NULL)
        {
            int i;

            for (i = 0; i < module->num_product_definitions; ++i)
            {
                product_definition_delete(module->product_definition[i]);
            }

            free(module->product_definition);
        }

        if (module->option_definition != NULL)
        {
            int i;

            for (i = 0; i < module->num_option_definitions; i++)
            {
                ingestion_option_definition_delete(module->option_definition[i]);
            }

            free(module->option_definition);
        }

        free(module);
    }
}

static int ingestion_module_get_option_index(const harp_ingestion_module *module, const char *name)
{
    int i;

    for (i = 0; i < module->num_option_definitions; i++)
    {
        if (strcmp(module->option_definition[i]->name, name) == 0)
        {
            return i;
        }
    }

    return -1;
}

static int ingestion_module_has_option(const harp_ingestion_module *module, const char *name)
{
    return (ingestion_module_get_option_index(module, name) >= 0);
}

static int ingestion_module_add_option(harp_ingestion_module *module, harp_ingestion_option_definition *option)
{
    assert(module != NULL);
    assert(option != NULL);
    assert(!ingestion_module_has_option(module, option->name));

    if (module->num_option_definitions % BLOCK_SIZE == 0)
    {
        harp_ingestion_option_definition **new_option_definition;
        size_t new_size;

        new_size = (module->num_option_definitions + BLOCK_SIZE) * sizeof(harp_ingestion_option_definition *);
        new_option_definition = (harp_ingestion_option_definition **)realloc(module->option_definition, new_size);
        if (new_option_definition == NULL)
        {
            harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)", new_size,
                           __FILE__, __LINE__);
            return -1;
        }
        module->option_definition = new_option_definition;
    }
    module->option_definition[module->num_option_definitions++] = option;

    return 0;
}

static int ingestion_module_add_product(harp_ingestion_module *module, harp_product_definition *product)
{
    assert(module != NULL);
    assert(product != NULL);

    if (module->num_product_definitions % BLOCK_SIZE == 0)
    {
        harp_product_definition **new_product_definition;
        size_t new_size;

        new_size = (module->num_product_definitions + BLOCK_SIZE) * sizeof(harp_product_definition *);
        new_product_definition = (harp_product_definition **)realloc(module->product_definition, new_size);
        if (new_product_definition == NULL)
        {
            harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)", new_size,
                           __FILE__, __LINE__);
            return -1;
        }
        module->product_definition = new_product_definition;
    }
    module->product_definition[module->num_product_definitions++] = product;

    return 0;
}

static int ingestion_register_module(harp_ingestion_module *module)
{
    if (module_register == NULL)
    {
        harp_set_error(HARP_ERROR_INGESTION, "ingestion module register unavailable (%s:%u)", __FILE__, __LINE__);
        return -1;
    }

    if (module == NULL)
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "module is NULL (%s:%u)", __FILE__, __LINE__);
        return -1;
    }

    if (module_register->num_ingestion_modules % BLOCK_SIZE == 0)
    {
        harp_ingestion_module **new_ingestion_module;
        size_t new_size;

        new_size = (module_register->num_ingestion_modules + BLOCK_SIZE) * sizeof(harp_ingestion_module *);
        new_ingestion_module = (harp_ingestion_module **)realloc(module_register->ingestion_module, new_size);
        if (new_ingestion_module == NULL)
        {
            harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)", new_size,
                           __FILE__, __LINE__);
            return -1;
        }

        module_register->ingestion_module = new_ingestion_module;
    }

    module_register->ingestion_module[module_register->num_ingestion_modules] = module;
    module_register->num_ingestion_modules++;

    return 0;
}

static int read_index(void *user_data, long index, harp_array data)
{
    (void)user_data;

    assert(index <= INT32_MAX);
    *data.int32_data = (int32_t)index;
    return 0;
}

harp_ingestion_module *harp_ingestion_register_module_coda(const char *name, const char *product_group,
                                                           const char *product_class, const char *product_type,
                                                           const char *description,
                                                           int (*ingestion_init) (const harp_ingestion_module *module,
                                                                                  coda_product *product,
                                                                                  const harp_ingestion_options *options,
                                                                                  harp_product_definition **definition,
                                                                                  void **user_data),
                                                           void (*ingestion_done) (void *user_data))
{
    harp_ingestion_module *module;

    if (ingestion_module_new(name, product_group, product_class, product_type, description, ingestion_init, NULL, NULL,
                             ingestion_done, &module) != 0)
    {
        assert(0);
    }

    if (ingestion_register_module(module) != 0)
    {
        assert(0);
    }

    return module;
}

harp_ingestion_module *harp_ingestion_register_module_custom(const char *name, const char *product_group,
                                                             const char *description,
                                                             int (*verify_product_type) (const harp_ingestion_module
                                                                                         *module, const char *filename),
                                                             int (*ingestion_init) (const harp_ingestion_module *module,
                                                                                    const char *filename,
                                                                                    const harp_ingestion_options
                                                                                    *options,
                                                                                    harp_product_definition
                                                                                    **definition, void **user_data),
                                                             void (*ingestion_done) (void *user_data))
{
    harp_ingestion_module *module;

    if (ingestion_module_new(name, product_group, NULL, NULL, description, NULL, verify_product_type, ingestion_init,
                             ingestion_done, &module) != 0)
    {
        assert(0);
    }

    if (ingestion_register_module(module) != 0)
    {
        assert(0);
    }

    return module;
}

harp_ingestion_option_definition *harp_ingestion_register_option(harp_ingestion_module *module, const char *name,
                                                                 const char *description, int num_allowed_values,
                                                                 const char *allowed_value[])
{
    harp_ingestion_option_definition *option_definition;

    assert(module != NULL);
    if (ingestion_option_definition_new(name, description, num_allowed_values, allowed_value, &option_definition) != 0)
    {
        assert(0);
    }

    if (ingestion_module_add_option(module, option_definition) != 0)
    {
        assert(0);
    }

    return option_definition;
}

harp_product_definition *harp_ingestion_register_product(harp_ingestion_module *module, const char *name,
                                                         const char *description,
                                                         int (*read_dimensions) (void *user_data,
                                                                                 long dimension[HARP_NUM_DIM_TYPES]))
{
    harp_product_definition *product_definition;

    assert(module != NULL);
    if (product_definition_new(name, description, read_dimensions, &product_definition) != 0)
    {
        assert(0);
    }

    if (ingestion_module_add_product(module, product_definition) != 0)
    {
        assert(0);
    }

    return product_definition;
}

harp_variable_definition *harp_ingestion_register_variable_full_read
    (harp_product_definition *product_definition, const char *name, harp_data_type data_type, int num_dimensions,
     const harp_dimension_type *dimension_type, const long *dimension, const char *description, const char *unit,
     int (*exclude) (void *user_data), int (*read_all) (void *user_data, harp_array data))
{
    harp_variable_definition *variable_definition;

    assert(product_definition != NULL);
    if (variable_definition_new(name, data_type, num_dimensions, dimension_type, dimension, description, unit, exclude,
                                read_all, NULL, NULL, NULL, &variable_definition) != 0)
    {
        assert(0);
    }

    if (product_definition_add_variable(product_definition, variable_definition) != 0)
    {
        assert(0);
    }

    return variable_definition;
}

harp_variable_definition *harp_ingestion_register_variable_range_read
    (harp_product_definition *product_definition, const char *name, harp_data_type data_type, int num_dimensions,
     const harp_dimension_type *dimension_type, const long *dimension, const char *description, const char *unit,
     int (*exclude) (void *user_data), long (*get_max_range) (void *user_data),
     int (*read_range) (void *user_data, long index_offset, long index_length, harp_array data))
{
    harp_variable_definition *variable_definition;

    assert(product_definition != NULL);
    if (variable_definition_new(name, data_type, num_dimensions, dimension_type, dimension, description, unit, exclude,
                                NULL, read_range, get_max_range, NULL, &variable_definition) != 0)
    {
        assert(0);
    }

    if (product_definition_add_variable(product_definition, variable_definition) != 0)
    {
        assert(0);
    }

    return variable_definition;
}

harp_variable_definition *harp_ingestion_register_variable_sample_read
    (harp_product_definition *product_definition, const char *name, harp_data_type data_type, int num_dimensions,
     const harp_dimension_type *dimension_type, const long *dimension, const char *description, const char *unit,
     int (*exclude) (void *user_data), int (*read_sample) (void *user_data, long index, harp_array data))
{
    harp_variable_definition *variable_definition;

    assert(product_definition != NULL);
    if (variable_definition_new(name, data_type, num_dimensions, dimension_type, dimension, description, unit, exclude,
                                NULL, NULL, NULL, read_sample, &variable_definition) != 0)
    {
        assert(0);
    }

    if (product_definition_add_variable(product_definition, variable_definition) != 0)
    {
        assert(0);
    }

    return variable_definition;
}

void harp_variable_definition_add_mapping(harp_variable_definition *variable_definition, const char *ingestion_option,
                                          const char *condition, const char *path, const char *description)
{
    assert(variable_definition != NULL);
    assert(ingestion_option != NULL || condition != NULL || path != NULL || description != NULL);

    if (variable_definition->num_mappings % BLOCK_SIZE == 0)
    {
        harp_mapping_description **new_mapping;
        size_t new_size;

        new_size = (variable_definition->num_mappings + BLOCK_SIZE) * sizeof(harp_mapping_description *);
        new_mapping = (harp_mapping_description **)realloc(variable_definition->mapping, new_size);
        assert(new_mapping != NULL);
        variable_definition->mapping = new_mapping;
    }
    variable_definition->mapping[variable_definition->num_mappings] =
        mapping_description_new(ingestion_option, condition, path, description);
    variable_definition->num_mappings++;
}

void harp_variable_definition_set_valid_range_int8(harp_variable_definition *variable_definition, int8_t valid_min,
                                                   int8_t valid_max)
{
    assert(variable_definition != NULL);
    assert(variable_definition->data_type == harp_type_int8);

    variable_definition->valid_min.int8_data = valid_min;
    variable_definition->valid_max.int8_data = valid_max;
}

void harp_variable_definition_set_valid_range_int16(harp_variable_definition *variable_definition, int16_t valid_min,
                                                    int16_t valid_max)
{
    assert(variable_definition != NULL);
    assert(variable_definition->data_type == harp_type_int16);

    variable_definition->valid_min.int16_data = valid_min;
    variable_definition->valid_max.int16_data = valid_max;
}

void harp_variable_definition_set_valid_range_int32(harp_variable_definition *variable_definition, int32_t valid_min,
                                                    int32_t valid_max)
{
    assert(variable_definition != NULL);
    assert(variable_definition->data_type == harp_type_int32);

    variable_definition->valid_min.int32_data = valid_min;
    variable_definition->valid_max.int32_data = valid_max;
}

void harp_variable_definition_set_valid_range_float(harp_variable_definition *variable_definition, float valid_min,
                                                    float valid_max)
{
    assert(variable_definition != NULL);
    assert(variable_definition->data_type == harp_type_float);

    variable_definition->valid_min.float_data = valid_min;
    variable_definition->valid_max.float_data = valid_max;
}

void harp_variable_definition_set_valid_range_double(harp_variable_definition *variable_definition, double valid_min,
                                                     double valid_max)
{
    assert(variable_definition != NULL);
    assert(variable_definition->data_type == harp_type_double);

    variable_definition->valid_min.double_data = valid_min;
    variable_definition->valid_max.double_data = valid_max;
}

int harp_variable_definition_has_dimension_types(const harp_variable_definition *variable_definition,
                                                 int num_dimensions, const harp_dimension_type *dimension_type)
{
    int i;

    if (variable_definition->num_dimensions != num_dimensions)
    {
        return 0;
    }

    for (i = 0; i < num_dimensions; i++)
    {
        if (variable_definition->dimension_type[i] != dimension_type[i])
        {
            return 0;
        }
    }

    return 1;
}

int harp_variable_definition_has_dimension_type(const harp_variable_definition *variable_definition,
                                                harp_dimension_type dimension_type)
{
    return harp_variable_definition_has_dimension_types(variable_definition, 1, &dimension_type);
}

int harp_variable_definition_exclude(const harp_variable_definition *variable_definition, void *user_data)
{
    return (variable_definition->exclude != NULL && variable_definition->exclude(user_data));
}

void harp_product_definition_add_mapping(harp_product_definition *product_definition, const char *mapping_description,
                                         const char *ingestion_option)
{
    if (mapping_description != NULL)
    {
        if (product_definition->mapping_description != NULL)
        {
            char *new_description;
            long length;

            /* append description */
            length = strlen(product_definition->mapping_description) + strlen(mapping_description) + 1;
            new_description = realloc(product_definition->mapping_description, length);
            assert(new_description != NULL);
            strcat(new_description, mapping_description);
            product_definition->mapping_description = new_description;
        }
        else
        {
            product_definition->mapping_description = strdup(mapping_description);
        }
        assert(product_definition->mapping_description != NULL);
    }
    if (ingestion_option != NULL)
    {
        assert(product_definition->ingestion_option == NULL);
        product_definition->ingestion_option = strdup(ingestion_option);
        assert(product_definition->ingestion_option != NULL);
    }
}

int harp_product_definition_has_dimension_type(const harp_product_definition *product_definition,
                                               harp_dimension_type dimension_type)
{
    int i;

    for (i = 0; i < product_definition->num_variable_definitions; i++)
    {
        const harp_variable_definition *variable_definition = product_definition->variable_definition[i];
        int j;

        for (j = 0; j < variable_definition->num_dimensions; j++)
        {
            if (variable_definition->dimension_type[j] == dimension_type)
            {
                return 1;
            }
        }
    }

    return 0;
}

int harp_product_definition_has_variable(const harp_product_definition *product_definition, const char *name)
{
    return (harp_product_definition_get_variable_index(product_definition, name) >= 0);
}

harp_variable_definition *harp_product_definition_find_variable(const harp_product_definition *product_definition,
                                                                const char *name)
{
    int index;

    index = harp_product_definition_get_variable_index(product_definition, name);
    return (index >= 0 ? product_definition->variable_definition[index] : NULL);
}

int harp_product_definition_get_variable_index(const harp_product_definition *product_definition, const char *name)
{
    long index;

    index = hashtable_get_index_from_name(product_definition->variable_definition_hash_data, name);
    assert(index >= -1 && index < product_definition->num_variable_definitions);
    return (int)index;
}

int harp_ingestion_module_validate_options(harp_ingestion_module *module, const harp_ingestion_options *options)
{
    int i;

    if (module == NULL)
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "module is NULL (%s:%u)", __FILE__, __LINE__);
        return -1;
    }

    if (options == NULL)
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "options is NULL (%s:%u)", __FILE__, __LINE__);
        return -1;
    }

    for (i = 0; i < options->num_options; i++)
    {
        const harp_ingestion_option *option;
        int index;
        int j;

        option = options->option[i];
        index = ingestion_module_get_option_index(module, option->name);
        if (index < 0)
        {
            harp_set_error(HARP_ERROR_INVALID_INGESTION_OPTION, "ingestion module '%s' has no option named '%s'",
                           module->name, option->name);
            return -1;
        }
        for (j = 0; j < module->option_definition[index]->num_allowed_values; j++)
        {
            if (strcmp(option->value, module->option_definition[index]->allowed_value[j]) == 0)
            {
                break;
            }
        }
        if (j == module->option_definition[index]->num_allowed_values)
        {
            harp_set_error(HARP_ERROR_INVALID_INGESTION_OPTION_VALUE, "invalid value '%s' for option '%s' of ingestion "
                           "module '%s'", option->value, option->name, module->name);
            return -1;
        }
    }

    return 0;
}

int harp_ingestion_find_module(const char *filename, harp_ingestion_module **module, coda_product **cproduct)
{
    coda_product *product;
    int result;
    int i;

    assert(module_register != NULL);
    assert(filename != NULL);
    assert(cproduct != NULL);

    /* Try to identify the product using CODA. */
    result = coda_open(filename, &product);
    if (result != 0 && coda_errno == CODA_ERROR_FILE_OPEN && coda_get_option_use_mmap())
    {
        /* There may not be enough memory space available to map the file into memory => temporarily disable memory
         * mapping of files and try again.
         */
        coda_set_option_use_mmap(0);
        result = coda_open(filename, &product);
        coda_set_option_use_mmap(1);
    }

    if (result == 0)
    {
        const char *product_class;
        const char *product_type;

        if (coda_get_product_class(product, &product_class) != 0)
        {
            harp_set_error(HARP_ERROR_CODA, NULL);
            coda_close(product);
            return -1;
        }
        if (coda_get_product_type(product, &product_type) != 0)
        {
            harp_set_error(HARP_ERROR_CODA, NULL);
            coda_close(product);
            return -1;
        }

        /* Look for a compatible ingestion module by comparing product_class and product_type. */
        if (product_class != NULL && product_type != NULL)
        {
            for (i = 0; i < module_register->num_ingestion_modules; i++)
            {
                harp_ingestion_module *ingestion_module;

                ingestion_module = module_register->ingestion_module[i];
                if (ingestion_module->product_class == NULL || ingestion_module->product_type == NULL)
                {
                    continue;
                }

                if (strcmp(ingestion_module->product_class, product_class) != 0)
                {
                    continue;
                }

                if (strcmp(ingestion_module->product_type, product_type) != 0)
                {
                    continue;
                }

                *module = ingestion_module;
                *cproduct = product;
                return 0;
            }

            harp_set_error(HARP_ERROR_UNSUPPORTED_PRODUCT, "%s: unsupported product class/type '%s/%s'", filename,
                           product_class, product_type);
            coda_close(product);
            return -1;
        }

        coda_close(product);
    }
    else
    {
        if (coda_errno != CODA_ERROR_UNSUPPORTED_PRODUCT)
        {
            harp_set_error(HARP_ERROR_CODA, NULL);
            return -1;
        }

        /* Could not identify product using CODA => try verify_product_type() for custom modules */
        for (i = 0; i < module_register->num_ingestion_modules; i++)
        {
            harp_ingestion_module *ingestion_module;

            ingestion_module = module_register->ingestion_module[i];
            if (ingestion_module->verify_product_type == NULL)
            {
                continue;
            }
            if (ingestion_module->verify_product_type(ingestion_module, filename) != 0)
            {
                continue;
            }

            *module = ingestion_module;
            return 0;
        }
    }

    harp_set_error(HARP_ERROR_UNSUPPORTED_PRODUCT, "%s: unsupported product", filename);
    return -1;
}

int harp_ingestion_init(void)
{
    int i;

    if (module_register != NULL)
    {
        /* Already initialized. */
        return 0;
    }

    if (coda_init() != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }

    module_register = (harp_ingestion_module_register *)malloc(sizeof(harp_ingestion_module_register));
    if (module_register == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       sizeof(harp_ingestion_module_register), __FILE__, __LINE__);
        return -1;
    }
    module_register->num_ingestion_modules = 0;
    module_register->ingestion_module = NULL;

    /* Make sure that udunits gets initialized as well */
    if (!harp_unit_is_valid(""))
    {
        return -1;
    }

    for (i = 0; i < NUM_INGESTION_MODULES; i++)
    {
        if (module_init_func[i] () != 0)
        {
            return -1;
        }
    }

    /* Add the variable index {time} to all product definitions of which at least one variable depends on the time
     * dimension.
     */
    for (i = 0; i < module_register->num_ingestion_modules; i++)
    {
        harp_ingestion_module *module = module_register->ingestion_module[i];
        int j;

        for (j = 0; j < module->num_product_definitions; j++)
        {
            harp_product_definition *product_definition = module->product_definition[j];

            if (harp_product_definition_has_dimension_type(product_definition, harp_dimension_time))
            {
                harp_dimension_type dimension_type[1] = { harp_dimension_time };
                harp_ingestion_register_variable_sample_read(product_definition, "index", harp_type_int32, 1,
                                                             dimension_type, NULL, "zero-based index of the sample "
                                                             "within the source product", NULL, NULL, read_index);
            }
        }
    }

    return 0;
}

void harp_ingestion_done(void)
{
    if (module_register != NULL)
    {
        if (module_register->ingestion_module != NULL)
        {
            int i;

            for (i = 0; i < module_register->num_ingestion_modules; i++)
            {
                ingestion_module_delete(module_register->ingestion_module[i]);
            }

            free(module_register->ingestion_module);
        }

        free(module_register);
        module_register = NULL;

        coda_done();
    }
}

harp_ingestion_module_register *harp_ingestion_get_module_register(void)
{
    return module_register;
}

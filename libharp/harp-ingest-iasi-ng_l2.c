#include "coda.h"
#include "harp-ingestion.h"

#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Macro to determine the number of elements in a one dimensional C array. */
#define ARRAY_SIZE(X) (sizeof((X)) / sizeof((X)[0]))

/* Maximum length of a path string in generated mapping descriptions. */
#define MAX_PATH_LENGTH 256

typedef enum iasi_ng_product_type_enum
{
    iasi_ng_type_co,
    iasi_ng_type_nac,
    iasi_ng_type_o3,
    iasi_ng_type_so2,
    iasi_ng_type_sfc,
    iasi_ng_type_cld,
    iasi_ng_type_ghg,
    iasi_ng_type_twv,
} iasi_ng_product_type;

#define IASI_NG_NUM_PRODUCT_TYPES (((int)iasi_ng_type_twv) + 1)

typedef enum iasi_ng_dimension_type_enum
{
    iasi_ng_dim_lines,
    iasi_ng_dim_for,
    iasi_ng_dim_fov,
    iasi_ng_dim_level,
} iasi_ng_dimension_type;

/* handy constant: last enum value + 1 */
#define IASI_NG_NUM_DIM_TYPES ((int)iasi_ng_dim_level + 1)

static const char *iasi_ng_dimension_name[IASI_NG_NUM_PRODUCT_TYPES][IASI_NG_NUM_DIM_TYPES] = {
    {"n_lines", "n_for", "n_fov", NULL},        /* CO  */
    {"n_lines", "n_for", "n_fov", NULL},        /* NAC */
    {"n_lines", "n_for", "n_fov", NULL},        /* O3  */
    {"n_lines", "n_for", "n_fov", NULL},        /* SO2 */
    {"n_lines", "n_for", "n_fov", NULL},        /* SFC */
    {"n_lines", "n_for", "n_fov", "n_clevels"}, /* CLD */
    {"n_lines", "n_for", "n_fov", "n_n2o"},     /* GHG */
    {"n_lines", "n_for", "n_fov", "n_levels"},  /* TWV */
};

typedef struct ingest_info_struct
{
    coda_product *product;

    iasi_ng_product_type product_type;

    /* dimensions */
    long num_lines;
    long num_for;
    long num_fov;
    long num_levels;

    /* cursors */
    coda_cursor data_cursor;
    coda_cursor geolocation_cursor;
    coda_cursor surface_cursor;
    coda_cursor stat_retrieval_cursor;
    coda_cursor l2p_sst_cursor;

    /* from S5 module */
    int processor_version;
    int collection_number;
    uint8_t *surface_layer_status;

} ingest_info;

/* The routines start here
 */

static const char *get_product_type_name(iasi_ng_product_type product_type)
{
    switch (product_type)
    {
        case iasi_ng_type_co:
            return "IAS_02_CO_";
        case iasi_ng_type_nac:
            return "IAS_02_NAC";
        case iasi_ng_type_o3:
            return "IAS_02_O3_";
        case iasi_ng_type_so2:
            return "IAS_02_SO2";
        case iasi_ng_type_sfc:
            return "IAS_02_SFC";
        case iasi_ng_type_cld:
            return "IAS_02_CLD";
        case iasi_ng_type_ghg:
            return "IAS_02_GHG";
        case iasi_ng_type_twv:
            return "IAS_02_TWV";
    }

    assert(0);
    exit(1);
}

/* Tiny helper for get_product_type() */
static void dash_to_underscore(char *s)
{
    /* use size_t for byte offsets into the char array */
    size_t i;

    /* Changing '-' to '_' */
    for (i = 0; s[i] != '\0'; ++i)
    {
        if (s[i] == '-')
        {
            s[i] = '_';
        }
    }
}

static void broadcast_array_float(long num_scanlines, long num_pixels, float *data)
{
    long i;

    /* Repeat the value for each scanline for all pixels in that scanline. Iterate
     * in reverse to avoid overwriting scanline values.
     */
    for (i = num_scanlines - 1; i >= 0; i--)
    {
        long j;

        for (j = 0; j < num_pixels; j++)
        {
            data[i * num_pixels + j] = data[i];
        }
    }
}

static void broadcast_array_double(long num_lines, long num_for, long num_fov, double *data)
{
    long i;

    /* last source element */
    long in_idx = num_lines * num_for - 1;

    /* last destination element      */
    long out_idx = num_lines * num_for * num_fov - 1;

    for (i = num_lines - 1; i >= 0; i--)
    {
        long j;

        for (j = num_for - 1; j >= 0; j--)
        {
            long k;

            /* source value */
            double v = data[in_idx--];

            for (k = 0; k < num_fov; k++)
            {
                /* replicate across FOV */
                data[out_idx--] = v;
            }
        }
    }
}


static int get_product_type(coda_product *product, iasi_ng_product_type *product_type)
{
    coda_cursor cursor, child, *src = NULL;
    char buf[256];      /* plenty of room for long IDs   */
    long len;
    int i;

    /* 1. bind root */
    if (coda_cursor_set_product(&cursor, product) != 0)
    {
        return harp_set_error(HARP_ERROR_CODA, NULL), -1;
    }

    /* 2. first try the clean ProductShortName */
    if (coda_cursor_goto(&cursor, "/METADATA/GRANULE_DESCRIPTION@ProductShortName") == 0)
    {
        src = &cursor;
    }
    else if (coda_cursor_goto(&cursor, "/@product_name") == 0)
    {
        /* may be scalar or 1-D array */
        coda_type_class tc;

        if (coda_cursor_get_type_class(&cursor, &tc) != 0)
        {
            return harp_set_error(HARP_ERROR_CODA, NULL), -1;
        }

        if (tc == coda_array_class)
        {
            child = cursor;
            if (coda_cursor_goto_first_array_element(&child) != 0)
            {
                return harp_set_error(HARP_ERROR_CODA, NULL), -1;
            }
            src = &child;
        }
        else
        {
            src = &cursor;
        }
    }
    else
    {
        return harp_set_error(HARP_ERROR_INGESTION, "cannot find product identifier"), -1;
    }

    /* 3. read the string */
    if (coda_cursor_get_string_length(src, &len) != 0 ||
        len <= 0 || len >= (long)sizeof(buf) || coda_cursor_read_string(src, buf, sizeof(buf)) != 0)
    {
        return harp_set_error(HARP_ERROR_CODA, NULL), -1;
    }

    /* 4. normalise and show */
    dash_to_underscore(buf);

    /* 5. search for any known short code */
    for (i = 0; i < IASI_NG_NUM_PRODUCT_TYPES; i++)
    {
        const char *code = get_product_type_name((iasi_ng_product_type)i);

        if (strstr(buf, code) != NULL)
        {
            *product_type = (iasi_ng_product_type)i;
            return 0;
        }
    }

    return harp_set_error(HARP_ERROR_INGESTION, "unsupported product type '%s'", buf), -1;
}

/* Recursively search for the named 1D dimension field within a CODA structure. */
static int find_dimension_length_recursive(coda_cursor *cursor, const char *name, long *length)
{
    coda_type_class type_class;

    if (coda_cursor_get_type_class(cursor, &type_class) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, "Failed to get type class");
        return -1;
    }

    if (type_class == coda_record_class)
    {
        coda_cursor sub_cursor = *cursor;

        /* Navigate to the first field */
        if (coda_cursor_goto_first_record_field(&sub_cursor) == 0)
        {
            do
            {
                /* Attempt to navigate to the field by name */
                coda_cursor test_cursor = *cursor;

                if (coda_cursor_goto_record_field_by_name(&test_cursor, name) == 0)
                {
                    long coda_dim[CODA_MAX_NUM_DIMS];
                    int num_dims;

                    if (coda_cursor_get_array_dim(&test_cursor, &num_dims, coda_dim) != 0)
                    {
                        harp_set_error(HARP_ERROR_CODA, "Failed to get array dimensions");
                        return -1;
                    }

                    if (num_dims != 1)
                    {
                        harp_set_error(HARP_ERROR_INGESTION, "Field '%s' is not a 1D array", name);
                        return -1;
                    }

                    *length = coda_dim[0];
                    return 0;
                }

                /* Recursively search in the substructure */
                if (find_dimension_length_recursive(&sub_cursor, name, length) == 0)
                {
                    return 0;
                }

            } while (coda_cursor_goto_next_record_field(&sub_cursor) == 0);
        }
    }
    else if (type_class == coda_array_class)
    {
        long num_elements;

        if (coda_cursor_get_num_elements(cursor, &num_elements) != 0)
        {
            harp_set_error(HARP_ERROR_CODA, "Failed to get number of array elements");
            return -1;
        }

        if (num_elements > 0)
        {
            coda_cursor sub_cursor = *cursor;

            if (coda_cursor_goto_array_element_by_index(&sub_cursor, 0) != 0)
            {
                harp_set_error(HARP_ERROR_CODA, "Failed to go to array element");
                return -1;
            }

            if (find_dimension_length_recursive(&sub_cursor, name, length) == 0)
            {
                return 0;
            }
        }
    }

    /* Not found in this branch */
    return -1;
}

/* Find dimension length by recursively searching under data/. */
static int get_dimension_length(ingest_info *info, const char *name, long *length)
{
    coda_cursor cursor = info->data_cursor;

    if (find_dimension_length_recursive(&cursor, name, length) != 0)
    {
        harp_set_error(HARP_ERROR_INGESTION, "Dimension '%s' not found in product structure", name);
        return -1;
    }

    return 0;
}


/* Init Routines */

/* Initialize CODA cursors for main record groups with inline comments. */
static int init_cursors(ingest_info *info)
{
    coda_cursor cursor;

    /* Bind a cursor to the root of the CODA product */
    if (coda_cursor_set_product(&cursor, info->product) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }

    if (coda_cursor_goto_record_field_by_name(&cursor, "data") != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    /* Save data/ cursor; subsequent navigation is relative to this. */
    info->data_cursor = cursor;

    /* Geolocation group */
    if (coda_cursor_goto_record_field_by_name(&cursor, "geolocation_information") != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    info->geolocation_cursor = cursor;

    /* Back to data/ */
    coda_cursor_goto_parent(&cursor);

    /* Instrument data: '/data/surface_info'. Only TWV, SFC, CLD, and GHG have it. */
    if (info->product_type == iasi_ng_type_twv || info->product_type == iasi_ng_type_sfc ||
        info->product_type == iasi_ng_type_cld || info->product_type == iasi_ng_type_ghg)
    {
        if (coda_cursor_goto_record_field_by_name(&cursor, "surface_info") != 0)
        {
            harp_set_error(HARP_ERROR_CODA, NULL);
            return -1;
        }
        info->surface_cursor = cursor;

        /* Back to data/ */
        coda_cursor_goto_parent(&cursor);
    }

    /* Statistical retrieval data: '/data/statistical_retrieval'. Only SFC and TWV have it. */
    if (info->product_type == iasi_ng_type_twv || info->product_type == iasi_ng_type_sfc)
    {
        if (coda_cursor_goto_record_field_by_name(&cursor, "statistical_retrieval") != 0)
        {
            harp_set_error(HARP_ERROR_CODA, NULL);
            return -1;
        }
        info->stat_retrieval_cursor = cursor;

        /* Back to data/ */
        coda_cursor_goto_parent(&cursor);
    }

    /* data/l2p_sst. Only SFC has it. */
    if (info->product_type == iasi_ng_type_sfc)
    {
        if (coda_cursor_goto_record_field_by_name(&cursor, "l2p_sst") != 0)
        {
            harp_set_error(HARP_ERROR_CODA, NULL);
            return -1;
        }
        info->l2p_sst_cursor = cursor;

        /* Back to data/ */
        coda_cursor_goto_parent(&cursor);
    }

    return 0;
}

/* Initialize record dimension lengths for the Sentinel-5 simulated L1b dataset */
static int init_dimensions(ingest_info *info)
{
    /* Get number of lines */
    if (iasi_ng_dimension_name[info->product_type][iasi_ng_dim_lines] != NULL)
    {
        if (get_dimension_length(info, iasi_ng_dimension_name[info->product_type][iasi_ng_dim_lines],
                                 &info->num_lines) != 0)
        {
            return -1;
        }
    }

    /* Get number of field of regard */
    if (iasi_ng_dimension_name[info->product_type][iasi_ng_dim_for] != NULL)
    {
        if (get_dimension_length(info, iasi_ng_dimension_name[info->product_type][iasi_ng_dim_for], &info->num_for) !=
            0)
        {
            return -1;
        }
    }

    /* Get number of field of views */
    if (iasi_ng_dimension_name[info->product_type][iasi_ng_dim_fov] != NULL)
    {
        if (get_dimension_length(info, iasi_ng_dimension_name[info->product_type][iasi_ng_dim_fov], &info->num_fov) !=
            0)
        {
            return -1;
        }
    }

    /* Get number of levels */
    if (iasi_ng_dimension_name[info->product_type][iasi_ng_dim_level] != NULL)
    {
        if (get_dimension_length(info, iasi_ng_dimension_name[info->product_type][iasi_ng_dim_level], &info->num_levels)
            != 0)
        {
            return -1;
        }
    }

    return 0;
}

/* Extract Sentinel-5 L1b product collection and processor version
 * from the global "logical product name".
 */
static int init_versions(ingest_info *info)
{
    coda_cursor cursor;
    char product_name[84];

    if (coda_cursor_set_product(&cursor, info->product) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_goto(&cursor, "/@id") != 0)
    {
        /* no global 'id' attribute */
        return 0;
    }
    if (coda_cursor_read_string(&cursor, product_name, 84) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (strlen(product_name) != 83)
    {
        /* 'id' attribute does not contain a valid logical product name */
        return 0;
    }

    /* Populating the variables */
    info->collection_number = (int)strtol(&product_name[58], NULL, 10);
    info->processor_version = (int)strtol(&product_name[61], NULL, 10);

    return 0;
}

static void ingestion_done(void *user_data)
{
    ingest_info *info = (ingest_info *)user_data;

    if (info->surface_layer_status != NULL)
    {
        free(info->surface_layer_status);
    }

    free(info);
}

static int ingestion_init(const harp_ingestion_module *module, coda_product *product,
                          const harp_ingestion_options *options, harp_product_definition **definition, void **user_data)
{
    const char *option_value;
    ingest_info *info;

    info = (ingest_info *)malloc(sizeof(ingest_info));
    if (info == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       sizeof(ingest_info), __FILE__, __LINE__);
        return -1;
    }

    info->product = product;
    info->surface_layer_status = NULL;

    /* Dimensions */
    info->num_lines = 0;
    info->num_fov = 0;
    info->num_for = 0;
    info->num_levels = 0;


    if (get_product_type(info->product, &info->product_type) != 0)
    {
        ingestion_done(info);
        return -1;
    }

    if (init_versions(info) != 0)
    {
        ingestion_done(info);
        return -1;
    }

    *definition = *module->product_definition;


    if (init_cursors(info) != 0)
    {
        ingestion_done(info);
        return -1;
    }


    /* Getting input product dimensios */
    if (init_dimensions(info) != 0)
    {
        ingestion_done(info);
        return -1;
    }

    *user_data = info;

    return 0;
}

/* Reading Routines */

static int read_dimensions(void *user_data, long dimension[HARP_NUM_DIM_TYPES])
{
    ingest_info *info = (ingest_info *)user_data;

    dimension[harp_dimension_time] = info->num_lines * info->num_for * info->num_fov;
    dimension[harp_dimension_vertical] = info->num_levels;

    return 0;
}

static int read_dataset(coda_cursor cursor, const char *dataset_name, harp_data_type data_type, long num_elements,
                        harp_array data)
{
    long coda_num_elements;

    if (coda_cursor_goto_record_field_by_name(&cursor, dataset_name) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_get_num_elements(&cursor, &coda_num_elements) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_num_elements != num_elements)
    {
        harp_set_error(HARP_ERROR_INGESTION, "dataset has %ld elements; expected %ld", coda_num_elements, num_elements);
        harp_add_coda_cursor_path_to_error_message(&cursor);
        return -1;
    }

    switch (data_type)
    {
        case harp_type_int8:
            {
                coda_native_type read_type;

                if (coda_cursor_goto_first_array_element(&cursor) != 0)
                {
                    harp_set_error(HARP_ERROR_CODA, NULL);
                    return -1;
                }
                if (coda_cursor_get_read_type(&cursor, &read_type) != 0)
                {
                    harp_set_error(HARP_ERROR_CODA, NULL);
                    return -1;
                }
                coda_cursor_goto_parent(&cursor);
                if (read_type == coda_native_type_uint8)
                {
                    if (coda_cursor_read_uint8_array(&cursor, (uint8_t *)data.int8_data, coda_array_ordering_c) != 0)
                    {
                        harp_set_error(HARP_ERROR_CODA, NULL);
                        return -1;
                    }
                }
                else
                {
                    if (coda_cursor_read_int8_array(&cursor, data.int8_data, coda_array_ordering_c) != 0)
                    {
                        harp_set_error(HARP_ERROR_CODA, NULL);
                        return -1;
                    }
                }
            }
            break;
        case harp_type_int16:
            {
                coda_native_type read_type;

                if (coda_cursor_goto_first_array_element(&cursor) != 0)
                {
                    harp_set_error(HARP_ERROR_CODA, NULL);
                    return -1;
                }
                if (coda_cursor_get_read_type(&cursor, &read_type) != 0)
                {
                    harp_set_error(HARP_ERROR_CODA, NULL);
                    return -1;
                }
                coda_cursor_goto_parent(&cursor);
                if (read_type == coda_native_type_uint16)
                {
                    if (coda_cursor_read_uint16_array(&cursor, (uint16_t *)data.int16_data, coda_array_ordering_c) != 0)
                    {
                        harp_set_error(HARP_ERROR_CODA, NULL);
                        return -1;
                    }
                }
                else
                {
                    if (coda_cursor_read_int16_array(&cursor, data.int16_data, coda_array_ordering_c) != 0)
                    {
                        harp_set_error(HARP_ERROR_CODA, NULL);
                        return -1;
                    }
                }
            }
            break;
        case harp_type_int32:
            {
                coda_native_type read_type;

                if (coda_cursor_goto_first_array_element(&cursor) != 0)
                {
                    harp_set_error(HARP_ERROR_CODA, NULL);
                    return -1;
                }
                if (coda_cursor_get_read_type(&cursor, &read_type) != 0)
                {
                    harp_set_error(HARP_ERROR_CODA, NULL);
                    return -1;
                }
                coda_cursor_goto_parent(&cursor);
                if (read_type == coda_native_type_uint32)
                {
                    if (coda_cursor_read_uint32_array(&cursor, (uint32_t *)data.int32_data, coda_array_ordering_c) != 0)
                    {
                        harp_set_error(HARP_ERROR_CODA, NULL);
                        return -1;
                    }
                }
                else
                {
                    if (coda_cursor_read_int32_array(&cursor, data.int32_data, coda_array_ordering_c) != 0)
                    {
                        harp_set_error(HARP_ERROR_CODA, NULL);
                        return -1;
                    }
                }
            }
            break;
        case harp_type_float:
            if (coda_cursor_read_float_array(&cursor, data.float_data, coda_array_ordering_c) != 0)
            {
                harp_set_error(HARP_ERROR_CODA, NULL);
                return -1;
            }
            break;

        case harp_type_double:
            if (coda_cursor_read_double_array(&cursor, data.double_data, coda_array_ordering_c) != 0)
            {
                harp_set_error(HARP_ERROR_CODA, NULL);
                return -1;
            }
            break;
        default:
            assert(0);
            exit(1);
    }

    return 0;
}

/* Read the absolute orbit number from the global attribute */
static int read_orbit_index(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    coda_cursor cursor;
    coda_native_type read_type;
    uint32_t uval;
    int32_t ival;

    /* 1) Bind a cursor to the root product */
    if (coda_cursor_set_product(&cursor, info->product) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }

    /* 2) Try /@orbit_start first, then /@orbit */
    if (coda_cursor_goto(&cursor, "/@orbit_start") != 0 && coda_cursor_goto(&cursor, "/@orbit") != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }

    /* 3) If it's an array, move to its first element */
    {
        coda_type_class tc;

        if (coda_cursor_get_type_class(&cursor, &tc) != 0)
        {
            return -1;
        }
        if (tc == coda_array_class)
        {
            if (coda_cursor_goto_first_array_element(&cursor) != 0)
            {
                harp_set_error(HARP_ERROR_CODA, NULL);
                return -1;
            }
        }
    }

    /* 4) Determine the native storage type and read appropriately */
    if (coda_cursor_get_read_type(&cursor, &read_type) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (read_type == coda_native_type_uint32)
    {
        /* Stored as an unsigned 32-bit */
        if (coda_cursor_read_uint32(&cursor, &uval) != 0)
        {
            harp_set_error(HARP_ERROR_CODA, NULL);
            return -1;
        }
        ival = (int32_t)uval;
    }
    else
    {
        /* Stored as a signed 32-bit (or other compatible) */
        if (coda_cursor_read_int32(&cursor, &ival) != 0)
        {
            harp_set_error(HARP_ERROR_CODA, NULL);
            return -1;
        }
    }

    /* 5) Write back into the HARP buffer */
    data.int32_data[0] = ival;
    return 0;
}

/* Field: data */

static int read_data_surface_altitude(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->data_cursor, "surface_z", harp_type_float,
                        info->num_lines * info->num_for * info->num_fov, data);
}

static int read_data_quality_flag(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    const char *var_name;

    /* only CO, NAC, O3, and SO2 have this field */
    if (info->product_type == iasi_ng_type_co)
    {
        var_name = "co_qflag";
    }
    else if (info->product_type == iasi_ng_type_nac)
    {
        var_name = "hno3_qflag";
    }
    else if (info->product_type == iasi_ng_type_o3)
    {
        var_name = "o3_qflag";
    }
    else if (info->product_type == iasi_ng_type_so2)
    {
        var_name = "so2_qflag";
    }
    else
    {
        harp_set_error(HARP_ERROR_CODA, "dataset %s does not contain *_qflag variable", info->product_type);
        return -1;
    }

    return read_dataset(info->data_cursor, var_name, harp_type_int8, info->num_lines * info->num_for * info->num_fov,
                        data);
}

static int read_data_bdiv(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    const char *var_name;

    /* only CO, NAC, and O3 have this field */
    if (info->product_type == iasi_ng_type_co)
    {
        var_name = "co_bdiv";
    }
    else if (info->product_type == iasi_ng_type_nac)
    {
        var_name = "hno3_bdiv";
    }
    else if (info->product_type == iasi_ng_type_o3)
    {
        var_name = "o3_bdiv";
    }
    else
    {
        harp_set_error(HARP_ERROR_CODA, "dataset %s does not contain *_bdiv variable", info->product_type);
        return -1;
    }

    return read_dataset(info->data_cursor, var_name, harp_type_int32, info->num_lines * info->num_for * info->num_fov,
                        data);
}

static int read_data_column_density(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    const char *var_name;

    /* only CO, NAC, and O3 have this field */
    if (info->product_type == iasi_ng_type_co)
    {
        var_name = "atmosphere_mass_content_of_carbon_monoxide";
    }
    else if (info->product_type == iasi_ng_type_nac)
    {
        var_name = "atmosphere_mass_content_of_nitric_acid";
    }
    else if (info->product_type == iasi_ng_type_o3)
    {
        var_name = "atmosphere_mass_content_of_ozone";
    }
    else
    {
        harp_set_error(HARP_ERROR_CODA, "dataset %s does not contain atmosphere_mass_content_of_ozone_* variable",
                       info->product_type);
        return -1;
    }

    return read_dataset(info->data_cursor, var_name, harp_type_float, info->num_lines * info->num_for * info->num_fov,
                        data);
}

static int read_data_dust_indicator(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    if (info->product_type == iasi_ng_type_cld)
    {
        return read_dataset(info->data_cursor, "dust_indicator", harp_type_float,
                            info->num_lines * info->num_for * info->num_fov, data);
    }
    else if (info->product_type == iasi_ng_type_sfc)
    {
        return read_dataset(info->l2p_sst_cursor, "dust_indicator", harp_type_float,
                            info->num_lines * info->num_for * info->num_fov, data);
    }

    return -1;
}

static int read_data_nitrous_oxide_column_density(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    if (read_dataset
        (info->data_cursor, "atmosphere_mass_content_of_nitrous_oxide", harp_type_float,
         info->num_lines * info->num_for * info->num_fov * info->num_levels, data) != 0)
    {
        return -1;
    }

    return 0;
}

static int read_data_methane_column_density(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->data_cursor, "atmosphere_mass_content_of_methane", harp_type_float,
                        info->num_lines * info->num_for * info->num_fov * info->num_levels, data);
}

static int read_data_carbon_dioxide_column_density(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->data_cursor, "atmosphere_mass_content_of_carbon_dioxide", harp_type_float,
                        info->num_lines * info->num_for * info->num_fov * info->num_levels, data);
}

/* Field: data/statistical_retrieval */

static int read_statistical_surface_temperature(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->stat_retrieval_cursor, "surface_temperature", harp_type_float,
                        info->num_lines * info->num_for * info->num_fov, data);
}

static int read_statistical_surface_air_pressure(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->stat_retrieval_cursor, "surface_air_pressure", harp_type_float,
                        info->num_lines * info->num_for * info->num_fov, data);
}

/* Field: data/l2p_sst*/

static int read_l2p_sst_wind_speed(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->l2p_sst_cursor, "wind_speed", harp_type_float,
                        info->num_lines * info->num_for * info->num_fov, data);
}

/* Field: data/surface_info */

static int read_surface_height(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->surface_cursor, "height", harp_type_float,
                        info->num_lines * info->num_for * info->num_fov, data);
}

static int read_surface_height_std(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->surface_cursor, "height_std", harp_type_float,
                        info->num_lines * info->num_for * info->num_fov, data);
}

/* Helper function */
static int convert_percentage_fraction(void *user_data, const char *variable_name, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    long i;

    if (read_dataset
        (info->surface_cursor, variable_name, harp_type_float, info->num_lines * info->num_for * info->num_fov,
         data) != 0)
    {
        return -1;
    }
    for (i = 0; i < info->num_lines * info->num_for * info->num_fov; i++)
    {
        if (data.float_data[i] >= 0.0f && data.float_data[i] <= 100.0f)
        {
            data.float_data[i] /= (float)100.0;
        }
    }

    return 0;
}

static int read_surface_ice_fraction(void *user_data, harp_array data)
{
    return convert_percentage_fraction(user_data, "ice_fraction", data);
}

static int read_surface_land_fraction(void *user_data, harp_array data)
{
    return convert_percentage_fraction(user_data, "land_fraction", data);
}

/* Field: data/geolocation_information */

static int read_geolocation_time(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    /* original 2-D */
    long n_src = (long)info->num_lines * info->num_for;

    /* step 1: read the [line,for] array into the 'front' of the buffer */
    if (read_dataset(info->geolocation_cursor, "onboard_utc", harp_type_double, n_src, data) != 0)
    {
        return -1;
    }

    /* step 2: broadcast in place to full [line,for,fov] */
    broadcast_array_double(info->num_lines, info->num_for, info->num_fov, data.double_data);

    /* buffer now holds n_out samples; HARP flattening order is fine */
    return 0;
}

static int read_geolocation_latitude(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->geolocation_cursor, "sounder_pixel_latitude", harp_type_float,
                        info->num_lines * info->num_for * info->num_fov, data);
}

static int read_geolocation_longitude(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->geolocation_cursor, "sounder_pixel_longitude", harp_type_float,
                        info->num_lines * info->num_for * info->num_fov, data);
}

static int read_geolocation_solar_azimuth_angle(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->geolocation_cursor, "sounder_pixel_sun_azimuth", harp_type_float,
                        info->num_lines * info->num_for * info->num_fov, data);
}

static int read_geolocation_solar_zenith_angle(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->geolocation_cursor, "sounder_pixel_sun_zenith", harp_type_float,
                        info->num_lines * info->num_for * info->num_fov, data);
}

static int read_geolocation_sensor_azimuth_angle(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->geolocation_cursor, "sounder_pixel_azimuth", harp_type_float,
                        info->num_lines * info->num_for * info->num_fov, data);
}

static int read_geolocation_sensor_zenith_angle(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->geolocation_cursor, "sounder_pixel_zenith", harp_type_float,
                        info->num_lines * info->num_for * info->num_fov, data);
}

/*
 * Products' Registration Routines
 */

static void register_core_variables(harp_product_definition *product_definition)
{
    const char *description;
    harp_variable_definition *variable_definition;

    /* orbit_index */
    description = "absolute orbit number";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "orbit_index", harp_type_int32, 0, NULL, NULL,
                                                   description, NULL, NULL, read_orbit_index);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, "/@orbit_start", NULL);

}

static void register_data_variables(harp_product_definition *product_definition, const char *product_type)
{
    char path[MAX_PATH_LENGTH];
    const char *description;
    const char *variable_name_in;
    const char *variable_name_out;

    harp_variable_definition *variable_definition;
    harp_dimension_type dimension_type_1d[1] = { harp_dimension_time };

    /* surface_altitude */
    if (strcmp(product_type, "IAS_02_CO_") == 0 || strcmp(product_type, "IAS_02_NAC") == 0 ||
        strcmp(product_type, "IAS_02_O3_") == 0)
    {
        variable_name_in = "surface_z";
        description = "Altitude of surface";
        variable_definition =
            harp_ingestion_register_variable_full_read(product_definition, "surface_altitude", harp_type_float, 1,
                                                       dimension_type_1d, NULL, description, "m", NULL,
                                                       read_data_surface_altitude);

        snprintf(path, MAX_PATH_LENGTH, "/data/%s[]", variable_name_in);
        harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);
    }

    /* validity */
    if (strcmp(product_type, "IAS_02_CO_") == 0 || strcmp(product_type, "IAS_02_NAC") == 0 ||
        strcmp(product_type, "IAS_02_O3_") == 0 || strcmp(product_type, "IAS_02_SO2") == 0)
    {
        if (strcmp(product_type, "IAS_02_CO_") == 0)
        {
            variable_name_in = "co_qflag";
        }
        else if (strcmp(product_type, "IAS_02_NAC") == 0)
        {
            variable_name_in = "hno3_qflag";
        }
        else if (strcmp(product_type, "IAS_02_O3_") == 0)
        {
            variable_name_in = "o3_qflag";
        }
        else if (strcmp(product_type, "IAS_02_SO2") == 0)
        {
            variable_name_in = "so2_qflag";
        }
        description = "General retrieval quality flag";
        variable_definition =
            harp_ingestion_register_variable_full_read(product_definition, "validity", harp_type_int8, 1,
                                                       dimension_type_1d, NULL, description, HARP_UNIT_DIMENSIONLESS,
                                                       NULL, read_data_quality_flag);
        snprintf(path, MAX_PATH_LENGTH, "/data/%s[]", variable_name_in);
        description = "the uint8 data is cast to int8";
        harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, description);
    }

    /* column_number_density_validity */
    if (strcmp(product_type, "IAS_02_CO_") == 0 || strcmp(product_type, "IAS_02_NAC") == 0 ||
        strcmp(product_type, "IAS_02_O3_") == 0)
    {
        if (strcmp(product_type, "IAS_02_CO_") == 0)
        {
            variable_name_in = "co_bdiv";
            variable_name_out = "CO_column_number_density_validity";
        }
        else if (strcmp(product_type, "IAS_02_NAC") == 0)
        {
            variable_name_in = "hno3_bdiv";
            variable_name_out = "HNO3_column_number_density_validity";
        }
        else if (strcmp(product_type, "IAS_02_O3_") == 0)
        {
            variable_name_in = "o3_bdiv";
            variable_name_out = "O3_column_number_density_validity";
        }
        description = "Retrieval flags";
        variable_definition =
            harp_ingestion_register_variable_full_read(product_definition, variable_name_out, harp_type_int32, 1,
                                                       dimension_type_1d, NULL, description, HARP_UNIT_DIMENSIONLESS,
                                                       NULL, read_data_bdiv);
        snprintf(path, MAX_PATH_LENGTH, "/data/%s[]", variable_name_in);
        description = "the uint32 data is cast to int32";
        harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, description);
    }

    /* column_density */
    if (strcmp(product_type, "IAS_02_CO_") == 0 || strcmp(product_type, "IAS_02_NAC") == 0 ||
        strcmp(product_type, "IAS_02_O3_") == 0)
    {
        if (strcmp(product_type, "IAS_02_CO_") == 0)
        {
            variable_name_in = "atmosphere_mass_content_of_carbon_monoxide";
            variable_name_out = "CO_column_density";
            description = "Integrated CO";
        }
        else if (strcmp(product_type, "IAS_02_NAC") == 0)
        {
            variable_name_in = "atmosphere_mass_content_of_nitric_acid";
            variable_name_out = "NH3_column_density";
            description = "Integrated NH3";
        }
        else if (strcmp(product_type, "IAS_02_O3_") == 0)
        {
            variable_name_in = "atmosphere_mass_content_of_ozone";
            variable_name_out = "O3_column_density";
            description = "Integrated ozone";
        }
        variable_definition =
            harp_ingestion_register_variable_full_read(product_definition, variable_name_out, harp_type_float, 1,
                                                       dimension_type_1d, NULL, description, "kg/m2", NULL,
                                                       read_data_column_density);
        snprintf(path, MAX_PATH_LENGTH, "/data/%s[]", variable_name_in);
        description = NULL;
        harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, description);
    }

    /* dust */
    if (strcmp(product_type, "IAS_02_CLD") == 0 || strcmp(product_type, "IAS_02_SFC") == 0)
    {
        variable_name_in = "dust_indicator";
        variable_name_out = "dust";
        description = "Indicator of dust (more likely for higher values)";

        variable_definition =
            harp_ingestion_register_variable_full_read(product_definition, variable_name_out, harp_type_float, 1,
                                                       dimension_type_1d, NULL, description, HARP_UNIT_DIMENSIONLESS,
                                                       NULL, read_data_dust_indicator);
        snprintf(path, MAX_PATH_LENGTH, "/data/%s[]", variable_name_in);
        description = NULL;
        harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, description);
    }

}

static void register_geolocation_variables(harp_product_definition *product_definition)
{
    const char *path;
    const char *description;
    harp_variable_definition *variable_definition;

    harp_dimension_type dimension_type_1d[1] = { harp_dimension_time };

    /* time */
    description = "On-board time in UTC";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "time", harp_type_double, 1, dimension_type_1d,
                                                   NULL, description, "s", NULL, read_geolocation_time);

    path = "/data/geolocation_information/onboard_utc[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* longitude */
    description = "Geocentric longitude at sounder pixel centre";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "longitude", harp_type_float, 1,
                                                   dimension_type_1d, NULL, description, "degree_east", NULL,
                                                   read_geolocation_longitude);
    harp_variable_definition_set_valid_range_float(variable_definition, -180.0, 180.0);
    path = "/data/geolocation_information/sounder_pixel_longitude[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* latitude */
    description = "Geodetic latitude at sounder pixel centre";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "latitude", harp_type_float, 1,
                                                   dimension_type_1d, NULL, description, "degree_north", NULL,
                                                   read_geolocation_latitude);
    harp_variable_definition_set_valid_range_float(variable_definition, -90.0, 90.0);
    path = "/data/geolocation_information/sounder_pixel_latitude[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* solar_azimuth_angle */
    description = "Solar azimuth angle at sounder pixel centre";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "solar_azimuth_angle", harp_type_float, 1,
                                                   dimension_type_1d, NULL, description, "degree", NULL,
                                                   read_geolocation_solar_azimuth_angle);
    harp_variable_definition_set_valid_range_float(variable_definition, 0.0, 360.0);
    path = "/data/geolocation_information/sounder_pixel_sun_azimuth[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* solar_zenith_angle */
    description = "Solar zenith angle at sounder pixel centre";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "solar_zenith_angle", harp_type_float,
                                                   1, dimension_type_1d, NULL, description, "degree", NULL,
                                                   read_geolocation_solar_zenith_angle);
    harp_variable_definition_set_valid_range_float(variable_definition, 0.0, 180.0);
    path = "/data/geolocation_information/sounder_pixel_sun_zenith[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* sensor_azimuth_angle */
    description = "Measurement azimuth angle at sounder pixel centre";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "sensor_azimuth_angle", harp_type_float, 1,
                                                   dimension_type_1d, NULL, description, "degree", NULL,
                                                   read_geolocation_sensor_azimuth_angle);
    harp_variable_definition_set_valid_range_float(variable_definition, 0.0, 360.0);
    path = "/data/geolocation_information/sounder_pixel_azimuth[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* sensor_zenith_angle */
    description = "Measurement zenith angle at sounder pixel centre";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "sensor_zenith_angle", harp_type_float, 1,
                                                   dimension_type_1d, NULL, description, "degree", NULL,
                                                   read_geolocation_sensor_zenith_angle);
    harp_variable_definition_set_valid_range_float(variable_definition, 0.0, 180.0);
    path = "/data/geolocation_information/sounder_pixel_zenith[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);
}

static void register_surface_variables(harp_product_definition *product_definition)
{
    const char *path;
    const char *description;
    harp_variable_definition *variable_definition;
    harp_dimension_type dimension_type_1d[1] = { harp_dimension_time };

    /* ice_fraction */
    description = "Fraction of IFOV covered by sea ice";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "ice_fraction", harp_type_float, 1,
                                                   dimension_type_1d, NULL, description, HARP_UNIT_DIMENSIONLESS, NULL,
                                                   read_surface_ice_fraction);
    path = "/data/surface_info/ice_fraction[]";
    description = NULL;
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, description);

    /* land_fraction */
    description = "Land fraction";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "land_fraction", harp_type_float, 1,
                                                   dimension_type_1d, NULL, description, HARP_UNIT_DIMENSIONLESS, NULL,
                                                   read_surface_land_fraction);
    path = "/data/surface_info/land_fraction[]";
    description = NULL;
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, description);

    /* surface_altitude */
    description = "Surface elevation";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "surface_altitude", harp_type_float, 1,
                                                   dimension_type_1d, NULL, description, "m", NULL,
                                                   read_surface_height);
    path = "/data/surface_info/height[]";
    description = NULL;
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, description);

    /* surface_altitude */
    description = "Standard deviation of surface elevation";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "surface_altitude_uncertainty", harp_type_float,
                                                   1, dimension_type_1d, NULL, description, "m", NULL,
                                                   read_surface_height_std);
    path = "/data/surface_info/height_std[]";
    description = NULL;
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, description);
}

static void register_statistical_variables(harp_product_definition *product_definition, const char *product_type)
{
    char path[MAX_PATH_LENGTH];
    const char *description;
    const char *variable_name_in;
    const char *variable_name_out;

    harp_variable_definition *variable_definition;
    harp_dimension_type dimension_type_1d[1] = { harp_dimension_time };

    /* surface_temperature */
    if (strcmp(product_type, "IAS_02_SFC") == 0)
    {
        variable_name_in = "surface_temperature";
        variable_name_out = "surface_temperature";
        description = "A-priori surface skin temperature";
        variable_definition =
            harp_ingestion_register_variable_full_read(product_definition, variable_name_out, harp_type_float, 1,
                                                       dimension_type_1d, NULL, description, "K", NULL,
                                                       read_statistical_surface_temperature);

        snprintf(path, MAX_PATH_LENGTH, "/data/statistical_retrieval/%s[]", variable_name_in);
        harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);
    }

    /* surface_pressure */
    if (strcmp(product_type, "IAS_02_SFC") == 0)
    {
        variable_name_in = "surface_air_pressure";
        variable_name_out = "surface_pressure";
        description = "Surface pressure";
        variable_definition =
            harp_ingestion_register_variable_full_read(product_definition, variable_name_out, harp_type_float, 1,
                                                       dimension_type_1d, NULL, description, "hPa", NULL,
                                                       read_statistical_surface_air_pressure);

        snprintf(path, MAX_PATH_LENGTH, "/data/statistical_retrieval/%s[]", variable_name_in);
        harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);
    }

}


static void register_co_product(void)
{
    harp_ingestion_module *module;
    harp_product_definition *product_definition;

    const char *product_type = "IAS_02_CO_";

    /* Product Registration Phase */
    module =
        harp_ingestion_register_module("IAS_02_CO", "IASI-NG", "EPS_SG", product_type,
                                       "IASI-NG L2 CO total column densities", ingestion_init, ingestion_done);

    /* harp_ingestion_register_product( module ptr, "ProductShortName", options table (NULL), dimension-callback ) */
    product_definition = harp_ingestion_register_product(module, "IAS_02_CO", NULL, read_dimensions);

    /* Variables' Registration Phase */

    register_core_variables(product_definition);
    register_data_variables(product_definition, product_type);
    register_geolocation_variables(product_definition);

}

static void register_nac_product(void)
{
    harp_ingestion_module *module;
    harp_product_definition *product_definition;

    const char *product_type = "IAS_02_NAC";

    /* Product Registration Phase */
    module =
        harp_ingestion_register_module("IAS_02_NAC", "IASI-NG", "EPS_SG", product_type,
                                       "IASI-NG L2 NAC total column densities", ingestion_init, ingestion_done);

    /* harp_ingestion_register_product( module ptr, "ProductShortName", options table (NULL), dimension-callback ) */
    product_definition = harp_ingestion_register_product(module, "IAS_02_NAC", NULL, read_dimensions);

    /* Variables' Registration Phase */

    register_core_variables(product_definition);
    register_data_variables(product_definition, product_type);
    register_geolocation_variables(product_definition);

}

static void register_o3_product(void)
{
    harp_ingestion_module *module;
    harp_product_definition *product_definition;

    const char *product_type = "IAS_02_O3_";

    /* Product Registration Phase */
    module =
        harp_ingestion_register_module("IAS_02_O3", "IASI-NG", "EPS_SG", product_type,
                                       "IASI-NG L2 O3 total column densities", ingestion_init, ingestion_done);

    /* harp_ingestion_register_product( module ptr, "ProductShortName", options table (NULL), dimension-callback ) */
    product_definition = harp_ingestion_register_product(module, "IAS_02_O3", NULL, read_dimensions);

    /* Variables' Registration Phase */

    register_core_variables(product_definition);
    register_data_variables(product_definition, product_type);
    register_geolocation_variables(product_definition);

}

static void register_so2_product(void)
{
    harp_ingestion_module *module;
    harp_product_definition *product_definition;

    const char *product_type = "IAS_02_SO2";

    /* Product Registration Phase */
    module =
        harp_ingestion_register_module("IAS_02_O3", "IASI-NG", "EPS_SG", product_type,
                                       "IASI-NG L2 SO2 total column densities", ingestion_init, ingestion_done);

    /* harp_ingestion_register_product( module ptr, "ProductShortName", options table (NULL), dimension-callback ) */
    product_definition = harp_ingestion_register_product(module, "IAS_02_SO2", NULL, read_dimensions);

    /* Variables' Registration Phase */

    register_core_variables(product_definition);
    register_data_variables(product_definition, product_type);
    register_geolocation_variables(product_definition);

}

static void register_cld_product(void)
{
    harp_ingestion_module *module;
    harp_product_definition *product_definition;

    const char *product_type = "IAS_02_CLD";

    /* Product Registration Phase */
    module =
        harp_ingestion_register_module("IAS_02_CLD", "IASI-NG", "EPS_SG", product_type,
                                       "IASI-NG L2 CLD total column densities", ingestion_init, ingestion_done);

    /* harp_ingestion_register_product( module ptr, "ProductShortName", options table (NULL), dimension-callback ) */
    product_definition = harp_ingestion_register_product(module, "IAS_02_CLD", NULL, read_dimensions);

    /* Variables' Registration Phase */

    register_core_variables(product_definition);
    register_data_variables(product_definition, product_type);
    register_surface_variables(product_definition);
    register_geolocation_variables(product_definition);

}

static void register_ghg_product(void)
{
    char path[MAX_PATH_LENGTH];
    const char *description;
    const char *variable_name_in;
    const char *variable_name_out;

    harp_ingestion_module *module;
    harp_product_definition *product_definition;
    harp_variable_definition *variable_definition;

    harp_dimension_type dimension_type_2d[2] = { harp_dimension_time, harp_dimension_vertical };

    const char *product_type = "IAS_02_GHG";

    /* Product Registration Phase */
    module =
        harp_ingestion_register_module("IAS_02_GHG", "IASI-NG", "EPS_SG", product_type,
                                       "IASI-NG L2 GHG total column densities", ingestion_init, ingestion_done);

    /* harp_ingestion_register_product( module ptr, "ProductShortName", options table (NULL), dimension-callback ) */
    product_definition = harp_ingestion_register_product(module, "IAS_02_GHG", NULL, read_dimensions);

    /* Variables' Registration Phase */

    register_core_variables(product_definition);
    register_data_variables(product_definition, product_type);
    register_surface_variables(product_definition);
    register_geolocation_variables(product_definition);

    /* NO2_column_density */
    variable_name_in = "atmosphere_mass_content_of_nitrous_oxide";
    variable_name_out = "NO2_column_density";
    description = "Coarse N2O profile";

    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, variable_name_out, harp_type_float, 2,
                                                   dimension_type_2d, NULL, description, "kg/m2", NULL,
                                                   read_data_nitrous_oxide_column_density);
    snprintf(path, MAX_PATH_LENGTH, "/data/%s[]", variable_name_in);
    description = NULL;
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, description);

    /* CH4_column_density */
    variable_name_in = "atmosphere_mass_content_of_nitrous_oxide";
    variable_name_out = "CH4_column_density";
    description = "Coarse CH4 profile";

    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, variable_name_out, harp_type_float, 2,
                                                   dimension_type_2d, NULL, description, "kg/m2", NULL,
                                                   read_data_methane_column_density);
    snprintf(path, MAX_PATH_LENGTH, "/data/%s[]", variable_name_in);
    description = NULL;
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, description);

    /* CO2_column_density */
    variable_name_in = "atmosphere_mass_content_of_nitrous_oxide";
    variable_name_out = "CO2_column_density";
    description = "Coarse CO2 profile";

    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, variable_name_out, harp_type_float, 2,
                                                   dimension_type_2d, NULL, description, "kg/m2", NULL,
                                                   read_data_carbon_dioxide_column_density);
    snprintf(path, MAX_PATH_LENGTH, "/data/%s[]", variable_name_in);
    description = NULL;
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, description);
}

static void register_sfc_product(void)
{
    const char *path;
    const char *description;

    harp_ingestion_module *module;
    harp_product_definition *product_definition;
    harp_variable_definition *variable_definition;

    harp_dimension_type dimension_type_1d[1] = { harp_dimension_time };

    const char *product_type = "IAS_02_SFC";

    /* Product Registration Phase */
    module =
        harp_ingestion_register_module("IAS_02_SFC", "IASI-NG", "EPS_SG", product_type,
                                       "IASI-NG L2 SFC total column densities", ingestion_init, ingestion_done);

    /* harp_ingestion_register_product( module ptr, "ProductShortName", options table (NULL), dimension-callback ) */
    product_definition = harp_ingestion_register_product(module, "IAS_02_SFC", NULL, read_dimensions);

    /* Variables' Registration Phase */

    register_core_variables(product_definition);
    /* only dust is taken from this one */
    register_data_variables(product_definition, product_type);
    register_statistical_variables(product_definition, product_type);
    register_surface_variables(product_definition);
    register_geolocation_variables(product_definition);

    /* wind_speed */
    description = "10m wind speed";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "wind_speed", harp_type_float, 1,
                                                   dimension_type_1d, NULL, description, "m/s", NULL,
                                                   read_l2p_sst_wind_speed);
    path = "/data/l2p_sst/wind_speed[]";
    description = NULL;
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, description);

}

static void register_twv_product(void)
{
    harp_ingestion_module *module;
    harp_product_definition *product_definition;

    const char *product_type = "IAS_02_TWV";

    /* Product Registration Phase */
    module =
        harp_ingestion_register_module("IAS_02_TWV", "IASI-NG", "EPS_SG", product_type,
                                       "IASI-NG L2 TWV total column densities", ingestion_init, ingestion_done);

    /* harp_ingestion_register_product( module ptr, "ProductShortName", options table (NULL), dimension-callback ) */
    product_definition = harp_ingestion_register_product(module, "IAS_02_TWV", NULL, read_dimensions);

    /* Variables' Registration Phase */

    register_core_variables(product_definition);
    /* only dust is taken from this one */
    register_data_variables(product_definition, product_type);
    register_statistical_variables(product_definition, product_type);
    register_surface_variables(product_definition);
    register_geolocation_variables(product_definition);

}

int harp_ingestion_module_iasi_ng_l2_init(void)
{
    register_co_product();
    register_nac_product();
    register_o3_product();
    register_so2_product();
    register_cld_product();
    register_ghg_product();
    register_sfc_product();
    register_twv_product();

    return 0;
}

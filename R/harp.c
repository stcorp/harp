#include <harp.h>

#include <R.h>
#include <Rinternals.h>
#include <Rdefines.h>

const char *dimension_name[6] = {
    "independent",
    "time",
    "latitude",
    "longitude",
    "vertical",
    "spectral",
};

const char *data_types[6] = {
    "int8",
    "int16",
    "int32",
    "float",
    "double",
    "string",
};

/* make R "string" (string vector of length 1) */
SEXP mkstring(const char *x)
{
    SEXP str = PROTECT(allocVector(STRSXP, 1));

    SET_STRING_ELT(str, 0, mkChar(x));
    return str;
}

/* return list element with given name */
SEXP rharp_named_element(SEXP l, const char *name)
{
    SEXP names = getAttrib(l, R_NamesSymbol);

    for (R_len_t i = 0; i < length(l); i++)
    {
        const char *attrname = CHAR(STRING_ELT(names, i));

        if (strcmp(attrname, name) == 0)
        {
            return VECTOR_ELT(l, i);
        }
    }
    return R_NilValue;
}

/* report errors to R */
void rharp_error()
{
    error(harp_errno_to_string(harp_errno));
}

void rharp_var_error(const char *varname)
{
    error("variable '%s': %s", varname, harp_errno_to_string(harp_errno));
}

void var_error(const char *varname, char *msg)
{
    error("variable '%s': %s", varname, msg);
}

/* create R variable from harp variable */
SEXP rharp_import_variable(harp_variable *hv)
{
    /* create variable (named list) */
    const char *varfields[] =
        { "name", "description", "unit", "data", "dimension", "type", "enum", "valid_min", "valid_max", "" };
    SEXP dim, data, var;
    int protected = 1;
    const char *datatype;
    int int_valid_min, int_valid_max;
    double double_valid_min, double_valid_max;
    int k;

    var = PROTECT(mkNamed(VECSXP, varfields));

    if (hv->data_type == harp_type_int8)
    {
        int_valid_min = hv->valid_min.int8_data;
        int_valid_max = hv->valid_max.int8_data;
        datatype = "int8";
    }
    else if (hv->data_type == harp_type_int16)
    {
        int_valid_min = hv->valid_min.int16_data;
        int_valid_max = hv->valid_max.int16_data;
        datatype = "int16";
    }
    else if (hv->data_type == harp_type_int32)
    {
        int_valid_min = hv->valid_min.int32_data;
        int_valid_max = hv->valid_max.int32_data;
        datatype = "int32";
    }
    else if (hv->data_type == harp_type_float)
    {
        double_valid_min = hv->valid_min.float_data;
        double_valid_max = hv->valid_max.float_data;
        datatype = "float";
    }
    else if (hv->data_type == harp_type_double)
    {
        double_valid_min = hv->valid_min.double_data;
        double_valid_max = hv->valid_max.double_data;
        datatype = "double";
    }
    else if (hv->data_type == harp_type_string)
    {
        datatype = "string";
    }
    else
    {
        var_error(hv->name, "unsupported data type");
    }

    if (hv->num_dimensions == 0)
    {
        /* scalar */
        /* TODO check num_elements */
        if (hv->num_elements != 1)
        {
            var_error(hv->name, "not exactly 1 element for scalar");
        }

        if (hv->data_type == harp_type_int8)
        {
            data = PROTECT(allocVector(INTSXP, 1));
            protected++;
            INTEGER(data)[0] = hv->data.int8_data[0];
        }
        else if (hv->data_type == harp_type_int16)
        {
            data = PROTECT(allocVector(INTSXP, 1));
            protected++;
            INTEGER(data)[0] = hv->data.int16_data[0];
        }
        else if (hv->data_type == harp_type_int32)
        {
            data = PROTECT(allocVector(INTSXP, 1));
            protected++;
            INTEGER(data)[0] = hv->data.int32_data[0];
        }
        else if (hv->data_type == harp_type_float)
        {
            data = PROTECT(allocVector(REALSXP, 1));
            protected++;
            REAL(data)[0] = hv->data.float_data[0];
        }
        else if (hv->data_type == harp_type_double)
        {
            data = PROTECT(allocVector(REALSXP, 1));
            protected++;
            REAL(data)[0] = hv->data.double_data[0];
        }
        else if (hv->data_type == harp_type_string)
        {
            data = PROTECT(allocVector(STRSXP, 1));
            protected++;
            SET_STRING_ELT(data, 0, mkChar(hv->data.string_data[0]));
        }

    }
    else
    {
        /* array: reverse dimensions because of row-major/minor mismatch */
        dim = PROTECT(Rf_allocVector(INTSXP, hv->num_dimensions));
        protected++;
        for (k = 0; k < hv->num_dimensions; k++)
        {
            INTEGER(dim)[hv->num_dimensions - 1 - k] = hv->dimension[k];
        }

        /* convert data */
        if (hv->data_type == harp_type_int8)
        {
            data = PROTECT(Rf_allocArray(INTSXP, dim));
            protected++;
            for (k = 0; k < hv->num_elements; k++)
            {
                INTEGER(data)[k] = hv->data.int8_data[k];
            }
        }
        else if (hv->data_type == harp_type_int16)
        {
            data = PROTECT(Rf_allocArray(INTSXP, dim));
            protected++;
            for (k = 0; k < hv->num_elements; k++)
            {
                INTEGER(data)[k] = hv->data.int16_data[k];
            }
        }
        else if (hv->data_type == harp_type_int32)
        {
            data = PROTECT(Rf_allocArray(INTSXP, dim));
            protected++;
            for (k = 0; k < hv->num_elements; k++)
            {
                INTEGER(data)[k] = hv->data.int32_data[k];
            }
        }
        else if (hv->data_type == harp_type_float)
        {
            data = PROTECT(Rf_allocArray(REALSXP, dim));
            protected++;
            for (k = 0; k < hv->num_elements; k++)
            {
                REAL(data)[k] = hv->data.float_data[k];
            }
        }
        else if (hv->data_type == harp_type_double)
        {
            data = PROTECT(Rf_allocArray(REALSXP, dim));
            protected++;
            for (k = 0; k < hv->num_elements; k++)
            {
                REAL(data)[k] = hv->data.double_data[k];
            }
        }
        else if (hv->data_type == harp_type_string)
        {
            data = PROTECT(Rf_allocArray(STRSXP, dim));
            protected++;
            for (k = 0; k < hv->num_elements; k++)
            {
                SET_STRING_ELT(data, k, mkChar(hv->data.string_data[k]));
            }
        }
    }

    /* set name */
    SET_VECTOR_ELT(var, 0, mkstring(hv->name)); /* TODO: use mkString()? shouldn't we PROTECT the mkstring() result? */
    protected++;

    /* set description */
    if (hv->description)
    {
        SET_VECTOR_ELT(var, 1, mkstring(hv->description));
        protected++;
    }

    /* set unit */
    if (hv->unit)
    {
        SET_VECTOR_ELT(var, 2, mkstring(hv->unit));
        protected++;
    }

    /* set data */
    SET_VECTOR_ELT(var, 3, data);

    /* set dimension */
    dim = PROTECT(allocVector(STRSXP, hv->num_dimensions));

    protected++;
    for (k = 0; k < hv->num_dimensions; k++)
    {
        SET_STRING_ELT(dim, hv->num_dimensions - 1 - k, mkChar(dimension_name[hv->dimension_type[k] + 1]));
    }
    SET_VECTOR_ELT(var, 4, dim);

    /* set type */
    SET_VECTOR_ELT(var, 5, mkstring(datatype));
    protected++;

    /* set enum */
    if (hv->num_enum_values)
    {
        SEXP senum = PROTECT(allocVector(STRSXP, hv->num_enum_values));

        protected++;
        for (k = 0; k < hv->num_enum_values; k++)
        {
            SET_STRING_ELT(senum, k, mkChar(hv->enum_name[k]));
        }
        SET_VECTOR_ELT(var, 6, senum);
    }

    /* set valid min/max */
    if (hv->data_type == harp_type_float || hv->data_type == harp_type_double)
    {
        SEXP svalidmin, svalidmax;

        svalidmin = PROTECT(allocVector(REALSXP, 1));
        protected++;
        REAL(svalidmin)[0] = double_valid_min;
        SET_VECTOR_ELT(var, 7, svalidmin);

        svalidmax = PROTECT(allocVector(REALSXP, 1));
        protected++;
        REAL(svalidmax)[0] = double_valid_max;
        SET_VECTOR_ELT(var, 8, svalidmax);
    }
    else if (hv->data_type == harp_type_int8 || hv->data_type == harp_type_int16 || hv->data_type == harp_type_int32)
    {
        SEXP svalidmin, svalidmax;

        svalidmin = PROTECT(allocVector(INTSXP, 1));
        protected++;
        INTEGER(svalidmin)[0] = int_valid_min;
        SET_VECTOR_ELT(var, 7, svalidmin);

        svalidmax = PROTECT(allocVector(INTSXP, 1));
        protected++;
        INTEGER(svalidmax)[0] = int_valid_max;
        SET_VECTOR_ELT(var, 8, svalidmax);
    }

    UNPROTECT(protected);
    return var;
}

/* create harp variable from R variable */
harp_variable *rharp_export_variable(SEXP var, const char *name)
{
    SEXP sdescription, sname, sunit, sdata, sdimension, senum, svalidmin, svalidmax, stype, sdimlens;
    unsigned int dimlens;
    harp_variable *hv;
    long dim[HARP_MAX_NUM_DIMS];
    harp_dimension_type dim_type[HARP_MAX_NUM_DIMS];
    int num_dims = 0;
    const char *description = NULL;
    const char *dtype;
    const char *unit = NULL;
    int num_elements = 0;
    int datatype;
    int i, j, k;

    /* check 'name' field */
    sname = rharp_named_element(var, "name");
    if (sname != R_NilValue)
    {
        if (TYPEOF(sname) != STRSXP || LENGTH(sname) != 1)
        {
            var_error(name, "'name' field not a string");
        }
        else if (strcmp(name, CHAR(STRING_ELT(sname, 0))) != 0)
        {
            var_error(name, "'name' field inconsistent");
        }
    }

    /* check 'description' field */
    sdescription = rharp_named_element(var, "description");
    if (sdescription != R_NilValue)
    {
        if (TYPEOF(sdescription) != STRSXP || LENGTH(sdescription) != 1)
        {
            var_error(name, "'description' field not a string");
        }
        description = CHAR(STRING_ELT(sdescription, 0));
    }

    /* check 'unit' field */
    sunit = rharp_named_element(var, "unit");
    if (sunit != R_NilValue)
    {
        if (TYPEOF(sunit) != STRSXP || LENGTH(sunit) != 1)
        {
            var_error(name, "'unit' field not a string");
        }
        unit = CHAR(STRING_ELT(sunit, 0));
    }

    /* check 'data' field */
    sdata = rharp_named_element(var, "data");
    if (sdata == R_NilValue)
    {
        var_error(name, "no 'data' field");
    }

    /* check 'dimension' field */
    sdimension = rharp_named_element(var, "dimension");
    if (sdimension == R_NilValue)
    {
        var_error(name, "no 'dimension' field");
    }
    if (TYPEOF(sdimension) != STRSXP)
    {
        var_error(name, "'dimension' field not a string vector");
    }
    num_dims = length(sdimension);

    /* check 'enum' field */
    senum = rharp_named_element(var, "enum");
    if (senum != R_NilValue)
    {
        if (TYPEOF(senum) != STRSXP)
        {
            var_error(name, "'enum' field not a string vector");
        }
    }

    /* check 'valid_min' field */
    svalidmin = rharp_named_element(var, "valid_min");

    /* check 'valid_max' field */
    svalidmax = rharp_named_element(var, "valid_max");

    /* check 'type' field */
    stype = rharp_named_element(var, "type");
    if (stype != R_NilValue)
    {
        int found = 0;

        if (TYPEOF(stype) != STRSXP || LENGTH(stype) != 1)
        {
            var_error(name, "'type' field not a string");
        }
        dtype = CHAR(STRING_ELT(stype, 0));

        for (k = 0; k < 6; k++)
        {
            if (strcmp(dtype, data_types[k]) == 0)
            {
                found = 1;
                break;
            }
        }
        if (!found)
        {
            var_error(name, "unknown data type");
        }
    }

    /* get dimension types (reversing dimensions) */
    for (j = 0; j < num_dims; j++)
    {
        const char *dimname = CHAR(STRING_ELT(sdimension, j));
        int found = 0;

        for (k = 0; k < 6; k++)
        {
            if (strcmp(dimname, dimension_name[k]) == 0)
            {
                dim_type[num_dims - 1 - j] = k - 1;
                found = 1;
                break;
            }
        }
        if (!found)
        {
            var_error(name, "unknown dimension");
        }
    }

    /* get dimension lengths (reversing dimensions) */
    sdimlens = getAttrib(sdata, R_DimSymbol);
    if (sdimlens == R_NilValue)
    {
        dimlens = 0;
    }
    else
    {
        dimlens = LENGTH(sdimlens);
    }
    if (dimlens != num_dims)
    {
        var_error(name, "'data' dimensions inconsistent with 'dimensions'");
    }

    for (j = 0; j < dimlens; j++)
    {
        int dimlen = INTEGER(sdimlens)[j];

        dim[num_dims - 1 - j] = dimlen;
        num_elements = (num_elements ? num_elements : 1) * dimlen;
    }

    /* convert data */
    datatype = TYPEOF(sdata);
    harp_data_type hdatatype;

    if (num_dims == 0)
    {
        /* scalar */
        if (datatype == INTSXP)
        {
            hdatatype = harp_type_int32;
            if (harp_variable_new(name, hdatatype, num_dims, dim_type, dim, &hv) != 0)
            {
                rharp_error();
            }
            hv->data.int32_data[0] = INTEGER(sdata)[0];
        }
        else if (datatype == REALSXP)
        {
            hdatatype = harp_type_double;
            if (harp_variable_new(name, hdatatype, num_dims, dim_type, dim, &hv) != 0)
            {
                rharp_error();
            }
            hv->data.double_data[0] = REAL(sdata)[0];
        }
        else if (datatype == STRSXP)
        {
            hdatatype = harp_type_string;
            if (harp_variable_new(name, hdatatype, num_dims, dim_type, dim, &hv) != 0)
            {
                rharp_error();
            }
            hv->data.string_data[0] = strdup(CHAR(STRING_ELT(sdata, 0)));
        }
        else
        {
            var_error(name, "unsupported data type");
        }
    }
    else
    {
        /* array */
        if (!isArray(sdata))
        {
            var_error(name, "'data' field not an array");
        }

        if (datatype == INTSXP)
        {
            hdatatype = harp_type_int32;        /* R has no smaller datatypes */

            /* determine smallest fitting storage size */
            int min_value = 0;
            int max_value = 0;

            for (j = 0; j < num_elements; j++)
            {
                int value = INTEGER(sdata)[j];

                if (value < min_value)
                {
                    min_value = value;
                }
                if (value > max_value)
                {
                    max_value = value;
                }
            }

            if (svalidmin != R_NilValue)
            {
                int validmin = INTEGER(svalidmin)[0];

                if (validmin < min_value)
                {
                    min_value = validmin;
                }
            }

            if (svalidmax != R_NilValue)
            {
                int validmax = INTEGER(svalidmax)[0];

                if (validmax > max_value)
                {
                    max_value = validmax;
                }
            }

            if (min_value >= -128 && max_value <= 127)
            {
                hdatatype = harp_type_int8;
            }
            else if (min_value >= -32768 && max_value <= 32767)
            {
                hdatatype = harp_type_int16;
            }
            else
            {
                hdatatype = harp_type_int32;
            }

            /* create variable */
            if (harp_variable_new(name, hdatatype, num_dims, dim_type, dim, &hv) != 0)
            {
                rharp_error();
            }

            /* copy over data */
            for (j = 0; j < num_elements; j++)
            {
                if (hdatatype == harp_type_int8)
                {
                    hv->data.int8_data[j] = INTEGER(sdata)[j];
                }
                else if (hdatatype == harp_type_int16)
                {
                    hv->data.int16_data[j] = INTEGER(sdata)[j];
                }
                else
                {
                    hv->data.int32_data[j] = INTEGER(sdata)[j];
                }
            }
        }
        else if (datatype == REALSXP)
        {
            hdatatype = harp_type_double;       /* R has no smaller datatype */

            if (stype != R_NilValue && strcmp(dtype, "float") == 0)
            {
                hdatatype = harp_type_float;
            }

            if (harp_variable_new(name, hdatatype, num_dims, dim_type, dim, &hv) != 0)
            {
                rharp_error();
            }

            for (j = 0; j < num_elements; j++)
            {
                if (hdatatype == harp_type_float)
                {
                    hv->data.float_data[j] = REAL(sdata)[j];
                }
                else
                {
                    hv->data.double_data[j] = REAL(sdata)[j];
                }
            }
        }
        else if (datatype == STRSXP)
        {
            hdatatype = harp_type_string;

            if (harp_variable_new(name, hdatatype, num_dims, dim_type, dim, &hv) != 0)
            {
                rharp_error();
            }

            for (j = 0; j < num_elements; j++)
            {
                hv->data.string_data[j] = strdup(CHAR(STRING_ELT(sdata, j)));
            }
        }
        else
        {
            var_error(name, "unsupported data type");
        }
    }

    /* set description */
    if (description)
    {
        if (harp_variable_set_description(hv, description) != 0)
        {
            rharp_error();
        }
    }

    /* set unit */
    if (unit)
    {
        if (harp_variable_set_unit(hv, unit) != 0)
        {
            rharp_error();
        }
    }

    /* set enum */
    if (length(senum))
    {
        const char **enumvals;

        enumvals = (const char **)malloc(length(senum) * sizeof(const char *));
        if (enumvals == NULL)
        {
            error("out of memory");
        }
        for (i = 0; i < length(senum); i++)
        {
            enumvals[i] = CHAR(STRING_ELT(senum, i));
        }
        if (harp_variable_set_enumeration_values(hv, length(senum), enumvals) != 0)
        {
            rharp_error();
        }
        free(enumvals);
    }

    /* set valid min/max */
    if (hdatatype == harp_type_int8)
    {
        if (svalidmin != R_NilValue)
        {
            hv->valid_min.int8_data = INTEGER(svalidmin)[0];
        }
        if (svalidmax != R_NilValue)
        {
            hv->valid_max.int8_data = INTEGER(svalidmax)[0];
        }
    }
    else if (hdatatype == harp_type_int16)
    {
        if (svalidmin != R_NilValue)
        {
            hv->valid_min.int16_data = INTEGER(svalidmin)[0];
        }
        if (svalidmax != R_NilValue)
        {
            hv->valid_max.int16_data = INTEGER(svalidmax)[0];
        }
    }
    else if (hdatatype == harp_type_int32)
    {
        if (svalidmin != R_NilValue)
        {
            hv->valid_min.int32_data = INTEGER(svalidmin)[0];
        }
        if (svalidmax != R_NilValue)
        {
            hv->valid_max.int32_data = INTEGER(svalidmax)[0];
        }
    }
    else if (hdatatype == harp_type_float)
    {
        if (svalidmin != R_NilValue)
        {
            hv->valid_min.float_data = REAL(svalidmin)[0];
        }
        if (svalidmax != R_NilValue)
        {
            hv->valid_max.float_data = REAL(svalidmax)[0];
        }
    }
    else if (hdatatype == harp_type_double)
    {
        if (svalidmin != R_NilValue)
        {
            hv->valid_min.double_data = REAL(svalidmin)[0];
        }
        if (svalidmax != R_NilValue)
        {
            hv->valid_max.double_data = REAL(svalidmax)[0];
        }
    }

    return hv;
}

/* import harp product as R nested lists */
SEXP rharp_import_product(SEXP sname, SEXP soperations, SEXP soptions)
{
    SEXP product;
    const char **productfields;
    harp_product *hp;
    const char *filename;
    const char *operations = NULL;
    const char *options = NULL;
    int protected = 0;
    int i;

    /* check filename */
    if (TYPEOF(sname) != STRSXP || LENGTH(sname) != 1)
    {
        error("filename argument not a string");
    }
    filename = CHAR(STRING_ELT(sname, 0));

    /* check operations */
    if (soperations != R_NilValue)
    {
        if (TYPEOF(soperations) != STRSXP || LENGTH(soperations) != 1)
        {
            error("operations argument not a string");
        }
        operations = CHAR(STRING_ELT(soperations, 0));
    }

    /* check options */
    if (soptions != R_NilValue)
    {
        if (TYPEOF(soptions) != STRSXP || LENGTH(soptions) != 1)
        {
            error("soptions argument not a string");
        }
        options = CHAR(STRING_ELT(soptions, 0));
    }

    /* harp import */
    if (harp_import(filename, operations, options, &hp) != 0)
    {
        rharp_error();
    }

    /* create product (named list) */
    productfields = (const char **)malloc((hp->num_variables + 3) * sizeof(const char *));
    if (productfields == NULL)
    {
        error("out of memory");
    }
    productfields[0] = "source_product";
    productfields[1] = "history";
    for (i = 0; i < hp->num_variables; i++)
    {
        productfields[i + 2] = hp->variable[i]->name;
    }
    productfields[hp->num_variables + 2] = "";

    product = PROTECT(mkNamed(VECSXP, productfields));
    protected++;
    if (hp->source_product)
    {
        SET_VECTOR_ELT(product, 0, mkstring(hp->source_product));
        protected++;
    }
    if (hp->history)
    {
        SET_VECTOR_ELT(product, 1, mkstring(hp->history));
        protected++;
    }

    /* add variables */
    for (i = 0; i < hp->num_variables; i++)
    {
        harp_variable *hv = hp->variable[i];
        SEXP var = rharp_import_variable(hv);   /* TODO: shouldn't we PROTECT var? */

        SET_VECTOR_ELT(product, i + 2, var);
    }

    /* cleanup */
    UNPROTECT(protected);
    free(productfields);

    return product;
}

/* export R nested lists as harp product */
SEXP rharp_export_product(SEXP product, SEXP sfilename, SEXP sformat)
{
    SEXP names;
    const char *filename;
    const char *format;
    harp_product *hp;

    if (TYPEOF(product) != VECSXP)
    {
        error("product argument not a list");
    }
    if (TYPEOF(sfilename) != STRSXP || LENGTH(sfilename) != 1)
    {
        error("filename argument not a string");
    }
    if (TYPEOF(sformat) != STRSXP || LENGTH(sformat) != 1)
    {
        error("format argument not a string");
    }

    filename = CHAR(STRING_ELT(sfilename, 0));
    format = CHAR(STRING_ELT(sformat, 0));

    if (harp_product_new(&hp) != 0)
    {
        rharp_error();
    }

    /* product attributes */
    names = getAttrib(product, R_NamesSymbol);
    for (R_len_t i = 0; i < length(product); i++)
    {
        const char *attrname = CHAR(STRING_ELT(names, i));
        SEXP elmt = VECTOR_ELT(product, i);

        if (strcmp(attrname, "source_product") == 0)
        {
            /* set source_product */
            if (TYPEOF(elmt) != STRSXP || LENGTH(elmt) != 1)
            {
                error("'source_product' field not a string");
            }

            if (harp_product_set_source_product(hp, CHAR(STRING_ELT(elmt, 0))) != 0)
            {
                rharp_var_error(attrname);
            }
        }
        else if (strcmp(attrname, "history") == 0)
        {
            /* set history */
            if (elmt != R_NilValue)
            {
                if (TYPEOF(elmt) != STRSXP || LENGTH(elmt) != 1)
                {
                    error("'history' field not a string");
                }

                if (harp_product_set_history(hp, CHAR(STRING_ELT(elmt, 0))) != 0)
                {
                    rharp_var_error(attrname);
                }
            }
        }
        else
        {
            harp_variable *hv;

            /* set variable */
            if (TYPEOF(elmt) != VECSXP)
            {
                error("variable '%s' not a list", attrname);
            }

            hv = rharp_export_variable(elmt, attrname);
            if (harp_product_add_variable(hp, hv) != 0)
            {
                rharp_error(attrname);
            }
        }
    }

    if (harp_export(filename, format, hp) != 0)
    {
        rharp_error();
    }

    /* cleanup */
    harp_product_delete(hp);

    return R_NilValue;
}

SEXP rharp_version()
{
    SEXP sversion = mkstring(libharp_version);

    UNPROTECT(1);
    return sversion;
}

SEXP rharp_init(SEXP spath)
{
    const char *path = CHAR(STRING_ELT(spath, 0));

    harp_init();

    if (getenv("CODA_DEFINITION") == NULL)
    {
        harp_set_coda_definition_path_conditional("DESCRIPTION", path, "../../../../share/coda/definitions/");
    }
    if (getenv("UDUNITS2_XML_PATH") == NULL)
    {
        harp_set_udunits2_xml_path_conditional("DESCRIPTION", path, "../../../../share/harp/udunits2.xml");
    }

    return R_NilValue;
}

SEXP rharp_done()
{
    harp_done();
    return R_NilValue;
}

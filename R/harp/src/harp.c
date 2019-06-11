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

// make R "string" (string vector of length 1)
SEXP mkstring(const char *x) {
    SEXP str = PROTECT(allocVector(STRSXP, 1));
    SET_STRING_ELT(str, 0, mkChar(x));
    return str;
}

// return list element with given name
SEXP rharp_named_element(SEXP l, const char *name) {
    SEXP names = getAttrib(l, R_NamesSymbol);
    for (R_len_t i=0; i<length(l); i++) {
        const char *attrname = CHAR(STRING_ELT(names, i));
        if(strcmp(attrname, name) == 0)
            return VECTOR_ELT(l, i);
    }
    return R_NilValue;
}

// report errors to R
void rharp_error() {
    error(harp_errno_to_string(harp_errno));
}

void rharp_var_error(const char *varname) {
    error("variable '%s': %s", varname, harp_errno_to_string(harp_errno));
}

void var_error(const char *varname, char *msg) {
    error("variable '%s': %s", varname, msg);
}

// create R variable from harp variable
SEXP rharp_import_variable(harp_variable *hv) {
    // create variable (named list)
    const char *varfields[] = {"name", "description", "unit", "data", "dimension", "type", "enum", "valid_min", "valid_max", ""};
    int protected = 6;
    SEXP var = PROTECT(mkNamed(VECSXP, varfields));
    SEXP array;
    const char *datatype;

    // reverse dimensions because of row-major/minor mismatch
    SEXP dim;
    PROTECT(dim = Rf_allocVector(INTSXP, hv->num_dimensions));
    for(unsigned int k=0; k<hv->num_dimensions; k++) {
        INTEGER(dim)[hv->num_dimensions-1-k] = hv->dimension[k];
    }

    int int_valid_min, int_valid_max;
    double double_valid_min, double_valid_max;

    // convert data
    if(hv->data_type == harp_type_int8) {
        PROTECT(array = Rf_allocArray(INTSXP, dim));
        for(int k=0; k<hv->num_elements; k++)
           INTEGER(array)[k] = hv->data.int8_data[k];
        datatype = "integer";
        int_valid_min = hv->valid_min.int8_data;
        int_valid_max = hv->valid_max.int8_data;
    }
    else if(hv->data_type == harp_type_int16) {
        PROTECT(array = Rf_allocArray(INTSXP, dim));
        for(int k=0; k<hv->num_elements; k++)
           INTEGER(array)[k] = hv->data.int16_data[k];
        datatype = "integer";
        int_valid_min = hv->valid_min.int16_data;
        int_valid_max = hv->valid_max.int16_data;
    }
    else if(hv->data_type == harp_type_int32) {
        PROTECT(array = Rf_allocArray(INTSXP, dim));
        for(int k=0; k<hv->num_elements; k++)
           INTEGER(array)[k] = hv->data.int32_data[k];
        datatype = "integer";
        int_valid_min = hv->valid_min.int32_data;
        int_valid_max = hv->valid_max.int32_data;
    }
    else if(hv->data_type == harp_type_float) {
        PROTECT(array = Rf_allocArray(REALSXP, dim));
        for(int k=0; k<hv->num_elements; k++)
           REAL(array)[k] = hv->data.float_data[k];
        datatype = "real";
        double_valid_min = hv->valid_min.float_data;
        double_valid_max = hv->valid_max.float_data;
    }
    else if(hv->data_type == harp_type_double) {
        PROTECT(array = Rf_allocArray(REALSXP, dim));
        for(int k=0; k<hv->num_elements; k++)
           REAL(array)[k] = hv->data.double_data[k];
        datatype = "real";
        double_valid_min = hv->valid_min.double_data;
        double_valid_max = hv->valid_max.double_data;
    }
    else
        error("unsupported data type");

    // set name
    SET_VECTOR_ELT(var, 0, mkstring(hv->name));

    // set description
    if(hv->description) {
        SET_VECTOR_ELT(var, 1, mkstring(hv->description));
        protected += 1;
    }

    // set unit
    if(hv->unit) {
        SET_VECTOR_ELT(var, 2, mkstring(hv->unit));
        protected += 1;
    }

    // set data 
    SET_VECTOR_ELT(var, 3, array);

    // set dimension
    SEXP dimension = PROTECT(allocVector(STRSXP, hv->num_dimensions));
    for(unsigned int k=0; k<hv->num_dimensions; k++) {
       SET_STRING_ELT(dimension, hv->num_dimensions-1-k,
               mkChar(dimension_name[hv->dimension_type[k]+1]));
    }
    SET_VECTOR_ELT(var, 4, dimension);

    // set type
    SET_VECTOR_ELT(var, 5, mkstring(datatype));

    // set enum
    if(hv->num_enum_values) {
        SEXP senum = PROTECT(allocVector(STRSXP, hv->num_enum_values));
        protected += 1;
        for(unsigned int k=0; k<hv->num_enum_values; k++)
            SET_STRING_ELT(senum, k, mkChar(hv->enum_name[k]));
        SET_VECTOR_ELT(var, 6, senum);
    }

    // set valid min/max
    if(hv->data_type == harp_type_float || hv->data_type == harp_type_double) {
        SEXP svalidmin = PROTECT(allocVector(REALSXP, 1));
        protected += 1;
        REAL(svalidmin)[0] = double_valid_min;
        SET_VECTOR_ELT(var, 7, svalidmin);

        SEXP svalidmax = PROTECT(allocVector(REALSXP, 1));
        protected += 1;
        REAL(svalidmax)[0] = double_valid_max;
        SET_VECTOR_ELT(var, 8, svalidmax);
    }
    else {
        SEXP svalidmin = PROTECT(allocVector(INTSXP, 1));
        protected += 1;
        INTEGER(svalidmin)[0] = int_valid_min;
        SET_VECTOR_ELT(var, 7, svalidmin);

        SEXP svalidmax = PROTECT(allocVector(INTSXP, 1));
        protected += 1;
        INTEGER(svalidmax)[0] = int_valid_max;
        SET_VECTOR_ELT(var, 8, svalidmax);
    }

    UNPROTECT(protected);
    return var;
}

// create harp variable from R variable
harp_variable *rharp_export_variable(SEXP var, const char *name) {
    harp_variable *hv;
    long dim[HARP_MAX_NUM_DIMS];
    harp_dimension_type dim_type[HARP_MAX_NUM_DIMS];
    int num_dims = 0;
    const char *description = NULL;
    const char *unit = NULL;
    int num_elements = 0;
    int datatype;

    // check 'name' field
    SEXP sname = rharp_named_element(var, "name");
    if(sname != R_NilValue) {
       if (TYPEOF(sname) != STRSXP || LENGTH(sname) != 1)
           var_error(name, "'name' field not a string");
       else if(strcmp(name, CHAR(STRING_ELT(sname, 0))) != 0)
           var_error(name, "'name' field inconsistent");
    }

    // check 'description' field
    SEXP sdescription = rharp_named_element(var, "description");
    if(sdescription != R_NilValue) {
        if (TYPEOF(sdescription) != STRSXP || LENGTH(sdescription) != 1)
            var_error(name, "'description' field not a string");
        description = CHAR(STRING_ELT(sdescription, 0));
    }

    // check 'unit' field
    SEXP sunit = rharp_named_element(var, "unit");
    if(sunit != R_NilValue) {
        if (TYPEOF(sunit) != STRSXP || LENGTH(sunit) != 1)
            var_error(name, "'unit' field not a string");
        unit = CHAR(STRING_ELT(sunit, 0));
    }

    // check 'data' field
    SEXP sdata = rharp_named_element(var, "data");
    if(sdata == R_NilValue)
        var_error(name, "no 'data' field");
    if(!isArray(sdata))
        var_error(name, "'data' field not an array");

    // check 'dimension' field
    SEXP sdimension = rharp_named_element(var, "dimension");
    if(sdimension == R_NilValue)
        var_error(name, "no 'dimension' field");
    if (TYPEOF(sdimension) != STRSXP)
        var_error(name, "'dimension' field not a string vector");
    num_dims = length(sdimension);
    if(num_dims == 0)
        var_error(name, "empty 'dimension' vector");

    // check 'enum' field
    SEXP senum = rharp_named_element(var, "enum");
    if(senum != R_NilValue) {
        if (TYPEOF(senum) != STRSXP)
            var_error(name, "'enum' field not a string vector");
    }

    // get dimension types (reversing dimensions)
    for(unsigned int j = 0; j < num_dims; j++) {
        const char *dimname = CHAR(STRING_ELT(sdimension, j));
        int found = 0;
        for(unsigned int k = 0; k < 6; k++) {
            if(strcmp(dimname, dimension_name[k]) == 0) {
                dim_type[num_dims-1-j] = k-1;
                found = 1;
            }
        }
        if(!found)
            var_error(name, "unknown dimension");
    }

    // get dimension lengths (reversing dimensions)
    SEXP dimlens = getAttrib(sdata, R_DimSymbol);
    if(LENGTH(dimlens) != num_dims)
        var_error(name, "'data' dimensions inconsistent with 'dimensions'");

    for(unsigned int j=0; j<LENGTH(dimlens); j++) {
        int dimlen = INTEGER(dimlens)[j];
        dim[num_dims-1-j] = dimlen;
        num_elements = (num_elements?num_elements:1) * dimlen;
    }

    // convert data
    datatype = TYPEOF(sdata);
    harp_data_type hdatatype;

    if(datatype == INTSXP) { // TODO check valid_min/max to determine datatype
        hdatatype = harp_type_int32; // R has no smaller datatypes

        // use smallest datatype for enums
        if(length(senum)) {
            if(length(senum) < 1<<8)
                hdatatype = harp_type_int8;
            else if(length(senum) < 1<<16)
                hdatatype = harp_type_int16;
            else
                hdatatype = harp_type_int32;
        }

        // create variable
        if(harp_variable_new(name, hdatatype, num_dims, dim_type, dim, &hv) != 0)
            rharp_error();

        // copy over data
        for(unsigned int j=0; j<num_elements; j++) {
            if(hdatatype == harp_type_int8)
                hv->data.int8_data[j] = INTEGER(sdata)[j];
            else
                hv->data.int32_data[j] = INTEGER(sdata)[j];
        }
    }
    else if (datatype == REALSXP) {
        hdatatype = harp_type_double; // R has no smaller datatype

        if(harp_variable_new(name, hdatatype, num_dims, dim_type, dim, &hv) != 0) // R has no smaller datatype
            rharp_error();

        for(unsigned int j=0; j<num_elements; j++)
            hv->data.double_data[j] = REAL(sdata)[j];
    }
    else
        var_error(name, "unsupported data type");

    // set description
    if(description) {
        if(harp_variable_set_description(hv, description) != 0)
            rharp_error();
    }

    // set unit
    if(unit) {
        if(harp_variable_set_unit(hv, unit) != 0)
            rharp_error();
    }

    // set enum
    if(length(senum)) {
        const char **enumvals = (const char **)malloc(length(senum)*sizeof(const char *));
        for(unsigned int i=0; i<length(senum); i++)
            enumvals[i] = CHAR(STRING_ELT(senum, i));
        if(harp_variable_set_enumeration_values(hv, length(senum), enumvals) != 0)
            rharp_error();
        free(enumvals);
    }

    // TODO check if hdatatype matches with valid_min/max data

    // check 'valid_min' field
    SEXP svalidmin = rharp_named_element(var, "valid_min");

    // check 'valid_max' field
    SEXP svalidmax = rharp_named_element(var, "valid_max");


    // set valid min/max
    if(hdatatype == harp_type_int8) {
        if(svalidmin != R_NilValue)
            hv->valid_min.int8_data = INTEGER(svalidmin)[0];
        if(svalidmax != R_NilValue)
            hv->valid_max.int8_data = INTEGER(svalidmax)[0];
    }
    else if(hdatatype == harp_type_int16) {
        if(svalidmin != R_NilValue)
            hv->valid_min.int16_data = INTEGER(svalidmin)[0];
        if(svalidmax != R_NilValue)
            hv->valid_max.int16_data = INTEGER(svalidmax)[0];
    }
    else if(hdatatype == harp_type_int32) {
        if(svalidmin != R_NilValue)
            hv->valid_min.int32_data = INTEGER(svalidmin)[0];
        if(svalidmax != R_NilValue)
            hv->valid_max.int32_data = INTEGER(svalidmax)[0];
    }
    else if(hdatatype == harp_type_float) {
        if(svalidmin != R_NilValue)
            hv->valid_min.float_data = REAL(svalidmin)[0];
        if(svalidmax != R_NilValue)
            hv->valid_max.float_data = REAL(svalidmax)[0];
    }
    else if(hdatatype == harp_type_double) {
        if(svalidmin != R_NilValue)
            hv->valid_min.double_data = REAL(svalidmin)[0];
        if(svalidmax != R_NilValue)
            hv->valid_max.double_data = REAL(svalidmax)[0];
    }

    return hv;
}

// import harp product as R nested lists
SEXP rharp_import_product(SEXP sname, SEXP soperations, SEXP soptions) {
    harp_product *hp;
    const char *filename;
    const char *operations = NULL;
    const char *options = NULL;

    // check filename
    if(TYPEOF(sname) != STRSXP || LENGTH(sname) != 1)
        error("filename argument not a string");
    filename = CHAR(STRING_ELT(sname, 0));

    // check operations
    if(soperations != R_NilValue) {
        if (TYPEOF(soperations) != STRSXP || LENGTH(soperations) != 1)
            error("operations argument not a string");
        operations = CHAR(STRING_ELT(soperations, 0));
    }

    // check options
    if(soptions != R_NilValue) {
        if (TYPEOF(soptions) != STRSXP || LENGTH(soptions) != 1)
            error("soptions argument not a string");
        options = CHAR(STRING_ELT(soptions, 0));
    }

    // harp import
    if(harp_import(filename, operations, options, &hp) != 0)
        rharp_error();

    // create product (named list)
    const char **productfields = (const char **)malloc((hp->num_variables+3)*sizeof(const char *));
    productfields[0] = "source_product";
    productfields[1] = "history";
    for(unsigned int i=0; i<hp->num_variables; i++) {
        productfields[i+2] = hp->variable[i]->name;
    }
    productfields[hp->num_variables+2] = "";

    SEXP product = PROTECT(mkNamed(VECSXP, productfields));
    SET_VECTOR_ELT(product, 0, mkstring(hp->source_product));
    SET_VECTOR_ELT(product, 1, mkstring(hp->history));

    // add variables
    for(unsigned int i=0; i<hp->num_variables; i++) {
        harp_variable *hv = hp->variable[i];
        SEXP var = rharp_import_variable(hv);
        SET_VECTOR_ELT(product, i+2, var);
    }

    // cleanup
    UNPROTECT(3);
    free(productfields);

    return product;
}

// export R nested lists as harp product
SEXP rharp_export_product(SEXP product, SEXP sfilename, SEXP sformat) {
    harp_product *hp;

    if(TYPEOF(product) != VECSXP)
        error("product argument not a list");
    if(TYPEOF(sfilename) != STRSXP || LENGTH(sfilename) != 1)
        error("filename argument not a string");
    if(TYPEOF(sformat) != STRSXP || LENGTH(sformat) != 1)
        error("format argument not a string");

    const char *filename = CHAR(STRING_ELT(sfilename, 0));
    const char *format = CHAR(STRING_ELT(sformat, 0));

    if(harp_product_new(&hp) != 0)
        rharp_error();

    // product attributes
    SEXP names = getAttrib(product, R_NamesSymbol);

    for (R_len_t i = 0; i < length(product); i++) {
        const char *attrname = CHAR(STRING_ELT(names, i));
        SEXP elmt = VECTOR_ELT(product, i);

        // set source_product
        if(strcmp(attrname, "source_product") == 0) {
            if(TYPEOF(elmt) != STRSXP || LENGTH(elmt) != 1)
                error("'source_product' field not a string");

            if(harp_product_set_source_product(hp, CHAR(STRING_ELT(elmt, 0))) != 0)
                rharp_var_error(attrname);
        }

        // set history
        else if(strcmp(attrname, "history") == 0) {
            if(TYPEOF(elmt) != STRSXP || LENGTH(elmt) != 1)
                error("'history' field not a string");

            if(harp_product_set_history(hp, CHAR(STRING_ELT(elmt, 0))) != 0)
                rharp_var_error(attrname);
        }

        // set variable
        else {
            if(TYPEOF(elmt) != VECSXP)
                error("variable '%s' not a list", attrname);

            harp_variable *hv = rharp_export_variable(elmt, attrname);
            if(harp_product_add_variable(hp, hv) != 0)
                rharp_error(attrname);
        }
    }

    if(harp_export(filename, format, hp) != 0)
        rharp_error();

    // cleanup
    harp_product_delete(hp);

    return R_NilValue;
}

SEXP rharp_init() {
    harp_init();
    return R_NilValue;
}

SEXP rharp_done() {
    harp_done();
    return R_NilValue;
}

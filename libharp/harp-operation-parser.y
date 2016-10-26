%include {
    #include <stdio.h>
    #include <string.h>
    #include <assert.h>
    #include "harp-operation.h"
    #include "harp-program.h"
    #include "harp-parser-state.h"

    typedef struct float_with_unit_struct {
        float value;
        char *unit;
    } float_with_unit;

    typedef struct int_with_unit_struct {
        int value;
        char *unit;
    } int_with_unit;

    typedef struct harp_sized_array_struct {
        int num_elements;
        harp_array array;
    } harp_sized_array;

    static int harp_sized_array_new(harp_sized_array **new_array)
    {
        harp_sized_array *l;
        l = (harp_sized_array *)malloc(sizeof(harp_sized_array));
        if (l == NULL)
        {
            harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
            sizeof(harp_sized_array), __FILE__, __LINE__);
            return -1;
        }

        l->num_elements = 0;
        l->array.ptr = NULL;

        *new_array = l;

        return 0;
    }

    static void harp_sized_array_delete(harp_sized_array *l)
    {
        free(l->array.ptr);
        free(l);
    }

    static int harp_sized_array_add_string(harp_sized_array *harp_sized_array, const char *str)
    {
        if (harp_sized_array->num_elements % BLOCK_SIZE == 0)
        {
            char **string_data;
            int new_num = (harp_sized_array->num_elements + BLOCK_SIZE);

            string_data = (char **)realloc(harp_sized_array->array.string_data, new_num * sizeof(char *));
            if (string_data == NULL)
            {
                harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                new_num * sizeof(char *), __FILE__, __LINE__);
                return -1;
            }

            harp_sized_array->array.string_data = strdup(string_data);
        }

        harp_sized_array->array.string_data[harp_sized_array->num_elements] = str;
        harp_sized_array->num_elements++;

        return 0;
    }

    static int harp_sized_array_add_double(harp_sized_array *harp_sized_array, const double d)
    {
        if (harp_sized_array->num_elements % BLOCK_SIZE == 0)
        {
            double *double_data;
            int new_num = (harp_sized_array->num_elements + BLOCK_SIZE);

            double_data = (double *)realloc(harp_sized_array->array.double_data, new_num * sizeof(double));
            if (double_data == NULL)
            {
                harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                new_num * sizeof(double), __FILE__, __LINE__);
                return -1;
            }

            harp_sized_array->array.double_data = double_data;
        }

        harp_sized_array->array.double_data[harp_sized_array->num_elements] = d;
        harp_sized_array->num_elements++;

        return 0;
    }
}

%syntax_error
{
    char msg[100];
    sprintf(msg, "syntax error near '%s'", TOKEN);
    harp_parser_state_set_error(state, msg);
}

%extra_argument { harp_parser_state* state }

%left COMMA.

%token_type {const char*}
%token_destructor {
  free($$);
}

start ::= operations SEMICOLON.
start ::= operations.

%type id {char *}
id(i) ::= ID(s). {
    i = s;
}

%type ids {harp_sized_array *}
ids(l) ::= ids(m) COMMA id(i). {
    if (harp_sized_array_add_string(m, i) != 0)
    {
        harp_parser_state_set_error(state, harp_errno_to_string(harp_errno));
    }

    l = m;

    free(i);
}
ids(l) ::= id(i). {
    if (harp_sized_array_new(&l) != 0 || harp_sized_array_add_string(l, i) != 0)
    {
        harp_parser_state_set_error(state, harp_errno_to_string(harp_errno));
    }

    free(i);
}

%type unit_opt {char *}
unit_opt(u) ::= UNIT(s). {
  int len = strlen(s) - 2;

  u = malloc((len + 1) * sizeof(char));
  if (u == NULL)
  {
    harp_parser_state_set_error(state, "out of memory (could not memory for unit string)");
    return;
  }

  /* copy the unit and null terminate the string */
  memcpy(u, &s[1], len);
  u[len] = '\0';

  /* free the now deprecated token */
  free(s);
}
unit_opt(u) ::= . {u = NULL;}

%type float {double}
float(A) ::= FLOAT(F). {
    if(harp_parse_double(F, strlen(F), &A, 0) != strlen(F))
    {
        harp_parser_state_set_error(state, "Could not parse double from string");
    }

    /* free the token */
    free(F);
}
float(A) ::= INT(I). {
    if(harp_parse_double(I, strlen(I), &A, 0) != strlen(I))
    {
        harp_parser_state_set_error(state, "Could not parse double from string");
    }

    /* free the token */
    free(I);
}
float(v) ::= NAN. {v = harp_nan();}
float(v) ::= INF(s). {
    if (s[0] == '-') {
        v = harp_mininf();
    } else {
        v = harp_plusinf();
    }

    /* free the token */
    free(s);
}

%type int {int}
int(i) ::= INT(s). {
    i = atoi(s);

    /* free the token */
    free(s);
}

%type floatvaluelist {harp_sized_array *}
floatvaluelist(l) ::= floatvaluelist(m) COMMA float(d). {
    if (harp_sized_array_add_double(m, d) != 0)
    {
        harp_parser_state_set_error(state, harp_errno_to_string(harp_errno));
    }

    l = m;
}
floatvaluelist(l) ::= float(d). {
    if (harp_sized_array_new(&l) != 0 || harp_sized_array_add_double(l, d) != 0)
    {
        harp_parser_state_set_error(state, harp_errno_to_string(harp_errno));
    }
}

%type stringvaluelist {harp_sized_array *}
stringvaluelist(l) ::= stringvaluelist(m) COMMA stringvalue(s). {
    if (harp_sized_array_add_string(m, s) != 0)
    {
        harp_parser_state_set_error(state, harp_errno_to_string(harp_errno));
    }

    l = m;
    free(s);
}
stringvaluelist(l) ::= stringvalue(s). {
    if (harp_sized_array_new(&l) != 0 || harp_sized_array_add_string(l, s) != 0)
    {
        harp_parser_state_set_error(state, harp_errno_to_string(harp_errno));
    }
    free(s);
}

/*
 * dimensions
 */

%token_class dimtype DIM_TIME|DIM_LAT|DIM_LON|DIM_VERTICAL|DIM_SPECTRAL|DIM_INDEP.
%type dimension {char *}
dimension(d) ::= dimtype(s). {
    d = s;
}

%type dimensionlist {harp_sized_array *}
dimensionlist(l) ::= dimensionlist(m) COMMA dimension(d). {
    if (harp_sized_array_add_string(m, d) != 0)
    {
        harp_parser_state_set_error(state, harp_errno_to_string(harp_errno));
    }

    l = m;
    free(d);
}
dimensionlist(l) ::= dimension(s). {
    if (harp_sized_array_new(&l) != 0 || harp_sized_array_add_string(l, s) != 0)
    {
        harp_parser_state_set_error(state, harp_errno_to_string(harp_errno));
    }

    free(s);
}

%type dimensionspec {harp_sized_array *}
dimensionspec(l) ::= LEFT_CURLY dimensionlist(m) RIGHT_CURLY. { l = m; }

/*
 * values
 */

%type floatvalue { float_with_unit }
floatvalue(v) ::= float(f) unit_opt(u). {
  v.value = f;
  v.unit = u;
}

%type collocation_column { char }
collocation_column(col) ::= COLLOCATION_COLUMN(s). { col = s[0]; }

%type stringvalue {char *}
stringvalue(x) ::= STRING(t). {
    int len = strlen(t) - 2;

    x = malloc((len + 1) * sizeof(char));
    if (x == NULL)
    {
        harp_parser_state_set_error(state, "out of memory (could not memory for unit string)");
        return;
    }

    /* copy the string without the quotes and null terminate the string */
    memcpy(x, &t[1], len);
    x[len] = '\0';

    /* destroy the unused original */
    free(t);
}

/*
    * functions
    */

%type functioncall {harp_operation*}
functioncall(F) ::= F_COLLOCATE_LEFT LEFT_PAREN stringvalue(s) RIGHT_PAREN. {
    if (harp_collocation_filter_new(s, harp_collocation_left, &F) != 0) {
        harp_parser_state_set_error(state, harp_errno_to_string(harp_errno));
    }
}
functioncall(F) ::= F_COLLOCATE_RIGHT LEFT_PAREN stringvalue(s) RIGHT_PAREN. {
    if (harp_collocation_filter_new(s, harp_collocation_right, &F) != 0) {
        harp_parser_state_set_error(state, harp_errno_to_string(harp_errno));
    }
}
functioncall(F) ::= F_VALID LEFT_PAREN id(i) RIGHT_PAREN. {
    if (harp_valid_range_filter_new(i, &F) != 0)
    {
        harp_parser_state_set_error(state, harp_errno_to_string(harp_errno));
    }
}
functioncall(F) ::= F_LON_RANGE LEFT_PAREN floatvalue(min) COMMA floatvalue(max) RIGHT_PAREN. {
    if (harp_longitude_range_filter_new(min.value, min.unit, max.value, max.unit, &F) != 0)
    {
        harp_parser_state_set_error(state, harp_errno_to_string(harp_errno));
    }
}
functioncall(F) ::= F_POINT_DIST LEFT_PAREN floatvalue(lon) COMMA floatvalue(lat) COMMA floatvalue(dist) RIGHT_PAREN. {
    if (harp_point_distance_filter_new(lon.value, lon.unit, lat.value, lat.unit, dist.value, dist.unit, &F) != 0)
    {
        harp_parser_state_set_error(state, harp_errno_to_string(harp_errno));
    }
}
functioncall(F) ::= F_AREA_MASK_COVERS_POINT LEFT_PAREN stringvalue(file) RIGHT_PAREN. {
    if (harp_area_mask_covers_point_filter_new(file, &F) != 0)
    {
        harp_parser_state_set_error(state, harp_errno_to_string(harp_errno));
    }
}
functioncall(F) ::= F_AREA_MASK_COVERS_AREA LEFT_PAREN stringvalue(file) RIGHT_PAREN. {
    if (harp_area_mask_covers_area_filter_new(file, &F) != 0)
    {
        harp_parser_state_set_error(state, harp_errno_to_string(harp_errno));
    }
}
functioncall(F) ::= F_AREA_MASK_INTERSECTS_AREA LEFT_PAREN stringvalue(file) COMMA float(f) RIGHT_PAREN. {
    if (harp_area_mask_intersects_area_filter_new(file, f, &F) != 0)
    {
        harp_parser_state_set_error(state, harp_errno_to_string(harp_errno));
    }
}
functioncall(F) ::= F_DERIVE LEFT_PAREN id(var) dimensionspec(dims) unit_opt(u) RIGHT_PAREN. {
    harp_dimension_type *dimspec;
    int i;

    dimspec = (harp_dimension_type *)malloc(dims->num_elements * sizeof(harp_dimension_type));
    if (dimspec == NULL)
    {
        harp_parser_state_set_error(state, "out of memory");
        return;
    }

    /* parse the dimspec */
    for (i = 0; i < dims->num_elements; i++)
    {
        if (harp_parse_dimension_type(dims->array.string_data[i], &dimspec[i]) != 0)
        {
            harp_parser_state_set_error(state, harp_errno_to_string(harp_errno));
            return;
        }
    }

    if (harp_variable_derivation_new(var, dims->num_elements, dimspec, u, &F) != 0)
    {
        harp_parser_state_set_error(state, harp_errno_to_string(harp_errno));
    }
}
functioncall(F) ::= F_KEEP LEFT_PAREN ids(i) RIGHT_PAREN. {
    if (harp_variable_inclusion_new(i->num_elements, i->array.string_data, &F) != 0)
    {
        harp_parser_state_set_error(state, harp_errno_to_string(harp_errno));
    }
    harp_sized_array_delete(i);
}
functioncall(F) ::= F_EXCLUDE LEFT_PAREN ids(i) RIGHT_PAREN. {
    if (harp_variable_exclusion_new(i->num_elements, i->array.string_data, &F) != 0)
    {
        harp_parser_state_set_error(state, harp_errno_to_string(harp_errno));
    }
}
functioncall(F) ::= F_FLATTEN LEFT_PAREN dimension(d) RIGHT_PAREN. {
    harp_dimension_type dimtype;
    if (harp_parse_dimension_type(d, &dimtype) != 0 || harp_flatten_new(dimtype, &F) != 0)
    {
        harp_parser_state_set_error(state, harp_errno_to_string(harp_errno));
    }
}
functioncall(F) ::= F_REGRID_COLLOCATED LEFT_PAREN stringvalue(csv) COMMA stringvalue(d) COMMA collocation_column(c) id(axis) RIGHT_PAREN. {
    if (harp_regrid_collocated_new(csv, d, c, axis, &F) != 0)
    {
        harp_parser_state_set_error(state, harp_errno_to_string(harp_errno));
    }
}
functioncall(F) ::= F_REGRID LEFT_PAREN stringvalue(s) RIGHT_PAREN. {
    if (harp_regrid_new(s, &F) != 0)
    {
        harp_parser_state_set_error(state, harp_errno_to_string(harp_errno));
    }
    free(s);
}

/*
 * operations
 */

%type comparison_operator {harp_comparison_operator_type}
comparison_operator(OP) ::= OP_EQ. {OP = harp_operator_eq;}
comparison_operator(OP) ::= OP_NE. {OP = harp_operator_ne;}
comparison_operator(OP) ::= OP_GE. {OP = harp_operator_ge;}
comparison_operator(OP) ::= OP_LE. {OP = harp_operator_le;}
comparison_operator(OP) ::= OP_GT. {OP = harp_operator_gt;}
comparison_operator(OP) ::= OP_LT. {OP = harp_operator_lt;}

%type bitmask_operator {harp_bit_mask_operator_type}
bitmask_operator(O) ::= OP_BIT_NAND. {O = harp_operator_bit_mask_none;}
bitmask_operator(O) ::= OP_BIT_AND. {O = harp_operator_bit_mask_any;}

%type membership_operator {harp_membership_operator_type}
membership_operator(O) ::= NOT IN. {O = harp_operator_not_in;}
membership_operator(O) ::= IN. {O = harp_operator_in;}

%type operation {harp_operation*}
operation(O) ::= functioncall(F). {
    O = F;
}
operation(O) ::= id(V) bitmask_operator(OP) int(E). {
    if(harp_bit_mask_filter_new(V, OP, E, &O) != 0) {
        harp_parser_state_set_error(state, harp_errno_to_string(harp_errno));
    }
}
operation(O) ::= id(V) comparison_operator(OP) floatvalue(E). {
    if(harp_comparison_filter_new(V, OP, E.value, E.unit, &O) != 0) {
        harp_parser_state_set_error(state, harp_errno_to_string(harp_errno));
    }
}
operation(O) ::= id(V) comparison_operator(OP) stringvalue(E). {
    if(harp_string_comparison_filter_new(V, OP, E, &O) != 0) {
        harp_parser_state_set_error(state, harp_errno_to_string(harp_errno));
    }
}
operation(O) ::= id(V) membership_operator(OP) LEFT_PAREN floatvaluelist(l) RIGHT_PAREN unit_opt(u). {
    if(harp_membership_filter_new(V, OP, l->num_elements, l->array.double_data, u, &O) != 0)
    {
        harp_parser_state_set_error(state, harp_errno_to_string(harp_errno));
    }
}
operation(O) ::= id(V) membership_operator(OP) LEFT_PAREN stringvaluelist(l) RIGHT_PAREN. {
    if(harp_string_membership_filter_new(V, OP, l->num_elements, l->array.string_data, &O) != 0)
    {
        harp_parser_state_set_error(state, harp_errno_to_string(harp_errno));
    }
}

operations ::= operations SEMICOLON operation(O). {
    harp_program_add_operation(state->result, O);
}
operations ::= operation(O). {
    harp_program_add_operation(state->result, O);
}

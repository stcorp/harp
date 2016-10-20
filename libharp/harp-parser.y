%include {
    #include <stdio.h>
    #include <assert.h>
    #include "harp-operation.h"
    #include "harp-program.h"
    #include "harp-parser-state.h"

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

    static int harp_sized_array_add_string(harp_sized_array *harp_sized_array, char *str)
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

            harp_sized_array->array.string_data = string_data;
        }

        harp_sized_array->array.string_data[harp_sized_array->num_elements] = str;
        harp_sized_array->num_elements++;

        return 0;
    }

    static int harp_sized_array_add_double(harp_sized_array *harp_sized_array, double d)
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
    harp_parser_state_set_error(state, strdup(msg));
}

%extra_argument { harp_parser_state* state }

%left COMMA.

%token_type {const char*}

    start ::= operations.

    %token_class id ID.

    %type ids {harp_sized_array *}
    ids(l) ::= ids(m) COMMA id(i). {
        if (harp_sized_array_add_string(m, i) != 0)
        {
            harp_parser_state_set_error(state, harp_errno_to_string(harp_errno));
            return -1;
        }

        l = m;
    }
    ids(l) ::= id(i). {
        if (harp_sized_array_new(&l) != 0 || harp_sized_array_add_string(l, i) != 0)
        {
            harp_parser_state_set_error(state, harp_errno_to_string(harp_errno));
            return -1;
        }
    }

    unit_opt ::= UNIT.
    unit_opt ::= .

    %type float {double}
    float(A) ::= FLOAT(F). {
        if(harp_parse_double(F, strlen(F), &A, 0) != strlen(F))
        {
            harp_parser_state_set_error(state, "Could not parse double from string");
        }
    }
    float(A) ::= INT(I). {
        if(harp_parse_double(I, strlen(I), &A, 0) != strlen(I))
        {
            harp_parser_state_set_error(state, "Could not parse double from string");
        }
    }
    float(v) ::= NAN. {v = harp_nan();}
    float(v) ::= INF(s). {
    if (s[0] == '-') {
            v = harp_mininf();
        } else {
            v = harp_plusinf();
        }
    }

    %type floatvalue {double}
    floatvalue ::= float unit_opt.

    /*intvaluelist ::= intvaluelist COMMA INT.
    intvaluelist ::= INT.*/

    %type floatvaluelist {harp_sized_array *}
    floatvaluelist(l) ::= floatvaluelist(m) COMMA float(d). {
        if (harp_sized_array_add_double(m, d) != 0)
        {
            harp_parser_state_set_error(state, harp_errno_to_string(harp_errno));
            return -1;
        }

        l = m;
    }
    floatvaluelist(l) ::= float(d). {
        if (harp_sized_array_new(&l) != 0 || harp_sized_array_add_double(l, d) != 0)
        {
            harp_parser_state_set_error(state, harp_errno_to_string(harp_errno));
            return -1;
        }
    }

    %type stringvaluelist {harp_sized_array *}
    stringvaluelist(l) ::= stringvaluelist(m) COMMA STRING(s). {
        if (harp_sized_array_add_string(m, s) != 0)
        {
            harp_parser_state_set_error(state, harp_errno_to_string(harp_errno));
            return -1;
        }

        l = m;
    }
    stringvaluelist(l) ::= STRING(s). {
        if (harp_sized_array_new(&l) != 0 || harp_sized_array_add_string(l, s) != 0)
        {
            harp_parser_state_set_error(state, harp_errno_to_string(harp_errno));
            return -1;
        }
    }

    /*
     * dimensions
     */

    dimension ::= DIM_TIME.
    dimension ::= DIM_LAT.
    dimension ::= DIM_LON.
    dimension ::= DIM_VERTICAL.
    dimension ::= DIM_SPECTRAL.
    dimension ::= DIM_INDEP.

    %type dimensionlist {harp_sized_array *}
    dimensionlist(l) ::= dimensionlist(m) COMMA dimension(d). {
        if (harp_sized_array_add_string(m, d) != 0)
        {
            harp_parser_state_set_error(state, harp_errno_to_string(harp_errno));
            return -1;
        }

        l = m;
    }
    dimensionlist(l) ::= dimension(s). {
        if (harp_sized_array_new(&l) != 0 || harp_sized_array_add_string(l, s) != 0)
        {
            harp_parser_state_set_error(state, harp_errno_to_string(harp_errno));
            return -1;
        }
    }

    %type dimensionspec {harp_sized_array *}
    dimensionspec(l) ::= LEFT_CURLY dimensionlist(m) RIGHT_CURLY. { l = m; }

    /*
     * values
     */

    %type stringvalue {const char*}
    stringvalue(s) ::= STRING(t). {
      s = t;
    }

    %type intvalue {int}
    intvalue(v) ::= INT(i) unit_opt. {
      v = atoi(i);
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
    functioncall(F) ::= F_VALID LEFT_PAREN ID RIGHT_PAREN. {F = NULL;}
    functioncall(F) ::= F_LON_RANGE LEFT_PAREN floatvalue COMMA floatvalue RIGHT_PAREN. {F = NULL;}
    functioncall(F) ::= F_POINT_DIST LEFT_PAREN floatvalue COMMA floatvalue COMMA floatvalue RIGHT_PAREN. {F = NULL;}
    functioncall(F) ::= F_AREA_MASK_COVERS_POINT LEFT_PAREN stringvalue RIGHT_PAREN. {F = NULL;}
    functioncall(F) ::= F_AREA_MASK_COVERS_AREA LEFT_PAREN stringvalue RIGHT_PAREN. {F = NULL;}
    functioncall(F) ::= F_AREA_MASK_INTERSECTS_AREA LEFT_PAREN stringvalue COMMA floatvalue RIGHT_PAREN. {F = NULL;}
    functioncall(F) ::= F_DERIVE LEFT_PAREN id(var) dimensionspec(dims) unit_opt RIGHT_PAREN. {
        harp_dimension_type *dimspec;
        int i;

        dimspec = (harp_dimension_type *)malloc(dims->num_elements * sizeof(harp_dimension_type));
        if (dimspec == NULL)
        {
            harp_parser_state_set_error(HARP_ERROR_OUT_OF_MEMORY,
                                        "out of memory (could not allocate %lu bytes) (%s:%u)",
                                        (dims->num_elements * sizeof(harp_dimension_type)),
                                        __FILE__, __LINE__);
            return -1;
        }

        /* parse the dimspec */
        for (i = 0; i < dims->num_elements; i++)
        {
            if (harp_parse_dimension_type(dims->array.string_data[i], &dimspec[i]) != 0)
            {
                harp_parser_state_set_error(state, harp_errno_to_string(harp_errno));
                return -1;
            }
        }

        if (harp_variable_derivation_new(var, dims->num_elements, dimspec, NULL, &F) != 0)
        {
            harp_parser_state_set_error(state, harp_errno_to_string(harp_errno));
        }
    }
    functioncall(F) ::= F_KEEP LEFT_PAREN ids(i) RIGHT_PAREN. {
        if (harp_variable_inclusion_new(i->num_elements, i->array.string_data, &F) != 0)
        {
            harp_parser_state_set_error(state, harp_errno_to_string(harp_errno));
        }
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
    operation(O) ::= id(V) bitmask_operator(OP) intvalue(E). {
        if(harp_bit_mask_filter_new(V, OP, E, &O) != 0) {
            harp_parser_state_set_error(state, harp_errno_to_string(harp_errno));
        }
    }
    operation(O) ::= id(V) comparison_operator(OP) floatvalue(E). {
        /* TODO unit */
        if(harp_comparison_filter_new(V, OP, E, NULL, &O) != 0) {
            harp_parser_state_set_error(state, harp_errno_to_string(harp_errno));
        }
    }
    operation(O) ::= id(V) comparison_operator(OP) stringvalue(E). {
        /* TODO unit */
        if(harp_string_comparison_filter_new(V, OP, E, &O) != 0) {
            harp_parser_state_set_error(state, harp_errno_to_string(harp_errno));
        }
    }
    operation(O) ::= id(V) membership_operator(OP) LEFT_PAREN floatvaluelist RIGHT_PAREN unit_opt. {
        if(harp_membership_filter_new(V, OP, 0, NULL, NULL, &O) != 0)
        {
            harp_parser_state_set_error(state, harp_errno_to_string(harp_errno));
        }
    }
    operation(O) ::= id(V) membership_operator(OP) LEFT_PAREN stringvaluelist RIGHT_PAREN unit_opt. {
    if(harp_string_membership_filter_new(V, OP, 0, NULL, &O) != 0)
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

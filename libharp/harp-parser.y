%include {
    #include <stdio.h>
    #include <assert.h>
    #include "harp-operation.h"
    #include "harp-program.h"
    #include "harp-parser-state.h"
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

    //variablelist ::= variablelist COMMA ID.
    //variablelist ::= ID.

    %token_class id ID.

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

    floatvaluelist ::= floatvaluelist COMMA float.
    floatvaluelist ::= float.

    stringvaluelist ::= stringvaluelist COMMA STRING.
    stringvaluelist ::= STRING.

    dimension ::= DIM_TIME.
    dimension ::= DIM_LAT.
    dimension ::= DIM_LON.
    dimension ::= DIM_VERTICAL.
    dimension ::= DIM_SPECTRAL.
    dimension ::= DIM_INDEP.

    dimensionlist ::= dimensionlist COMMA dimension.
    dimensionlist ::= dimension.

    dimensionspec ::= LEFT_CURLY dimensionlist RIGHT_CURLY.

    %type stringvalue {const char*}
    stringvalue(s) ::= STRING(t). {
      s = t;
    }

    %type intvalue {int}
    intvalue(v) ::= INT(i) unit_opt. {
      v = atoi(i);
    }

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
    functioncall(F) ::= F_DERIVE LEFT_PAREN ID COMMA dimensionspec unit_opt RIGHT_PAREN. {F = NULL;}
    functioncall(F) ::= F_KEEP LEFT_PAREN id(i) RIGHT_PAREN. {
        char **varnames = (char **)malloc(1);
        varnames[0] = i;

        if (harp_variable_inclusion_new(1, varnames, &F))
        {
            harp_parser_state_set_error(state, harp_errno_to_string(harp_errno));
        }
    }
    functioncall(F) ::= F_EXCLUDE LEFT_PAREN ID RIGHT_PAREN. {F = NULL;}
    functioncall(F) ::= F_FLATTEN LEFT_PAREN dimension RIGHT_PAREN. {F = NULL;}

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

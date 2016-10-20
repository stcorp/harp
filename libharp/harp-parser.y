%include {
    #include <stdio.h>
    #include <assert.h>
    #include "harp-operation.h"
    #include "harp-program.h"
    #include "harp-parser-state.h"
}

%syntax_error
{
    harp_parser_state_set_error(state, "Syntax error");
}

%extra_argument { harp_parser_state* state }

%left COMMA.

%token_type {const char*}

    start ::= operations.

    //variablelist ::= variablelist COMMA ID.
    //variablelist ::= ID.

    id(i) ::= ID(v). {
      i = strdup(v);
    }

    unit_opt ::= UNIT.
    unit_opt ::= .

    not_opt ::= NOT.
    not_opt ::= .

    %type float {double}
    float(A) ::= FLOAT(F). {
        if(harp_parse_double(F, strlen(F), &A, 0) != strlen(F))
        {
            harp_parser_state_set_error(state, "Could not parse double from string");
        }
    }
    /* TODO resolve this parsing conflict
    float(A) ::= INT(I). {
        if(harp_parse_double(I, strlen(I), &A, 0) != 0)
        {
            harp_parser_state_set_error(state, "Could not parse double from string");
        }
    }*/

    intvaluelist ::= intvaluelist COMMA INT.
    intvaluelist ::= INT.

    floatvaluelist ::= floatvaluelist COMMA float.
    floatvaluelist ::= float.

    stringvaluelist ::= stringvaluelist COMMA STRING.
    stringvaluelist ::= STRING.

    valuelist ::= intvaluelist.
    valuelist ::= floatvaluelist.
    valuelist ::= stringvaluelist.

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
    %type floatvalue {double}
    floatvalue(v) ::= float(n) unit_opt. {v = n;}
    floatvalue(v) ::= NAN unit_opt. {v = harp_nan();}
    floatvalue(v) ::= INF unit_opt. {v = harp_nan();} /* TODO */

    %type intvalue {int}
    intvalue(v) ::= INT unit_opt. {
      v = 0;
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
    functioncall(F) ::= F_AREA_MASK_INTERSECTS_AREA LEFT_PAREN stringvalue COMMA FLOAT RIGHT_PAREN. {F = NULL;}
    functioncall(F) ::= F_DERIVE LEFT_PAREN ID COMMA dimensionspec unit_opt RIGHT_PAREN. {F = NULL;}
    functioncall(F) ::= F_KEEP LEFT_PAREN ID RIGHT_PAREN. {F = NULL;}
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
        if(harp_comparison_filter_new(V, OP, E, NULL, &O) != 0) {
            harp_parser_state_set_error(state, harp_errno_to_string(harp_errno));
        }
    }
    operation(O) ::= id not_opt IN LEFT_PAREN valuelist RIGHT_PAREN unit_opt. {
      O = NULL;
    }

    operations ::= operations SEMICOLON operation(O). {
        harp_program_add_operation(state->result, O);
    }
    operations ::= operation(O). {
        harp_program_add_operation(state->result, O);
    }

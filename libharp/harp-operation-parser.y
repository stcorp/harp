%define api.pure full
%define api.token.prefix {HARP_OP_TOK_}
%define api.prefix {harp_operation_parser_}

%code requires {
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "harp-operation.h"
#include "harp-program.h"
#include "harp-operation-parser-state.h"
}

%union {
    double double_val;
    int int_val;
    char *string;
    char char_val;
    harp_operation *operation;
    harp_sized_array *array;
    float_with_unit float_with_unit_val;
    int_with_unit int_with_unit_val;
    harp_comparison_operator_type operator;
    harp_bit_mask_operator_type bitmask_operator;
    harp_membership_operator_type membership_operator;
    harp_program *program;

    /* raw pointer access */
    void *ptr;

    /* error flag */
    int error;
}

%start start

%token ID
%token STRING
%token COLLOCATION_COLUMN
%token INT
%token FLOAT
%token NAN
%token INF
%token DIM_INDEP
%token DIM_TIME
%token DIM_LAT
%token DIM_LON
%token DIM_SPECTRAL
%token DIM_VERTICAL
%token UNIT
%token COMMA
%token SEMICOLON
%token LEFT_CURLY
%token RIGHT_CURLY
%token LEFT_PAREN
%token RIGHT_PAREN
%token F_COLLOCATE_LEFT
%token F_COLLOCATE_RIGHT
%token F_VALID
%token F_LON_RANGE
%token F_POINT_DIST
%token F_AREA_MASK_COVERS_POINT
%token F_AREA_MASK_COVERS_AREA
%token F_AREA_MASK_INTERSECTS_AREA
%token F_DERIVE
%token F_KEEP
%token F_EXCLUDE
%token F_FLATTEN
%token F_REGRID_COLLOCATED
%token F_REGRID
%token OP_EQ
%token OP_NE
%token OP_GE
%token OP_LE
%token OP_GT
%token OP_LT
%token OP_BIT_NAND
%token OP_BIT_AND
%token NOT
%token IN

%type<program> start operations
%type<operation> operation
%type<string> id unit_opt stringvalue ID STRING FLOAT COLLOCATION_COLUMN dimension INT
%type<string> DIM_TIME DIM_VERTICAL DIM_LAT DIM_LON DIM_SPECTRAL DIM_INDEP INF UNIT
%type<char_val> collocation_column
%type<int_val> intval
%type<float_with_unit_val> floatvalue
%type<array> ids floatlist dimensionlist stringlist dimensionspec
%type<double_val> floatval
%type<membership_operator> membership_operator;
%type<operator> comparison_operator;
%type<bitmask_operator> bitmask_operator;

%%

start:
    operations SEMICOLON { $$ = $1; }
    | operations { $$ = $1; }

/* %type  id {char *} */
id:
    ID {
        $$ = $1;
    }

/* %type  ids {harp_sized_array *} */
ids:
    ids COMMA id {
        if (harp_sized_array_add_string($1, $3) != 0)
        {
            YYERROR;
            harp_set_error(HARP_ERROR_OPERATION_SYNTAX, harp_errno_to_string(harp_errno));
        }

        $$ = $1;

        free($3);
    }
    | id {
        if (harp_sized_array_new(&$$) != 0 || harp_sized_array_add_string($$, $1) != 0)
        {
            YYERROR;
            harp_set_error(HARP_ERROR_OPERATION_SYNTAX, harp_errno_to_string(harp_errno));
        }

        free($1);
    }

/* %type  float {double} */
floatval:
    FLOAT {
        if(harp_parse_double($1, strlen($1), &$$, 0) != strlen($1))
        {
            YYERROR;
            // TODO
        }

        free($1);
    }
    | INT {
        if(harp_parse_double($1, strlen($1), &$$, 0) != strlen($1))
        {
            // harp_parser_state_set_error(state, "Could not parse double from string");
            YYERROR;
        }

        free($1);
    }
    | NAN {
        $$ = harp_nan();
    }
    | INF {
        if ($1[0] == '-') {
            $$ = harp_mininf();
        } else {
            $$ = harp_plusinf();
        }

        free($1);
    }

/* %type  int {int} */
intval:
    INT {
        $$ = atoi($1);
        free($1);
    }

/* %type  unit_opt {char *} */
unit_opt:
    UNIT {
        int len = strlen($1) - 2;

        $$ = malloc((len + 1) * sizeof(char));
        if ($$ == NULL)
        {
            YYERROR;
            // harp_parser_state_set_error(state, "out of memory (could not memory for unit string)");
            return;
        }

        /* copy the unit and null terminate the string */
        memcpy($$, &$1[1], len);
        $$[len] = '\0';

        /* free the now deprecated token */
        free($1);
    }
    | %empty {$$ = NULL;}

/* %type  floatlist {harp_sized_array *} */
floatlist:
    floatlist COMMA floatval {
        if (harp_sized_array_add_double($1, $3) != 0)
        {
            // harp_parser_state_set_error(state, harp_errno_to_string(harp_errno));
            YYERROR;
        }

        $$ = $1;
    }
    | floatval {
        if (harp_sized_array_new(&$$) != 0 || harp_sized_array_add_double($$, $1) != 0)
        {
            // harp_parser_state_set_error(state, harp_errno_to_string(harp_errno));
            YYERROR;
        }
    }

/* %type  stringlist {harp_sized_array *} */
stringlist:
    stringlist COMMA stringvalue {
        if (harp_sized_array_add_string($1, $3) != 0)
        {
            // harp_parser_state_set_error(state, harp_errno_to_string(harp_errno));
            YYERROR;
        }

        $$ = $3;

        free($3);
    }
    | stringvalue {
        if (harp_sized_array_new(&$$) != 0 || harp_sized_array_add_string($$, $1) != 0)
        {
            // harp_parser_state_set_error(state, harp_errno_to_string(harp_errno));
            YYERROR;
        }

        free($1);
    }

/*
 * dimensions
 */

dimension:
    DIM_TIME {$$ = $1;}
    | DIM_LAT {$$ = $1;}
    | DIM_LON {$$ = $1;}
    | DIM_VERTICAL {$$ = $1;}
    | DIM_SPECTRAL {$$ = $1;}
    | DIM_INDEP {$$ = $1;}

/* %type  dimensionlist {harp_sized_array *} */
dimensionlist:
    dimensionlist COMMA dimension {
        if (harp_sized_array_add_string($1, $3) != 0)
        {
            // harp_parser_state_set_error(state, harp_errno_to_string(harp_errno));
            YYERROR;
        }

        $$ = $1;

        free($3);
    }
    | dimension {
        if (harp_sized_array_new(&$$) != 0 || harp_sized_array_add_string($$, $1) != 0)
        {
            // harp_parser_state_set_error(state, harp_errno_to_string(harp_errno));
            YYERROR;
        }

        free($1);
    }

/* %type  dimensionspec {harp_sized_array *} */
dimensionspec:
    LEFT_CURLY dimensionlist RIGHT_CURLY { $$ = $2; }

/*
 * values
 */

/* %type  floatvalue { float_with_unit } */
floatvalue:
    floatval unit_opt {
        $$.value = $1;
        $$.unit = $2;
    }

/* %type  collocation_column { char } */
collocation_column:
    COLLOCATION_COLUMN {
        $$ = $1[0];
        free($1);
    }

/* %type  stringvalue {char *} */
stringvalue:
    STRING {
        char *t = $1;
        int len = strlen(t) - 2;

        $$ = malloc((len + 1) * sizeof(char));
        if ($$ == NULL)
        {
            // harp_parser_state_set_error(state, "out of memory (could not memory for unit string)");
            YYERROR;
            return;
        }

        /* copy the string without the quotes and null terminate the string */
        memcpy($$, &t[1], len);
        $$[len] = '\0';

        /* destroy the unused original */
        free(t);
    }

/*
 * functions
 */

/* %type  functioncall {harp_operation*} */
/*functioncall(F) ::= F_COLLOCATE_LEFT LEFT_PAREN stringvalue(s) RIGHT_PAREN. {
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
}*/

/*
 * operations
 */

/* %type  comparison_operator {harp_comparison_operator_type} */
comparison_operator:
    OP_EQ { $$ = harp_operator_eq; }
    | OP_NE { $$ = harp_operator_ne; }
    | OP_GE { $$ = harp_operator_ge; }
    | OP_LE { $$ = harp_operator_le; }
    | OP_GT { $$ = harp_operator_gt; }
    | OP_LT { $$ = harp_operator_lt; }

/* %type  bitmask_operator {harp_bit_mask_operator_type} */
bitmask_operator:
    OP_BIT_NAND { $$ = harp_operator_bit_mask_none; }
    | OP_BIT_AND { $$ = harp_operator_bit_mask_any; }

/* %type  membership_operator {harp_membership_operator_type} */
membership_operator:
    NOT IN { $$ = harp_operator_not_in; }
    | IN { $$ = harp_operator_in; }

/* %type  operation {harp_operation*} */
/*operation(O) ::= functioncall(F). {
    O = F;
    }*/
operation:
    id bitmask_operator intval {
        if(harp_bit_mask_filter_new($1, $2, $3, &$$) != 0) {
            //harp_parser_state_set_error(state, harp_errno_to_string(harp_errno));
            YYERROR;
        }
        free($1);
    }
    | id comparison_operator floatvalue {
        if(harp_comparison_filter_new($1, $2, $3.value, $3.unit, &$$) != 0) {
            // harp_parser_state_set_error(state, harp_errno_to_string(harp_errno));
            YYERROR;
        }
        free($1);
    }
    | id comparison_operator stringvalue {
        if(harp_string_comparison_filter_new($1, $2, $3, &$$) != 0) {
            // harp_parser_state_set_error(state, harp_errno_to_string(harp_errno));
            YYERROR;
        }

        free($1);
        free($3);
    }
    | id membership_operator LEFT_PAREN floatlist RIGHT_PAREN unit_opt {
        if(harp_membership_filter_new($1, $2, $4->num_elements, $4->array.double_data, $6, &$$) != 0)
        {
            // harp_parser_state_set_error(state, harp_errno_to_string(harp_errno));
            YYERROR;
        }
        free($1);
        free($6);
    }
    | id membership_operator LEFT_PAREN stringlist RIGHT_PAREN {
        if(harp_string_membership_filter_new($1, $2, $4->num_elements, $4->array.string_data, &$$) != 0)
        {
            // harp_parser_state_set_error(state, harp_errno_to_string(harp_errno));
            YYERROR;
        }
        free($1);
    }

operations:
    operations SEMICOLON operation {
        if (harp_program_add_operation($1, $3) != 0)
        {
            YYERROR;
        }
    }
    | operation {
        if (harp_program_new(&$$) != 0 && harp_program_add_operation($$, $1) != 0)
        {
            YYERROR;
        }
    }

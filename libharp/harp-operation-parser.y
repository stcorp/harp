/*
 * Copyright (C) 2015-2016 S[&]T, The Netherlands.
 *
 * This file is part of HARP.
 *
 * HARP is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * HARP is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with HARP; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

/* *INDENT-OFF* */

%{

/* *INDENT-ON* */

/* Make parser independent from other parsers */
#define yyerror harp_operation_parser_error
#define yylex   harp_operation_parser_lex
#define yyparse harp_operation_parser_parse

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "harp-operation.h"
#include "harp-program.h"

static harp_program *parsed_program;

/* tokenizer declarations */
int harp_operation_parser_lex(void);
void *harp_operation_parser__scan_string(const char *yy_str);
void harp_operation_parser__delete_buffer(void *);

typedef struct harp_sized_array_struct {
    harp_data_type data_type;
    int num_elements;
    harp_array array;
} harp_sized_array;

static void harp_operation_parser_error(const char *error)
{
    harp_set_error(HARP_ERROR_OPERATION_SYNTAX, "%s", error);
}

int harp_sized_array_new(harp_sized_array **new_array, harp_data_type data_type)
{
    harp_sized_array *sized_array;
    sized_array = (harp_sized_array *)malloc(sizeof(harp_sized_array));
    if (sized_array == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       sizeof(harp_sized_array), __FILE__, __LINE__);
        return -1;
    }

    sized_array->data_type = data_type;
    sized_array->num_elements = 0;
    sized_array->array.ptr = NULL;

    *new_array = sized_array;

    return 0;
}

void harp_sized_array_delete(harp_sized_array *sized_array)
{
    if (sized_array->array.ptr != NULL)
    {
        if (sized_array->data_type == harp_type_string)
        {
            long i;

            for (i = 0; i < sized_array->num_elements; i++)
            {
                if (sized_array->array.string_data[i] != NULL)
                {
                    free(sized_array->array.string_data[i]);
                }
            }
        }
        free(sized_array->array.ptr);
    }
    free(sized_array);
}

int harp_sized_array_add_string(harp_sized_array *sized_array, const char *str)
{
    assert(sized_array->data_type == harp_type_string);

    if (sized_array->num_elements % BLOCK_SIZE == 0)
    {
        char **string_data;
        int new_num = (sized_array->num_elements + BLOCK_SIZE);

        string_data = (char **)realloc(sized_array->array.string_data, new_num * sizeof(char *));
        if (string_data == NULL)
        {
            harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
            new_num * sizeof(char *), __FILE__, __LINE__);
            return -1;
        }

        sized_array->array.string_data = string_data;
    }

    sized_array->array.string_data[sized_array->num_elements] = strdup(str);
    sized_array->num_elements++;

    return 0;
}

int harp_sized_array_add_double(harp_sized_array *sized_array, double value)
{
    assert(sized_array->data_type == harp_type_double);

    if (sized_array->num_elements % BLOCK_SIZE == 0)
    {
        double *double_data;
        int new_num = (sized_array->num_elements + BLOCK_SIZE);

        double_data = (double *)realloc(sized_array->array.double_data, new_num * sizeof(double));
        if (double_data == NULL)
        {
            harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
            new_num * sizeof(double), __FILE__, __LINE__);
            return -1;
        }

        sized_array->array.double_data = double_data;
    }

    sized_array->array.double_data[sized_array->num_elements] = value;
    sized_array->num_elements++;
    
    return 0;
}

int harp_sized_array_add_int32(harp_sized_array *sized_array, int32_t value)
{
    assert(sized_array->data_type == harp_type_int32);

    if (sized_array->num_elements % BLOCK_SIZE == 0)
    {
        int32_t *int32_data;
        int new_num = (sized_array->num_elements + BLOCK_SIZE);

        int32_data = (int32_t *)realloc(sized_array->array.int32_data, new_num * sizeof(int32_t));
        if (int32_data == NULL)
        {
            harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
            new_num * sizeof(int32_t), __FILE__, __LINE__);
            return -1;
        }

        sized_array->array.int32_data = int32_data;
    }

    sized_array->array.int32_data[sized_array->num_elements] = value;
    sized_array->num_elements++;

    return 0;
}

/* *INDENT-OFF* */

%}

%union {
    double double_val;
    int32_t int32_val;
    char *string_val;
    const char *const_string_val;
    harp_operation *operation;
    harp_sized_array *array;
    harp_program *program;

    harp_comparison_operator_type comparison_operator;
    harp_bit_mask_operator_type bit_mask_operator;
    harp_membership_operator_type membership_operator;
}

%token  <string_val>    NAME
%token  <string_val>    STRING_VALUE
%token  <string_val>    INTEGER_VALUE
%token  <string_val>    DOUBLE_VALUE
%token  <int32_val>     DIMENSION
%token  <string_val>    UNIT
%token                  FUNC_COLLOCATE_LEFT
%token                  FUNC_COLLOCATE_RIGHT
%token                  FUNC_VALID
%token                  FUNC_LONGITUDE_RANGE
%token                  FUNC_POINT_DISTANCE
%token                  FUNC_AREA_MASK_COVERS_POINT
%token                  FUNC_AREA_MASK_COVERS_AREA
%token                  FUNC_AREA_MASK_INTERSECTS_AREA
%token                  FUNC_DERIVE
%token                  FUNC_KEEP
%token                  FUNC_EXCLUDE
%token                  FUNC_FLATTEN
%token                  FUNC_REGRID_COLLOCATED
%token                  FUNC_REGRID
%token                  IN
%token                  NAN
%token                  INF
%left                   EQUAL NOT_EQUAL GREATER_EQUAL LESS_EQUAL
%left                   BIT_NAND BIT_AND
%nonassoc               NOT

%type   <program>               program
%type   <operation>             operation
%type   <int32_val>             int32_value
%type   <double_val>            double_value
%type   <string_val>            identifier
%type   <const_string_val>      reserved_identifier
%type   <array>                 double_array string_array identifier_array dimension_array dimensionspec
%type   <membership_operator>   membership_operator;
%type   <comparison_operator>   comparison_operator;
%type   <bit_mask_operator>     bit_mask_operator;

%destructor { harp_sized_array_delete($$); } double_array string_array identifier_array dimension_array dimensionspec
%destructor { harp_operation_delete($$); } operation
%destructor { harp_program_delete($$); } program
%destructor { free($$); } STRING_VALUE INTEGER_VALUE DOUBLE_VALUE NAME UNIT identifier

%error-verbose

%%

input:
    program { parsed_program = $1; }

reserved_identifier:
      NAN { $$ = "nan"; }
    | INF { $$ = "inf"; }
    | NOT { $$ = "not"; }
    | IN { $$ = "in"; }
    | DIMENSION { $$ = harp_get_dimension_type_name($1); }
    | FUNC_VALID { $$ = "valid"; }
    | FUNC_DERIVE { $$ = "derive"; }
    | FUNC_KEEP { $$ = "keep"; }
    | FUNC_EXCLUDE { $$ = "exclude"; }
    | FUNC_FLATTEN { $$ = "flatten"; }
    | FUNC_REGRID { $$ = "regrid"; }
    ;

identifier:
      NAME
    | reserved_identifier { $$ = strdup($1); }
    ;

double_value:
      DOUBLE_VALUE {
            long length = strlen($1);
            if (harp_parse_double($1, length, &$$, 0) != length) YYERROR;
            free($1);
        }
    | int32_value { $$ = (double)$1; }
    | NAN { $$ = harp_nan(); }
    | '+' NAN { $$ = harp_nan(); }
    | '-' NAN { $$ = harp_nan(); }
    | '+' INF { $$ = harp_plusinf(); }
    | '-' INF { $$ = harp_mininf(); }
    ;

int32_value:
    INTEGER_VALUE { $$ = (int32_t)atol($1); free($1); }
    ;

double_array:
      double_array ',' double_value {
            if (harp_sized_array_add_double($1, $3) != 0) YYERROR;
            $$ = $1;
        }
    | double_value {
            if (harp_sized_array_new(&$$, harp_type_int32) != 0) YYERROR;
            if (harp_sized_array_add_double($$, $1) != 0) YYERROR;
        }
    ;

string_array:
      string_array ',' STRING_VALUE {
            if (harp_sized_array_add_string($1, $3) != 0) YYERROR;
            $$ = $1;
            free($3);
        }
    | STRING_VALUE {
            if (harp_sized_array_new(&$$, harp_type_string) != 0) YYERROR;
            if (harp_sized_array_add_string($$, $1) != 0) YYERROR;
            free($1);
        }
    ;

identifier_array:
      identifier_array ',' identifier {
            if (harp_sized_array_add_string($1, $3) != 0) YYERROR;
            $$ = $1;
            free($3);
        }
    | identifier {
            if (harp_sized_array_new(&$$, harp_type_string) != 0) YYERROR;
            if (harp_sized_array_add_string($$, $1) != 0) YYERROR;
            free($1);
        }
    ;

dimension_array:
      dimension_array ',' DIMENSION {
            if (harp_sized_array_add_int32($1, $3) != 0) YYERROR;
            $$ = $1;
        }
    | DIMENSION {
            if (harp_sized_array_new(&$$, harp_type_int32) != 0) YYERROR;
            if (harp_sized_array_add_int32($$, $1) != 0) YYERROR;
        }
    ;

dimensionspec:
      '{' dimension_array '}' { $$ = $2; }
    | '{' '}' { if (harp_sized_array_new(&$$, harp_type_int32) != 0) YYERROR; }
    ;

comparison_operator:
      EQUAL { $$ = harp_operator_eq; }
    | NOT_EQUAL { $$ = harp_operator_ne; }
    | GREATER_EQUAL { $$ = harp_operator_ge; }
    | LESS_EQUAL { $$ = harp_operator_le; }
    | '>' { $$ = harp_operator_gt; }
    | '<' { $$ = harp_operator_lt; }
    ;

bit_mask_operator:
      BIT_NAND { $$ = harp_operator_bit_mask_none; }
    | BIT_AND { $$ = harp_operator_bit_mask_any; }
    ;

membership_operator:
      NOT IN { $$ = harp_operator_not_in; }
    | IN { $$ = harp_operator_in; }
    ;

operation:
      identifier bit_mask_operator int32_value {
            if (harp_bit_mask_filter_new($1, $2, $3, &$$) != 0) YYERROR;
        }
    | identifier comparison_operator double_value {
            if (harp_comparison_filter_new($1, $2, $3, NULL, &$$) != 0) YYERROR;
            free($1);
        }
    | identifier comparison_operator double_value UNIT {
            if (harp_comparison_filter_new($1, $2, $3, $4, &$$) != 0) YYERROR;
            free($1);
            free($4);
        }
    | identifier comparison_operator STRING_VALUE {
            if (harp_string_comparison_filter_new($1, $2, $3, &$$) != 0) YYERROR;
            free($1);
            free($3);
        }
    | identifier membership_operator '(' double_array ')' UNIT {
            if (harp_membership_filter_new($1, $2, $4->num_elements, $4->array.double_data, $6, &$$) != 0) YYERROR;
            free($1);
            harp_sized_array_delete($4);
        }
    | identifier membership_operator '(' double_array ')' {
            if (harp_membership_filter_new($1, $2, $4->num_elements, $4->array.double_data, NULL, &$$) != 0) YYERROR;
            free($1);
            harp_sized_array_delete($4);
        }
    | identifier membership_operator '(' string_array ')' {
            if (harp_string_membership_filter_new($1, $2, $4->num_elements, (const char **)$4->array.string_data, &$$)
                != 0) YYERROR;
            free($1);
            harp_sized_array_delete($4);
        }
    | FUNC_COLLOCATE_LEFT '(' STRING_VALUE ')' {
            if (harp_collocation_filter_new($3, harp_collocation_left, &$$) != 0) YYERROR;
            free($3);
        }
    | FUNC_COLLOCATE_RIGHT '(' STRING_VALUE ')' {
            if (harp_collocation_filter_new($3, harp_collocation_right, &$$) != 0) YYERROR;
            free($3);
        }
    | FUNC_VALID '(' identifier ')' {
            if (harp_valid_range_filter_new($3, &$$) != 0) YYERROR;
            free($3);
        }
    | FUNC_LONGITUDE_RANGE '(' double_value ',' double_value ')' {
            if (harp_longitude_range_filter_new($3, NULL, $5, NULL, &$$) != 0) YYERROR;
    }
    | FUNC_LONGITUDE_RANGE '(' double_value ',' double_value UNIT ')' {
            if (harp_longitude_range_filter_new($3, NULL, $5, $6, &$$) != 0) YYERROR;
        }
    | FUNC_LONGITUDE_RANGE '(' double_value UNIT ',' double_value ')' {
            if (harp_longitude_range_filter_new($3, $4, $6, NULL, &$$) != 0) YYERROR;
        }
    | FUNC_LONGITUDE_RANGE '(' double_value UNIT ',' double_value UNIT ')' {
            if (harp_longitude_range_filter_new($3, $4, $6, $7, &$$) != 0) YYERROR;
        }
    | FUNC_POINT_DISTANCE '(' double_value ',' double_value ',' double_value ')' {
            if (harp_point_distance_filter_new($3, NULL, $5, NULL, $7, NULL, &$$) != 0) YYERROR;
        }
    | FUNC_POINT_DISTANCE '(' double_value ',' double_value ',' double_value UNIT ')' {
            if (harp_point_distance_filter_new($3, NULL, $5, NULL, $7, $8, &$$) != 0) YYERROR;
        }
    | FUNC_POINT_DISTANCE '(' double_value ',' double_value UNIT ',' double_value ')' {
            if (harp_point_distance_filter_new($3, NULL, $5, $6, $8, NULL, &$$) != 0) YYERROR;
        }
    | FUNC_POINT_DISTANCE '(' double_value ',' double_value UNIT ',' double_value UNIT ')' {
            if (harp_point_distance_filter_new($3, NULL, $5, $6, $8, $9, &$$) != 0) YYERROR;
        }
    | FUNC_POINT_DISTANCE '(' double_value UNIT ',' double_value ',' double_value ')' {
            if (harp_point_distance_filter_new($3, $4, $6, NULL, $8, NULL, &$$) != 0) YYERROR;
        }
    | FUNC_POINT_DISTANCE '(' double_value UNIT ',' double_value ',' double_value UNIT ')' {
            if (harp_point_distance_filter_new($3, $4, $6, NULL, $8, $9, &$$) != 0) YYERROR;
        }
    | FUNC_POINT_DISTANCE '(' double_value UNIT ',' double_value UNIT ',' double_value ')' {
            if (harp_point_distance_filter_new($3, $4, $6, $7, $9, NULL, &$$) != 0) YYERROR;
        }
    | FUNC_POINT_DISTANCE '(' double_value UNIT ',' double_value UNIT ',' double_value UNIT ')' {
            if (harp_point_distance_filter_new($3, $4, $6, $7, $9, $10, &$$) != 0) YYERROR;
        }
    | FUNC_AREA_MASK_COVERS_POINT '(' STRING_VALUE ')' {
            if (harp_area_mask_covers_point_filter_new($3, &$$) != 0) YYERROR;
        }
    | FUNC_AREA_MASK_COVERS_AREA '(' STRING_VALUE ')' {
            if (harp_area_mask_covers_area_filter_new($3, &$$) != 0) YYERROR;
        }
    | FUNC_AREA_MASK_INTERSECTS_AREA '(' STRING_VALUE ',' double_value ')' {
            if (harp_area_mask_intersects_area_filter_new($3, $5, &$$) != 0) YYERROR;
        }
    | FUNC_DERIVE '(' identifier dimensionspec ')' {
            if (harp_variable_derivation_new($3, $4->num_elements, $4->array.int32_data, NULL, &$$) != 0) YYERROR;
        }
    | FUNC_DERIVE '(' identifier dimensionspec UNIT ')' {
            if (harp_variable_derivation_new($3, $4->num_elements, $4->array.int32_data, $5, &$$) != 0) YYERROR;
        }
    | FUNC_KEEP '(' identifier_array ')' {
            if (harp_variable_inclusion_new($3->num_elements, (const char **)$3->array.string_data, &$$) != 0) YYERROR;
        }
    | FUNC_EXCLUDE '(' identifier_array ')' {
            if (harp_variable_exclusion_new($3->num_elements, (const char **)$3->array.string_data, &$$) != 0) YYERROR;
        }
    | FUNC_FLATTEN '(' DIMENSION ')' {
            if (harp_flatten_new($3, &$$) != 0) YYERROR;
        }
    | FUNC_REGRID_COLLOCATED '(' STRING_VALUE ',' STRING_VALUE ',' 'a' ',' identifier ')' {
            if (harp_regrid_collocated_new($3, $5, 'a', $9, &$$) != 0) YYERROR;
        }
    | FUNC_REGRID_COLLOCATED '(' STRING_VALUE ',' STRING_VALUE ',' 'b' ',' identifier ')' {
            if (harp_regrid_collocated_new($3, $5, 'b', $9, &$$) != 0) YYERROR;
        }
    | FUNC_REGRID '(' STRING_VALUE ')' {
            if (harp_regrid_new($3, &$$) != 0) YYERROR;
        }
    ;

program:
      program ';' operation {
            if (harp_program_add_operation($1, $3) != 0) YYERROR;
            $$ = $1;
        }
    | operation {
            if (harp_program_new(&$$) != 0) YYERROR;
            if (harp_program_add_operation($$, $1) != 0) YYERROR;
        }
    ;

%%

/* *INDENT-ON* */

int harp_program_from_string(const char *str, harp_program **program)
{
    void *bufstate;

    /* if this doesn't hold we need to introduce a separate harp_sized_array for enums */
    assert(sizeof(int32_t) == sizeof(harp_dimension_type));

    harp_errno = 0;
    parsed_program = NULL;
    bufstate = (void *)harp_operation_parser__scan_string(str);
    if (harp_operation_parser_parse() != 0)
    {
        if (harp_errno == 0)
        {
            harp_set_error(HARP_ERROR_OPERATION_SYNTAX, NULL);
        }
        harp_operation_parser__delete_buffer(bufstate);
        return -1;
    }
    harp_operation_parser__delete_buffer(bufstate);
    *program = parsed_program;

    return 0;
}

/*
 * Copyright (C) 2015-2021 S[&]T, The Netherlands.
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

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "harp-operation.h"
#include "harp-program.h"

static harp_program *parsed_program;

/* tokenizer declarations */
int harp_operation_parser_lex(void);
void *harp_operation_parser__scan_string(const char *yy_str);
void harp_operation_parser__delete_buffer(void *);

typedef struct harp_sized_array_struct
{
    harp_data_type data_type;
    int num_elements;
    harp_array array;
} harp_sized_array;

static void harp_operation_parser_error(const char *error)
{
    harp_set_error(HARP_ERROR_OPERATION_SYNTAX, "%s", error);
}

static int parse_int32(const char *buffer, int32_t *dst)
{
    int32_t value = 0;

    while (*buffer != '\0')
    {
        int32_t digit;

        digit = *buffer - '0';
        if (value > (0x7FFFFFFFl - digit) / 10)
        {
            harp_set_error(HARP_ERROR_OPERATION_SYNTAX, "value too large for int32");
            return -1;
        }
        value = 10 * value + digit;
        buffer++;
    }
    *dst = value;

    return 0;
}

int harp_sized_array_new(harp_data_type data_type, harp_sized_array **new_array)
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
%token  <string_val>    NAME_PATTERN
%token  <string_val>    STRING_VALUE
%token  <string_val>    INTEGER_VALUE
%token  <string_val>    DOUBLE_VALUE
%token  <int32_val>     DATATYPE
%token  <int32_val>     DIMENSION
%token  <string_val>    UNIT
%token                  ID_A
%token                  ID_B
%token                  FUNC_AREA_COVERS_AREA
%token                  FUNC_AREA_COVERS_POINT
%token                  FUNC_AREA_INSIDE_AREA
%token                  FUNC_AREA_INTERSECTS_AREA
%token                  FUNC_BIN
%token                  FUNC_BIN_SPATIAL
%token                  FUNC_CLAMP
%token                  FUNC_COLLOCATE_LEFT
%token                  FUNC_COLLOCATE_RIGHT
%token                  FUNC_DERIVE
%token                  FUNC_DERIVE_SMOOTHED_COLUMN
%token                  FUNC_EXCLUDE
%token                  FUNC_FLATTEN
%token                  FUNC_INDEX
%token                  FUNC_KEEP
%token                  FUNC_LONGITUDE_RANGE
%token                  FUNC_POINT_DISTANCE
%token                  FUNC_POINT_IN_AREA
%token                  FUNC_REBIN
%token                  FUNC_REGRID
%token                  FUNC_RENAME
%token                  FUNC_SET
%token                  FUNC_SMOOTH
%token                  FUNC_SORT
%token                  FUNC_SQUASH
%token                  FUNC_VALID
%token                  FUNC_WRAP
%token                  NAN
%token                  INF
%token                  IN
%left                   EQUAL NOT_EQUAL GREATER_EQUAL LESS_EQUAL
%left                   BIT_NAND BIT_AND
%nonassoc               NOT

%type   <program>               program
%type   <operation>             operation
%type   <int32_val>             int32_value
%type   <double_val>            double_value
%type   <string_val>            identifier identifier_pattern
%type   <const_string_val>      reserved_identifier
%type   <array>                 double_array int32_array string_array identifier_array identifier_pattern_array dimension_array dimensionspec
%type   <membership_operator>   membership_operator;
%type   <comparison_operator>   comparison_operator;
%type   <bit_mask_operator>     bit_mask_operator;

%destructor { harp_sized_array_delete($$); } double_array int32_array string_array identifier_array identifier_pattern_array dimension_array dimensionspec
%destructor { harp_operation_delete($$); } operation
%destructor { harp_program_delete($$); } program
%destructor { free($$); } STRING_VALUE INTEGER_VALUE DOUBLE_VALUE NAME NAME_PATTERN UNIT identifier

%error-verbose

%%

input:
      program ';' { parsed_program = $1; }
    | program     { parsed_program = $1; }
    ;

reserved_identifier:
      NAN { $$ = "nan"; }
    | INF { $$ = "inf"; }
    | NOT { $$ = "not"; }
    | IN { $$ = "in"; }
    | DIMENSION { $$ = harp_get_dimension_type_name($1); }
    | DATATYPE { $$ = harp_get_data_type_name($1); }
    | ID_A { $$ = "a"; }
    | ID_B { $$ = "b"; }
    | FUNC_AREA_COVERS_AREA { $$ = "area_covers_area"; }
    | FUNC_AREA_COVERS_POINT { $$ = "area_covers_point"; }
    | FUNC_AREA_INSIDE_AREA { $$ = "area_inside_area"; }
    | FUNC_AREA_INTERSECTS_AREA { $$ = "area_intersects_area"; }
    | FUNC_BIN { $$ = "bin"; }
    | FUNC_BIN_SPATIAL { $$ = "bin_spatial"; }
    | FUNC_CLAMP { $$ = "clamp"; }
    | FUNC_COLLOCATE_LEFT { $$ = "collocate_left"; }
    | FUNC_COLLOCATE_RIGHT { $$ = "collocate_right"; }
    | FUNC_DERIVE { $$ = "derive"; }
    | FUNC_DERIVE_SMOOTHED_COLUMN { $$ = "derive_smoothed_column"; }
    | FUNC_EXCLUDE { $$ = "exclude"; }
    | FUNC_FLATTEN { $$ = "flatten"; }
    | FUNC_INDEX { $$ = "index"; }
    | FUNC_KEEP { $$ = "keep"; }
    | FUNC_LONGITUDE_RANGE { $$ = "longitude_range"; }
    | FUNC_POINT_DISTANCE { $$ = "point_distance"; }
    | FUNC_POINT_IN_AREA { $$ = "point_in_area"; }
    | FUNC_REBIN { $$ = "rebin"; }
    | FUNC_REGRID { $$ = "regrid"; }
    | FUNC_RENAME { $$ = "rename"; }
    | FUNC_SET { $$ = "set"; }
    | FUNC_SMOOTH { $$ = "smooth"; }
    | FUNC_SORT { $$ = "sort"; }
    | FUNC_SQUASH { $$ = "squash"; }
    | FUNC_VALID { $$ = "valid"; }
    | FUNC_WRAP { $$ = "wrap"; }
    ;

identifier:
      NAME
    | reserved_identifier { $$ = strdup($1); }
    ;

identifier_pattern:
      NAME_PATTERN
    | identifier
    ;

double_value:
      DOUBLE_VALUE {
            long length = (long)strlen($1);
            if (harp_parse_double($1, length, &$$, 0) != length)
            {
                free($1);
                YYERROR;
            }
            free($1);
        }
    | '+' DOUBLE_VALUE {
            long length = (long)strlen($2);
            if (harp_parse_double($2, length, &$$, 0) != length)
            {
                free($2);
                YYERROR;
            }
            free($2);
        }
    | '-' DOUBLE_VALUE {
            long length = (long)strlen($2);
            if (harp_parse_double($2, length, &$$, 0) != length)
            {
                free($2);
                YYERROR;
            }
            free($2);
            $$ = -$$;
        }
    | int32_value { $$ = (double)$1; }
    | NAN { $$ = harp_nan(); }
    | '+' NAN { $$ = harp_nan(); }
    | '-' NAN { $$ = harp_nan(); }
    | '+' INF { $$ = harp_plusinf(); }
    | '-' INF { $$ = harp_mininf(); }
    ;

int32_value:
      INTEGER_VALUE {
            if (parse_int32($1, &$$) != 0)
            {
                free($1);
                YYERROR;
            }
            free($1);
        }
    | '+' INTEGER_VALUE {
            if (parse_int32($2, &$$) != 0)
            {
                free($2);
                YYERROR;
            }
            free($2);
        }
    | '-' INTEGER_VALUE {
            if (parse_int32($2, &$$) != 0)
            {
                free($2);
                YYERROR;
            }
            $$ = -$$;
            free($2);
        }
    ;

double_array:
      double_array ',' double_value {
            if (harp_sized_array_add_double($1, $3) != 0) YYERROR;
            $$ = $1;
        }
    | double_value {
            if (harp_sized_array_new(harp_type_double, &$$) != 0) YYERROR;
            if (harp_sized_array_add_double($$, $1) != 0) YYERROR;
        }
    ;

int32_array:
      int32_array ',' int32_value {
            if (harp_sized_array_add_int32($1, $3) != 0) YYERROR;
            $$ = $1;
        }
    | int32_value {
            if (harp_sized_array_new(harp_type_int32, &$$) != 0) YYERROR;
            if (harp_sized_array_add_int32($$, $1) != 0) YYERROR;
        }
    ;

string_array:
      string_array ',' STRING_VALUE {
            if (harp_sized_array_add_string($1, $3) != 0)
            {
                harp_sized_array_delete($1);
                free($3);
                YYERROR;
            }
            $$ = $1;
            free($3);
        }
    | STRING_VALUE {
            if (harp_sized_array_new(harp_type_string, &$$) != 0)
            {
                free($1);
                YYERROR;
            }
            if (harp_sized_array_add_string($$, $1) != 0)
            {
                harp_sized_array_delete($$);
                free($1);
                YYERROR;
            }
            free($1);
        }
    ;

identifier_array:
      identifier_array ',' identifier {
            if (harp_sized_array_add_string($1, $3) != 0)
            {
                harp_sized_array_delete($1);
                free($3);
                YYERROR;
            }
            $$ = $1;
            free($3);
        }
    | identifier {
            if (harp_sized_array_new(harp_type_string, &$$) != 0)
            {
                free($1);
                YYERROR;
            }
            if (harp_sized_array_add_string($$, $1) != 0)
            {
                harp_sized_array_delete($$);
                free($1);
                YYERROR;
            }
            free($1);
        }
    ;

identifier_pattern_array:
      identifier_pattern_array ',' identifier_pattern {
            if (harp_sized_array_add_string($1, $3) != 0)
            {
                harp_sized_array_delete($1);
                free($3);
                YYERROR;
            }
            $$ = $1;
            free($3);
        }
    | identifier_pattern {
            if (harp_sized_array_new(harp_type_string, &$$) != 0)
            {
                free($1);
                YYERROR;
            }
            if (harp_sized_array_add_string($$, $1) != 0)
            {
                harp_sized_array_delete($$);
                free($1);
                YYERROR;
            }
            free($1);
        }
    ;

dimension_array:
      dimension_array ',' DIMENSION {
            if (harp_sized_array_add_int32($1, $3) != 0)
            {
                harp_sized_array_delete($1);
                YYERROR;
            }
            $$ = $1;
        }
    | DIMENSION {
            if (harp_sized_array_new(harp_type_int32, &$$) != 0) YYERROR;
            if (harp_sized_array_add_int32($$, $1) != 0)
            {
                harp_sized_array_delete($$);
                YYERROR;
            }
        }
    ;

dimensionspec:
      '{' dimension_array '}' { $$ = $2; }
    | '{' '}' { if (harp_sized_array_new(harp_type_int32, &$$) != 0) YYERROR; }
    ;

comparison_operator:
      EQUAL { $$ = operator_eq; }
    | NOT_EQUAL { $$ = operator_ne; }
    | GREATER_EQUAL { $$ = operator_ge; }
    | LESS_EQUAL { $$ = operator_le; }
    | '>' { $$ = operator_gt; }
    | '<' { $$ = operator_lt; }
    ;

bit_mask_operator:
      BIT_NAND { $$ = operator_bit_mask_none; }
    | BIT_AND { $$ = operator_bit_mask_any; }
    ;

membership_operator:
      NOT IN { $$ = operator_not_in; }
    | IN { $$ = operator_in; }
    ;

operation:
      identifier bit_mask_operator int32_value {
            if (harp_operation_bit_mask_filter_new($1, $2, $3, &$$) != 0)
            {
                free($1);
                YYERROR;
            }
            free($1);
        }
    | identifier comparison_operator double_value {
            if (harp_operation_comparison_filter_new($1, $2, $3, NULL, &$$) != 0)
            {
                free($1);
                YYERROR;
            }
            free($1);
        }
    | identifier comparison_operator double_value UNIT {
            if (harp_operation_comparison_filter_new($1, $2, $3, $4, &$$) != 0)
            {
                free($1);
                free($4);
                YYERROR;
            }
            free($1);
            free($4);
        }
    | identifier comparison_operator STRING_VALUE {
            if (harp_operation_string_comparison_filter_new($1, $2, $3, &$$) != 0)
            {
                free($1);
                free($3);
                YYERROR;
            }
            free($1);
            free($3);
        }
    | identifier membership_operator '(' double_array ')' UNIT {
            if (harp_operation_membership_filter_new($1, $2, $4->num_elements, $4->array.double_data, $6, &$$) != 0)
            {
                free($1);
                harp_sized_array_delete($4);
                free($6);
                YYERROR;
            }
            free($1);
            harp_sized_array_delete($4);
            free($6);
        }
    | identifier membership_operator '(' double_array ')' {
            if (harp_operation_membership_filter_new($1, $2, $4->num_elements, $4->array.double_data, NULL, &$$) != 0)
            {
                free($1);
                harp_sized_array_delete($4);
                YYERROR;
            }
            free($1);
            harp_sized_array_delete($4);
        }
    | identifier membership_operator '(' string_array ')' {
            if (harp_operation_string_membership_filter_new($1, $2, $4->num_elements,
                                                            (const char **)$4->array.string_data, &$$) != 0)
            {
                free($1);
                harp_sized_array_delete($4);
                YYERROR;
            }
            free($1);
            harp_sized_array_delete($4);
        }
    | FUNC_INDEX '(' DIMENSION ')' comparison_operator int32_value {
            if (harp_operation_index_comparison_filter_new($3, $5, $6, &$$) != 0)
            {
                YYERROR;
            }
        }
    | FUNC_INDEX '(' DIMENSION ')' membership_operator '(' int32_array ')' {
            if (harp_operation_index_membership_filter_new($3, $5, $7->num_elements, $7->array.int32_data, &$$) !=
                0)
            {
                harp_sized_array_delete($7);
                YYERROR;
            }
            harp_sized_array_delete($7);
        }
    | FUNC_AREA_COVERS_AREA '(' '(' double_array ')' ',' '(' double_array ')' ')' {
            if (harp_operation_area_covers_area_filter_new(NULL, $4->num_elements, $4->array.double_data, NULL,
                                                           $8->num_elements, $8->array.double_data, NULL, &$$) != 0)
            {
                harp_sized_array_delete($4);
                harp_sized_array_delete($8);
                YYERROR;
            }
            harp_sized_array_delete($4);
            harp_sized_array_delete($8);
        }
    | FUNC_AREA_COVERS_AREA '(' '(' double_array ')' ',' '(' double_array ')' UNIT ')' {
            if (harp_operation_area_covers_area_filter_new(NULL, $4->num_elements, $4->array.double_data, NULL,
                                                           $8->num_elements, $8->array.double_data, $10, &$$) != 0)
            {
                harp_sized_array_delete($4);
                harp_sized_array_delete($8);
                free($10);
                YYERROR;
            }
            harp_sized_array_delete($4);
            harp_sized_array_delete($8);
            free($10);
        }
    | FUNC_AREA_COVERS_AREA '(' '(' double_array ')' UNIT ',' '(' double_array ')' ')' {
            if (harp_operation_area_covers_area_filter_new(NULL, $4->num_elements, $4->array.double_data, $6,
                                                           $9->num_elements, $9->array.double_data, NULL, &$$) != 0)
            {
                harp_sized_array_delete($4);
                free($6);
                harp_sized_array_delete($9);
                YYERROR;
            }
            harp_sized_array_delete($4);
            free($6);
            harp_sized_array_delete($9);
        }
    | FUNC_AREA_COVERS_AREA '(' '(' double_array ')' UNIT ',' '(' double_array ')' UNIT ')' {
            if (harp_operation_area_covers_area_filter_new(NULL, $4->num_elements, $4->array.double_data, $6,
                                                           $9->num_elements, $9->array.double_data, $11, &$$) != 0)
            {
                harp_sized_array_delete($4);
                free($6);
                harp_sized_array_delete($9);
                free($11);
                YYERROR;
            }
            harp_sized_array_delete($4);
            free($6);
            harp_sized_array_delete($9);
            free($11);
        }
    | FUNC_AREA_COVERS_AREA '(' STRING_VALUE ')' {
            if (harp_operation_area_covers_area_filter_new($3, 0, NULL, NULL, 0, NULL, NULL, &$$) != 0)
            {
                free($3);
                YYERROR;
            }
            free($3);
        }
    | FUNC_AREA_COVERS_POINT '(' double_value ',' double_value ')' {
            if (harp_operation_area_covers_point_filter_new($3, NULL, $5, NULL, &$$) != 0) YYERROR;
        }
    | FUNC_AREA_COVERS_POINT '(' double_value ',' double_value UNIT ')' {
            if (harp_operation_area_covers_point_filter_new($3, NULL, $5, $6, &$$) != 0)
            {
                free($6);
                YYERROR;
            }
            free($6);
        }
    | FUNC_AREA_COVERS_POINT '(' double_value UNIT ',' double_value ')' {
            if (harp_operation_area_covers_point_filter_new($3, $4, $6, NULL, &$$) != 0)
            {
                free($4);
                YYERROR;
            }
            free($4);
        }
    | FUNC_AREA_COVERS_POINT '(' double_value UNIT ',' double_value UNIT ')' {
            if (harp_operation_area_covers_point_filter_new($3, $4, $6, $7, &$$) != 0)
            {
                free($4);
                free($7);
                YYERROR;
            }
            free($4);
            free($7);
        }
    | FUNC_AREA_INSIDE_AREA '(' '(' double_array ')' ',' '(' double_array ')' ')' {
            if (harp_operation_area_inside_area_filter_new(NULL, $4->num_elements, $4->array.double_data, NULL,
                                                           $8->num_elements, $8->array.double_data, NULL, &$$) != 0)
            {
                harp_sized_array_delete($4);
                harp_sized_array_delete($8);
                YYERROR;
            }
            harp_sized_array_delete($4);
            harp_sized_array_delete($8);
        }
    | FUNC_AREA_INSIDE_AREA '(' '(' double_array ')' ',' '(' double_array ')' UNIT ')' {
            if (harp_operation_area_inside_area_filter_new(NULL, $4->num_elements, $4->array.double_data, NULL,
                                                           $8->num_elements, $8->array.double_data, $10, &$$) != 0)
            {
                harp_sized_array_delete($4);
                harp_sized_array_delete($8);
                free($10);
                YYERROR;
            }
            harp_sized_array_delete($4);
            harp_sized_array_delete($8);
            free($10);
        }
    | FUNC_AREA_INSIDE_AREA '(' '(' double_array ')' UNIT ',' '(' double_array ')' ')' {
            if (harp_operation_area_inside_area_filter_new(NULL, $4->num_elements, $4->array.double_data, $6,
                                                           $9->num_elements, $9->array.double_data, NULL, &$$) != 0)
            {
                harp_sized_array_delete($4);
                free($6);
                harp_sized_array_delete($9);
                YYERROR;
            }
            harp_sized_array_delete($4);
            free($6);
            harp_sized_array_delete($9);
        }
    | FUNC_AREA_INSIDE_AREA '(' '(' double_array ')' UNIT ',' '(' double_array ')' UNIT ')' {
            if (harp_operation_area_inside_area_filter_new(NULL, $4->num_elements, $4->array.double_data, $6,
                                                           $9->num_elements, $9->array.double_data, $11, &$$) != 0)
            {
                harp_sized_array_delete($4);
                free($6);
                harp_sized_array_delete($9);
                free($11);
                YYERROR;
            }
            harp_sized_array_delete($4);
            free($6);
            harp_sized_array_delete($9);
            free($11);
        }
    | FUNC_AREA_INSIDE_AREA '(' STRING_VALUE ')' {
            if (harp_operation_area_inside_area_filter_new($3, 0, NULL, NULL, 0, NULL, NULL, &$$) != 0)
            {
                free($3);
                YYERROR;
            }
            free($3);
        }
    | FUNC_AREA_INTERSECTS_AREA '(' '(' double_array ')' ',' '(' double_array ')' ')' {
            if (harp_operation_area_intersects_area_filter_new(NULL, $4->num_elements, $4->array.double_data, NULL,
                                                               $8->num_elements, $8->array.double_data, NULL, NULL, &$$)
                != 0)
            {
                harp_sized_array_delete($4);
                harp_sized_array_delete($8);
                YYERROR;
            }
            harp_sized_array_delete($4);
            harp_sized_array_delete($8);
        }
    | FUNC_AREA_INTERSECTS_AREA '(' '(' double_array ')' ',' '(' double_array ')' UNIT ')' {
            if (harp_operation_area_intersects_area_filter_new(NULL, $4->num_elements, $4->array.double_data, NULL,
                                                               $8->num_elements, $8->array.double_data, $10, NULL, &$$)
                != 0)
            {
                harp_sized_array_delete($4);
                harp_sized_array_delete($8);
                free($10);
                YYERROR;
            }
            harp_sized_array_delete($4);
            harp_sized_array_delete($8);
            free($10);
        }
    | FUNC_AREA_INTERSECTS_AREA '(' '(' double_array ')' UNIT ',' '(' double_array ')' ')' {
            if (harp_operation_area_intersects_area_filter_new(NULL, $4->num_elements, $4->array.double_data, $6,
                                                               $9->num_elements, $9->array.double_data, NULL, NULL, &$$)
                != 0)
            {
                harp_sized_array_delete($4);
                free($6);
                harp_sized_array_delete($9);
                YYERROR;
            }
            harp_sized_array_delete($4);
            free($6);
            harp_sized_array_delete($9);
        }
    | FUNC_AREA_INTERSECTS_AREA '(' '(' double_array ')' UNIT ',' '(' double_array ')' UNIT ')' {
            if (harp_operation_area_intersects_area_filter_new(NULL, $4->num_elements, $4->array.double_data, $6,
                                                               $9->num_elements, $9->array.double_data, $11, NULL, &$$)
                != 0)
            {
                harp_sized_array_delete($4);
                free($6);
                harp_sized_array_delete($9);
                free($11);
                YYERROR;
            }
            harp_sized_array_delete($4);
            free($6);
            harp_sized_array_delete($9);
            free($11);
        }
    | FUNC_AREA_INTERSECTS_AREA '(' STRING_VALUE ')' {
            if (harp_operation_area_intersects_area_filter_new($3, 0, NULL, NULL, 0, NULL, NULL, NULL, &$$) != 0)
            {
                free($3);
                YYERROR;
            }
            free($3);
        }
    | FUNC_AREA_INTERSECTS_AREA '(' '(' double_array ')' ',' '(' double_array ')' ',' double_value ')' {
            double min_fraction = $11;

            if (harp_operation_area_intersects_area_filter_new(NULL, $4->num_elements, $4->array.double_data, NULL,
                                                               $8->num_elements, $8->array.double_data, NULL,
                                                               &min_fraction, &$$) != 0)
            {
                harp_sized_array_delete($4);
                harp_sized_array_delete($8);
                YYERROR;
            }
            harp_sized_array_delete($4);
            harp_sized_array_delete($8);
        }
    | FUNC_AREA_INTERSECTS_AREA '(' '(' double_array ')' ',' '(' double_array ')' UNIT ',' double_value ')' {
            double min_fraction = $12;

            if (harp_operation_area_intersects_area_filter_new(NULL, $4->num_elements, $4->array.double_data, NULL,
                                                               $8->num_elements, $8->array.double_data, $10,
                                                               &min_fraction, &$$) != 0)
            {
                harp_sized_array_delete($4);
                harp_sized_array_delete($8);
                free($10);
                YYERROR;
            }
            harp_sized_array_delete($4);
            harp_sized_array_delete($8);
            free($10);
        }
    | FUNC_AREA_INTERSECTS_AREA '(' '(' double_array ')' UNIT ',' '(' double_array ')' ',' double_value ')' {
            double min_fraction = $12;

            if (harp_operation_area_intersects_area_filter_new(NULL, $4->num_elements, $4->array.double_data, $6,
                                                               $9->num_elements, $9->array.double_data, NULL,
                                                               &min_fraction, &$$) != 0)
            {
                harp_sized_array_delete($4);
                free($6);
                harp_sized_array_delete($9);
                YYERROR;
            }
            harp_sized_array_delete($4);
            free($6);
            harp_sized_array_delete($9);
        }
    | FUNC_AREA_INTERSECTS_AREA '(' '(' double_array ')' UNIT ',' '(' double_array ')' UNIT ',' double_value ')' {
            double min_fraction = $13;

            if (harp_operation_area_intersects_area_filter_new(NULL, $4->num_elements, $4->array.double_data, $6,
                                                               $9->num_elements, $9->array.double_data, $11,
                                                               &min_fraction, &$$) != 0)
            {
                harp_sized_array_delete($4);
                free($6);
                harp_sized_array_delete($9);
                free($11);
                YYERROR;
            }
            harp_sized_array_delete($4);
            free($6);
            harp_sized_array_delete($9);
            free($11);
        }
    | FUNC_AREA_INTERSECTS_AREA '(' STRING_VALUE ',' double_value ')' {
            double min_fraction = $5;

            if (harp_operation_area_intersects_area_filter_new($3, 0, NULL, NULL, 0, NULL, NULL, &min_fraction, &$$) !=
                0)
            {
                free($3);
                YYERROR;
            }
            free($3);
        }
    | FUNC_BIN '(' ')' {
            if (harp_operation_bin_full_new(&$$) != 0)
            {
                YYERROR;
            }
        }
    | FUNC_BIN '(' identifier ')' {
            if (harp_operation_bin_with_variables_new(1, (const char **)&$3, &$$) != 0)
            {
                free($3);
                YYERROR;
            }
            free($3);
        }
    | FUNC_BIN '(' '(' identifier_array ')' ')' {
            if (harp_operation_bin_with_variables_new($4->num_elements, (const char **)$4->array.string_data, &$$) != 0)
            {
                harp_sized_array_delete($4);
                YYERROR;
            }
            harp_sized_array_delete($4);
        }
    | FUNC_BIN '(' STRING_VALUE ',' ID_A ')' {
            if (harp_operation_bin_collocated_new($3, 'a', &$$) != 0)
            {
                free($3);
                YYERROR;
            }
            free($3);
        }
    | FUNC_BIN '(' STRING_VALUE ',' ID_B ')' {
            if (harp_operation_bin_collocated_new($3, 'b', &$$) != 0)
            {
                free($3);
                YYERROR;
            }
            free($3);
        }
    | FUNC_BIN_SPATIAL '(' '(' double_array ')' ',' '(' double_array ')' ')' {
            if (harp_operation_bin_spatial_new($4->num_elements, $4->array.double_data,
                                               $8->num_elements, $8->array.double_data, &$$) != 0)
            {
                harp_sized_array_delete($4);
                harp_sized_array_delete($8);
                YYERROR;
            }
            harp_sized_array_delete($4);
            harp_sized_array_delete($8);
        }
    | FUNC_BIN_SPATIAL '(' int32_value ',' double_value ',' double_value ',' int32_value ',' double_value ','
      double_value ')' {
            harp_sized_array *lat_array;
            harp_sized_array *lon_array;
            long i;

            if (harp_sized_array_new(harp_type_double, &lat_array) != 0)
            {
                YYERROR;
            }
            for (i = 0; i < $3; i++)
            {
                if (harp_sized_array_add_double(lat_array, $5 + i * $7) != 0)
                {
                    harp_sized_array_delete(lat_array);
                    YYERROR;
                }
            }
            if (harp_sized_array_new(harp_type_double, &lon_array) != 0)
            {
                harp_sized_array_delete(lat_array);
                YYERROR;
            }
            for (i = 0; i < $9; i++)
            {
                if (harp_sized_array_add_double(lon_array, $11 + i * $13) != 0)
                {
                    harp_sized_array_delete(lat_array);
                    harp_sized_array_delete(lon_array);
                    YYERROR;
                }
            }
            if (harp_operation_bin_spatial_new(lat_array->num_elements, lat_array->array.double_data,
                                               lon_array->num_elements, lon_array->array.double_data, &$$) != 0)
            {
                harp_sized_array_delete(lat_array);
                harp_sized_array_delete(lon_array);
                YYERROR;
            }
            harp_sized_array_delete(lat_array);
            harp_sized_array_delete(lon_array);
        }
    | FUNC_CLAMP '(' DIMENSION ',' identifier UNIT ',' '(' double_value ',' double_value ')' ')' {
            if (harp_operation_clamp_new($3, $5, $6, $9, $11, &$$) != 0)
            {
                free($5);
                free($6);
                YYERROR;
            }
            free($5);
            free($6);
        }
    | FUNC_COLLOCATE_LEFT '(' STRING_VALUE ')' {
            if (harp_operation_collocation_filter_new($3, harp_collocation_left, -1, -1, &$$) != 0)
            {
                free($3);
                YYERROR;
            }
            free($3);
        }
    | FUNC_COLLOCATE_LEFT '(' STRING_VALUE ',' int32_value ')' {
            if (harp_operation_collocation_filter_new($3, harp_collocation_left, $5, -1, &$$) != 0)
            {
                free($3);
                YYERROR;
            }
            free($3);
        }
    | FUNC_COLLOCATE_LEFT '(' STRING_VALUE ',' int32_value ',' int32_value ')' {
            if (harp_operation_collocation_filter_new($3, harp_collocation_left, $5, $7, &$$) != 0)
            {
                free($3);
                YYERROR;
            }
            free($3);
        }
    | FUNC_COLLOCATE_RIGHT '(' STRING_VALUE ')' {
            if (harp_operation_collocation_filter_new($3, harp_collocation_right, -1, -1, &$$) != 0)
            {
                free($3);
                YYERROR;
            }
            free($3);
        }
    | FUNC_COLLOCATE_RIGHT '(' STRING_VALUE ',' int32_value ')' {
            if (harp_operation_collocation_filter_new($3, harp_collocation_right, $5, -1, &$$) != 0)
            {
                free($3);
                YYERROR;
            }
            free($3);
        }
    | FUNC_COLLOCATE_RIGHT '(' STRING_VALUE ',' int32_value ',' int32_value ')' {
            if (harp_operation_collocation_filter_new($3, harp_collocation_right, $5, $7, &$$) != 0)
            {
                free($3);
                YYERROR;
            }
            free($3);
        }
    | FUNC_DERIVE '(' identifier ')' {
            /* even though it does nothing, we don't want this case to throw errors */
            /* it can also be used to perform an assert that a certain variable is available
             * (without having to perform a keep() with the full set of variables */
            if (harp_operation_derive_variable_new($3, NULL, -1, NULL, NULL, &$$) != 0)
            {
                free($3);
                YYERROR;
            }
            free($3);
        }
    | FUNC_DERIVE '(' identifier UNIT ')' {
            if (harp_operation_derive_variable_new($3, NULL, -1, NULL, $4, &$$) != 0)
            {
                free($3);
                free($4);
                YYERROR;
            }
            free($3);
            free($4);
        }
    | FUNC_DERIVE '(' identifier DATATYPE ')' {
            harp_data_type data_type = $4;

            if (harp_operation_derive_variable_new($3, &data_type, -1, NULL, NULL, &$$) != 0)
            {
                free($3);
                YYERROR;
            }
            free($3);
        }
    | FUNC_DERIVE '(' identifier DATATYPE UNIT ')' {
            harp_data_type data_type = $4;

            if (harp_operation_derive_variable_new($3, &data_type, -1, NULL, $5, &$$) != 0)
            {
                free($3);
                free($5);
                YYERROR;
            }
            free($3);
            free($5);
        }
    | FUNC_DERIVE '(' identifier dimensionspec ')' {
            if (harp_operation_derive_variable_new($3, NULL, $4->num_elements, $4->array.int32_data, NULL,
                                                   &$$) != 0)
            {
                free($3);
                harp_sized_array_delete($4);
                YYERROR;
            }
            free($3);
            harp_sized_array_delete($4);
        }
    | FUNC_DERIVE '(' identifier dimensionspec UNIT ')' {
            if (harp_operation_derive_variable_new($3, NULL, $4->num_elements, $4->array.int32_data, $5,
                                                   &$$) != 0)
            {
                free($3);
                harp_sized_array_delete($4);
                free($5);
                YYERROR;
            }
            free($3);
            harp_sized_array_delete($4);
            free($5);
        }
    | FUNC_DERIVE '(' identifier DATATYPE dimensionspec ')' {
            harp_data_type data_type = $4;

            if (harp_operation_derive_variable_new($3, &data_type, $5->num_elements, $5->array.int32_data, NULL, &$$) !=
                0)
            {
                free($3);
                harp_sized_array_delete($5);
                YYERROR;
            }
            free($3);
            harp_sized_array_delete($5);
        }
    | FUNC_DERIVE '(' identifier DATATYPE dimensionspec UNIT ')' {
            harp_data_type data_type = $4;

            if (harp_operation_derive_variable_new($3, &data_type, $5->num_elements, $5->array.int32_data, $6, &$$) !=
                0)
            {
                free($3);
                harp_sized_array_delete($5);
                free($6);
                YYERROR;
            }
            free($3);
            harp_sized_array_delete($5);
            free($6);
        }
    | FUNC_DERIVE_SMOOTHED_COLUMN '(' identifier dimensionspec UNIT ',' identifier UNIT ',' STRING_VALUE ',' ID_A ','
      STRING_VALUE ')' {
            if (harp_operation_derive_smoothed_column_collocated_dataset_new($3, $4->num_elements, $4->array.int32_data,
                                                                             $5, $7, $8, $10, 'a', $14, &$$) != 0)
            {
                free($3);
                harp_sized_array_delete($4);
                free($5);
                free($7);
                free($8);
                free($10);
                free($14);
                YYERROR;
            }
            free($3);
            harp_sized_array_delete($4);
            free($5);
            free($7);
            free($8);
            free($10);
            free($14);
        }
    | FUNC_DERIVE_SMOOTHED_COLUMN '(' identifier dimensionspec UNIT ',' identifier UNIT ',' STRING_VALUE ',' ID_B ','
      STRING_VALUE ')' {
            if (harp_operation_derive_smoothed_column_collocated_dataset_new($3, $4->num_elements, $4->array.int32_data,
                                                                             $5, $7, $8, $10, 'b', $14, &$$) != 0)
            {
                free($3);
                harp_sized_array_delete($4);
                free($5);
                free($7);
                free($8);
                free($10);
                free($14);
                YYERROR;
            }
            free($3);
            harp_sized_array_delete($4);
            free($5);
            free($7);
            free($8);
            free($10);
            free($14);
        }
    | FUNC_DERIVE_SMOOTHED_COLUMN '(' identifier dimensionspec UNIT ',' identifier UNIT ',' STRING_VALUE ')' {
            if (harp_operation_derive_smoothed_column_collocated_product_new($3, $4->num_elements, $4->array.int32_data,
                                                                             $5, $7, $8, $10, &$$) != 0)
            {
                free($3);
                harp_sized_array_delete($4);
                free($5);
                free($7);
                free($8);
                free($10);
                YYERROR;
            }
            free($3);
            harp_sized_array_delete($4);
            free($5);
            free($7);
            free($8);
            free($10);
        }
    | FUNC_EXCLUDE '(' identifier_pattern_array ')' {
            if (harp_operation_exclude_variable_new($3->num_elements, (const char **)$3->array.string_data, &$$) != 0)
            {
                harp_sized_array_delete($3);
                YYERROR;
            }
            harp_sized_array_delete($3);
        }
    | FUNC_FLATTEN '(' DIMENSION ')' {
            if (harp_operation_flatten_new($3, &$$) != 0) YYERROR;
        }
    | FUNC_KEEP '(' identifier_pattern_array ')' {
            if (harp_operation_keep_variable_new($3->num_elements, (const char **)$3->array.string_data, &$$) != 0)
            {
                harp_sized_array_delete($3);
                YYERROR;
            }
            harp_sized_array_delete($3);
        }
    | FUNC_LONGITUDE_RANGE '(' double_value ',' double_value ')' {
            if (harp_operation_longitude_range_filter_new($3, NULL, $5, NULL, &$$) != 0) YYERROR;
        }
    | FUNC_LONGITUDE_RANGE '(' double_value ',' double_value UNIT ')' {
            if (harp_operation_longitude_range_filter_new($3, NULL, $5, $6, &$$) != 0)
            {
                free($6);
                YYERROR;
            }
            free($6);
        }
    | FUNC_LONGITUDE_RANGE '(' double_value UNIT ',' double_value ')' {
            if (harp_operation_longitude_range_filter_new($3, $4, $6, NULL, &$$) != 0)
            {
                free($4);
                YYERROR;
            }
            free($4);
        }
    | FUNC_LONGITUDE_RANGE '(' double_value UNIT ',' double_value UNIT ')' {
            if (harp_operation_longitude_range_filter_new($3, $4, $6, $7, &$$) != 0)
            {
                free($4);
                free($7);
                YYERROR;
            }
            free($4);
            free($7);
        }
    | FUNC_POINT_DISTANCE '(' double_value ',' double_value ',' double_value ')' {
            if (harp_operation_point_distance_filter_new($3, NULL, $5, NULL, $7, NULL, &$$) != 0) YYERROR;
        }
    | FUNC_POINT_DISTANCE '(' double_value ',' double_value ',' double_value UNIT ')' {
            if (harp_operation_point_distance_filter_new($3, NULL, $5, NULL, $7, $8, &$$) != 0)
            {
                free($8);
                YYERROR;
            }
            free($8);
        }
    | FUNC_POINT_DISTANCE '(' double_value ',' double_value UNIT ',' double_value ')' {
            if (harp_operation_point_distance_filter_new($3, NULL, $5, $6, $8, NULL, &$$) != 0)
            {
                free($6);
                YYERROR;
            }
            free($6);
        }
    | FUNC_POINT_DISTANCE '(' double_value ',' double_value UNIT ',' double_value UNIT ')' {
            if (harp_operation_point_distance_filter_new($3, NULL, $5, $6, $8, $9, &$$) != 0)
            {
                free($6);
                free($9);
                YYERROR;
            }
            free($6);
            free($9);
        }
    | FUNC_POINT_DISTANCE '(' double_value UNIT ',' double_value ',' double_value ')' {
            if (harp_operation_point_distance_filter_new($3, $4, $6, NULL, $8, NULL, &$$) != 0)
            {
                free($4);
                YYERROR;
            }
            free($4);
        }
    | FUNC_POINT_DISTANCE '(' double_value UNIT ',' double_value ',' double_value UNIT ')' {
            if (harp_operation_point_distance_filter_new($3, $4, $6, NULL, $8, $9, &$$) != 0)
            {
                free($4);
                free($9);
                YYERROR;
            }
            free($4);
            free($9);
        }
    | FUNC_POINT_DISTANCE '(' double_value UNIT ',' double_value UNIT ',' double_value ')' {
            if (harp_operation_point_distance_filter_new($3, $4, $6, $7, $9, NULL, &$$) != 0)
            {
                free($4);
                free($7);
                YYERROR;
            }
            free($4);
            free($7);
        }
    | FUNC_POINT_DISTANCE '(' double_value UNIT ',' double_value UNIT ',' double_value UNIT ')' {
            if (harp_operation_point_distance_filter_new($3, $4, $6, $7, $9, $10, &$$) != 0)
            {
                free($4);
                free($7);
                free($10);
                YYERROR;
            }
            free($4);
            free($7);
            free($10);
        }
    | FUNC_POINT_IN_AREA '(' '(' double_array ')' ',' '(' double_array ')' ')' {
            if (harp_operation_point_in_area_filter_new(NULL, $4->num_elements, $4->array.double_data, NULL,
                                                        $8->num_elements, $8->array.double_data, NULL, &$$) != 0)
            {
                harp_sized_array_delete($4);
                harp_sized_array_delete($8);
                YYERROR;
            }
            harp_sized_array_delete($4);
            harp_sized_array_delete($8);
        }
    | FUNC_POINT_IN_AREA '(' '(' double_array ')' ',' '(' double_array ')' UNIT ')' {
            if (harp_operation_point_in_area_filter_new(NULL, $4->num_elements, $4->array.double_data, NULL,
                                                        $8->num_elements, $8->array.double_data, $10, &$$) != 0)
            {
                harp_sized_array_delete($4);
                harp_sized_array_delete($8);
                free($10);
                YYERROR;
            }
            harp_sized_array_delete($4);
            harp_sized_array_delete($8);
            free($10);
        }
    | FUNC_POINT_IN_AREA '(' '(' double_array ')' UNIT ',' '(' double_array ')' ')' {
            if (harp_operation_point_in_area_filter_new(NULL, $4->num_elements, $4->array.double_data, $6,
                                                        $9->num_elements, $9->array.double_data, NULL, &$$) != 0)
            {
                harp_sized_array_delete($4);
                free($6);
                harp_sized_array_delete($9);
                YYERROR;
            }
            harp_sized_array_delete($4);
            free($6);
            harp_sized_array_delete($9);
        }
    | FUNC_POINT_IN_AREA '(' '(' double_array ')' UNIT ',' '(' double_array ')' UNIT ')' {
            if (harp_operation_point_in_area_filter_new(NULL, $4->num_elements, $4->array.double_data, $6,
                                                        $9->num_elements, $9->array.double_data, $11, &$$) != 0)
            {
                harp_sized_array_delete($4);
                free($6);
                harp_sized_array_delete($9);
                free($11);
                YYERROR;
            }
            harp_sized_array_delete($4);
            free($6);
            harp_sized_array_delete($9);
            free($11);
        }
    | FUNC_POINT_IN_AREA '(' STRING_VALUE ')' {
            if (harp_operation_point_in_area_filter_new($3, 0, NULL, NULL, 0, NULL, NULL, &$$) != 0)
            {
                free($3);
                YYERROR;
            }
            free($3);
        }
    | FUNC_REBIN '(' DIMENSION ',' identifier UNIT ',' '(' double_array ')' ')' {
            if (harp_operation_rebin_new($3, $5, $6, $9->num_elements, $9->array.double_data, &$$) != 0)
            {
                free($5);
                free($6);
                harp_sized_array_delete($9);
                YYERROR;
            }
            free($5);
            free($6);
            harp_sized_array_delete($9);
        }
    | FUNC_REBIN '(' DIMENSION ',' identifier UNIT ',' int32_value ',' double_value ',' double_value ')' {
            harp_sized_array *array;
            long i;

            if (harp_sized_array_new(harp_type_double, &array) != 0)
            {
                free($5);
                free($6);
                YYERROR;
            }
            for (i = 0; i < $8; i++)
            {
                if (harp_sized_array_add_double(array, $10 + i * $12) != 0)
                {
                    harp_sized_array_delete(array);
                    free($5);
                    free($6);
                    YYERROR;
                }
            }
            if (harp_operation_rebin_new($3, $5, $6, array->num_elements, array->array.double_data, &$$) != 0)
            {
                harp_sized_array_delete(array);
                free($5);
                free($6);
                YYERROR;
            }
            harp_sized_array_delete(array);
            free($5);
            free($6);
        }
    | FUNC_REGRID '(' DIMENSION ',' identifier UNIT ',' '(' double_array ')' ')' {
            if (harp_operation_regrid_new($3, $5, $6, $9->num_elements, $9->array.double_data, 0, NULL, &$$) != 0)
            {
                free($5);
                free($6);
                harp_sized_array_delete($9);
                YYERROR;
            }
            free($5);
            free($6);
            harp_sized_array_delete($9);
        }
    | FUNC_REGRID '(' DIMENSION ',' identifier UNIT ',' '(' double_array ')' ',' '(' double_array ')' ')' {
            if (harp_operation_regrid_new($3, $5, $6, $9->num_elements, $9->array.double_data, $13->num_elements,
                                          $13->array.double_data, &$$) != 0)
            {
                free($5);
                free($6);
                harp_sized_array_delete($9);
                harp_sized_array_delete($13);
                YYERROR;
            }
            free($5);
            free($6);
            harp_sized_array_delete($9);
            harp_sized_array_delete($13);
        }
    | FUNC_REGRID '(' DIMENSION ',' identifier UNIT ',' int32_value ',' double_value ',' double_value ')' {
            harp_sized_array *array;
            long i;

            if (harp_sized_array_new(harp_type_double, &array) != 0)
            {
                free($5);
                free($6);
                YYERROR;
            }
            for (i = 0; i < $8; i++)
            {
                if (harp_sized_array_add_double(array, $10 + i * $12) != 0)
                {
                    harp_sized_array_delete(array);
                    free($5);
                    free($6);
                    YYERROR;
                }
            }
            if (harp_operation_regrid_new($3, $5, $6, array->num_elements, array->array.double_data, 0, NULL, &$$) != 0)
            {
                harp_sized_array_delete(array);
                free($5);
                free($6);
                YYERROR;
            }
            harp_sized_array_delete(array);
            free($5);
            free($6);
        }
    | FUNC_REGRID '(' DIMENSION ',' identifier UNIT ',' STRING_VALUE ',' ID_A ',' STRING_VALUE ')' {
            if (harp_operation_regrid_collocated_dataset_new($3, $5, $6, $8, 'a', $12, &$$) != 0)
            {
                free($5);
                free($6);
                free($8);
                free($12);
                YYERROR;
            }
            free($5);
            free($6);
            free($8);
            free($12);
        }
    | FUNC_REGRID '(' DIMENSION ',' identifier UNIT ',' STRING_VALUE ',' ID_B ',' STRING_VALUE ')' {
            if (harp_operation_regrid_collocated_dataset_new($3, $5, $6, $8, 'b', $12, &$$) != 0)
            {
                free($5);
                free($6);
                free($8);
                free($12);
                YYERROR;
            }
            free($5);
            free($6);
            free($8);
            free($12);
        }
    | FUNC_REGRID '(' DIMENSION ',' identifier UNIT ',' STRING_VALUE ')' {
            if (harp_operation_regrid_collocated_product_new($3, $5, $6, $8, &$$) != 0)
            {
                free($5);
                free($6);
                free($8);
                YYERROR;
            }
            free($5);
            free($6);
            free($8);
        }
    | FUNC_RENAME '(' identifier ',' identifier ')' {
            if (harp_operation_rename_new($3, $5, &$$) != 0)
            {
                free($3);
                free($5);
                YYERROR;
            }
            free($3);
            free($5);
        }
    | FUNC_SET '(' STRING_VALUE ',' STRING_VALUE ')' {
            if (harp_operation_set_new($3, $5, &$$) != 0)
            {
                free($3);
                free($5);
                YYERROR;
            }
            free($3);
            free($5);
        }
    | FUNC_SMOOTH '(' identifier ',' DIMENSION ',' identifier UNIT ',' STRING_VALUE ',' ID_A ',' STRING_VALUE ')' {
            if (harp_operation_smooth_collocated_dataset_new(1, (const char **)&$3, $5, $7, $8, $10, 'a', $14, &$$) !=
                0)
            {
                free($3);
                free($7);
                free($8);
                free($10);
                free($14);
                YYERROR;
            }
            free($3);
            free($7);
            free($8);
            free($10);
            free($14);
        }
    | FUNC_SMOOTH '(' identifier ',' DIMENSION ',' identifier UNIT ',' STRING_VALUE ',' ID_B ',' STRING_VALUE ')' {
            if (harp_operation_smooth_collocated_dataset_new(1, (const char **)&$3, $5, $7, $8, $10, 'b', $14, &$$) !=
                0)
            {
                free($3);
                free($7);
                free($8);
                free($10);
                free($14);
                YYERROR;
            }
            free($3);
            free($7);
            free($8);
            free($10);
            free($14);
        }
    | FUNC_SMOOTH '(' identifier ',' DIMENSION ',' identifier UNIT ',' STRING_VALUE ')' {
            if (harp_operation_smooth_collocated_product_new(1, (const char **)&$3, $5, $7, $8, $10, &$$) != 0)
            {
                free($3);
                free($7);
                free($8);
                free($10);
                YYERROR;
            }
            free($3);
            free($7);
            free($8);
            free($10);
        }
    | FUNC_SMOOTH '(' '(' identifier_array ')' ',' DIMENSION ',' identifier UNIT ',' STRING_VALUE ',' ID_A ','
      STRING_VALUE ')' {
            if (harp_operation_smooth_collocated_dataset_new($4->num_elements, (const char **)$4->array.string_data,
                                                     $7, $9, $10, $12, 'a', $16, &$$) != 0)
            {
                harp_sized_array_delete($4);
                free($9);
                free($10);
                free($12);
                free($16);
                YYERROR;
            }
            harp_sized_array_delete($4);
            free($9);
            free($10);
            free($12);
            free($16);
        }
    | FUNC_SMOOTH '(' '(' identifier_array ')' ',' DIMENSION ',' identifier UNIT ',' STRING_VALUE ',' ID_B ','
      STRING_VALUE ')' {
            if (harp_operation_smooth_collocated_dataset_new($4->num_elements, (const char **)$4->array.string_data,
                                                     $7, $9, $10, $12, 'b', $16, &$$) != 0)
            {
                harp_sized_array_delete($4);
                free($9);
                free($10);
                free($12);
                free($16);
                YYERROR;
            }
            harp_sized_array_delete($4);
            free($9);
            free($10);
            free($12);
            free($16);
        }
    | FUNC_SMOOTH '(' '(' identifier_array ')' ',' DIMENSION ',' identifier UNIT ',' STRING_VALUE ')' {
            if (harp_operation_smooth_collocated_product_new($4->num_elements, (const char **)$4->array.string_data,
                                                             $7, $9, $10, $12, &$$) != 0)
            {
                harp_sized_array_delete($4);
                free($9);
                free($10);
                free($12);
                YYERROR;
            }
            harp_sized_array_delete($4);
            free($9);
            free($10);
            free($12);
        }
    | FUNC_SORT '(' identifier ')' {
            if (harp_operation_sort_new(1, (const char **)&$3, &$$) != 0)
            {
                free($3);
                YYERROR;
            }
            free($3);
        }
    | FUNC_SORT '(' '(' identifier_array ')' ')' {
            if (harp_operation_sort_new($4->num_elements, (const char **)$4->array.string_data, &$$) != 0)
            {
                harp_sized_array_delete($4);
                YYERROR;
            }
            harp_sized_array_delete($4);
        }
    | FUNC_SQUASH '(' DIMENSION ',' identifier ')' {
            if (harp_operation_squash_new($3, 1, (const char **)&$5, &$$) != 0)
            {
                free($5);
                YYERROR;
            }
            free($5);
        }
    | FUNC_SQUASH '(' DIMENSION ',' '(' identifier_array ')' ')' {
            if (harp_operation_squash_new($3, $6->num_elements, (const char **)$6->array.string_data, &$$) != 0)
            {
                harp_sized_array_delete($6);
                YYERROR;
            }
            harp_sized_array_delete($6);
        }
    | FUNC_VALID '(' identifier ')' {
            if (harp_operation_valid_range_filter_new($3, &$$) != 0)
            {
                free($3);
                YYERROR;
            }
            free($3);
        }
    | FUNC_WRAP '(' identifier ',' double_value ',' double_value ')' {
            if (harp_operation_wrap_new($3, NULL, $5, $7, &$$) != 0)
            {
                free($3);
                YYERROR;
            }
            free($3);
        }
    | FUNC_WRAP '(' identifier UNIT ',' double_value ',' double_value ')' {
            if (harp_operation_wrap_new($3, $4, $6, $8, &$$) != 0)
            {
                free($3);
                free($4);
                YYERROR;
            }
            free($3);
            free($4);
        }
    ;

program:
      program ';' operation {
            if (harp_program_add_operation($1, $3) != 0)
            {
                harp_program_delete($1);
                harp_operation_delete($3);
                YYERROR;
            }
            $$ = $1;
        }
    | operation {
            if (harp_program_new(&$$) != 0)
            {
                harp_operation_delete($1);
                YYERROR;
            }
            if (harp_program_add_operation($$, $1) != 0)
            {
                harp_operation_delete($1);
                harp_program_delete($$);
                YYERROR;
            }
        }
    | /* empty */ {
            if (harp_program_new(&$$) != 0) YYERROR;
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
        if (parsed_program != NULL)
        {
            harp_program_delete(parsed_program);
        }
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

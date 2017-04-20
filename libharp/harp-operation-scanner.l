/*
 * Copyright (C) 2015-2017 S[&]T, The Netherlands.
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

%option prefix="harp_operation_parser_"
%option noyywrap
%option noinput
%option nounput
%option always-interactive

%{
#include <stdlib.h>
#include <string.h>

#include "harp-operation.h"
#include "harp-program.h"

typedef struct harp_sized_array_struct harp_sized_array;

#include "harp-operation-parser.h"

%}

INTEGER      [0-9]+
SIGN         [+-]
FL_MANTISSA  ({INTEGER}".")|("."{INTEGER})|({INTEGER}"."{INTEGER})
FL_EXPONENT  [Ee]{SIGN}?{INTEGER}
FLOAT        ({FL_MANTISSA}{FL_EXPONENT}?)|({INTEGER}{FL_EXPONENT})
IDENTIFIER   [[:alpha:]][[:alnum:]_]*
WHITESPACE   [[:space:]]+

%%

"\""([^\"\\]|\\.)*"\""  {
                            yylval.string_val = (char *)malloc(yyleng - 1);
                            memcpy(yylval.string_val, yytext + 1, yyleng - 2);
                            yylval.string_val[yyleng - 2] = '\0';
                            return STRING_VALUE;
                        }
"["([^\[])*"]"          {
                            yylval.string_val = (char *)malloc(yyleng - 1);
                            memcpy(yylval.string_val, yytext + 1, yyleng - 2);
                            yylval.string_val[yyleng - 2] = '\0';
                            return UNIT;
                        }

"=="                    return EQUAL;
"!="                    return NOT_EQUAL;
">="                    return GREATER_EQUAL;
"<="                    return LESS_EQUAL;
"=&"                    return BIT_AND;
"!&"                    return BIT_NAND;

"nan"                   return NAN;
"inf"                   return INF;
"not"                   return NOT;
"in"                    return IN;

"a"                     return ID_A;
"b"                     return ID_B;

"area_mask_covers_area" return FUNC_AREA_MASK_COVERS_AREA;
"area_mask_covers_point"    return FUNC_AREA_MASK_COVERS_POINT;
"area_mask_intersects_area" return FUNC_AREA_MASK_INTERSECTS_AREA;
"bin"                   return FUNC_BIN;
"collocate_left"        return FUNC_COLLOCATE_LEFT;
"collocate_right"       return FUNC_COLLOCATE_RIGHT;
"derive"                return FUNC_DERIVE;
"derive_smoothed_column"	return FUNC_DERIVE_SMOOTHED_COLUMN;
"exclude"               return FUNC_EXCLUDE;
"flatten"               return FUNC_FLATTEN;
"keep"                  return FUNC_KEEP;
"longitude_range"       return FUNC_LONGITUDE_RANGE;
"point_distance"        return FUNC_POINT_DISTANCE;
"point_in_area"         return FUNC_POINT_IN_AREA;
"regrid"                return FUNC_REGRID;
"rename"                return FUNC_RENAME;
"set"                   return FUNC_SET;
"smooth"                return FUNC_SMOOTH;
"valid"                 return FUNC_VALID;
"wrap"                  return FUNC_WRAP;

"int8"                  {
                            yylval.int32_val = harp_type_int8;
                            return DATATYPE;
                        }
"int16"                 {
                            yylval.int32_val = harp_type_int16;
                            return DATATYPE;
                        }
"int32"                 {
                            yylval.int32_val = harp_type_int32;
                            return DATATYPE;
                        }
"float"                 {
                            yylval.int32_val = harp_type_float;
                            return DATATYPE;
                        }
"double"                {
                            yylval.int32_val = harp_type_double;
                            return DATATYPE;
                        }
"string"                {
                            yylval.int32_val = harp_type_string;
                            return DATATYPE;
                        }

"time"                  {
                            yylval.int32_val = harp_dimension_time;
                            return DIMENSION;
                        }
"latitude"              {
                            yylval.int32_val = harp_dimension_latitude;
                            return DIMENSION;
                        }
"longitude"             {
                            yylval.int32_val = harp_dimension_longitude;
                            return DIMENSION;
                        }
"vertical"              {
                            yylval.int32_val = harp_dimension_vertical;
                            return DIMENSION;
                        }
"spectral"              {
                            yylval.int32_val = harp_dimension_spectral;
                            return DIMENSION;
                        }
"independent"           {
                            yylval.int32_val = harp_dimension_independent;
                            return DIMENSION;
                        }

{IDENTIFIER}            {
                            yylval.string_val = strdup(yytext);
                            return NAME;
                        }
{INTEGER}               {
                            yylval.string_val = strdup(yytext);
                            return INTEGER_VALUE;
                        }
{FLOAT}                 {
                            yylval.string_val = strdup(yytext);
                            return DOUBLE_VALUE;
                        }
{WHITESPACE}            /* eat whitespace */
.                       return (*yytext);

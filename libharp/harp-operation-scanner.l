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

"\""([^\"\\]|\\.)*"\"" {
                            yylval.string_val = (char *)malloc(yyleng - 1);
                            memcpy(yylval.string_val, yytext + 1, yyleng - 2);
                            yylval.string_val[yyleng - 2] = '\0';
                            return STRING_VALUE;
                        }
"[""([^\[])*""]"        {
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
"collocate-left"        return FUNC_COLLOCATE_LEFT;
"collocate-right"       return FUNC_COLLOCATE_RIGHT;
"valid"                 return FUNC_VALID;
"longitude-range"       return FUNC_LONGITUDE_RANGE;
"point-distance"        return FUNC_POINT_DISTANCE;
"area-mask-covers-point"    return FUNC_AREA_MASK_COVERS_POINT;
"area-mask-covers-area" return FUNC_AREA_MASK_COVERS_AREA;
"area-mask-intersects-area" return FUNC_AREA_MASK_INTERSECTS_AREA;
"derive"                return FUNC_DERIVE;
"keep"                  return FUNC_KEEP;
"exclude"               return FUNC_EXCLUDE;
"flatten"               return FUNC_FLATTEN;
"regrid"                return FUNC_REGRID;
"regrid-collocated"     return FUNC_REGRID_COLLOCATED;

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

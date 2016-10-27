%{
#include "harp-operation-parser.h"
#define YYSTYPE HARP_OPERATION_PARSER_STYPE
%}

%option prefix="harp_operation_parser_"
%option reentrant
%option noyywrap
%option noinput
%option nounput
%option bison-bridge

digit         [0-9]
letter        [a-zA-Z]

%%

[\[][^\]]+[\]]  {
  yylval->string = strdup(yytext);
  return HARP_OP_TOK_UNIT;
}
","                   {return HARP_OP_TOK_COMMA;}
";"                   {return HARP_OP_TOK_SEMICOLON;}
[-+]?[Nn][aA][Nn]     {return HARP_OP_TOK_NAN;}
[-+]?[Ii][nN][fF]     {return HARP_OP_TOK_INF;}
[-+]?[0-9]+           {return HARP_OP_TOK_INT;}
[-+]?[0-9]+.[0-9]+    {return HARP_OP_TOK_FLOAT;}
[-+]?[0-9]+(.[0-9]+)?[dDeE][+-]?[0-9]+ {return HARP_OP_TOK_FLOAT;}
"time"                {return HARP_OP_TOK_DIM_TIME;}
"latitude"            {return HARP_OP_TOK_DIM_LAT;}
"longitude"           {return HARP_OP_TOK_DIM_LON;}
"vertical"            {return HARP_OP_TOK_DIM_VERTICAL;}
"spectral"            {return HARP_OP_TOK_DIM_SPECTRAL;}
"independent"         {return HARP_OP_TOK_DIM_INDEP;}
"{"                   {return HARP_OP_TOK_LEFT_CURLY;}
"}"                   {return HARP_OP_TOK_RIGHT_CURLY;}
"("                   {return HARP_OP_TOK_LEFT_PAREN;}
")"                   {return HARP_OP_TOK_RIGHT_PAREN;}
"collocate-left"      {return HARP_OP_TOK_F_COLLOCATE_LEFT;}
"collocate-right"     {return HARP_OP_TOK_F_COLLOCATE_RIGHT;}
"valid"               {return HARP_OP_TOK_F_VALID;}
"longitude-range"     {return HARP_OP_TOK_F_LON_RANGE;}
"point-distance"      {return HARP_OP_TOK_F_POINT_DIST;}
"area-mask-covers-point" {return HARP_OP_TOK_F_AREA_MASK_COVERS_POINT;}
"area-mask-covers-area"  {return HARP_OP_TOK_F_AREA_MASK_COVERS_AREA;}
"area-mask-intersects-area" {return HARP_OP_TOK_F_AREA_MASK_INTERSECTS_AREA;}
"derive"              {return HARP_OP_TOK_F_DERIVE;}
"keep"                {return HARP_OP_TOK_F_KEEP;}
"exclude"             {return HARP_OP_TOK_F_EXCLUDE;}
"flatten"             {return HARP_OP_TOK_F_FLATTEN;}
"regrid"              {return HARP_OP_TOK_F_REGRID;}
"regrid-collocated"   {return HARP_OP_TOK_F_REGRID_COLLOCATED;}
"=="                  {return HARP_OP_TOK_OP_EQ;}
"!="                  {return HARP_OP_TOK_OP_NE;}
">="                  {return HARP_OP_TOK_OP_GE;}
"<="                  {return HARP_OP_TOK_OP_LE;}
"<"                   {return HARP_OP_TOK_OP_LT;}
">"                   {return HARP_OP_TOK_OP_GT;}
"=&"                  {return HARP_OP_TOK_OP_BIT_AND;}
"!&"                  {return HARP_OP_TOK_OP_BIT_NAND;}
"not"                 {return HARP_OP_TOK_NOT;}
"in"                  {return HARP_OP_TOK_IN;}
[ab]                  {return HARP_OP_TOK_COLLOCATION_COLUMN;}
{letter}({letter}|_|{digit})* {return HARP_OP_TOK_ID;}
\"(\\.|[^"])*\"       {return HARP_OP_TOK_STRING;}
[ \t]+

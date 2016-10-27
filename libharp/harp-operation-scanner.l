%{
  #include "harp-operation-parser.h"
%}

%option prefix="harp_operation_"
%option reentrant
%option noyywrap
%option noinput
%option nounput

digit         [0-9]
letter        [a-zA-Z]

%%

[\[][^\]]+[\]]  {return OPERATION_UNIT;}
","                   {return OPERATION_COMMA;}
";"                   {return OPERATION_SEMICOLON;}
[-+]?[Nn][aA][Nn]     {return OPERATION_NAN;}
[-+]?[Ii][nN][fF]     {return OPERATION_INF;}
[-+]?[0-9]+           {return OPERATION_INT;}
[-+]?[0-9]+.[0-9]+    {return OPERATION_FLOAT;}
[-+]?[0-9]+(.[0-9]+)?[dDeE][+-]?[0-9]+ {return OPERATION_FLOAT;}
"time"                {return OPERATION_DIM_TIME;}
"latitude"            {return OPERATION_DIM_LAT;}
"longitude"           {return OPERATION_DIM_LON;}
"vertical"            {return OPERATION_DIM_VERTICAL;}
"spectral"            {return OPERATION_DIM_SPECTRAL;}
"independent"         {return OPERATION_DIM_INDEP;}
"{"                   {return OPERATION_LEFT_CURLY;}
"}"                   {return OPERATION_RIGHT_CURLY;}
"("                   {return OPERATION_LEFT_PAREN;}
")"                   {return OPERATION_RIGHT_PAREN;}
"collocate-left"      {return OPERATION_F_COLLOCATE_LEFT;}
"collocate-right"     {return OPERATION_F_COLLOCATE_RIGHT;}
"valid"               {return OPERATION_F_VALID;}
"longitude-range"     {return OPERATION_F_LON_RANGE;}
"point-distance"      {return OPERATION_F_POINT_DIST;}
"area-mask-covers-point" {return OPERATION_F_AREA_MASK_COVERS_POINT;}
"area-mask-covers-area"  {return OPERATION_F_AREA_MASK_COVERS_AREA;}
"area-mask-intersects-area" {return OPERATION_F_AREA_MASK_INTERSECTS_AREA;}
"derive"              {return OPERATION_F_DERIVE;}
"keep"                {return OPERATION_F_KEEP;}
"exclude"             {return OPERATION_F_EXCLUDE;}
"flatten"             {return OPERATION_F_FLATTEN;}
"regrid"              {return OPERATION_F_REGRID;}
"regrid-collocated"   {return OPERATION_F_REGRID_COLLOCATED;}
"=="                  {return OPERATION_OP_EQ;}
"!="                  {return OPERATION_OP_NE;}
">="                  {return OPERATION_OP_GE;}
"<="                  {return OPERATION_OP_LE;}
"<"                   {return OPERATION_OP_LT;}
">"                   {return OPERATION_OP_GT;}
"=&"                  {return OPERATION_OP_BIT_AND;}
"!&"                  {return OPERATION_OP_BIT_NAND;}
"not"                 {return OPERATION_NOT;}
"in"                  {return OPERATION_IN;}
[ab]                  {return OPERATION_COLLOCATION_COLUMN;}
{letter}({letter}|_|{digit})* {return OPERATION_ID;}
\"(\\.|[^"])*\"       {return OPERATION_STRING;}
[ \t]+

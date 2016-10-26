%{
  #include "harp-parser.h"
%}

%option reentrant
%option noyywrap

digit         [0-9]
letter        [a-zA-Z]

%%

[\[][^\]]+[\]]  {return UNIT;}
","                   {return COMMA;}
";"                   {return SEMICOLON;}
[-+]?[Nn][aA][Nn]     {return NAN;}
[-+]?[Ii][nN][fF]     {return INF;}
[-+]?[0-9]+           {return INT;}
[-+]?[0-9]+.[0-9]+    {return FLOAT;}
[-+]?[0-9]+(.[0-9]+)?[dDeE][+-]?[0-9]+ {return FLOAT;}
"time"                {return DIM_TIME;}
"latitude"            {return DIM_LAT;}
"longitude"           {return DIM_LON;}
"vertical"            {return DIM_VERTICAL;}
"spectral"            {return DIM_SPECTRAL;}
"independent"         {return DIM_INDEP;}
"{"                   {return LEFT_CURLY;}
"}"                   {return RIGHT_CURLY;}
"("                   {return LEFT_PAREN;}
")"                   {return RIGHT_PAREN;}
"collocate-left"      {return F_COLLOCATE_LEFT;}
"collocate-right"     {return F_COLLOCATE_RIGHT;}
"valid"               {return F_VALID;}
"longitude-range"     {return F_LON_RANGE;}
"point-distance"      {return F_POINT_DIST;}
"area-mask-covers-point" {return F_AREA_MASK_COVERS_POINT;}
"area-mask-covers-area"  {return F_AREA_MASK_COVERS_AREA;}
"area-mask-intersects-area" {return F_AREA_MASK_INTERSECTS_AREA;}
"derive"              {return F_DERIVE;}
"keep"                {return F_KEEP;}
"exclude"             {return F_EXCLUDE;}
"flatten"             {return F_FLATTEN;}
"regrid"              {return F_REGRID;}
"regrid-collocated"   {return F_REGRID_COLLOCATED;}
"=="                  {return OP_EQ;}
"!="                  {return OP_NE;}
">="                  {return OP_GE;}
"<="                  {return OP_LE;}
"<"                   {return OP_LT;}
">"                   {return OP_GT;}
"=&"                  {return OP_BIT_AND;}
"!&"                  {return OP_BIT_NAND;}
"not"                 {return NOT;}
"in"                  {return IN;}
[ab]                  {return COLLOCATION_COLUMN;}
{letter}({letter}|_|{digit})* {return ID;}
\"(\\.|[^"])*\"       {return STRING;}
[ \t]+

%{
  #include "harp-parser.h"

  int yyerror(const char *str)
  {
  printf("Supr!\n");
  }
%}

%option reentrant
%option noyywrap

%x DOUBLE_QUOTED

digit         [0-9]
letter        [a-zA-Z]

%%

"["[a-zA-Z0-9._-]"]"  {return UNIT;}
","                   {return COMMA;}
";"                   {return SEMICOLON;}
[-+]?[Nn][aA][Nn]     {return NAN;}
[-+]?[Ii][nN][fF]     {return INF;}
[-+]?[0-9]+           {return INT;}
[-+]?[0-9]+.[0-9]+    {return FLOAT;}
[-+]?[0-9]+(.[0-9]+)?[dDeE][+-]?[0-9]+ {return FLOAT;}
"time"                {return DIM_TIME;}
"lat"                 {return DIM_LAT;}
"longitude"           {return DIM_LON;}
"latitude"            {return DIM_VERTICAL;}
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
{letter}({letter}|{digit})* { return ID; }
[ \t]+

["]                    {BEGIN(DOUBLE_QUOTED);}
<DOUBLE_QUOTED>[^"\\]+ {}
<DOUBLE_QUOTED>[\\].   {}
<DOUBLE_QUOTED><<EOF>> {return -1;}
<DOUBLE_QUOTED>["]     {BEGIN(INITIAL); return STRING;}

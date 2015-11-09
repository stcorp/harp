%option noyywrap
%option always-interactive
%option noinput
%option nounput

%x COMMENT
%x STRING
%x TYPEDEF

%{
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int depth;
%}

ID                  [a-zA-Z][a-zA-Z0-9_]*
%%

"/*"                BEGIN(COMMENT);
<COMMENT>"*/"       BEGIN(INITIAL);
"\""                BEGIN(STRING);
<STRING>"\\\""      /* eat escaped double quotes */
<STRING>"\""        BEGIN(INITIAL);
{ID}                {
                        if (strcmp(yytext,"typedef") == 0)
                        {
                            depth = 0;
                            BEGIN(TYPEDEF);
                        }
                    }
<TYPEDEF>"{"        depth++;
<TYPEDEF>"}"        depth--;
<TYPEDEF>{ID}("["[0-9, ]+"]")*[ \t\n]*";" {
                        if (depth == 0)
                        {
                            int n;
                            n = 0;
                            while (yytext[n] != ';' &&
                                   yytext[n] != '[' &&
                                   !isspace(yytext[n]))
                            {
                                n++;
                            }
                            yytext[n] = '\0';
                            fprintf(yyout, "%s\n", yytext);
                            BEGIN(INITIAL);
                        }
                    }
<*>.|"\n"           /* eat up characters */

%%

int main(int argc, char *argv[])
{
    if (argc > 1)
    {
        yyin = fopen(argv[1], "r");
        if (yyin == NULL)
        {
            fprintf(stderr, "ERROR: could not open file\n");
            exit(1);
        }
    }
    else
    {
        yyin = stdin;
    }
    yyout = stdout;

    yylex();

    return 0;
}

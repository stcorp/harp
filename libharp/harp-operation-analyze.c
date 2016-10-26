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

#include "harp-internal.h"
#include "harp-operation.h"
#include "harp-program.h"
#include "harp-parser.h"
#include "harp-parser-state.h"
#include "harp-scanner.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#define MAX_NUM_FUNCTION_ARGUMENTS 5

int harp_program_from_string(const char *str, harp_program **new_program)
{
    harp_program *program;
    harp_parser_state *state;
    YY_BUFFER_STATE buf;
    yyscan_t scanner;
    void *operationParser;
    int lexCode;
    int i;

    // set up the parser state
    if (harp_parser_state_new(&state))
    {
        return -1;
    }

    // Set up the scanner
    yylex_init(&scanner);
    // yyset_in(stdin, scanner);
    buf = yy_scan_string(str, scanner);

    // Set up the parser
    operationParser = ParseAlloc(malloc);

    // Do it!
    do
    {
        lexCode = yylex(scanner);
        Parse(operationParser, lexCode, strdup(yyget_text(scanner)), state);
    } while (lexCode > 0 && !state->hasError);

    if (-1 == lexCode)
    {
        fprintf(stderr, "The scanner encountered an error\n");

        // Cleanup the scanner and parser
        yy_delete_buffer(buf, scanner);
        yylex_destroy(scanner);
        ParseFree(operationParser, free);
        harp_parser_state_delete(state);

        return -1;
    }
    if (state->hasError)
    {
        fprintf(stderr, "Parser error: %s\n", state->error);

        // Cleanup the scanner and parser
        yy_delete_buffer(buf, scanner);
        yylex_destroy(scanner);
        ParseFree(operationParser, free);
        harp_parser_state_delete(state);

        return -1;
    }

    if (harp_program_copy(state->result, new_program) != 0)
    {
        return -1;
    }

    // Cleanup the scanner and parser
    yy_delete_buffer(buf, scanner);
    yylex_destroy(scanner);
    ParseFree(operationParser, free);
    harp_parser_state_delete(state);

    return 0;
}

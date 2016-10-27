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
#include "harp-operation-parser.h"
#include "harp-operation-parser-state.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#define MAX_NUM_FUNCTION_ARGUMENTS 5

typedef struct yy_buffer_state *YY_BUFFER_STATE;
typedef void* yyscan_t;

int harp_operation_lex(yyscan_t yyscanner);
int harp_operation_lex_init(yyscan_t *scanner);
int harp_operation_lex_destroy(yyscan_t yyscanner);
YY_BUFFER_STATE harp_operation__scan_string (const char *yy_str, yyscan_t yyscanner);
void *harp_operation_parser_Alloc(void *(*mallocProc)(size_t));
void *harp_operation_parser_Free(void *p, void (*freeProc)(void*));
void harp_operation_parser_(void *yyp, int yymajor, const char* yyminor, harp_parser_state* state);
char *harp_operation_get_text(yyscan_t yyscanner);
void harp_operation__delete_buffer(YY_BUFFER_STATE b, yyscan_t yyscanner);

int harp_program_from_string(const char *str, harp_program **program)
{
    harp_parser_state *state;
    YY_BUFFER_STATE buf;
    yyscan_t scanner;
    void *operationParser;
    int lexCode;

    // set up the parser state
    if (harp_parser_state_new(&state))
    {
        return -1;
    }

    // Set up the scanner
    harp_operation_lex_init(&scanner);
    // yyset_in(stdin, scanner);
    buf = harp_operation__scan_string(str, scanner);

    // Set up the parser
    operationParser = harp_operation_parser_Alloc(malloc);

    // Do it!
    do
    {
        lexCode = harp_operation_lex(scanner);
        harp_operation_parser_(operationParser, lexCode, strdup(harp_operation_get_text(scanner)), state);
    } while (lexCode > 0 && !state->hasError);

    if (-1 == lexCode)
    {
        // Cleanup the scanner and parser
        harp_operation__delete_buffer(buf, scanner);
        harp_operation_lex_destroy(scanner);
        harp_operation_parser_Free(operationParser, free);
        harp_parser_state_delete(state);

        harp_set_error(HARP_ERROR_OPERATION_SYNTAX, "the scanner encountered an error");
        return -1;
    }
    if (state->hasError)
    {
        // Cleanup the scanner and parser
        harp_operation__delete_buffer(buf, scanner);
        harp_operation_lex_destroy(scanner);
        harp_operation_parser_Free(operationParser, free);
        harp_parser_state_delete(state);

        harp_set_error(HARP_ERROR_OPERATION_SYNTAX, "parser error: %s", state->error);
        return -1;
    }

    if (harp_program_copy(state->result, program) != 0)
    {
        return -1;
    }

    // Cleanup the scanner and parser
    harp_operation__delete_buffer(buf, scanner);
    harp_operation_lex_destroy(scanner);
    harp_operation_parser_Free(operationParser, free);
    harp_parser_state_delete(state);

    return 0;
}

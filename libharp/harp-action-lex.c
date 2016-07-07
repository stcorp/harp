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
#include "harp-action-lex.h"

#include <assert.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

static char *skip_white_space(char *str)
{
    while (*str != '\0' && isspace(*str))
    {
        str++;
    }

    return str;
}

static long match_identifier(const char *str)
{
    const char *cursor;

    if (!isalpha(*str))
    {
        return -1;
    }

    for (cursor = str + 1; *cursor != '\0' && (isalnum(*cursor) || strchr("_-.", *cursor) != NULL); cursor++)
    {
    }

    return cursor - str;
}

static long match_quoted_string(const char *str)
{
    const char *cursor;

    if (*str != '"')
    {
        return -1;
    }

    for (cursor = str + 1; strchr("\"", *cursor) == NULL; cursor++)
    {
        if (*cursor == '\\')
        {
            cursor++;

            if (*cursor == '\0')
            {
                break;
            }
        }
    }

    return (*cursor == '\0' ? -1 : (cursor - str + 1));
}

static long match_unit(const char *str)
{
    const char *cursor;

    if (*str != '[')
    {
        return -1;
    }

    for (cursor = str + 1; *cursor != '\0' && *cursor != ']'; cursor++)
    {
    }

    return (*cursor == '\0' ? -1 : (cursor - str + 1));
}

static long match_double(const char *str, long length)
{
    double value;

    return harp_parse_double(str, length, &value, 1);
}

static int has_more_characters(harp_lexer *lexer)
{
    return (lexer->mark - lexer->root) <= lexer->length;
}

static int lex_token(harp_lexer *lexer, harp_token *next_token)
{
    harp_token token;

    if (!has_more_characters(lexer))
    {
        harp_set_error(HARP_ERROR_ACTION_SYNTAX, "unexpected end of input (%s:%u)", __FILE__, __LINE__);
        return -1;
    }

    /* Skip leading whitespace. */
    token.root = skip_white_space(lexer->mark);
    token.position = token.root - lexer->root + 1;

    if (*token.root == '\0')
    {
        token.length = 1;
        token.type = harp_token_end;
    }
    else if (*token.root == '"')
    {
        long length;

        /* String. */
        length = match_quoted_string(token.root);
        assert(length < 0 || length >= 2);
        if (length < 0)
        {
            harp_set_error(HARP_ERROR_ACTION_SYNTAX, "char %lu: unterminated string (%s:%u)", token.position, __FILE__,
                           __LINE__);
            return -1;
        }

        token.length = length;
        token.type = harp_token_string;
    }
    else if (*token.root == '[')
    {
        long length;

        /* Unit. */
        length = match_unit(token.root);
        assert(length < 0 || length >= 2);
        if (length < 0)
        {
            harp_set_error(HARP_ERROR_ACTION_SYNTAX, "char %lu: unterminated unit (%s:%u)", token.position, __FILE__,
                           __LINE__);
            return -1;
        }

        token.length = length;
        token.type = harp_token_unit;
    }
    else if (strchr(",;(){}", *token.root) != NULL)
    {
        /* Delimiters. */
        switch (*token.root)
        {
            case ',':
                token.type = harp_token_comma;
                break;
            case ';':
                token.type = harp_token_semi_colon;
                break;
            case '(':
                token.type = harp_token_left_parenthesis;
                break;
            case ')':
                token.type = harp_token_right_parenthesis;
                break;
            case '{':
                token.type = harp_token_left_brace;
                break;
            case '}':
                token.type = harp_token_right_brace;
                break;
            default:
                assert(0);
                exit(1);
        }

        token.length = 1;
    }
    else if (strchr("<>=!", *token.root) != NULL)
    {
        /* Operators. */
        if (strncmp("=&", token.root, 2) == 0)
        {
            token.length = 2;
            token.type = harp_token_bit_mask_any;
        }
        else if (strncmp("!&", token.root, 2) == 0)
        {
            token.length = 2;
            token.type = harp_token_bit_mask_none;
        }
        else if (*(token.root + 1) == '=')
        {
            token.length = 2;

            switch (*token.root)
            {
                case '=':
                    token.type = harp_token_eq;
                    break;
                case '!':
                    token.type = harp_token_ne;
                    break;
                case '<':
                    token.type = harp_token_le;
                    break;
                case '>':
                    token.type = harp_token_ge;
                    break;
                default:
                    assert(0);
                    exit(1);
            }
        }
        else
        {
            token.length = 1;

            switch (*token.root)
            {
                case '<':
                    token.type = harp_token_lt;
                    break;
                case '>':
                    token.type = harp_token_gt;
                    break;
                default:
                    harp_set_error(HARP_ERROR_ACTION_SYNTAX, "char %lu: invalid operator '%c' (%s:%u)", token.position,
                                   *token.root, __FILE__, __LINE__);
                    return -1;
            }
        }
    }
    else if (isdigit(*token.root) || strchr("+-.", *token.root) != NULL)
    {
        long length;

        length = match_double(token.root, lexer->length - (token.root - lexer->root));
        if (length < 0)
        {
            harp_set_error(HARP_ERROR_ACTION_SYNTAX, "char %lu: invalid number (%s:%u)", token.position, __FILE__,
                           __LINE__);
            return -1;
        }

        token.length = length;
        token.type = harp_token_number;
    }
    else if (isalpha(*token.root))
    {
        token.length = match_identifier(token.root);
        assert(token.length >= 0);

        if (strncmp("not", token.root, token.length) == 0)
        {
            token.type = harp_token_not;
        }
        else if (strncmp("in", token.root, token.length) == 0)
        {
            token.type = harp_token_in;
        }
        else if (strncasecmp("nan", token.root, token.length) == 0 || strncasecmp("inf", token.root, token.length) == 0)
        {
            token.type = harp_token_number;
        }
        else
        {
            token.type = harp_token_name;
        }
    }
    else
    {
        harp_set_error(HARP_ERROR_ACTION_SYNTAX, "char %lu: syntax error (%s:%u)", token.position, __FILE__, __LINE__);
        return -1;
    }

    lexer->mark = token.root + token.length;
    assert((lexer->mark - lexer->root) <= (lexer->length + 1));

    *next_token = token;
    return 0;
}

const char *harp_get_token_type_name(harp_token_type type)
{
    switch (type)
    {
        case harp_token_unknown:
            return "unknown";
        case harp_token_unit:
            return "unit";
        case harp_token_string:
            return "string";
        case harp_token_number:
            return "number";
        case harp_token_keyword:
            return "keyword";
        case harp_token_name:
            return "name";
        case harp_token_left_parenthesis:
            return "(";
        case harp_token_right_parenthesis:
            return ")";
        case harp_token_left_brace:
            return "{";
        case harp_token_right_brace:
            return "}";
        case harp_token_comma:
            return ",";
        case harp_token_semi_colon:
            return ";";
        case harp_token_eq:
            return "==";
        case harp_token_ne:
            return "!=";
        case harp_token_lt:
            return "<";
        case harp_token_le:
            return "<=";
        case harp_token_gt:
            return ">";
        case harp_token_ge:
            return ">=";
        case harp_token_bit_mask_any:
            return "=&";
        case harp_token_bit_mask_none:
            return "!&";
        case harp_token_not:
            return "not";
        case harp_token_in:
            return "in";
        case harp_token_end:
            return "end";
        default:
            assert(0);
            exit(1);
    }
}

int harp_lexer_new(const char *str, harp_lexer **new_lexer)
{
    harp_lexer *lexer;

    if (str == NULL)
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "str argument is NULL (%s:%u)", __FILE__, __LINE__);
        return -1;
    }

    lexer = (harp_lexer *)malloc(sizeof(harp_lexer));
    if (lexer == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       sizeof(harp_lexer), __FILE__, __LINE__);
        return -1;
    }

    lexer->root = NULL;
    lexer->mark = NULL;
    lexer->length = strlen(str);
    lexer->num_tokens = 0;

    lexer->root = strdup(str);
    if (lexer->root == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not duplicate string) (%s:%u)", __FILE__,
                       __LINE__);
        harp_lexer_delete(lexer);
        return -1;
    }
    lexer->mark = lexer->root;

    *new_lexer = lexer;
    return 0;
}

void harp_lexer_delete(harp_lexer *lexer)
{
    if (lexer != NULL)
    {
        if (lexer->root != NULL)
        {
            free(lexer->root);
        }

        free(lexer);
    }
}

int harp_lexer_at_end(harp_lexer *lexer)
{
    return lexer->num_tokens == 0 && !has_more_characters(lexer);
}

void harp_lexer_consume_token(harp_lexer *lexer)
{
    int i;

    assert(lexer->num_tokens >= 1);
    for (i = 1; i < lexer->num_tokens; i++)
    {
        lexer->token[i - 1] = lexer->token[i];
    }

    lexer->num_tokens--;
}

int harp_lexer_next_token(harp_lexer *lexer, harp_token *next_token)
{
    if (lexer->num_tokens > 0)
    {
        *next_token = lexer->token[0];
        harp_lexer_consume_token(lexer);
        return 0;
    }

    return lex_token(lexer, next_token);
}

int harp_lexer_peek_token(harp_lexer *lexer, harp_token *next_token)
{
    for (; lexer->num_tokens < 1; lexer->num_tokens++)
    {
        if (lex_token(lexer, &lexer->token[lexer->num_tokens]) != 0)
        {
            return -1;
        }
    }

    *next_token = lexer->token[0];
    return 0;
}

int harp_lexer_peek_2nd_token(harp_lexer *lexer, harp_token *next_token)
{
    for (; lexer->num_tokens < 2; lexer->num_tokens++)
    {
        if (lex_token(lexer, &lexer->token[lexer->num_tokens]) != 0)
        {
            return -1;
        }
    }

    *next_token = lexer->token[1];
    return 0;
}

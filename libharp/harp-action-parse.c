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
#include "harp-action-parse.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

void harp_ast_node_delete(ast_node *node)
{
    if (node != NULL)
    {
        if (node->child_node != NULL)
        {
            int i;

            for (i = 0; i < node->num_child_nodes; ++i)
            {
                if (node->child_node[i] != NULL)
                {
                    harp_ast_node_delete(node->child_node[i]);
                }
            }

            free(node->child_node);
        }

        switch (node->type)
        {
            case ast_name:
            case ast_string:
            case ast_unit:
                if (node->payload.string != NULL)
                {
                    free(node->payload.string);
                }
                break;
            default:
                break;
        }

        free(node);
    }
}

static int ast_node_new(ast_node_type type, ast_node **new_node)
{
    ast_node *node;

    node = (ast_node *)malloc(sizeof(ast_node));
    if (node == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       sizeof(ast_node), __FILE__, __LINE__);
        return -1;
    }

    node->type = type;
    node->position = -1;
    node->num_child_nodes = 0;
    node->child_node = NULL;

    switch (node->type)
    {
        case ast_name:
        case ast_string:
        case ast_unit:
            node->payload.string = NULL;
            break;
        case ast_number:
            node->payload.number = harp_nan();
            break;
        default:
            break;
    }

    *new_node = node;
    return 0;
}

static int ast_node_append_child_node(ast_node *node, ast_node *child_node)
{
    if (node->num_child_nodes % BLOCK_SIZE == 0)
    {
        ast_node **new_child_node;

        new_child_node = (ast_node **)realloc(node->child_node, (node->num_child_nodes + BLOCK_SIZE)
                                              * sizeof(ast_node *));
        if (new_child_node == NULL)
        {
            harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                           (node->num_child_nodes + BLOCK_SIZE) * sizeof(ast_node *), __FILE__, __LINE__);
            return -1;

        }
        node->child_node = new_child_node;
    }

    node->child_node[node->num_child_nodes++] = child_node;
    return 0;
}

/* Taken from coda-expr.c. */
static long decode_escaped_string(char *str)
{
    long from;
    long to;

    if (str == NULL)
    {
        return 0;
    }

    from = 0;
    to = 0;

    while (str[from] != '\0')
    {
        if (str[from] == '\\')
        {
            from++;
            switch (str[from])
            {
                case 'e':
                    str[to++] = '\033'; /* windows does not recognize '\e' */
                    break;
                case 'a':
                    str[to++] = '\a';
                    break;
                case 'b':
                    str[to++] = '\b';
                    break;
                case 'f':
                    str[to++] = '\f';
                    break;
                case 'n':
                    str[to++] = '\n';
                    break;
                case 'r':
                    str[to++] = '\r';
                    break;
                case 't':
                    str[to++] = '\t';
                    break;
                case 'v':
                    str[to++] = '\v';
                    break;
                case '\\':
                    str[to++] = '\\';
                    break;
                case '"':
                    str[to++] = '"';
                    break;
                case '\'':
                    str[to++] = '\'';
                    break;
                default:
                    return -1;
            }
        }
        else
        {
            str[to++] = str[from];
        }
        from++;
    }

    str[to] = '\0';

    return to;
}

static int parse_string(harp_lexer *lexer, ast_node **result)
{
    ast_node *node;
    harp_token token;

    if (harp_lexer_next_token(lexer, &token) != 0)
    {
        return -1;
    }

    if (token.type != harp_token_string)
    {
        harp_set_error(HARP_ERROR_ACTION_SYNTAX, "char %lu: expected string (%s:%u)", token.position, __FILE__,
                       __LINE__);
        return -1;
    }
    assert(token.length >= 2);

    if (ast_node_new(ast_string, &node) != 0)
    {
        return -1;
    }

    node->position = token.position;
    node->payload.string = malloc(token.length - 1);
    if (node->payload.string == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %ld bytes) (%s:%u)",
                       token.length - 1, __FILE__, __LINE__);
        harp_ast_node_delete(node);
        return -1;
    }
    node->payload.string[token.length - 2] = '\0';
    memcpy(node->payload.string, token.root + 1, token.length - 2);

    /* Decode escape sequences. */
    if (decode_escaped_string(node->payload.string) < 0)
    {
        harp_set_error(HARP_ERROR_ACTION_SYNTAX, "char %lu: string contains invalid escape sequence (%s:%u)",
                       token.position, __FILE__, __LINE__);
        harp_ast_node_delete(node);
        return -1;
    }

    *result = node;
    return 0;
}

static int parse_unit(harp_lexer *lexer, ast_node **result)
{
    ast_node *node;
    harp_token token;
    char *unit;

    if (harp_lexer_next_token(lexer, &token) != 0)
    {
        return -1;
    }

    if (token.type != harp_token_unit)
    {
        harp_set_error(HARP_ERROR_ACTION_SYNTAX, "char %lu: expected unit (%s:%u)", token.position, __FILE__, __LINE__);
        return -1;
    }
    assert(token.length >= 2);

    unit = malloc(token.length - 1);
    if (unit == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %ld bytes) (%s:%u)",
                       token.length - 1, __FILE__, __LINE__);
        return -1;
    }
    unit[token.length - 2] = '\0';
    memcpy(unit, token.root + 1, token.length - 2);

    if (!harp_unit_is_valid(unit))
    {
        harp_set_error(HARP_ERROR_ACTION_SYNTAX, "char %lu: invalid unit '%s' (%s:%u)", token.position, unit, __FILE__,
                       __LINE__);
        free(unit);
        return -1;
    }

    if (ast_node_new(ast_unit, &node) != 0)
    {
        free(unit);
        return -1;
    }

    node->position = token.position;
    node->payload.string = unit;
    *result = node;

    return 0;
}

static int parse_name(harp_lexer *lexer, ast_node **result)
{
    ast_node *node;
    harp_token token;

    if (harp_lexer_next_token(lexer, &token) != 0)
    {
        return -1;
    }

    if (token.type != harp_token_name)
    {
        harp_set_error(HARP_ERROR_ACTION_SYNTAX, "char %lu: expected name (%s:%u)", token.position, __FILE__, __LINE__);
        return -1;
    }

    if (ast_node_new(ast_name, &node) != 0)
    {
        return -1;
    }

    node->position = token.position;
    node->payload.string = malloc(token.length + 1);
    if (node->payload.string == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %ld bytes) (%s:%u)",
                       token.length - 1, __FILE__, __LINE__);
        harp_ast_node_delete(node);
        return -1;
    }
    node->payload.string[token.length] = '\0';
    memcpy(node->payload.string, token.root, token.length);

    *result = node;
    return 0;
}

static int parse_number(harp_lexer *lexer, ast_node **result)
{
    ast_node *node;
    harp_token token;

    if (harp_lexer_next_token(lexer, &token) != 0)
    {
        return -1;
    }

    if (token.type != harp_token_number)
    {
        harp_set_error(HARP_ERROR_ACTION_SYNTAX, "char %lu: expected number (%s:%u)", token.position, __FILE__,
                       __LINE__);
        return -1;
    }

    if (ast_node_new(ast_number, &node) != 0)
    {
        return -1;
    }

    node->position = token.position;
    if (harp_parse_double(token.root, token.length, &node->payload.number, 0) != token.length)
    {
        harp_set_error(HARP_ERROR_ACTION_SYNTAX, "char %lu: invalid number (%s:%u)", token.position, __FILE__,
                       __LINE__);
        harp_ast_node_delete(node);
        return -1;
    }

    *result = node;
    return 0;
}

static int parse_literal(harp_lexer *lexer, ast_node **result)
{
    harp_token token;

    if (harp_lexer_peek_token(lexer, &token) != 0)
    {
        return -1;
    }

    switch (token.type)
    {
        case harp_token_string:
            return parse_string(lexer, result);
        case harp_token_number:
            return parse_number(lexer, result);
        default:
            harp_set_error(HARP_ERROR_ACTION_SYNTAX, "char %lu: expected number or string (%s:%u)", token.position,
                           __FILE__, __LINE__);
            return -1;
    }
}

static int parse_list(harp_lexer *lexer, ast_node **result)
{
    ast_node *node;
    harp_token token;

    if (harp_lexer_next_token(lexer, &token) != 0)
    {
        return -1;
    }

    if (token.type != harp_token_left_parenthesis)
    {
        harp_set_error(HARP_ERROR_ACTION_SYNTAX, "char %lu: expected '(' (%s:%u)", token.position, __FILE__, __LINE__);
        return -1;
    }

    if (ast_node_new(ast_list, &node) != 0)
    {
        return -1;
    }
    node->position = token.position;

    if (harp_lexer_peek_token(lexer, &token) != 0)
    {
        harp_ast_node_delete(node);
        return -1;
    }

    if (token.type != harp_token_right_parenthesis)
    {
        ast_node *literal;

        while (1)
        {
            if (parse_literal(lexer, &literal) != 0)
            {
                harp_ast_node_delete(node);
                return -1;
            }

            if (ast_node_append_child_node(node, literal) != 0)
            {
                harp_ast_node_delete(literal);
                harp_ast_node_delete(node);
                return -1;
            }

            if (harp_lexer_peek_token(lexer, &token) != 0)
            {
                harp_ast_node_delete(node);
                return -1;
            }

            if (token.type != harp_token_comma)
            {
                break;
            }

            harp_lexer_consume_token(lexer);
        }
    }

    if (harp_lexer_next_token(lexer, &token) != 0)
    {
        harp_ast_node_delete(node);
        return -1;
    }

    if (token.type != harp_token_right_parenthesis)
    {
        harp_set_error(HARP_ERROR_ACTION_SYNTAX, "char %lu: expected ')' (%s:%u)", token.position, __FILE__, __LINE__);
        harp_ast_node_delete(node);
        return -1;
    }

    *result = node;
    return 0;
}

static int get_ast_node_type(harp_token_type token_type, ast_node_type *node_type)
{
    switch (token_type)
    {
        case harp_token_eq:
            *node_type = ast_eq;
            return 0;
        case harp_token_ne:
            *node_type = ast_ne;
            return 0;
        case harp_token_lt:
            *node_type = ast_lt;
            return 0;
        case harp_token_le:
            *node_type = ast_le;
            return 0;
        case harp_token_gt:
            *node_type = ast_gt;
            return 0;
        case harp_token_ge:
            *node_type = ast_ge;
            return 0;
        default:
            break;
    }

    return -1;
}

static int parse_quantity(harp_lexer *lexer, ast_node **result)
{
    harp_token token;
    ast_node *node;
    ast_node *child_node;

    if (ast_node_new(ast_quantity, &node) != 0)
    {
        return -1;
    }

    if (parse_number(lexer, &child_node) != 0)
    {
        harp_ast_node_delete(node);
        return -1;
    }

    if (ast_node_append_child_node(node, child_node) != 0)
    {
        harp_ast_node_delete(node);
        harp_ast_node_delete(child_node);
        return -1;
    }
    node->position = child_node->position;

    if (harp_lexer_peek_token(lexer, &token) != 0)
    {
        harp_ast_node_delete(node);
        return -1;
    }

    if (token.type == harp_token_unit)
    {
        if (parse_unit(lexer, &child_node) != 0)
        {
            harp_ast_node_delete(node);
            return -1;
        }

        if (ast_node_append_child_node(node, child_node) != 0)
        {
            harp_ast_node_delete(node);
            harp_ast_node_delete(child_node);
            return -1;
        }
    }
    else
    {
        if (ast_node_append_child_node(node, NULL) != 0)
        {
            harp_ast_node_delete(node);
            return -1;
        }
    }

    *result = node;
    return 0;
}

static int parse_dimension_list(harp_lexer *lexer, ast_node **result)
{
    ast_node *node;
    harp_token token;
    ast_node *dimension_name;

    if (ast_node_new(ast_dimension_list, &node) != 0)
    {
        return -1;
    }

    if (harp_lexer_peek_token(lexer, &token) != 0)
    {
        harp_ast_node_delete(node);
        return -1;
    }

    if (token.type == harp_token_name)
    {
        while (1)
        {
            if (parse_name(lexer, &dimension_name) != 0)
            {
                harp_ast_node_delete(node);
                return -1;
            }

            if (ast_node_append_child_node(node, dimension_name) != 0)
            {
                harp_ast_node_delete(dimension_name);
                harp_ast_node_delete(node);
                return -1;
            }

            if (harp_lexer_peek_token(lexer, &token) != 0)
            {
                harp_ast_node_delete(node);
                return -1;
            }

            if (token.type != harp_token_comma)
            {
                break;
            }

            harp_lexer_consume_token(lexer);
        }
    }

    *result = node;
    return 0;
}

static int parse_qualified_name(harp_lexer *lexer, ast_node **result)
{
    ast_node *node;
    ast_node *child_node;
    harp_token token;

    if (ast_node_new(ast_qualified_name, &node) != 0)
    {
        return -1;
    }

    if (parse_name(lexer, &child_node) != 0)
    {
        harp_ast_node_delete(node);
        return -1;
    }

    if (ast_node_append_child_node(node, child_node) != 0)
    {
        harp_ast_node_delete(child_node);
        harp_ast_node_delete(node);
        return -1;
    }
    node->position = child_node->position;

    if (harp_lexer_peek_token(lexer, &token) != 0)
    {
        harp_ast_node_delete(node);
        return -1;
    }

    if (token.type == harp_token_left_brace)
    {
        harp_lexer_consume_token(lexer);

        if (parse_dimension_list(lexer, &child_node) != 0)
        {
            harp_ast_node_delete(node);
            return -1;
        }

        if (ast_node_append_child_node(node, child_node) != 0)
        {
            harp_ast_node_delete(child_node);
            harp_ast_node_delete(node);
            return -1;
        }

        if (harp_lexer_next_token(lexer, &token) != 0)
        {
            harp_ast_node_delete(node);
            return -1;
        }

        if (token.type != harp_token_right_brace)
        {
            harp_set_error(HARP_ERROR_ACTION_SYNTAX, "char %lu: expected '}' (%s:%u)", token.position, __FILE__,
                           __LINE__);
            harp_ast_node_delete(node);
            return -1;
        }

        if (harp_lexer_peek_token(lexer, &token) != 0)
        {
            harp_ast_node_delete(node);
            return -1;
        }
    }
    else
    {
        if (ast_node_append_child_node(node, NULL) != 0)
        {
            harp_ast_node_delete(node);
            return -1;
        }
    }

    if (token.type == harp_token_unit)
    {
        if (parse_unit(lexer, &child_node) != 0)
        {
            harp_ast_node_delete(node);
            return -1;
        }

        if (ast_node_append_child_node(node, child_node) != 0)
        {
            harp_ast_node_delete(child_node);
            harp_ast_node_delete(node);
            return -1;
        }
    }
    else
    {
        if (ast_node_append_child_node(node, NULL) != 0)
        {
            harp_ast_node_delete(node);
            return -1;
        }
    }

    *result = node;
    return 0;
}

static int parse_argument(harp_lexer *lexer, ast_node **result)
{
    harp_token token;

    if (harp_lexer_peek_token(lexer, &token) != 0)
    {
        return -1;
    }

    switch (token.type)
    {
        case harp_token_string:
            return parse_string(lexer, result);
        case harp_token_name:
            return parse_qualified_name(lexer, result);
        case harp_token_number:
            return parse_quantity(lexer, result);
        default:
            harp_set_error(HARP_ERROR_ACTION_SYNTAX, "char %lu: invalid argument (%s:%u)", token.position, __FILE__,
                           __LINE__);
            return -1;
    }
}

static int parse_argument_list(harp_lexer *lexer, ast_node **result)
{
    ast_node *node;
    harp_token token;
    ast_node *argument;

    if (ast_node_new(ast_argument_list, &node) != 0)
    {
        return -1;
    }

    if (harp_lexer_peek_token(lexer, &token) != 0)
    {
        harp_ast_node_delete(node);
        return -1;
    }

    if (token.type == harp_token_string || token.type == harp_token_name || token.type == harp_token_number)
    {
        while (1)
        {
            if (parse_argument(lexer, &argument) != 0)
            {
                harp_ast_node_delete(node);
                return -1;
            }

            if (ast_node_append_child_node(node, argument) != 0)
            {
                harp_ast_node_delete(argument);
                harp_ast_node_delete(node);
                return -1;
            }

            if (harp_lexer_peek_token(lexer, &token) != 0)
            {
                harp_ast_node_delete(node);
                return -1;
            }

            if (token.type != harp_token_comma)
            {
                break;
            }

            harp_lexer_consume_token(lexer);
        }
    }

    *result = node;
    return 0;
}

static int parse_comparison(harp_lexer *lexer, ast_node **result)
{
    ast_node *node;
    ast_node_type node_type;
    ast_node *name;
    ast_node *argument;
    harp_token token;

    if (parse_name(lexer, &name) != 0)
    {
        return -1;
    }

    if (harp_lexer_peek_token(lexer, &token) != 0)
    {
        harp_ast_node_delete(name);
        return -1;
    }

    if (get_ast_node_type(token.type, &node_type) != 0)
    {
        harp_set_error(HARP_ERROR_ACTION_SYNTAX, "char %lu: expected comparison operator (%s:%u)", token.position,
                       __FILE__, __LINE__);
        return -1;
    }
    harp_lexer_consume_token(lexer);

    if (ast_node_new(node_type, &node) != 0)
    {
        harp_ast_node_delete(name);
        return -1;
    }

    if (ast_node_append_child_node(node, name) != 0)
    {
        harp_ast_node_delete(name);
        harp_ast_node_delete(node);
        return -1;
    }
    node->position = name->position;

    if (parse_argument(lexer, &argument) != 0)
    {
        harp_ast_node_delete(node);
        return -1;
    }

    if (ast_node_append_child_node(node, argument) != 0)
    {
        harp_ast_node_delete(argument);
        harp_ast_node_delete(node);
        return -1;
    }

    *result = node;
    return 0;
}

static int parse_bit_mask_test(harp_lexer *lexer, ast_node **result)
{
    ast_node *node;
    ast_node_type node_type;
    ast_node *name;
    ast_node *argument;
    harp_token token;

    if (parse_name(lexer, &name) != 0)
    {
        return -1;
    }

    if (harp_lexer_peek_token(lexer, &token) != 0)
    {
        harp_ast_node_delete(name);
        return -1;
    }

    if (token.type == harp_token_bit_mask_any)
    {
        node_type = ast_bit_mask_any;
    }
    else if (token.type == harp_token_bit_mask_none)
    {
        node_type = ast_bit_mask_none;
    }
    else
    {
        harp_set_error(HARP_ERROR_ACTION_SYNTAX, "char %lu: expected bit mask test (%s:%u)", token.position, __FILE__,
                       __LINE__);
        return -1;
    }
    harp_lexer_consume_token(lexer);

    if (ast_node_new(node_type, &node) != 0)
    {
        harp_ast_node_delete(name);
        return -1;
    }

    if (ast_node_append_child_node(node, name) != 0)
    {
        harp_ast_node_delete(name);
        harp_ast_node_delete(node);
        return -1;
    }
    node->position = name->position;

    if (parse_number(lexer, &argument) != 0)
    {
        harp_ast_node_delete(node);
        return -1;
    }

    if (ast_node_append_child_node(node, argument) != 0)
    {
        harp_ast_node_delete(argument);
        harp_ast_node_delete(node);
        return -1;
    }

    *result = node;
    return 0;
}

static int parse_membership_test(harp_lexer *lexer, ast_node **result)
{
    ast_node *node;
    ast_node_type node_type;
    ast_node *name;
    ast_node *list;
    harp_token token;

    if (parse_name(lexer, &name) != 0)
    {
        return -1;
    }

    if (harp_lexer_next_token(lexer, &token) != 0)
    {
        harp_ast_node_delete(name);
        return -1;
    }

    if (token.type == harp_token_not)
    {
        if (harp_lexer_next_token(lexer, &token) != 0)
        {
            harp_ast_node_delete(name);
            return -1;
        }

        if (token.type != harp_token_in)
        {
            harp_set_error(HARP_ERROR_ACTION_SYNTAX, "char %lu: expected 'in' (%s:%u)", token.position, __FILE__,
                           __LINE__);
            harp_ast_node_delete(name);
            return -1;
        }

        node_type = ast_not_in;
    }
    else if (token.type == harp_token_in)
    {
        node_type = ast_in;
    }
    else
    {
        harp_set_error(HARP_ERROR_ACTION_SYNTAX, "char %lu: expected 'in' or 'not in' (%s:%u)", token.position,
                       __FILE__, __LINE__);
        harp_ast_node_delete(name);
        return -1;
    }

    if (ast_node_new(node_type, &node) != 0)
    {
        harp_ast_node_delete(name);
        return -1;
    }

    if (ast_node_append_child_node(node, name) != 0)
    {
        harp_ast_node_delete(name);
        harp_ast_node_delete(node);
        return -1;
    }
    node->position = name->position;

    if (parse_list(lexer, &list) != 0)
    {
        harp_ast_node_delete(node);
        return -1;
    }

    if (ast_node_append_child_node(node, list) != 0)
    {
        harp_ast_node_delete(list);
        harp_ast_node_delete(node);
        return -1;
    }

    if (harp_lexer_peek_token(lexer, &token) != 0)
    {
        harp_ast_node_delete(node);
        return -1;
    }

    if (token.type == harp_token_unit)
    {
        ast_node *unit;

        if (parse_unit(lexer, &unit) != 0)
        {
            harp_ast_node_delete(node);
            return -1;
        }

        if (ast_node_append_child_node(node, unit) != 0)
        {
            harp_ast_node_delete(unit);
            harp_ast_node_delete(node);
            return -1;
        }
    }
    else
    {
        if (ast_node_append_child_node(node, NULL) != 0)
        {
            harp_ast_node_delete(node);
            return -1;
        }
    }

    *result = node;
    return 0;
}

static int parse_function_call(harp_lexer *lexer, ast_node **result)
{
    ast_node *node;
    ast_node *name;
    ast_node *argument_list;
    harp_token token;

    if (ast_node_new(ast_function_call, &node) != 0)
    {
        return -1;
    }

    if (parse_name(lexer, &name) != 0)
    {
        harp_ast_node_delete(node);
        return -1;
    }

    if (ast_node_append_child_node(node, name) != 0)
    {
        harp_ast_node_delete(name);
        harp_ast_node_delete(node);
        return -1;
    }
    node->position = name->position;

    if (harp_lexer_next_token(lexer, &token) != 0)
    {
        harp_ast_node_delete(node);
        return -1;
    }

    if (token.type != harp_token_left_parenthesis)
    {
        harp_set_error(HARP_ERROR_ACTION_SYNTAX, "char %lu: expected '(' (%s:%u)", token.position, __FILE__, __LINE__);
        harp_ast_node_delete(node);
        return -1;
    }

    if (parse_argument_list(lexer, &argument_list) != 0)
    {
        harp_ast_node_delete(node);
        return -1;
    }
    argument_list->position = token.position;

    if (ast_node_append_child_node(node, argument_list) != 0)
    {
        harp_ast_node_delete(argument_list);
        harp_ast_node_delete(node);
        return -1;
    }

    if (harp_lexer_next_token(lexer, &token) != 0)
    {
        harp_ast_node_delete(node);
        return -1;
    }

    if (token.type != harp_token_right_parenthesis)
    {
        harp_set_error(HARP_ERROR_ACTION_SYNTAX, "char %lu: expected ')' (%s:%u)", token.position, __FILE__, __LINE__);
        harp_ast_node_delete(node);
        return -1;
    }

    *result = node;
    return 0;
}

static int parse_statement(harp_lexer *lexer, ast_node **result)
{
    harp_token token;

    if (harp_lexer_peek_token(lexer, &token) != 0)
    {
        return -1;
    }

    if (token.type != harp_token_name)
    {
        harp_set_error(HARP_ERROR_ACTION_SYNTAX, "char %lu: expected variable or function (%s:%u)", token.position,
                       __FILE__, __LINE__);
        return -1;
    }

    if (harp_lexer_peek_2nd_token(lexer, &token) != 0)
    {
        return -1;
    }

    if (token.type == harp_token_left_parenthesis)
    {
        return parse_function_call(lexer, result);
    }
    else if (token.type == harp_token_bit_mask_any || token.type == harp_token_bit_mask_none)
    {
        return parse_bit_mask_test(lexer, result);
    }
    else if (token.type == harp_token_not || token.type == harp_token_in)
    {
        return parse_membership_test(lexer, result);
    }
    else
    {
        return parse_comparison(lexer, result);
    }

    harp_set_error(HARP_ERROR_ACTION_SYNTAX, "char %lu: expected '(', comparison operator, bit mask test, or "
                   "membership test (%s:%u)", token.position, __FILE__, __LINE__);
    return -1;
}

static int parse_actions(harp_lexer *lexer, ast_node **result)
{
    harp_token token;
    ast_node *statement;
    ast_node *node;

    if (ast_node_new(ast_action_list, &node) != 0)
    {
        return -1;
    }

    while (1)
    {
        if (harp_lexer_peek_token(lexer, &token) != 0)
        {
            harp_ast_node_delete(node);
            return -1;
        }

        if (token.type == harp_token_end)
        {
            break;
        }

        if (parse_statement(lexer, &statement) != 0)
        {
            harp_ast_node_delete(node);
            return -1;
        }

        if (ast_node_append_child_node(node, statement) != 0)
        {
            harp_ast_node_delete(statement);
            harp_ast_node_delete(node);
            return -1;
        }

        if (harp_lexer_peek_token(lexer, &token) != 0)
        {
            harp_ast_node_delete(node);
            return -1;
        }

        if (token.type != harp_token_semi_colon)
        {
            break;
        }

        harp_lexer_consume_token(lexer);
    }

    *result = node;
    return 0;
}

int harp_parse_actions(const char *actions, ast_node **result)
{
    harp_lexer *lexer;
    ast_node *node;
    harp_token token;

    if (harp_lexer_new(actions, &lexer) != 0)
    {
        return -1;
    }

    if (parse_actions(lexer, &node) != 0)
    {
        harp_lexer_delete(lexer);
        return -1;
    }

    if (harp_lexer_next_token(lexer, &token) != 0)
    {
        harp_ast_node_delete(node);
        harp_lexer_delete(lexer);
        return -1;
    }

    if (token.type != harp_token_end)
    {
        harp_set_error(HARP_ERROR_ACTION_SYNTAX, "char %lu: trailing characters (%s:%u)", token.position, __FILE__,
                       __LINE__);
        harp_ast_node_delete(node);
        harp_lexer_delete(lexer);
        return -1;
    }
    harp_lexer_delete(lexer);

    *result = node;
    return 0;
}

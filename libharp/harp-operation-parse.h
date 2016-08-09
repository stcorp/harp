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

#ifndef HARP_OPERATION_PARSE_H
#define HARP_OPERATION_PARSE_H

#include "harp-operation-lex.h"

typedef enum ast_node_type_enum
{
    ast_name,
    ast_qualified_name,
    ast_unit,
    ast_string,
    ast_number,
    ast_quantity,
    ast_list,
    ast_eq,
    ast_ne,
    ast_lt,
    ast_le,
    ast_gt,
    ast_ge,
    ast_bit_mask_any,
    ast_bit_mask_none,
    ast_in,
    ast_not_in,
    ast_function_call,
    ast_argument_list,
    ast_dimension_list,
    ast_operation_list
} ast_node_type;

struct ast_node_struct
{
    ast_node_type type;
    long position;

    int num_child_nodes;
    struct ast_node_struct **child_node;

    union
    {
        char *string;
        double number;
    } payload;
};
typedef struct ast_node_struct ast_node;

void harp_ast_node_delete(ast_node *node);

int harp_parse_operations(const char *operations, ast_node **result);

#endif

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

#ifndef HARP_ACTION_LEX_H
#define HARP_ACTION_LEX_H

typedef enum harp_token_type_enum
{
    harp_token_unknown = -1,
    harp_token_string,
    harp_token_unit,
    harp_token_number,
    harp_token_keyword,
    harp_token_name,
    harp_token_left_parenthesis,
    harp_token_right_parenthesis,
    harp_token_left_brace,
    harp_token_right_brace,
    harp_token_comma,
    harp_token_semi_colon,
    harp_token_eq,
    harp_token_ne,
    harp_token_lt,
    harp_token_le,
    harp_token_gt,
    harp_token_ge,
    harp_token_bit_mask_any,
    harp_token_bit_mask_none,
    harp_token_not,
    harp_token_in,
    harp_token_end
} harp_token_type;

typedef struct harp_token_struct
{
    harp_token_type type;
    char *root;
    long position;
    long length;
} harp_token;

typedef struct harp_lexer_struct
{
    char *root;
    char *mark;
    long length;

    int num_tokens;
    harp_token token[2];
} harp_lexer;

const char *harp_get_token_type_name(harp_token_type type);

int harp_lexer_new(const char *str, harp_lexer **new_lexer);
void harp_lexer_delete(harp_lexer *lexer);

int harp_lexer_at_end(harp_lexer *lexer);
void harp_lexer_consume_token(harp_lexer *lexer);
int harp_lexer_next_token(harp_lexer *lexer, harp_token *next_token);
int harp_lexer_peek_token(harp_lexer *lexer, harp_token *next_token);
int harp_lexer_peek_2nd_token(harp_lexer *lexer, harp_token *next_token);

#endif

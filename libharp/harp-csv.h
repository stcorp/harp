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
#include <stdio.h>

#ifndef HARP_CSV_H
#define HARP_CSV_H

#define HARP_CSV_LINE_LENGTH 1024

void harp_csv_parse_double(char **str, double *value);
void harp_csv_parse_long(char **str, long *value);
void harp_csv_parse_string(char **str, char **value);
int harp_csv_get_num_lines(FILE *file, const char *filename, long *new_num_lines);

#endif

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

#ifndef IPOW_H
#define IPOW_H

/* Gives a ^ b where b is a small integer */
static double ipow(double a, int b)
{
    double val = 1.0;

    if (b < 0)
    {
        while (b++)
        {
            val *= a;
        }
        val = 1.0 / val;
    }
    else
    {
        while (b--)
        {
            val *= a;
        }
    }
    return val;
}

#endif

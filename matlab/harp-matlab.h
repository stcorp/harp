/*
 * Copyright (C) 2002-2016 S[&]T, The Netherlands.
 *
 * This file is part of HARP.
 *
 */

#ifndef HARP_MATLAB_H
#define HARP_MATLAB_H

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "mex.h"
#include "coda.h"
#include "harp.h"

/* harp-matlab-record functions */
mxArray *harp_matlab_get_product(harp_product **product);
harp_product *harp_matlab_set_product(const mxArray * array);

/* harp-matlab functions */
void harp_matlab_harp_error(void);

#endif

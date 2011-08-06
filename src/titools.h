/*
 * TITools
 *
 * Copyright (c) 2010 Benjamin Moody
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 3 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <glib.h>
#include <ticables.h>
#include <tifiles.h>
#include <ticalcs.h>
#include <ticonv.h>

#define VAR_TYPE_MASK 0x3f

/* common.c */

extern CalcModel calc_model;
extern CableHandle *cable_handle;
extern CalcHandle *calc_handle;

void tt_init(int argc, char **argv, const GOptionEntry* options,
	     int min_fn, CalcFeatures required_features, int silent_probe);

void tt_exit();

void tt_print_error(int e, const char *fmt, ...) G_GNUC_PRINTF(2, 3);

char * tt_format_varname(const VarEntry *ve);

/* glob.c */

int tt_globs_foreach(char **patterns, int (*func)(VarEntry *ve));
int tt_vars_foreach(int (*func)(VarEntry *ve));

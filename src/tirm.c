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

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <stdio.h>
#include <string.h>
#include "titools.h"

static char **patterns = NULL;

static const GOptionEntry app_options[] =
  {{ G_OPTION_REMAINING, 0, 0, G_OPTION_ARG_STRING_ARRAY,
     &patterns, NULL, "VAR-PATTERN ..." },
   { 0, 0, 0, 0, 0, 0, 0 }};

static int delete_var(VarEntry *ve)
{
  int e;
  char *name;

  if ((e = ticalcs_calc_del_var(calc_handle, ve))) {
    name = tt_format_varname(ve);
    tt_print_error(e, "unable to delete %s", name);
    g_free(name);
    return 1;
  }

  return 0;
}

int main(int argc, char **argv)
{
  int status = 0;

  tt_init(argc, argv, app_options, 1, OPS_DELVAR, 1);

  if (patterns && patterns[0])
    status = tt_globs_foreach(patterns, &delete_var);

  tt_exit();
  return status;
}

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
     &patterns, NULL, "[VAR-PATTERN ...]" },
   { 0, 0, 0, 0, 0, 0, 0 }};

static int print_var(VarEntry *ve)
{
  const char *as;
  char *s;

  if (ve->attr == ATTRB_NONE)
    as = "---";
  else if (ve->attr == ATTRB_LOCKED)
    as = "<L>";
  else if (ve->attr == ATTRB_PROTECTED)
    as = "<P>";
  else if (ve->attr == ATTRB_ARCHIVED)
    as = "<A>";
  else
    as = "<?>";

  g_print("%7d\t%s\t%s\t", ve->size, as,
	  tifiles_vartype2string(calc_model, ve->type));

  if (ve->folder[0]) {
    s = ticonv_varname_to_utf8(calc_model, ve->folder, -1);
    g_print("%s/", s);
    g_free(s);
  }

  s = ticonv_varname_to_utf8(calc_model, ve->name, ve->type);
  g_print("%s\n", s);
  g_free(s);

  return 0;
}

int main(int argc, char **argv)
{
  int status;

  tt_init(argc, argv, app_options, 0, OPS_DIRLIST, 1);

  if (patterns && patterns[0])
    status = tt_globs_foreach(patterns, &print_var);
  else
    status = tt_vars_foreach(&print_var);

  tt_exit();
  return status;
}

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
#include <stdlib.h>
#include <string.h>
#include "titools.h"

static char **input_strings;

static const GOptionEntry app_options[] =
  {{ G_OPTION_REMAINING, 0, 0, G_OPTION_ARG_STRING_ARRAY,
     &input_strings, NULL, "TEXT ..." },
   { 0, 0, 0, 0, 0, 0, 0 }};

static uint16_t ascii_to_key(CalcModel model, char c)
{
  const CalcKey *ck;

  if (c < 0 || c > '~')
    return 0;

  if (model == CALC_TI73)
    ck = ticalcs_keys_73(c);
  else if (model == CALC_TI82 || model == CALC_TI83)
    ck = ticalcs_keys_83(c);
  else if (model == CALC_TI83P || model == CALC_TI84P
	   || model == CALC_TI84P_USB)
    ck = ticalcs_keys_83p(c);
  else if (model == CALC_TI86)
    ck = ticalcs_keys_86(c);
  else if (model == CALC_TI89 || model == CALC_TI89T_USB)
    ck = ticalcs_keys_89(c);
  else if (model == CALC_TI92 || model == CALC_TI92P
	   || model == CALC_V200)
    ck = ticalcs_keys_89(c);
  else
    return 0;

  if (!ck)
    return 0;

  return ck->normal.value;
}

int main(int argc, char **argv)
{
  int i, n, k, e;
  uint16_t *kvalues;
  char *p, *q, *s;

  tt_init(argc, argv, app_options, 1, OPS_KEYS, 1);

  for (i = n = 0; input_strings && input_strings[i]; i++)
    n += strlen(input_strings[i]) + 1;

  kvalues = g_new(uint16_t, n);

  for (i = n = 0; input_strings && input_strings[i]; i++) {
    p = input_strings[i];
    while (*p) {
      if (*p == '\\' && p[1]) {
	if (p[1] == 'n' || p[1] == 'N'
	    || p[1] == 'r' || p[1] == 'R') {
	  k = ascii_to_key(calc_model, '\r');
	  q = p + 2;
	}
	else if (p[1] == 'x' || p[1] == 'X') {
	  k = strtol(p + 2, &q, 16);
	}
	else if (p[1] >= '0' && p[1] <= '7') {
	  k = strtol(p + 1, &q, 8);
	}
	else {
	  k = ascii_to_key(calc_model, p[1]);
	  q = g_utf8_next_char(p + 1);
	}
      }
      else {
	k = ascii_to_key(calc_model, *p);
	q = g_utf8_next_char(p);
      }

      if (!k) {
	s = g_strndup(p, q - p);
	g_printerr("%s: untranslatable character '%s'\n",
		   g_get_prgname(), s);
	g_free(s);
	g_free(kvalues);
	tt_exit();
	return 2;
      }
      kvalues[n++] = k;
      p = q;
    }
  }

  for (i = 0; i < n; i++) {
    if ((e = ticalcs_calc_send_key(calc_handle, kvalues[i]))) {
      tt_print_error(e, "unable to send key");
      g_free(kvalues);
      tt_exit();
      return 1;
    }
  }

  g_free(kvalues);
  tt_exit();
  return 0;
}

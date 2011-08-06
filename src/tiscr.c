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
#include <errno.h>
#include <glib/gstdio.h>
#include "titools.h"

static gboolean full_screen = FALSE;
static char *outfname = NULL;

static const GOptionEntry app_options[] =
  {{ "output", 'o', 0, G_OPTION_ARG_FILENAME, &outfname,
     "Write output to FILE", "FILE" },
   { "full", 'A', 0, G_OPTION_ARG_NONE, &full_screen,
     "Include invisible parts of screen buffer (TI-89 only)", NULL },
   { 0, 0, 0, 0, 0, 0, 0 }};

int main(int argc, char **argv)
{
  CalcScreenCoord sc;
  uint8_t *bitmap = NULL;
  int width, height, bpr, i, e;
  FILE *f;

  tt_init(argc, argv, app_options, 0, OPS_SCREEN, 1);

  if (full_screen)
    sc.format = SCREEN_FULL;
  else
    sc.format = SCREEN_CLIPPED;

  if ((e = ticalcs_calc_recv_screen(calc_handle, &sc, &bitmap))) {
    tt_print_error(e, "unable to read calculator screen");
    g_free(bitmap);
    tt_exit();
    return 1;
  }

  if (outfname) {
    f = g_fopen(outfname, "wb");
    if (!f) {
      g_printerr("%s: unable to open %s: %s\n",
		 g_get_prgname(), outfname, g_strerror(errno));
      g_free(bitmap);
      tt_exit();
      return 2;
    }
  }
  else
    f = stdout;

  if (full_screen) {
    width = sc.width;
    height = sc.height;
  }
  else {
    width = sc.clipped_width;
    height = sc.clipped_height;
  }

  bpr = (width + 7) / 8;

  fprintf(f, "P4\n%d %d\n", width, height);
  for (i = 0; i < height; i++)
    fwrite(bitmap + i * bpr, 1, bpr, f);

  if (f != stdout)
    fclose(f);

  g_free(bitmap);
  tt_exit();
  return 0;
}

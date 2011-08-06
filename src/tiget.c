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

/* FIXME:
   - prompt before overwriting
   - some weirdness with tig internal filenames */

static gboolean backup_mode = FALSE;
static char *output_name = NULL;
static char **patterns = NULL;

static const GOptionEntry app_options[] =
  {{ "backup", 'b', 0, G_OPTION_ARG_NONE, &backup_mode,
     "Full backup of all calculator contents", NULL },
   { "output", 'o', 0, G_OPTION_ARG_FILENAME, &output_name,
     "Write output to group FILE", "FILE" },
   { G_OPTION_REMAINING, 0, 0, G_OPTION_ARG_STRING_ARRAY,
     &patterns, NULL, "VAR-PATTERN ..." },
   { 0, 0, 0, 0, 0, 0, 0 }};

static FileContent **vars;
static int nvars;
static FlashContent **apps;
static int napps;

static int save_regular(FileContent *vcontent)
{
  char *name = NULL;
  int e, status = 0;

  if (output_name) {
    nvars++;
    vars = g_renew(FileContent *, vars, nvars + 1);
    vars[nvars - 1] = vcontent;
    vars[nvars] = NULL;
  }
  else {
    if ((e = tifiles_file_write_regular(NULL, vcontent, &name))) {
      tt_print_error(e, "unable to write output file");
      status = 3;
    }
    tifiles_content_delete_regular(vcontent);
    g_free(name);
  }

  return status;
}

static int save_flash(FlashContent *fcontent)
{
  char *name = NULL;
  int e, status = 0;

  if (output_name) {
    napps++;
    apps = g_renew(FlashContent *, apps, napps + 1);
    apps[napps - 1] = fcontent;
    apps[napps] = NULL;
  }
  else {
    if ((e = tifiles_file_write_flash2(NULL, fcontent, &name))) {
      tt_print_error(e, "unable to write output file");
      status = 3;
    }
    tifiles_content_delete_flash(fcontent);
    g_free(name);
  }

  return status;
}

static int get_vars_ns()
{
  FileContent *vcontent;
  VarEntry *head_entry = NULL;
  int e;

  vcontent = tifiles_content_create_regular(calc_model);
  if ((e = ticalcs_calc_recv_var_ns(calc_handle, MODE_BACKUP, vcontent,
				    &head_entry))
      || vcontent->num_entries == 0) {
    tt_print_error(e, "unable to retrieve variables");
    if (head_entry)
      tifiles_ve_delete(head_entry);
    tifiles_content_delete_regular(vcontent);
    return 1;
  }

  if (head_entry)
    tifiles_ve_delete(head_entry);

  return save_regular(vcontent);
}

static int get_var(VarEntry *ve)
{
  FileContent *vcontent;
  FlashContent *fcontent;
  int e;
  char *name;

  if (ve->type == tifiles_flash_type(calc_model)) {
    fcontent = tifiles_content_create_flash(calc_model);
    if ((e = ticalcs_calc_recv_app(calc_handle, fcontent, ve))) {
      name = tt_format_varname(ve);
      tt_print_error(e, "unable to retrieve %s", name);
      g_free(name);
      tifiles_content_delete_flash(fcontent);
      return 1;
    }

    return save_flash(fcontent);
  }
  else {
    vcontent = tifiles_content_create_regular(calc_model);
    if ((e = ticalcs_calc_recv_var(calc_handle, MODE_BACKUP, vcontent, ve))
	|| vcontent->num_entries == 0) {
      name = tt_format_varname(ve);
      tt_print_error(e, "unable to retrieve %s", name);
      g_free(name);
      tifiles_content_delete_regular(vcontent);
      return 1;
    }

    return save_regular(vcontent);
  }
}

static int get_backup()
{
  BackupContent *bcontent;
  int e, status = 0;

  if (!(ticalcs_calc_features(calc_handle) & FTS_BACKUP)) {
    g_printerr("%s: calculator does not support backup\n",
	       g_get_prgname());
    return 10;
  }

  if (!output_name) {
    g_printerr("%s: no output filename specified\n",
	       g_get_prgname());
    return 15;
  }

  bcontent = tifiles_content_create_backup(calc_model);

  do
    e = ticalcs_calc_recv_backup(calc_handle, bcontent);
  while (!(ticalcs_calc_features(calc_handle) & FTS_SILENT)
	 && e == ERROR_READ_TIMEOUT);

  if (e) {
    tt_print_error(e, "unable to retrieve backup");
    status = 1;
  }
  else if ((e = tifiles_file_write_backup(output_name, bcontent))) {
    tt_print_error(e, "unable to write output file");
    status = 2;
  }

  tifiles_content_delete_backup(bcontent);
  return status;
}

static int output_grouped()
{
  FileContent *grouped = NULL;
  int e, status = 0;

  if (napps > 0) {
    g_printerr("%s: cannot write vars and apps to a single group file\n",
	       g_get_prgname());
    return 3;
  }

  if ((e = tifiles_group_contents(vars, &grouped))) {
    tt_print_error(e, "unable to create group file");
    status = 3;
  }
  else if ((e = tifiles_file_write_regular(output_name, grouped, NULL))) {
    tt_print_error(e, "unable to write output file");
    status = 2;
  }

  if (grouped)
    tifiles_content_delete_regular(grouped);
  return status;
}

static int output_flash()
{
  int e;

  if (napps > 1) {
    g_printerr("%s: cannot write multiple apps to a single file\n",
	       g_get_prgname());
    return 3;
  }
  else if ((e = tifiles_file_write_flash(output_name, apps[0]))) {
    tt_print_error(e, "unable to write output file");
    return 2;
  }

  return 0;
}

static int output_tig()
{
  TigContent *tig = NULL;
  int e, status = 0;

  if ((e = tifiles_tigroup_contents(vars, apps, &tig))) {
    tt_print_error(e, "unable to create group file");
    status = 3;
  }
  else if ((e = tifiles_file_write_tigroup(output_name, tig))) {
    tt_print_error(e, "unable to write output file");
    status = 2;
  }

  if (tig)
    tifiles_content_delete_tigroup(tig);
  return status;
}

int main(int argc, char **argv)
{
  int i, status;
  const char *p;

  tt_init(argc, argv, app_options, 0, 0, 0);

  if (backup_mode)
    status = get_backup();
  else if (patterns && patterns[0])
    status = tt_globs_foreach(patterns, &get_var);
  else
    status = get_vars_ns();

  if (output_name && !status && (nvars > 0 || napps > 0)) {
    if ((p = strrchr(output_name, '.'))
	&& !g_ascii_strcasecmp(p, ".tig"))
      status = output_tig();
    else if (nvars > 0)
      status = output_grouped();
    else
      status = output_flash();
  }

  if (vars) {
    for (i = 0; i < nvars; i++)
      tifiles_content_delete_regular(vars[i]);
    g_free(vars);
  }

  if (apps) {
    for (i = 0; i < napps; i++)
      tifiles_content_delete_flash(apps[i]);
    g_free(apps);
  }

  tt_exit();
  return status;
}

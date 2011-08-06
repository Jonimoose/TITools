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

/* FIXME: don't bother with dirlist if sending backups, certificates,
   OSes. */

static gboolean force_archive = FALSE;
static gboolean force_unarchive = FALSE;
static gboolean no_check_overwrite = FALSE;
static gboolean non_silent = FALSE;
static gboolean no_eot = FALSE;
static char **input_files = NULL;

static const GOptionEntry app_options[] =
  {{ "archive", 'a', 0, G_OPTION_ARG_NONE, &force_archive,
     "Send all files to archive (Flash)", NULL },
   { "unarchive", 'u', 0, G_OPTION_ARG_NONE, &force_unarchive,
     "Send all files to RAM", NULL },
   { "force", 'f', 0, G_OPTION_ARG_NONE, &no_check_overwrite,
     "Don't ask before overwriting files", NULL },
   /* { "non-silent", 0, 0, G_OPTION_ARG_NONE, &non_silent,
      "Send files in calc-to-calc mode", NULL }, */
   { "continue", 'C', 0, G_OPTION_ARG_NONE, &no_eot,
     "Leave LINK RECEIVE mode active (TI-82/85 only)", NULL },
   { G_OPTION_REMAINING, 0, 0, G_OPTION_ARG_FILENAME_ARRAY, &input_files,
     NULL, "FILE ..." },
   { 0, 0, 0, 0, 0, 0, 0 }};

static GNode *vars_list = NULL, *apps_list = NULL;

static int confirm_overwrite(VarEntry *ve, gboolean check_apps)
{
  char *name;
  char buf[100];
  GNode *list;
  VarEntry *oldve;
  int e;

  if (no_check_overwrite)
    return 0;

  if (!vars_list) {
    if ((e = ticalcs_calc_get_dirlist(calc_handle,
				      &vars_list, &apps_list))) {
      tt_print_error(e, "unable to read directory listing");
      return 2;
    }
  }

  list = (check_apps ? apps_list : vars_list);
  oldve = ticalcs_dirlist_ve_exist(list, ve);
  if (!oldve)
    return 0;

  name = tt_format_varname(oldve);
  g_printerr("%s exists; overwrite? ", name);
  g_free(name);

  if (fgets(buf, sizeof(buf), stdin)
      && (buf[0] == 'y' || buf[0] == 'Y'))
    return 0;
  else
    return 1;
}

static int link_menu_ok = 0;

static void confirm_link_menu()
{
  char buf[100];

  if (non_silent && !link_menu_ok) {
    g_printerr("Please place the calculator in LINK RECEIVE mode."
	       "  Press Enter when ready: ");
    fgets(buf, sizeof(buf), stdin);
    link_menu_ok = 1;
  }
}

static int send_regular(FileContent *content, int final)
{
  int i, e;

  confirm_link_menu();

  for (i = 0; i < content->num_entries; i++) {
    if (force_archive)
      content->entries[i]->attr = ATTRB_ARCHIVED;
    if (force_unarchive && content->entries[i]->attr == ATTRB_ARCHIVED)
      content->entries[i]->attr = 0;

    e = confirm_overwrite(content->entries[i], 0);
    if (e == 2)
      return 2;
    else if (e)
      content->entries[i]->action = ACT_SKIP;
  }

  if (non_silent)
    e = ticalcs_calc_send_var_ns(calc_handle,
				    (final ? MODE_SEND_LAST_VAR : 0),
				    content);
  else
    e = ticalcs_calc_send_var(calc_handle, MODE_SEND_ONE_VAR, content);

  if (e == ERROR_ABORT) {
    g_printerr("%s: transfer cancelled\n", g_get_prgname());
    return -1;
  }
  else if (e) {
    tt_print_error(e, "unable to send file");
    return 1;
  }

  return 0;
}

static int send_app(FlashContent *content)
{
  VarEntry tmpve;
  int e;

  link_menu_ok = 0;

  tmpve.type = content->data_type;
  strncpy(tmpve.name, content->name, sizeof(tmpve.name));

  e = confirm_overwrite(&tmpve, 1);
  if (e == 2)
    return 2;
  else if (e)
    return 0;
  
  if ((e = ticalcs_calc_send_app(calc_handle, content))) {
    tt_print_error(e, "unable to send application");
    return 1;
  }

  return 0;
}

static int send_os(FlashContent *content)
{
  int e;

  link_menu_ok = 0;

  if ((e = ticalcs_calc_send_os(calc_handle, content))) {
    tt_print_error(e, "unable to send OS");
    return 1;
  }

  return 0;
}

static int send_cert(FlashContent *content)
{
  int e;

  link_menu_ok = 0;

  if ((e = ticalcs_calc_send_os(calc_handle, content))) {
    tt_print_error(e, "unable to send certificate");
    return 1;
  }

  return 0;
}

static int send_flash(FlashContent *content)
{
  if (content->data_type == 0x23)
    return send_os(content);
  else if (content->data_type == 0x24)
    return send_app(content);
  else if (content->data_type == 0x25)
    return send_cert(content);
  else {
    g_printerr("unknown Flash data type %x\n", content->data_type);
    return 3;
  }
}

static int send_backup(BackupContent *content)
{
  int e;

  confirm_link_menu();
  link_menu_ok = 0;

  if ((e = ticalcs_calc_send_backup(calc_handle, content))) {
    tt_print_error(e, "unable to send backup");
    return 1;
  }

  return 0;
}

static int send_tig_entry(TigEntry *entry, int final)
{
  switch (entry->type) {
  case TIFILE_SINGLE:
  case TIFILE_GROUP:
    return send_regular(entry->content.regular, final);

  case TIFILE_FLASH:
    return send_flash(entry->content.flash);

  case TIFILE_OS:
    return send_os(entry->content.flash);

  case TIFILE_APP:
    return send_app(entry->content.flash);

  default:
    g_printerr("unknown tig data type\n");
    return 3;
  }
}

static int send_file(const char *fname, int final)
{
  FileContent *regular;
  FlashContent *flash;
  BackupContent *backup;
  TigContent *tig;
  int i, e, status = 0;

  if (tifiles_file_is_tigroup(fname)) {
    tig = tifiles_content_create_tigroup(calc_model, 0);
    if (!(e = tifiles_file_read_tigroup(fname, tig))) {
      for (i = 0; !status && i < tig->n_vars; i++) {
	status = send_tig_entry(tig->var_entries[i],
				(final && i == tig->n_vars - 1
				 && tig->n_apps == 0));
      }
      for (i = 0; !status && i < tig->n_apps; i++) {
	status = send_tig_entry(tig->app_entries[i],
				(final && i == tig->n_apps - 1));
      }
    }
    tifiles_content_delete_tigroup(tig);
  }
  else if (tifiles_file_is_regular(fname)) {
    regular = tifiles_content_create_regular(calc_model);
    if (!(e = tifiles_file_read_regular(fname, regular)))
      status = send_regular(regular, final);
    tifiles_content_delete_regular(regular);
  }
  else if (tifiles_file_is_backup(fname)) {
    backup = tifiles_content_create_backup(calc_model);
    if (!(e = tifiles_file_read_backup(fname, backup)))
      status = send_backup(backup);
    tifiles_content_delete_backup(backup);
  }
  else if (tifiles_file_is_os(fname)) {
    flash = tifiles_content_create_flash(calc_model);
    if (!(e = tifiles_file_read_flash(fname, flash)))
      e = send_os(flash);
    tifiles_content_delete_flash(flash);
  }
  else if (tifiles_file_is_app(fname)) {
    flash = tifiles_content_create_flash(calc_model);
    if (!(e = tifiles_file_read_flash(fname, flash)))
      e = send_app(flash);
    tifiles_content_delete_flash(flash);
  }
  else if (tifiles_file_is_flash(fname)) {
    flash = tifiles_content_create_flash(calc_model);
    if (!(e = tifiles_file_read_flash(fname, flash)))
      e = send_flash(flash);
    tifiles_content_delete_flash(flash);
  }
  else {
    g_printerr("%s: %s: unknown file type\n", g_get_prgname(), fname);
    return 3;
  }

  if (e) {
    tt_print_error(e, "unable to read file");
    return 3;
  }

  return status;
}

int main(int argc, char **argv)
{
  int i, status = 0;

  tt_init(argc, argv, app_options, 1, 0, 0);

  if (!(ticalcs_calc_features(calc_handle) & FTS_SILENT)) {
    non_silent = TRUE;
    no_check_overwrite = TRUE;
  }

  if (!(ticalcs_calc_features(calc_handle) & OPS_DIRLIST)) {
    no_check_overwrite = TRUE;
  }

  for (i = 0; !status && input_files && input_files[i]; i++)
    status = send_file(input_files[i], (!input_files[i + 1] && !no_eot));

  if (status == -1) /* abort */
    status = 0;

  ticalcs_dirlist_destroy(&vars_list);
  ticalcs_dirlist_destroy(&apps_list);
  tt_exit();
  return status;
}

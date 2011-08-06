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
#include <locale.h>
#include "titools.h"

#define EXIT_INVALID_OPTIONS 15
#define EXIT_INTERNAL_ERROR 14
#define EXIT_NO_CABLE_FOUND 13
#define EXIT_CABLE_FAILED 12
#define EXIT_NO_CALC_FOUND 11
#define EXIT_CALC_UNSUPPORTED 10

CalcModel calc_model;
CableHandle *cable_handle;
CalcHandle *calc_handle;

/* Convert cable name to a CableModel.  ticables_string_to_model()
   uses names like "BlackLink" and "SilverLink"; we also accept
   3-letter abbreviations. */
static CableModel cable_name_to_model(const char *name)
{
  CableModel m;

  if (!g_ascii_strcasecmp(name, "gry"))
    return CABLE_GRY;
  else if (!g_ascii_strcasecmp(name, "blk")
	   || !g_ascii_strcasecmp(name, "ser"))
    return CABLE_BLK;
  else if (!g_ascii_strcasecmp(name, "par"))
    return CABLE_PAR;
  else if (!g_ascii_strcasecmp(name, "slv"))
    return CABLE_SLV;
  else if (!g_ascii_strcasecmp(name, "usb"))
    return CABLE_USB;
  else if (!g_ascii_strcasecmp(name, "vti"))
    return CABLE_VTI;
  else if (!g_ascii_strcasecmp(name, "tie"))
    return CABLE_TIE;
  else if (!g_ascii_strcasecmp(name, "dev"))
    return CABLE_DEV;
  else {
    m = ticables_string_to_model(name);
    if (m == CABLE_ILP)
      return 0;
    else
      return m;
  }
}

/* Convert file type to a CalcModel. */
static CalcModel fext_to_model(const char *ext)
{
  char c1, c2;
  c1 = g_ascii_tolower(ext[0]);
  c2 = g_ascii_tolower(ext[1]);

  if (c1 == '7' && c2 == '3') return CALC_TI73;
  if (c1 == '8' && c2 == '2') return CALC_TI82;
  if (c1 == '8' && c2 == '3') return CALC_TI83;
  if (c1 == '8' && c2 == 'x') return CALC_TI84P;
  if (c1 == '8' && c2 == '5') return CALC_TI85;
  if (c1 == '8' && c2 == '6') return CALC_TI86;
  if (c1 == '8' && c2 == '9') return CALC_TI89;
  if (c1 == '9' && c2 == '2') return CALC_TI92;
  if (c1 == '9' && c2 == 'x') return CALC_TI92P;
  if (c1 == 'v' && c2 == '2') return CALC_V200;
  if (c1 == 't' && c2 == 'n') return CALC_NSPIRE;
  return CALC_NONE;
}

/* Convert calculator name from command line to a CalcModel.  Model
   names used by ticalcs/tifiles use '+' for "plus"; we also accept
   'p' or 'P' */
static CalcModel calc_name_to_model(const char *name)
{
  CalcModel m;
  char *str;
  int i;

  if (strlen(name) == 2)
    return fext_to_model(name);

  m = ticalcs_string_to_model(name);
  if (m)
    return m;

  str = g_strdup(name);
  for (i = 0; str[i]; i++)
    if (str[i] == 'p' || str[i] == 'P')
      str[i] = '+';
  m = ticalcs_string_to_model(str);
  g_free(str);

  return m;
}

/* Convert an input filename to a CalcModel. */
static CalcModel filename_to_model(const char *name)
{
  const char *p;

  p = strrchr(name, '.');

  if (!p || strchr(p, '/') || strchr(p, '\\') || strlen(p) < 4)
    return CALC_NONE;

  return fext_to_model(p + 1);
}

/* Guess calculator model by looking at input filenames */
static CalcModel guess_model_from_options(const GOptionEntry *opts)
{
  CalcModel model = CALC_NONE, fmodel;
  int i, j;
  char **s, ***a;

  if (!opts)
    return CALC_NONE;

  for (i = 0; opts[i].long_name; i++) {
    if (opts[i].arg == G_OPTION_ARG_FILENAME
	|| opts[i].arg == G_OPTION_ARG_STRING) {

      s = opts[i].arg_data;
      if (s && *s) {
	fmodel = filename_to_model(*s);
	if (!model)
	  model = fmodel;
	else if (model != fmodel)
	  return CALC_NONE;
      }
    }
    else if (opts[i].arg == G_OPTION_ARG_FILENAME_ARRAY
	     || opts[i].arg == G_OPTION_ARG_STRING_ARRAY) {

      a = opts[i].arg_data;
      if (a && *a) {
	for (j = 0; (*a)[j]; j++) {
	  fmodel = filename_to_model((*a)[j]);
	  if (!model)
	    model = fmodel;
	  else if (model != fmodel)
	    return CALC_NONE;
	}
      }
    }
  }

  return model;
}

/* Check if USB PID matches calculator model */
static int model_matches_pid(CalcModel model, int pid)
{
  if (model == CALC_TI84P || model == CALC_TI84P_USB)
    return (pid == PID_TI84P || pid == PID_TI84P_SE);
  else if (model == CALC_TI89 || model == CALC_TI89T_USB)
    return (pid == PID_TI89TM);
  else if (model == CALC_NSPIRE)
    return (pid == PID_NSPIRE);
  else
    return 0;
}

/* Print out command-line help text, and exit */
static void print_usage(GOptionContext *ctx)
{
  char *usage = g_option_context_get_help(ctx, TRUE, NULL);
  g_printerr("%s", usage);
  g_free(usage);
  g_option_context_free(ctx);
  tt_exit();
  exit(EXIT_INVALID_OPTIONS);
}

static char *cable_name = NULL;
static char *calc_name = NULL;
static int timeout = DFLT_TIMEOUT * 100;
static gboolean verbose = FALSE;
static gboolean showversion = FALSE;

static const GOptionEntry comm_options[] =
  {{ "cable", 'c', 0, G_OPTION_ARG_STRING, &cable_name,
     "Specify cable type and port number", "CABLE[:PORT]" },
   { "calc", 'm', 0, G_OPTION_ARG_STRING, &calc_name,
     "Specify calculator model", "MODEL" },
   { "timeout", 'T', 0, G_OPTION_ARG_INT, &timeout,
     "Time out after N milliseconds", "N" },
   { "verbose", 'v', 0, G_OPTION_ARG_NONE, &verbose,
     "Show details of link operations", NULL },
   { "version", 0, 0, G_OPTION_ARG_NONE, &showversion,
     "Display program version info", NULL },
   { 0, 0, 0, 0, 0, 0, 0 }};

static void log_output(const gchar *domain, GLogLevelFlags level,
		       const gchar *message,
		       G_GNUC_UNUSED gpointer data)
{
  if (!verbose)
    return;

  g_printerr(" [%s] ", domain);

  switch (level & G_LOG_LEVEL_MASK) {
  case G_LOG_LEVEL_ERROR: g_printerr("ERROR: "); break;
  case G_LOG_LEVEL_CRITICAL: g_printerr("CRITICAL: "); break;
  case G_LOG_LEVEL_WARNING: g_printerr("WARNING: "); break;
  }

  g_printerr("%s\n", message);
}

/* General initialization for all of the titools.  Parse the
   command-line options, and open a link cable (global cable_handle),
   and a calculator handle (global calc_handle).

   ARGC and ARGV are the arguments to main().

   APP_OPTIONS is an array of GOptionEntries describing app-specific
   command-line options.  If "rest" arguments are allowed, APP_OPTIONS
   must contain a G_OPTION_REMAINING entry.  MIN_FN is the minimum
   allowed number of "rest" arguments.

   REQUIRED_FEATURES is a bit mask of calculator features that are
   required in order for the tool to be useful (e.g., OPS_DIRLIST must
   be available in order to use 'tils'.)

   SILENT_PROBE is 1 if it's OK to probe for the calculator type
   without asking.
 */
void tt_init(int argc, char **argv, const GOptionEntry *app_options,
	     int min_fn, CalcFeatures required_features, int silent_probe)
{
  GOptionContext *ctx;
  GError *err = NULL;
  CableModel cable_model = CABLE_NUL;
  int port_number = 0;
  int *usbpids = NULL, nusbpids;
  CableHandle *tmpcable;
  int probe_status;
  CalcFeatures feats;
  const char *v;
  char *cname, *cport, *p;
  char ***a;
  int i, j, e;

  calc_model = CALC_NONE;
  cable_handle = NULL;
  calc_handle = NULL;

  setlocale(LC_ALL, "");

  if ((v = g_getenv("TITOOLS_CABLE")))
    cable_name = g_strdup(v);

  if ((v = g_getenv("TITOOLS_CALC")))
    calc_name = g_strdup(v);

  if ((v = g_getenv("TITOOLS_TIMEOUT"))) {
    i = strtol(v, &p, 10);
    if (i > 0)
      timeout = i;
  }

  ctx = g_option_context_new("");

  if (app_options)
    g_option_context_add_main_entries(ctx, app_options, NULL);

  g_option_context_add_main_entries(ctx, comm_options, NULL);

  if (!g_option_context_parse(ctx, &argc, &argv, &err)) {
    g_printerr("%s: %s\n", g_get_prgname(), err->message);
    g_error_free(err);
    print_usage(ctx);
  }

  if (showversion) {
    g_print("%s (%s)\n"
	    "Copyright (C) 2010 Benjamin Moody\n"
	    "This program is free software. "
	    " There is NO WARRANTY of any kind.\n"
	    "Report bugs to %s.\n",
	    g_get_prgname(), PACKAGE_STRING, PACKAGE_BUGREPORT);
    tt_exit();
    exit(0);
  }

  /* check for unparsed options/filenames */
  if (argc != 1)
    print_usage(ctx);

  /* if "rest" arguments are allowed, check that the number is OK */
  for (i = 0; app_options && app_options[i].long_name; i++) {
    if (!strcmp(app_options[i].long_name, G_OPTION_REMAINING)) {
      a = app_options[i].arg_data;
      if (a && *a) {
	for (j = 0; (*a)[j]; j++)
	  ;
      }
      else {
	j = 0;
      }

      if (j < min_fn)
	print_usage(ctx);
    }
  }

  g_option_context_free(ctx);

  g_log_set_handler("ticables", G_LOG_LEVEL_MASK, &log_output, 0);
  g_log_set_handler("tifiles", G_LOG_LEVEL_MASK, &log_output, 0);
  g_log_set_handler("ticalcs", G_LOG_LEVEL_MASK, &log_output, 0);
  g_log_set_handler("calccables", G_LOG_LEVEL_MASK, &log_output, 0);
  g_log_set_handler("calcfiles", G_LOG_LEVEL_MASK, &log_output, 0);
  g_log_set_handler("calcprotocols", G_LOG_LEVEL_MASK, &log_output, 0);

  ticables_library_init();
  tifiles_library_init();
  ticalcs_library_init();

  /* Set calculator model based on options, if possible */

  if (!calc_name || !g_ascii_strcasecmp(calc_name, "dwim")) {
    /* try to guess calc model based on filenames */
    calc_model = guess_model_from_options(app_options);

    if (!calc_model && !silent_probe) {
      g_printerr("%s: unable to determine calculator type\n"
		 "(use -m MODEL, or -m 'auto' to probe)\n",
		 g_get_prgname());
      tt_exit();
      exit(EXIT_INVALID_OPTIONS);
    }
  }
  else if (calc_name && g_ascii_strcasecmp(calc_name, "auto")) {
    /* user set model explicitly */
    calc_model = calc_name_to_model(calc_name);
    if (!calc_model) {
      g_printerr("%s: unknown model '%s'\n",
		 g_get_prgname(), calc_name);
      tt_exit();
      exit(EXIT_INVALID_OPTIONS);
    }
  }

  /* Set cable model and port number */

  if (cable_name && g_ascii_strcasecmp(cable_name, "auto")) {
    /* user set cable explicitly */

    cname = g_strdup(cable_name);
    if ((cport = strchr(cname, ':'))) {
      *cport = 0;
      cport++;
    }

    /* parse cable model */
    cable_model = cable_name_to_model(cname);
    if (!cable_model) {
      g_printerr("%s: unknown cable type '%s'\n",
		 g_get_prgname(), cname);
      g_free(cname);
      tt_exit();
      exit(EXIT_INVALID_OPTIONS);
    }

    /* Require explicit port specification for serial/parallel cables
       (since probing is messy) */

    if ((cable_model == CABLE_BLK || cable_model == CABLE_GRY
	 || cable_model == CABLE_PAR) && !cport) {
      g_printerr("%s: port number required for %s cables\n"
		 "(use -c '%s:N', or -c '%s:auto' to probe)\n",
		 g_get_prgname(), ticables_model_to_string(cable_model),
		 cname, cname);
      g_free(cname);
      tt_exit();
      exit(EXIT_INVALID_OPTIONS);
    }

    /* parse port number */
    if (cport && g_ascii_strcasecmp(cport, "auto")) {
      port_number = strtol(cport, &p, 10);
      if (p == cport || *p || port_number < 0) {
	g_printerr("%s: invalid port number '%s'\n",
		   g_get_prgname(), cport);
	g_free(cname);
	tt_exit();
	exit(EXIT_INVALID_OPTIONS);
      }
    }
    else {
      port_number = 0;
    }

    if (port_number == 0 && cable_model != CABLE_TIE) {
      /* probe for port (except for TiEmu virtual link, which probes
	 automatically when port number is set to 0) */

      for (i = 1; i <= 4; i++) {
	tmpcable = ticables_handle_new(cable_model, i);
	if (tmpcable
	    && !ticables_cable_probe(tmpcable, &probe_status)
	    && probe_status) {
	  ticables_handle_del(tmpcable);
	  port_number = i;
	  break;
	}
	ticables_handle_del(tmpcable);
      }

      if (i > 4) {
	g_printerr("%s: no %s cable detected\n",
		   g_get_prgname(),
		   ticables_model_to_string(cable_model));
	g_free(cname);
	tt_exit();
	exit(EXIT_NO_CABLE_FOUND);
      }
    }

    g_free(cname);
  }
  else {
    /* auto-detect cable */

    if (!ticables_get_usb_devices(&usbpids, &nusbpids)) {
      if (calc_model) {
	/* Calc model known - if user has specified a USB calculator
	   and we see that device plugged in, use it; otherwise, use a
	   SilverLink if one is plugged in */
	for (i = 0; i < nusbpids; i++) {
	  if (model_matches_pid(calc_model, usbpids[i])) {
	    cable_model = CABLE_USB;
	    port_number = i + 1;
	    break;
	  }
	}

	if (!cable_model
	    && calc_model != CALC_TI84P_USB
	    && calc_model != CALC_TI89T_USB
	    && calc_model != CALC_NSPIRE) {
	  for (i = 0; i < nusbpids; i++) {
	    if (usbpids[i] == PID_TIGLUSB) {
	      cable_model = CABLE_SLV;
	      port_number = i + 1;
	      break;
	    }
	  }
	}
      }
      else if (nusbpids != 0) {
	/* Neither calc model nor cable model was specified.  Use the
	   first USB device we see. */
	if (usbpids[0] == PID_TIGLUSB)
	  cable_model = CABLE_SLV;
	else
	  cable_model = CABLE_USB;
	port_number = 1;
      }
    }

    if (usbpids)
      free(usbpids);

    if (!cable_model) {
      g_printerr("%s: no cable detected\n"
		 "(use -c to specify a serial or parallel cable)\n",
		 g_get_prgname());
      tt_exit();
      exit(EXIT_NO_CABLE_FOUND);
    }
  }

  /* Switch to USB protocol if needed */
  if (cable_model == CABLE_USB) {
    if (calc_model == CALC_TI83P || calc_model == CALC_TI84P)
      calc_model = CALC_TI84P_USB;
    else if (calc_model == CALC_TI89)
      calc_model = CALC_TI89T_USB;
  }

  /* Probe for calculator model, if still unknown (we only get to this
     point if SILENT_PROBE is set or user requested probing with -m
     auto.)  If -m auto is set, do a more thorough probe. */
  if (!calc_model) {
    if ((e = ticalcs_probe(cable_model, port_number, &calc_model,
			   (calc_name ? 1 : 0)))) {
      tt_print_error(e, "unable to detect calculator");
      tt_exit();
      exit(EXIT_NO_CALC_FOUND);
    }
  }

  /* Create calc handle */
  calc_handle = ticalcs_handle_new(calc_model);
  if (!calc_handle) {
    g_printerr("%s: unable to initialize calc %s\n",
	       g_get_prgname(), ticalcs_model_to_string(calc_model));
    tt_exit();
    exit(EXIT_INTERNAL_ERROR);
  }

  /* Check for required features */
  feats = ticalcs_calc_features(calc_handle);
  if (required_features & ~feats) {
    fprintf(stderr, "%s: calculator model %s does not support this operation\n",
	    g_get_prgname(), ticalcs_model_to_string(calc_model));
    tt_exit();
    exit(EXIT_CALC_UNSUPPORTED);
  }

  /* Create cable handle */
  cable_handle = ticables_handle_new(cable_model, port_number);
  if (!cable_handle) {
    fprintf(stderr, "%s: unable to initialize cable %s (port %d)\n",
	    g_get_prgname(), ticables_model_to_string(cable_model),
	    port_number);
    tt_exit();
    exit(EXIT_CABLE_FAILED);
  }

  ticables_options_set_timeout(cable_handle, (timeout + 99) / 100);

  /* Attach and open cable */
  if ((e = ticalcs_cable_attach(calc_handle, cable_handle))) {
    tt_print_error(e, "unable to connect to calculator");
    tt_exit();
    exit(EXIT_CABLE_FAILED);
  }

  /*Check if calc is ready */
  if ((e = ticalcs_calc_isready(calc_handle))) {
    tt_print_error(e, "calculator not ready");
    tt_exit();
    exit(EXIT_CABLE_FAILED);
  }
}

void tt_exit()
{
  if (calc_handle) {
    ticalcs_handle_del(calc_handle); /* detaches + closes cable if
					necessary */
    calc_handle = NULL;
  }

  if (cable_handle) {
    ticables_handle_del(cable_handle);
    cable_handle = NULL;
  }

  ticalcs_library_exit();
  tifiles_library_exit();
  ticables_library_exit();
  g_free(cable_name);
  cable_name = NULL;
  g_free(calc_name);
  calc_name = NULL;
}

void tt_print_error(int e, const char *msg, ...)
{
  va_list ap;
  char *ms, *es = NULL;

  va_start(ap, msg);
  ms = g_strdup_vprintf(msg, ap);
  va_end(ap);

  if (e && (!ticalcs_error_get(e, &es)
	    || !ticables_error_get(e, &es)
	    || !tifiles_error_get(e, &es)))
    g_printerr("%s: %s\n%s\n", g_get_prgname(), ms, es);
  else
    g_printerr("%s: %s\n", g_get_prgname(), ms);

  g_free(ms);
  g_free(es);
}

char * tt_format_varname(const VarEntry *ve)
{
  const char *type;
  char *folder;
  char *name;
  char *s;

  type = tifiles_vartype2string(calc_model, ve->type);

  if (ve->folder[0]) {
    folder = ticonv_varname_to_utf8(calc_model, ve->folder, -1);
    name = ticonv_varname_to_utf8(calc_model, ve->name, ve->type);
    s = g_strdup_printf("%s %s/%s", type, folder, name);
    g_free(folder);
    g_free(name);
    return s;
  }
  else {
    name = ticonv_varname_to_utf8(calc_model, ve->name, ve->type);
    s = g_strdup_printf("%s %s", type, name);
    g_free(name);
    return s;
  }
}

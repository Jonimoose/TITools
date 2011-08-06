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

/* Convert string (either a name like "PRGM" or a file type like
   "82p") to variable type */
static int name_to_type(const char *str)
{
  int t;
  const char *s;

  /* vartype2string returns an empty string if it's not a valid
     type */

  t = tifiles_fext2vartype(calc_model, str);
  s = tifiles_vartype2string(calc_model, t);
  if (s && s[0])
    return t;

  t = tifiles_string2vartype(calc_model, str);
  s = tifiles_vartype2string(calc_model, t);
  if (s && s[0])
    return t;
  else
    return -1;
}

static int model_uses_utf8(CalcModel model)
{
  if (model == CALC_TI73
      || model == CALC_TI82
      || model == CALC_TI83
      || model == CALC_TI83P
      || model == CALC_TI84P
      || model == CALC_TI85
      || model == CALC_TI86
      || model == CALC_TI89
      || model == CALC_TI92
      || model == CALC_TI92P
      || model == CALC_V200)
    return 0;
  else
    return 1;
}

/* Convert UTF-16 to TI (ticonv_charset_utf16_to_ti() is not
   implemented for most calcs) */
static char * utf16_to_ti(CalcModel model, const guint16 *str)
{
  int i, j, k, n;
  char *s;
  unsigned long c;
  const unsigned long *cs;

  if (!str || !*str)
    return g_strdup("");

  n = ticonv_utf16_strlen(str);
  s = g_new(char, n + 1);
  s[0] = 0;
  ticonv_charset_utf16_to_ti_s(model, str, s);
  if (s[0])
    return s;

  switch (model) {
  case CALC_TI73: cs = ti73_charset; break;
  case CALC_TI82: cs = ti82_charset; break;
  case CALC_TI83: cs = ti83_charset; break;
  case CALC_TI83P:
  case CALC_TI84P: cs = ti83p_charset; break;
  case CALC_TI85: cs = ti85_charset; break;
  case CALC_TI86: cs = ti86_charset; break;
  case CALC_TI89:
  case CALC_TI92:
  case CALC_TI92P:
  case CALC_V200: cs = ti9x_charset; break;
  default:
    return ticonv_utf16_to_utf8(str);
  }

  for (i = j = 0; i < n; i++) {
    c = str[i];
    if ((c & 0xfc00) == 0xd800)
      c = (c << 16) | str[++i];

    if (c < 256 && cs[c] == c)
      s[j] = c;
    else {
      s[j] = '?';
      for (k = 0; k < 256; k++) {
	if (cs[k] == c) {
	  s[j] = k;
	  break;
	}
      }
    }
    j++;
  }

  s[j] = 0;
  return s;
}

typedef struct _TTGlobPattern {
  enum { LITERAL, BRACKETS, QUESTION, STAR, SLASH, DOT } type;
  int n;			/* length of str */
  char *s;			/* characters in ticalcs encoding */
  struct _TTGlobPattern *next;
} TTGlobPattern;

typedef struct _TTGlob {
  TTGlobPattern *folder_pattern;
  TTGlobPattern *name_pattern;
  TTGlobPattern *type_pattern;
  int exact_type;
} TTGlob;

static void pat_append(TTGlobPattern **head, int type,
		       GString *str)
{
  TTGlobPattern *p, *q;
  guint16 *utf16;
  char *s;
  int n;

  if (str) {
    if (!str->str || !str->str[0])
      return;
    utf16 = ticonv_utf8_to_utf16(str->str);
    s = utf16_to_ti(calc_model, utf16);
    n = strlen(s);
    g_free(utf16);
  }
  else {
    s = NULL;
    n = 0;
  }

  p = g_slice_new(TTGlobPattern);
  p->type = type;
  p->n = n;
  p->s = s;
  p->next = NULL;

  if (*head) {
    q = *head;
    while (q->next)
      q = q->next;
    q->next = p;
  }
  else {
    *head = p;
  }
}

static void pat_free(TTGlobPattern *p)
{
  TTGlobPattern *q;

  while (p) {
    g_free(p->s);
    q = p->next;
    g_slice_free(TTGlobPattern, p);
    p = q;
  }
}

/* Parse a glob string into a TTGlob object that can be used to match
   filenames. */
static TTGlob * tt_glob_parse(const char *pattern)
{
  TTGlob *glob;
  TTGlobPattern *pat, *pslash, *pdot, *p;
  GString *pstr;
  int in_brackets = 0;
  int in_type = 0;
  const char *s, *e;

  g_return_val_if_fail(g_utf8_validate(pattern, -1, NULL), NULL);

  pat = NULL;
  pstr = g_string_new(NULL);
  s = pattern;

  while (*s) {
    if (s[0] == '[' && s[1] >= 'A' && s[1] <= 'Z' && s[2] == ']') {
      g_string_append_len(pstr, s, 3);
      s += 3;
    }
    else if (s[0] == '[') {
      if (in_brackets) {
	g_string_free(pstr, TRUE);
	pat_free(pat);
	return NULL;
      }

      pat_append(&pat, LITERAL, pstr);
      g_string_truncate(pstr, 0);
      in_brackets = 1;
      s++;
    }
    else if (s[0] == ']') {
      if (!in_brackets) {
	g_string_free(pstr, TRUE);
	pat_free(pat);
	return NULL;
      }

      pat_append(&pat, BRACKETS, pstr);
      g_string_truncate(pstr, 0);
      in_brackets = 0;
      s++;
    }
    else if (!in_brackets && s[0] == '*') {
      pat_append(&pat, LITERAL, pstr);
      pat_append(&pat, STAR, NULL);
      g_string_truncate(pstr, 0);
      s++;
    }
    else if (!in_brackets && s[0] == '?') {
      pat_append(&pat, LITERAL, pstr);
      pat_append(&pat, QUESTION, NULL);
      g_string_truncate(pstr, 0);
      s++;
    }
    else if (!in_brackets && s[0] == '/') {
      pat_append(&pat, LITERAL, pstr);
      pat_append(&pat, SLASH, NULL);
      g_string_truncate(pstr, 0);
      s++;
    }
    else if (!in_brackets && s[0] == '.') {
      pat_append(&pat, LITERAL, pstr);
      pat_append(&pat, DOT, NULL);
      g_string_truncate(pstr, 0);
      in_type = 1;
      s++;
    }
    else {
      if (s[0] == '\\' && s[1])
	s++;

      if (in_type && !(s[0] & 0x80)) {
	g_string_append_c(pstr, g_ascii_toupper(s[0]));
	s++;
      }
      else {
	e = g_utf8_next_char(s);
	g_string_append_len(pstr, s, e - s);
	s = e;
      }
    }
  }

  if (in_brackets) {
    g_string_free(pstr, TRUE);
    pat_free(pat);
    return NULL;
  }

  pat_append(&pat, LITERAL, pstr);
  g_string_free(pstr, TRUE);

  if (!pat)
    return NULL;

  if (pat->type == SLASH || pat->type == DOT) {
    pat_free(pat);
    return NULL;
  }

  pslash = pdot = NULL;
  for (p = pat; p->next; p = p->next) {
    if (p->next->type == SLASH) {
      if (pslash || pdot || !p->next->next) {
	pat_free(pat);
	return NULL;
      }
      pslash = p;
    }
    else if (p->next->type == DOT) {
      if (pdot || !p->next->next) {
	pat_free(pat);
	return NULL;
      }
      pdot = p;
    }
  }

  glob = g_slice_new(TTGlob);

  if (pslash) {
    glob->folder_pattern = pat;
    glob->name_pattern = pslash->next->next;
    g_slice_free(TTGlobPattern, pslash->next);
    pslash->next = NULL;
  }
  else {
    glob->folder_pattern = NULL;
    glob->name_pattern = pat;
  }

  if (pdot) {
    glob->type_pattern = pdot->next->next;
    g_slice_free(TTGlobPattern, pdot->next);
    pdot->next = NULL;
  }
  else {
    glob->type_pattern = NULL;
  }

  if (glob->type_pattern
      && glob->type_pattern->type == LITERAL
      && !glob->type_pattern->next)
    glob->exact_type = name_to_type(glob->type_pattern->s);
  else
    glob->exact_type = -1;

  return glob;
}

static void tt_glob_free(TTGlob *glob)
{
  if (!glob)
    return;

  pat_free(glob->name_pattern);
  pat_free(glob->folder_pattern);
  pat_free(glob->type_pattern);
  g_slice_free(TTGlob, glob);
}

/* Check if variable name should be tokenized.
   ticonv_varname_tokenize() should probably be more selective... */
static int is_tokenized_vartype(CalcModel model, int type)
{
  switch (model) {
  case CALC_TI73:
    if (type == 0x05 || type == 0x06 || type == 0x19 || type == 0x1A
	|| type >= 0x20)
      return 0;
    else
      return 1;

  case CALC_TI82:
  case CALC_TI83:
  case CALC_TI83P:
  case CALC_TI84P:
    if (type == 0x05 || type == 0x06 || type == 0x14 || type == 0x15
	|| type == 0x16 || type == 0x17 || type >= 0x20)
      return 0;
    else
      return 1;

  default:
    return 0;
  }
}

/* Check if glob is an exact variable name (no wildcards, includes a
   valid variable type, includes folder name if necessary) */
static int tt_glob_check_exact(const TTGlob *glob, VarRequest *vr)
{
  char *tokstr;

  if (vr)
    memset(vr, 0, sizeof(VarRequest));

  if (glob->exact_type < 0)
    return 0;

  if (vr)
    vr->type = glob->exact_type;

  if (!glob->name_pattern
      || glob->name_pattern->type != LITERAL
      || glob->name_pattern->next)
    return 0;

  if (vr) {
    if (is_tokenized_vartype(calc_model, vr->type)) {
      tokstr = ticonv_varname_tokenize(calc_model, glob->name_pattern->s,
				       vr->type);
      strncpy(vr->name, tokstr, sizeof(vr->name) - 1);
      g_free(tokstr);
    }
    else {
      strncpy(vr->name, glob->name_pattern->s, sizeof(vr->name) - 1);
    }
  }

  if (ticalcs_calc_features(calc_handle) & FTS_FOLDER) {
    if (!glob->folder_pattern)
      return 0;
    if (glob->folder_pattern->type != LITERAL
	|| glob->folder_pattern->next)
      return 0;

    if (vr) {
      strncpy(vr->folder, glob->name_pattern->s, sizeof(vr->folder) - 1);
    }
  }
  else {
    if (glob->folder_pattern)
      return 0;
  }

  return 1;
}

static int char_in_set_sbcs(const char *s, char c)
{
  while (*s) {
    if (s[1] == '-' && s[2]) {
      if (c >= s[0] && c <= s[2])
	return 1;
      s += 3;
    }
    else {
      if (c == s[0])
	return 1;
      s++;
    }
  }

  return 0;
}

static int char_in_set_utf8(const char *s, const char *c)
{
  const char *c1 = g_utf8_next_char(c);
  const char *s1;

  while (*s) {
    s1 = g_utf8_next_char(s);
    if (s1[0] == '-' && s1[1]) {
      if (memcmp(c, s, c1 - c) >= 0
	  && memcmp(c, s1 + 1, c1 - c) <= 0)
	return 1;
      s = g_utf8_next_char(s1);
    }
    else {
      if (memcmp(c, s, c1 - c) == 0)
	return 1;
      s = s1;
    }
  }

  return 0;
}

static int pat_match_sub(const TTGlobPattern *pat, const char *str, int utf8)
{
  while (pat) {
    switch (pat->type) {
    case LITERAL:
      if (memcmp(str, pat->s, pat->n) != 0)
	return 0;
      str += pat->n;
      break;

    case BRACKETS:
      if (!*str)
	return 0;

      if (utf8) {
	if (!char_in_set_utf8(pat->s, str))
	  return 0;
	str = g_utf8_next_char(str);
      }
      else {
	if (!char_in_set_sbcs(pat->s, *str))
	  return 0;
	str++;
      }
      break;

    case QUESTION:
      if (!*str)
	return 0;
      if (utf8)
	str = g_utf8_next_char(str);
      else
	str++;
      break;

    case STAR:
      if (!pat->next)
	return 1;

      while (*str) {
	if (pat_match_sub(pat->next, str, utf8))
	  return 1;
	if (utf8)
	  str = g_utf8_next_char(str);
	else
	  str++;
      }
      return 0;

    case SLASH:
    case DOT:
      return 0;
    }

    pat = pat->next;
  }

  return (!*str);
}

static int pat_match(const TTGlobPattern *pat, const char *str)
{
  if (model_uses_utf8(calc_model)) {
    if (!g_utf8_validate(str, -1, NULL))
      return 0;
    return pat_match_sub(pat, str, 1);
  }
  else {
    return pat_match_sub(pat, str, 0);
  }
}

static int pat_match_type(const TTGlobPattern *pat, int type)
{
  const char *s;
  char *us;

  s = tifiles_vartype2string(calc_model, type);
  if (!s || !s[0])
    return 0;
  us = g_ascii_strup(s, -1);
  if (pat_match(pat, us)) {
    g_free(us);
    return 1;
  }
  g_free(us);

  s = tifiles_vartype2fext(calc_model, type);
  us = g_ascii_strup(s, -1);
  if (pat_match(pat, us)) {
    g_free(us);
    return 1;
  }
  g_free(us);
  return 0;
}

/* Check if glob matches this variable */
static int glob_matches_var(const TTGlob *glob, const VarEntry *ve)
{
  char *s;

  if (!ve)
    return 0;
  if (!glob)
    return 1;

  if (glob->exact_type >= 0) {
    if (ve->type != glob->exact_type)
      return 0;
  }
  else if (glob->type_pattern) {
    if (!pat_match_type(glob->type_pattern, ve->type))
      return 0;
  }

  if (glob->folder_pattern) {
    if (!pat_match(glob->folder_pattern, ve->folder))
      return 0;
  }

  s = ticonv_varname_detokenize(calc_model, ve->name, ve->type);
  if (!pat_match(glob->name_pattern, s)) {
    g_free(s);
    return 0;
  }
  g_free(s);

  return 1;
}

/* Run func for every variable that matches */
static int tt_glob_foreach(const TTGlob *glob, GNode *vars, GNode *apps,
			   int (*func)(VarEntry *ve))
{
  GNode *f, *v;
  int matched = 0, e;
  VarEntry *ve;

  if (vars) {
    for (f = vars->children; f; f = f->next) {
      for (v = f->children; v; v = v->next) {
	ve = v->data;
	ve->type &= VAR_TYPE_MASK;

	if (!glob_matches_var(glob, ve))
	  continue;

	if ((e = (*func)(ve)))
	  return e;

	matched = 1;
      }
    }
  }

  if (apps) {
    for (f = apps->children; f; f = f->next) {
      for (v = f->children; v; v = v->next) {
	ve = v->data;
	ve->type &= VAR_TYPE_MASK;

	if (!glob_matches_var(glob, ve))
	  continue;

	if ((e = (*func)(ve)))
	  return e;

	matched = 1;
      }
    }
  }

  if (matched)
    return 0;
  else
    return -1;
}

/* Run func for every variable matching one of the input patterns */
int tt_globs_foreach(char **patterns, int (*func)(VarEntry *ve))
{
  int i, n, e, status = 0;
  TTGlob **globs;
  VarEntry ve;
  GNode *vars = NULL, *apps = NULL;

  for (i = 0; patterns && patterns[i]; i++)
    ;

  if (i == 0)
    return 0;

  n = i;
  globs = g_new(TTGlob *, n);

  for (i = 0; i < n; i++) {
    globs[i] = tt_glob_parse(patterns[i]);

    if (!globs[i]) {
      g_printerr("%s: invalid pattern '%s'\n",
		 g_get_prgname(), patterns[i]);
      status = 15;
    }
  }

  for (i = 0; !status && i < n; i++) {
    if (!tt_glob_check_exact(globs[i], NULL)) {
      if (!(ticalcs_calc_features(calc_handle) & OPS_DIRLIST)) {
	g_printerr("%s: calculator does not support"
		   " directory listing\n",
		   g_get_prgname());
	status = 10;
      }
      else if ((e = ticalcs_calc_get_dirlist(calc_handle, &vars, &apps))) {
	tt_print_error(e, "unable to read directory listing");
	status = 2;
      }
      break;
    }
  }

  for (i = 0; !status && i < n; i++) {
    if (tt_glob_check_exact(globs[i], &ve)) {
      status = (*func)(&ve);
    }
    else {
      status = tt_glob_foreach(globs[i], vars, apps, func);
      if (status < 0) {
	g_printerr("%s: variable '%s' not found\n",
		   g_get_prgname(), patterns[i]);
	status = 2;
      }
    }
  }

  for (i = 0; i < n; i++)
    tt_glob_free(globs[i]);
  g_free(globs);

  ticalcs_dirlist_destroy(&vars);
  ticalcs_dirlist_destroy(&apps);
  return status;
}

int tt_vars_foreach(int (*func)(VarEntry *ve))
{
  GNode *vars = NULL, *apps = NULL;
  int status, e;

  if (!(ticalcs_calc_features(calc_handle) & OPS_DIRLIST)) {
    g_printerr("%s: calculator does not support"
	       " directory listing\n",
	       g_get_prgname());
    return 10;
  }
  else if ((e = ticalcs_calc_get_dirlist(calc_handle, &vars, &apps))) {
    tt_print_error(e, "unable to read directory listing");
    ticalcs_dirlist_destroy(&vars);
    ticalcs_dirlist_destroy(&apps);
    return 2;
  }

  status = tt_glob_foreach(NULL, vars, apps, func);
  ticalcs_dirlist_destroy(&vars);
  ticalcs_dirlist_destroy(&apps);
  return status;
}

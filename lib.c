/*
 * Copyright (C) 1996-8 Michael R. Elkins <me@cs.hmc.edu>
 * Copyright (C) 1998 Brandon C. Long <blong@fiction.net>
 * 
 *     This program is free software; you can redistribute it and/or modify
 *     it under the terms of the GNU General Public License as published by
 *     the Free Software Foundation; either version 2 of the License, or
 *     (at your option) any later version.
 * 
 *     This program is distributed in the hope that it will be useful,
 *     but WITHOUT ANY WARRANTY; without even the implied warranty of
 *     MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *     GNU General Public License for more details.
 * 
 *     You should have received a copy of the GNU General Public License
 *     along with this program; if not, write to the Free Software
 *     Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */ 

/* File copied and modified from Mutt - http://www.mutt.org */

#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <pwd.h>

#include "gbuffy.h"

void safe_free (void **p)
{
  if (*p)
  {
    free (*p);
    *p = 0;
  }
}

char *safe_strdup (const char *s)
{
  char *p;
  size_t l;

  if (!s || !*s) return 0;
  l = strlen (s) + 1;
  p = (char *)malloc (l);
  memcpy (p, s, l);
  return (p);
}

void safe_realloc (void **p, size_t siz)
{
  void *r;

  if (siz == 0)
  {
    if (*p)
    {
      free (*p);
      *p = NULL;
    }
    return;
  }

  if (*p)
    r = (void *) realloc (*p, siz);
  else
  {
    /* realloc(NULL, nbytes) doesn't seem to work under SunOS 4.1.x */
    r = (void *) malloc (siz);
  }

  if (!r)
  {
    g_print ("-E- Out of memory!");
    sleep (1);
    gtk_exit (1);
  }

  *p = r;
}

void *safe_calloc (size_t nmemb, size_t size)
{
  void *p;

  if (!nmemb || !size)
    return NULL;
  if (!(p = calloc (nmemb, size)))
  {
    g_print ("Out of memory!");
    sleep (1);
    gtk_exit (1);
  }
  return p;
}

char *gbuffy_expand_path (char *s, size_t slen)
{
  char p[_POSIX_PATH_MAX] = "";
  char *q = NULL;

  if (*s == '~')
  {
    if (*(s + 1) == '/' || *(s + 1) == 0)
      snprintf (p, sizeof (p), "%s%s", NONULL(Homedir), s + 1);
    else
    {
      struct passwd *pw;

      q = strchr (s + 1, '/');
      if (q)
	*q = 0;
      if ((pw = getpwnam (s + 1)))
	snprintf (p, sizeof (p), "%s/%s", pw->pw_dir, q ? q + 1 : "");
      else
      {
	/* user not found! */
	if (q)
	  *q = '/';
	return (NULL);
      }
    }
  }
  else if (*s == '=' || *s == '+')
    snprintf (p, sizeof (p), "%s/%s", NONULL (Maildir), s + 1);
  else
    return s;

  if (*p)
  {
    strfcpy (s, p, slen); /* replace the string with the expanded version. */
  }
  return (s);
}

/* Reads an arbitrarily long header field, and looks ahead for continuation
 * lines.  ``line'' must point to a dynamically allocated string; it is
 * increased if more space is required to fit the whole line.
 */
char *read_rfc822_line (FILE *f, char *line, size_t *linelen)
{
  char *buf = line;
  char ch;
  size_t offset = 0;

  FOREVER
  {
    if (fgets (buf, *linelen - offset, f) == NULL ||	/* end of file or */
	(ISSPACE (*line) && !offset))			/* end of headers */ 
    {
      *line = 0;
      return (line);
    }

    buf += strlen (buf) - 1;
    if (*buf == '\n')
    {
      /* we did get a full line. remove trailing space */
      while (ISSPACE (*buf))
	*buf-- = 0;	/* we cannot come beyond line's beginning because
			 * it begins with a non-space */

      /* check to see if the next line is a continuation line */
      if ((ch = fgetc (f)) != ' ' && ch != '\t')
      {
	ungetc (ch, f);
	return (line); /* next line is a separate header field or EOH */
      }

      /* eat tabs and spaces from the beginning of the continuation line */
      while ((ch = fgetc (f)) == ' ' || ch == '\t')
	;
      ungetc (ch, f);
      *++buf = ' '; /* string is still terminated because we removed
		       at least one whitespace char above */
    }

    buf++;
    offset = buf - line;
    if (*linelen < offset + STRING)
    {
      /* grow the buffer */
      *linelen += STRING;
      safe_realloc ((void **) &line, *linelen);
      buf = line + offset;
    }
  }
  /* not reached */
}


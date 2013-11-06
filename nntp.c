/*
 * Copyright (C) 1998 Brandon Long <blong@fiction.net>
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

/* copied from my nntp patch for mutt 0.95, and modified */

#include <unistd.h>
#include <netinet/in.h>
#include <netdb.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <sys/socket.h>

#include "gbuffy.h"
#include "rfc2047.h"
#include "msocket.h"
#include "gtklib.h"

/* Minimal support for NNTP */

typedef struct
{
  unsigned int hasXHDR : 1;
  unsigned int hasXPAT : 1;
  unsigned int hasXGTITLE : 1;
  unsigned int hasXOVER : 1;
} NNTP_DATA;

static void nntp_error (const char *where, const char *msg)
{
  dprint (1, (debugfile, "nntp_error(): unexpected response in %s: %s\n", where, msg));
}

/* XOVER returns a tab separated list of:
 * id|subject|from|date|Msgid|references|bytes|lines|xref
 *
 * This has to duplicate some of the functionality of 
 * mutt_read_rfc822_header(), since it replaces the call to that (albeit with
 * a limited number of headers which are "parsed" by placement in the list)
 */
static int nntp_parse_xover (BOX_INFO *mbox, char *buf, GList *headers)
{
  char *p, *b;
  int x;
  int article_num;
  int recent = FALSE;
  char from[STRING] = "";
  char subject[STRING] = "";
  char temp[STRING];

  b = p = buf;

  for (x = 0; x < 9; x++)
  {
    while (*p && *p != '\t') p++;
    if (*p)
    {
      *p = '\0';
      p++;
    }
    switch (x)
    {
      case 0:

	article_num = atoi (b);
	if (!nntp_get_status (mbox->path, mbox->newsrc, article_num))
	  recent = TRUE;
	break;
      case 1:
	if (recent)
	{
	  rfc2047_decode (temp, b, sizeof (temp));
	  snprintf (subject, sizeof (subject), "Subject: %s", temp);
	}
	break;
      case 2:
	if (recent)
	{
	  rfc2047_decode (temp, b, sizeof (temp));
	  snprintf (from, sizeof (from), "From: %s", temp);
	}
	break;
      case 8:
#if 0
	/* Currently, we don't bother with the xref header */
	if (!hdr->read)
	  switch (nntp_parse_xref (nntp_data->server, nntp_data->group, b))
	  {
	    case 1:
	      if (option (OPTMARKOLD))
		hdr->old = 1; 
	      break;
	    case 2:
	      hdr->read = 1;
	      break;
	  }
#endif
      default:
	break;
    }
#if 0
    if (!*p)
      return (-1);
#endif
    b = p;
  }
  if (recent)
  {
    MESSAGE_INFO *m;

    m = (MESSAGE_INFO *) calloc (1, sizeof (MESSAGE_INFO));

    m->from = safe_strdup (from);
    m->subject = safe_strdup (subject);
    headers = g_list_append (headers, m);
  }
  return 0;
}

static int nntp_attempt_auth (BOX_INFO *mbox, CONNECTION *conn)
{
  char bufout[LONG_STRING];
  char buf[STRING];
  char user[SHORT_STRING];
  char pass[SHORT_STRING];

  msocket_write (conn, "STAT\r\n");
  msocket_read_line_d (bufout, sizeof (bufout), conn);

  if (!strncmp ("480 ", bufout, 4))
  {
    int r = 1;

    while (r != 0)
    {

      if (!mbox->login || !mbox->pass)
      {
	if (mbox->login)
	  strfcpy (user, mbox->login, sizeof (user));
	else
	  strfcpy (user, NONULL(Username), sizeof (user));
	pass[0] = '\0';
	/* prompt for username/password */
	snprintf (buf, sizeof (buf), "  Login for NNTP Server %s:%d  ",
	    conn->server, conn->port);
	if (!password_prompt_dialog (buf, user, sizeof (user), pass, sizeof (pass)))
	{
	  return (-1);
	}
      }
      else
      {
	strfcpy (user, mbox->login, sizeof (user));
	strfcpy (pass, mbox->pass, sizeof (pass));
      }

      snprintf (bufout, sizeof (bufout), "AUTHINFO USER %s\n", user);
      msocket_write (conn, bufout);
      msocket_read_line_d (bufout, sizeof (bufout), conn);
      if (strncmp (bufout, "381", 3))
      {
	/* expected response is 381 PASS required, on anything else,
	 * prompt again */
	FREE (&mbox->pass);
	continue;
      }

      snprintf (bufout, sizeof (bufout), "AUTHINFO PASS %s\n", pass);
      msocket_write (conn, bufout);
      msocket_read_line_d (bufout, sizeof (bufout), conn);
      if (bufout[0] == '5')
      {
	/* error on auth, try again */
	FREE (&mbox->pass);
      }
      else
	r = 0;
    }
    FREE (&mbox->login);
    mbox->login = safe_strdup (user);
    FREE (&mbox->pass);
    mbox->pass = safe_strdup (pass);
  }
  return 0;
}

#if 0
static char *nntp_get_desc (BOX_INFO *mbox, CONNECTION *conn)
{
  char *desc = NULL;
  char buf[HUGE_STRING];
  int n;
  NNTP_DATA *nntp_data = (NNTP_DATA *)(conn->data);

  /* Get newsgroup description, if we can */
  if (nntp_data->hasXGTITLE)
  {
    snprintf (buf, sizeof (buf), "XGTITLE %s\r\n", mbox->path);
    msocket_write (conn, buf);
    if (msocket_read_line_d (buf, sizeof (buf), conn) < 0)
    {
      nntp_error ("nntp_get_desc()", buf);
      close (conn->fd);
      return desc;
    }
    /* If valid reply */
    if (!strncmp ("282 ", buf, 4))
    {
      while (buf[0] != '.')
      {
	if (msocket_read_line_d (buf, sizeof (buf), conn) < 0)
	{
	  nntp_error ("nntp_get_desc()", buf);
	  close (conn->fd);
	  return desc;
	}
	n = 0;
	while (buf[n] && buf[n] != '\t' && buf[n] != ' ') n++;
	while (buf[n] && (buf[n] == '\t' || buf[n] == ' ')) n++;
	dprint (1, (debugfile, "desc: %s\n", &buf[n]));
	if (buf[n])
	  desc = safe_strdup (&buf[n]);
      }
    }
  }

  return desc;
}
#endif

static int nntp_attempt_features (BOX_INFO *mbox, CONNECTION *conn)
{
  char buf[LONG_STRING];
  NNTP_DATA *nntp_data;

  if (conn->data != NULL)
    FREE (&conn->data);

  nntp_data = (NNTP_DATA *) safe_calloc (1, sizeof (NNTP_DATA));
  conn->data = (void *)nntp_data;

  msocket_write (conn, "XHDR\r\n");
  msocket_read_line_d (buf, sizeof (buf), conn);
  if (strncmp ("500 ", buf, 4))
    nntp_data->hasXHDR = 1;

  msocket_write (conn, "XOVER\r\n");
  msocket_read_line_d (buf, sizeof (buf), conn);
  if (strncmp ("500 ", buf, 4))
    nntp_data->hasXOVER = 1;

  msocket_write (conn, "XPAT\r\n");
  msocket_read_line_d (buf, sizeof (buf), conn);
  if (strncmp ("500 ", buf, 4))
    nntp_data->hasXPAT = 1;

  /* Hmm, looks like the diablo server will respond with everything if
   * we don't specify a group, so specify a group */
  msocket_write (conn, "XGTITLE alt.test\r\n");
  msocket_read_line_d (buf, sizeof (buf), conn);
  if (strncmp ("500 ", buf, 4))
  {
    nntp_data->hasXGTITLE = 1;
    if (!strncmp ("282 ", buf, 4))
    {
      while (!(buf[0] == '.' && buf[1] == '\0'))
      {
	if (msocket_read_line (buf, sizeof (buf), conn) < 0)
	{
	  nntp_error ("nntp_attempt_features", "Lost connection to server!");
	  conn->uses = 0;
	  return (-1);
	}
      }
    }
  }
  return 0;
}

int nntp_open_connection (BOX_INFO *mbox, CONNECTION *conn)
{
  char buf[HUGE_STRING];

  if (msocket_open_connection (conn) < 0)
  {
    g_print ("Unable to open connection to NNTP server %s:%d\n",
	conn->server, conn->port);
    return (-1);
  }

  if (msocket_read_line_d (buf, sizeof (buf), conn) < 0)
  {
    close (conn->fd);
    return (-1);
  }

  if (buf[0] != '2')
  {
    nntp_error ("nntp_open_connection()", buf);
    close (conn->fd);
    conn->uses = 0;
    return (-1);
  } 

  nntp_attempt_auth (mbox, conn);

  nntp_attempt_features (mbox, conn);

  return 0;
}

int nntp_close_connection (CONNECTION *conn)
{
  char buf[LONG_STRING]; 

  if (conn->uses != 0)
  {
    snprintf (buf, sizeof (buf), "QUIT\r\n");
    msocket_write (conn, buf);
    close (conn->fd);
    conn->uses = 0;
  }
  return 0;
}

int nntp_folder_count (BOX_INFO *mbox, int force, GList *headers)
{
  CONNECTION *conn;
  NNTP_DATA *nntp_data;
  char bufout[LONG_STRING];
  char buf[HUGE_STRING];
  int first, last;
  int rlast, count;
  int oldnew;

  /* Check for valid NNTP box */
  if (!(mbox->server && mbox->path && mbox->newsrc))
    return (-1);


  oldnew = mbox->new_messages;

  conn = msocket_select_connection (mbox->server, mbox->port, 0);

  if (conn->uses == 0)
  {
    if (nntp_open_connection (mbox, conn) < 0)
      return (-1);
    /* For this application, this is only an "in use" variable */
    conn->uses++;
  }

  nntp_data = (NNTP_DATA *)(conn->data);
  snprintf (bufout, sizeof (bufout), "GROUP %s\r\n", mbox->path);
  msocket_write (conn, bufout);
  if (msocket_read_line_d (buf, sizeof (buf), conn) < 0)
  {
    nntp_error ("nntp_open_mailbox()", buf);
    close (conn->fd);
    return (-1);
  }

  if (strncmp ("211 ", buf, 4))
  {
    /* GROUP command failed */
    if (!strncmp ("411 ", buf, 4))
    {
      snprintf (bufout, sizeof (bufout), 
	  "Newsgroup %s not found on server %s", mbox->path, mbox->server);
      nntp_error ("nntp_open_mailbox()", bufout);
      sleep (1);
    }
    nntp_error ("nntp_open_mailbox()", buf);
    return (-1);
  }

  sscanf (buf + 4, "%d %d %d", &count, &first, &last);

  rlast = nntp_last_read (mbox->path, mbox->newsrc);

  mbox->num_messages = last-first;
  mbox->new_messages = (last-rlast > 0 ? last-rlast : 0);

  if (mbox->new_messages && headers)
  {
    if (nntp_data->hasXOVER)
    {
      snprintf (buf, sizeof (buf), "XOVER %d-%d\r\n", rlast, last);
      msocket_write (conn, buf);
      if (msocket_read_line_d (buf, sizeof (buf), conn) < 0)
      {
	nntp_error ("nntp_fetch_headers()", buf);
	close (conn->fd);
	conn->uses = 0;
	return (-1);
      }
      if (buf[0] != '2')
      {
	nntp_error ("XOVER command failed", buf);
	close (conn->fd);
	conn->uses = 0;
	return (-1);
      }
      FOREVER
      {
	char *pc;

	if (msocket_read_line (buf, sizeof (buf), conn) < 0)
	{
	  nntp_error ("nntp_fetch_headers()", buf);
	  close (conn->fd);
	  conn->uses = 0;
	  return (-1);
	}

	pc = buf;
	if (buf[0] == '.')
	{
	  if (buf[1] == '\0')
	    return 0;
	  else
	    pc++;
	}
	nntp_parse_xover (mbox, pc, headers);
      }
    }
  }

  if (oldnew != mbox->new_messages)
    return TRUE;
  else
    return FALSE;
}

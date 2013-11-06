/*
 * Copyright (C) 1996-8 Michael R. Elkins <me@cs.hmc.edu>
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

/* Based on the imap code from mutt 0.94.17 */

#include <unistd.h>
#include <netinet/in.h>
#include <netdb.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>

#include "gbuffy.h"
#include "rfc2047.h"
#include "msocket.h"
#include "gtklib.h"

/* Minimal support for IMAP 4rev1 */

#define SEQLEN 5

enum
{
  IMAP_FATAL = 1,
  IMAP_NEW_MAIL,
  IMAP_EXPUNGE,
  IMAP_BYE,
  IMAP_OK_FAIL
};

/* Linked list to hold header information while downloading message
 * headers
 */
typedef struct imap_header_info
{
  unsigned int read : 1;
  unsigned int old : 1;
  unsigned int deleted : 1;
  unsigned int flagged : 1;
  unsigned int replied : 1;
  unsigned int changed : 1;
  unsigned int number;

  time_t received;
  long content_length;
  struct imap_header_info *next;
} IMAP_HEADER_INFO;


static void imap_make_sequence (char *buf, size_t buflen)
{
  static int sequence = 0;
  
  snprintf (buf, buflen, "a%04d", sequence++);
}


static void imap_error (const char *where, const char *msg)
{
  g_print ("-E- imap_error(): unexpected response in %s: %s\n", where, msg);
}

static void imap_quote_string (char *dest, size_t slen, const char *src)
{
  char quote[] = "\"\\", *pt;
  const char *s;
  size_t len = slen;

  pt = dest;
  s  = src;

  *pt++ = '"';
  /* save room for trailing quote-char */
  len -= 2;
  
  for (; *s && len; s++)
  {
    if (strchr (quote, *s))
    {
      len -= 2;
      if (!len)
	break;
      *pt++ = '\\';
      *pt++ = *s;
    }
    else
    {
      *pt++ = *s;
      len--;
    }
  }
  *pt++ = '"';
  *pt = 0;
}

#if 0
static int imap_read_bytes (FILE *fp, CONNECTION *conn, long bytes)
{
  long pos;
  long len;
  char buf[LONG_STRING];

  for (pos = 0; pos < bytes; )
  {
    len = msocket_read_line (buf, sizeof (buf), conn);
    if (len < 0)
      return (-1);
    pos += len;
    fputs (buf, fp);
    fputs ("\n", fp);
  }

  return 0;
}
#endif

/* returns 1 if the command result was OK, or 0 if NO or BAD */
static int imap_code (const char *s)
{
  s += SEQLEN;
  SKIPWS (s);
  return (strncasecmp ("OK", s, 2) == 0);
}

static char *imap_next_word (char *s)
{
  while (*s && !ISSPACE (*s))
    s++;
  SKIPWS (s);
  return s;
}

/*
 * Execute a command, and wait for the response from the server.
 * Also, handle untagged responses
 * If flags == IMAP_OK_FAIL, then the calling procedure can handle a response 
 * failing, this is used for checking for a mailbox on append and login
 * Return 0 on success, -1 on Failure, -2 on OK Failure
 */
static int imap_exec (char *buf, size_t buflen, CONNECTION *conn, 
    const char *seq, const char *cmd, int flags)
{
  msocket_write (conn, cmd);

  do
  {
    if (msocket_read_line_d (buf, buflen, conn) < 0)
      return (-1);

    /* should handle untagged messages, but this implementation doesn't
     * need it */
  }
  while (strncmp (buf, seq, SEQLEN) != 0);

  if (!imap_code (buf))
  {
    char *pc;

    if (flags == IMAP_OK_FAIL)
      return (-2);
    dprint (1, (debugfile, "imap_exec(): command failed: %s\n", buf));
    pc = buf + SEQLEN;
    SKIPWS (pc);
    pc = imap_next_word (pc);
/*    mutt_error (pc); */
    sleep (1);
    return (-1);
  }

  return 0;
}

/* Given a CONNECTION, open the connection (the structure should be
 * filled out with a server/port number
 */
static int imap_open_connection (BOX_INFO *ibox, CONNECTION *conn)
{
  char buf[LONG_STRING];
  char user[SHORT_STRING], q_user[SHORT_STRING];
  char pass[SHORT_STRING], q_pass[SHORT_STRING];
  char seq[16];

  if (msocket_open_connection (conn) < 0)
  {
    g_print ("Unable to open connection to IMAP server %s:%d\n", 
	conn->server, conn->port);
    return (-1);
  }

  if (msocket_read_line_d (buf, sizeof (buf), conn) < 0)
  {
    close (conn->fd);
    return (-1);
  }

  if (strncmp ("* OK", buf, 4) == 0)
  {
    int r = 1;

    while (r != 0)
    {
      if (!ibox->login || !ibox->pass)
      {
	if (ibox->login)
	  strfcpy (user, ibox->login, sizeof (user));
	else
	  strfcpy (user, NONULL(Username), sizeof (user));
	pass[0]=0;
	/* prompt for username/password */
	snprintf (buf, sizeof (buf), "  Login for IMAP Server %s:%d  ", 
	    conn->server, conn->port);
	if (!password_prompt_dialog (buf, user, sizeof (user), pass, sizeof (pass)))
	{
	  return (-1);
	}
      }
      else
      {
	strfcpy (user, ibox->login, sizeof (user));
	strfcpy (pass, ibox->pass, sizeof (pass));
      }

      imap_quote_string (q_user, sizeof (q_user), user);
      imap_quote_string (q_pass, sizeof (q_pass), pass);

/*      mutt_message _("Logging in..."); */
      imap_make_sequence (seq, sizeof (seq));
      snprintf (buf, sizeof (buf), "%s LOGIN %s %s\r\n", seq, q_user, q_pass);
      r = imap_exec (buf, sizeof (buf), conn, seq, buf, IMAP_OK_FAIL);
      if (r == -1)
      {
	/* connection or protocol problem */
	imap_error ("imap_open_connection()", buf);
	return (-1);
      }
      else if (r == -2)
      {
	/* Login failed, try again */
/*	mutt_error _("Login failed."); */
	sleep (1);
	FREE (&ibox->login);
	FREE (&ibox->pass);
      }
      else
      {
	/* If they have a successful login, we may as well cache the 
	 * user/password. */
	FREE (&ibox->login);
	ibox->login = safe_strdup (user);
	FREE (&ibox->pass);
	ibox->pass = safe_strdup (pass);
      }
    }
  }
  else if (strncmp ("* PREAUTH", buf, 9) != 0)
  {
    imap_error ("imap_open_connection()", buf);
    close (conn->fd);
    return (-1);
  }

  return 0;
}

#if 0
int imap_close_connection (CONTEXT *ctx)
{
  char buf[LONG_STRING];
  char seq[8];

  dprint (1, (debugfile, "imap_close_connection(): closing connection\n"));
  /* if the server didn't shut down on us, close the connection gracefully */
  if (CTX_DATA->status != IMAP_BYE)
  {
    mutt_message _("Closing connection to IMAP server...");
    imap_make_sequence (seq, sizeof (seq));
    snprintf (buf, sizeof (buf), "%s LOGOUT\r\n", seq);
    mutt_socket_write (CTX_DATA->conn, buf);
    do
    {
      if (mutt_socket_read_line_d (buf, sizeof (buf), CTX_DATA->conn) < 0)
	break;
    }
    while (mutt_strncmp (seq, buf, SEQLEN) != 0);
    mutt_clear_error ();
  }
  close (CTX_DATA->conn->fd);
  CTX_DATA->conn->uses = 0;
  CTX_DATA->conn->data = NULL;
  return 0;
}
#endif

/* Ok, now fetch all the headers from the first UNSEEN through the
 * end of the mailbox.  This could be lower bandwidth (maybe) by first
 * fetching all of the flags, and then only fetching the headers of
 * the messages which are new, but there is a lot of protocol crap
 * which would probably negate the lower bandwidth */
static int parse_fetch (BOX_INFO *ibox, CONNECTION *conn, GList *headers, 
    int unseen)
{
  char from[LONG_STRING] = "";
  char subject[LONG_STRING] = "";
  char xface[LONG_STRING] = "";
  char buf[LONG_STRING];
  char seq[8];
  char *s, *last_head = NULL;
  int recent = 0;


  imap_make_sequence (seq, sizeof (seq));
  snprintf (buf, sizeof (buf), "%s FETCH %d:%d (FLAGS BODY.PEEK[HEADER.FIELDS (FROM SUBJECT X-FACE)])\r\n", seq, unseen, ibox->num_messages);
  msocket_write (conn, buf);

  do 
  {
    if (msocket_read_line_d (buf, sizeof (buf), conn) < 0)
    {
      conn->uses = 0;
      return (-1);
    }

    if (buf[0] == '*') {
      s = imap_next_word (buf);
      if (!isdigit (*s))
	continue;
      s = imap_next_word (s);
      if (strncasecmp ("FETCH", s, 5) != 0)
	continue;

      s = imap_next_word (s);
      if (*s == '(')
	s++;
      if (strncasecmp ("FLAGS", s, 5) != 0)
	continue;
      s = imap_next_word (s);
      if (*s == '(')
	s++;
      while (*s && (*s != ')') && !recent)
      {
	if (strncasecmp ("\\Recent", s, 7) == 0)
	  recent = 1;
	while (*s && (*s != ')') && !ISSPACE (*s))
	  s++;
	SKIPWS (s);
      }

      do
      {
	if (msocket_read_line_d (buf, sizeof (buf), conn) < 0)
	{
	  conn->uses = 0;
	  return (-1);
	}
	if (recent)
	{
	  if (!strncasecmp (buf, "From:", 5))
	  {
	    rfc2047_decode (from, buf, sizeof (from));
	    last_head = from;
	  }
	  else if (!strncasecmp (buf, "Subject:", 8))
	  {
	    rfc2047_decode (subject, buf, sizeof (subject));
	    last_head = subject;
	  }
	  else if (!strncasecmp (buf, "X-Face:", 7))
	  {
	    s = buf + 7;
	    while (*s && ISSPACE (*s)) s++;
	    strfcpy (xface, s, sizeof (xface));
	    if (strlen (s) > sizeof (xface))
	      g_print ("-E- xface header is larger than buffer\n");
	    last_head = xface;
	  }
	  else if (last_head && ISSPACE(buf[0])) {
	    s = buf;
	    while (*s && ISSPACE (*s)) s++;
	    if(strlen(s) + strlen(last_head) + 1 > LONG_STRING)
	      g_print ("-E- a continuing header is larger than buffer\n");
	    else {
	      /* If this is an X-Face line the space don't matter, but if this 
	       * is any other header the space is required. */
	      strcat(last_head, " ");
	      strncat(last_head, s, LONG_STRING - strlen(last_head));
	    }
	  } else {
	    last_head = NULL;
	  }
	}
      } 
      while (buf[0] != ')');
      if (recent)
      {
	MESSAGE_INFO *m;

	m = (MESSAGE_INFO *) calloc (1, sizeof (MESSAGE_INFO));

	m->from = safe_strdup (from);
	m->subject = safe_strdup (subject);
	m->face = safe_strdup (xface);
	*from = *subject = *xface = '\0';
	headers = g_list_append (headers, m);
      }
    }
    recent = 0;
  }
  while ((strncmp (buf, seq, SEQLEN) != 0));
  return 0;
}

static int imap_fetch_new_headers (BOX_INFO *ibox, CONNECTION *conn, GList *headers)
{
  char buf[LONG_STRING];
  char seq[8];
  char *s;
  int unseen = 1;

  /* Open mailbox (EXAMINE) and find the first UNSEEN message */
  imap_make_sequence (seq, sizeof (seq));
  snprintf (buf, sizeof (buf), "%s EXAMINE %s\r\n", seq, ibox->path);
  msocket_write (conn, buf);

  do 
  {
    if (msocket_read_line_d (buf, sizeof (buf), conn) < 0)
    {
      conn->uses = 0;
      return (-1);
    }

    if (buf[0] == '*') 
    {
      s = imap_next_word (buf);
      if (strncasecmp ("OK", s, 2) == 0)
      {
	s = imap_next_word (s);
	if (*s == '[')
	  s++;
	if (strncasecmp ("UNSEEN", s, 6) == 0)
	{
	  s = imap_next_word (s);
	  unseen = atoi (s);
	}
      }
      else if (isdigit (*s))
      {
	int num = atoi (s);
	s = imap_next_word (s);
	if (strncasecmp ("EXISTS", s, 6) == 0)
	  ibox->num_messages = num;
	else if (strncasecmp ("RECENT", s, 6) == 0)
	  ibox->new_messages = num;
      }
    }
  }
  while ((strncmp (buf, seq, SEQLEN) != 0));

  /* if the EXAMINE wasn't successful, return */
  s = imap_next_word (buf);
  if (strncasecmp ("OK", s, 2) != 0)
  {
    char *pc;

    dprint (1, (debugfile, "imap_fetch_new_headers(): command failed: %s\n", buf));
    pc = buf + SEQLEN;
    SKIPWS (pc);
    pc = imap_next_word (pc);
/*    mutt_error (pc); */
    sleep (1);
    return (-1);
  }

  if (parse_fetch (ibox, conn, headers, unseen) < 0)
    return (-1);

  return 0;
}

int imap_folder_count (BOX_INFO *ibox, int force, GList *headers)
{
  CONNECTION *conn;
  char buf[LONG_STRING];
  char seq[8];
  char *s;
  int oldnew;

  if (!ibox->server)
  {
    return FALSE;
  }

  conn = msocket_select_connection (ibox->server, ibox->port, 0);

  if (conn->uses == 0)
  {
    /* a bit hackish I guess */
    conn->ssl_mode = ibox->ssl_mode;

    if (imap_open_connection (ibox, conn) < 0)
      return (-1);
    /* For this application, this is only an "in use" variable */
    conn->uses++;
  }

  oldnew = ibox->new_messages;

  /* Technically, the new messages are "RECENT", but if anyone opens the
   * mailbox (including us) they message is no longer RECENT.  I don't
   * think we want that, so use UNSEEN instead */
  /* Go back to using RECENT, as it shouldn't be updated on EXAMINE */
  imap_make_sequence (seq, sizeof (seq));
  snprintf (buf, sizeof (buf), "%s STATUS %s (MESSAGES RECENT)\r\n", seq, 
      ibox->path);

  msocket_write (conn, buf);

  do 
  {
    if (msocket_read_line_d (buf, sizeof (buf), conn) < 0)
    {
      conn->uses = 0;
      return (-1);
    }

    if (buf[0] == '*') 
    {
      s = imap_next_word (buf);
      if (strncasecmp ("STATUS", s, 6) == 0)
      {
	s = imap_next_word (s);
	if ((strncmp (ibox->path, s, strlen (ibox->path)) == 0) ||
            (s[0] == '"' && 
             (!strncmp(ibox->path,s+1,strlen(ibox->path))) &&
             s[1+strlen(ibox->path)] == '"'))
	{
	  s = imap_next_word (s);
	  if (*s == '(')
	    s++;
	  while ((s != NULL) && (*s != '\0'))
	  {
	    if (strncmp ("MESSAGES", s, 8) == 0)
	    {
	      s = imap_next_word (s);
	      if (isdigit (*s))
	      {
/*		g_print ("-I- %s has %d messages\n", ibox->path, atoi (s)); */
		ibox->num_messages = atoi (s);
	      }
	    }
	    else if (strncmp ("RECENT", s, 6) == 0)
	    {
	      s = imap_next_word (s);
	      if (isdigit (*s))
	      {
/*		g_print ("-I- %s has %d new\n", ibox->path, atoi (s)); */
		ibox->new_messages = atoi (s);
	      }
	    }
	    s = imap_next_word (s);
	  }
	}
      }
    }
  }
  while ((strncmp (buf, seq, SEQLEN) != 0));

  if (headers != NULL && ibox->new_messages)
  {
    /* Need to fetch headers, looks for Subject: From: and X-Face: */
    imap_fetch_new_headers (ibox, conn, headers);
  }

  if (oldnew != ibox->new_messages)
    return TRUE;
  else 
    return FALSE;
}

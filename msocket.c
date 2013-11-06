static const char rcsid[]="$Id: msocket.c,v 1.2 2003/10/10 09:06:53 blong Exp $";
/*
 * Copyright (C) 1998 Michael R. Elkins <me@cs.hmc.edu>
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

/* Copied from mutt 0.95 */

#include "gbuffy.h"
#include "msocket.h"

#include <unistd.h>
#include <netinet/in.h>
#include <netdb.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>

#ifdef HAVE_SSL
#include <openssl/ssl.h>
#endif

/* support for multiple socket connections */

static CONNECTION *Connections = NULL;

/* simple read buffering to speed things up. */
int msocket_readchar (CONNECTION *conn, char *c)
{
  if (conn->bufpos >= conn->available)
  {
#ifdef HAVE_SSL
    if (conn->ssl_mode) {
	conn->available = SSL_read(conn->ssl, conn->inbuf, LONG_STRING-1);
    } else {
#endif
	conn->available = read (conn->fd, conn->inbuf, LONG_STRING-1);
#ifdef HAVE_SSL
    }
#endif
    conn->bufpos = 0;
    if (conn->available <= 0)
    {
#ifdef DEBUG
      fflush (debugfile);
#endif
      return conn->available; /* returns 0 for EOF or -1 for other error */
    }
  }
  if (conn->available > LONG_STRING || conn->bufpos > LONG_STRING)
    return -1;
  *c = conn->inbuf[conn->bufpos];
  conn->bufpos++;
  return 1;
}

int msocket_read_line (char *buf, size_t buflen, CONNECTION *conn)
{
  char ch;
  int i;

  for (i = 0; i < buflen; i++)
  {
    if (msocket_readchar (conn, &ch) != 1)
      return (-1);
    if (ch == '\n')
      break;
    buf[i] = ch;
  }
  if (i)
    buf[i-1] = 0;
  else
    buf[i] = 0;
  return (i + 1);
}

int msocket_read_line_d (char *buf, size_t buflen, CONNECTION *conn)
{
  int r = msocket_read_line (buf, buflen, conn);
  dprint (1,(debugfile,"msocket_read_line_d():%s\n", buf));
  return r;
}

int msocket_write (CONNECTION *conn, const char *buf)
{
  dprint (1,(debugfile,"msocket_write():%s", buf));
#ifdef HAVE_SSL
  if (conn->ssl_mode) {
	return SSL_write(conn->ssl, buf, strlen(buf));
  } else {
#endif
	return (write (conn->fd, buf, strlen (buf)));
#ifdef HAVE_SSL
  }
#endif
}

CONNECTION *msocket_select_connection (char *host, int port, int flags)
{
  CONNECTION *conn;

  if (flags != M_SOCKET_NEW)
  {
    conn = Connections;
    while (conn)
    {
      if (!strcmp (host, conn->server) && (port == conn->port))
	return conn;
      conn = conn->next;
    }
  }
  conn = (CONNECTION *) safe_calloc (1, sizeof (CONNECTION));
  conn->bufpos = 0;
  conn->available = 0;
  conn->uses = 0;
  conn->server = safe_strdup (host);
  conn->port = port;
  conn->ssl = NULL;
  conn->ssl_mode = 0;
  conn->next = Connections;
  Connections = conn;

  return conn;
}

int msocket_open_connection (CONNECTION *conn)
{
  struct sockaddr_in sin;
  struct hostent *he;
#ifdef HAVE_SSL
  SSL_CTX *ssl_ctx;
  SSL *ssl;
  int retval;
#endif

  memset (&sin, 0, sizeof (sin));
  sin.sin_port = htons (conn->port);
  sin.sin_family = AF_INET;
  if ((he = gethostbyname (conn->server)) == NULL)
  {
    perror (conn->server);
    return (-1);
  }
  memcpy (&sin.sin_addr, he->h_addr_list[0], he->h_length);

  if ((conn->fd = socket (AF_INET, SOCK_STREAM, IPPROTO_IP)) < 0)
  {
/*    mutt_perror ("socket"); */
    return (-1);
  }

/*  mutt_message (_("Connecting to %s..."), conn->server); */

  if (connect (conn->fd, (struct sockaddr *) &sin, sizeof (sin)) < 0)
  {
/*    mutt_perror ("connect"); */
    close (conn->fd);
  }

#ifdef HAVE_SSL
  if (conn->ssl_mode) {
	  SSL_library_init();
	  ssl_ctx = SSL_CTX_new(SSLv23_client_method());
	  ssl = SSL_new(ssl_ctx);
	  if (!SSL_set_fd(ssl, conn->fd)) {
		close(conn->fd);
		return -1;
	  }

	  if ((retval = SSL_connect(ssl)) < 1) {
		printf("Failed connecting to TLS server: %d\n", SSL_get_error(ssl, retval));
		close(conn->fd);
		return -1;
	  }

	  conn->ssl = ssl;
  }
#endif

  return 0;
}

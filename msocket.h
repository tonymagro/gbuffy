/* $Id: msocket.h,v 1.2 2003/10/10 09:06:53 blong Exp $ */
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
/* Copied and modified from mutt 0.95 */

#ifndef _MSOCKET_H_
#define _MSOCKET_H_ 1

#include "config.h"

#ifdef HAVE_SSL
#include <openssl/ssl.h>
#endif

typedef struct _connection
{
  int uses;

  char *server;
  int port;

  int fd;
  char inbuf[LONG_STRING];
  int bufpos;
  int available;

  void *data;

  struct _connection *next;

#ifdef HAVE_SSL
  int ssl_mode;
  SSL *ssl;
#endif
} CONNECTION;

#define M_SOCKET_NEW 1

int msocket_readchar (CONNECTION *conn, char *c);
int msocket_read_line (char *buf, size_t buflen, CONNECTION *conn);
int msocket_read_line_d (char *buf, size_t buflen, CONNECTION *conn);
int msocket_write (CONNECTION *conn, const char *buf);
CONNECTION *msocket_select_connection (char *host, int port, int flags);
int msocket_open_connection (CONNECTION *conn);

#endif /* _MSOCKET_H_ */

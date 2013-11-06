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

/* Copied from my nntp patch for mutt 0.95, and modified */

#include "gbuffy.h"
#include "msocket.h"

#include <unistd.h>
#include <netinet/in.h>
#include <netdb.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/stat.h>

typedef struct
{
  int first;
  int last;
} NEWSRC_ENTRY;

typedef struct _newsrc_line
{
  NEWSRC_ENTRY *entries;
  int num;			/* number of used entries */
  int max;			/* number of allocated entries */
  int subscribed : 1;		/* subscribed to group */
} NEWSRC_LINE;

typedef struct
{
  char *filename;
  time_t mtime;
  off_t size;
  char *newsgroup;
  NEWSRC_LINE line;
} NEWSRC;

static NEWSRC *newsrc_info = NULL;
static int num_newsrc = 0;

static int nntp_parse_newsrc_line (NEWSRC *newsrc, char *line)
{
  int x = 0;
  char *p = line;
  char *b, *h;

  while (*p)
  {
    if (*p == ',')
      x++;
    p++;
  }
  x++;

  newsrc->line.entries = (NEWSRC_ENTRY *) safe_calloc (x*2, sizeof (NEWSRC_ENTRY));
  newsrc->line.max = x*2;

  p = line;
  while (*p && (*p != ':' && *p != '!')) p++;
  if (!*p) return -1;
  if (*p == ':')
    newsrc->line.subscribed = 1;
  *p = '\0';
  /* line now points to the newsgroup name */
  p++;

  b = p;
  x = 0;
  while (*b)
  {
    while (*p && *p != ',') p++;
    if (*p)
    {
      *p = '\0';
      p++;
    }
    if ((h = strchr(b, '-')))
    {
      *h = '\0';
      h++;
      newsrc->line.entries[x].first = atoi(b);
      newsrc->line.entries[x].last = atoi(h);
    }
    else
    {
      newsrc->line.entries[x].first = atoi(b);
      newsrc->line.entries[x].last = newsrc->line.entries[x].first;
    }
    b = p;
    x++;
  }
  newsrc->line.num = x;
  dprint (1, (debugfile, "parse_line: Newsgroup %s\n", line));
  
  return 0;
}

static int slurp_newsrc (NEWSRC *newsrc, char *group, char *filename)
{
  FILE *fp;
  char buf[HUGE_STRING];
  struct stat sb;
  int grouplen = strlen (group);

  if (newsrc->filename == NULL)
    newsrc->filename = safe_strdup (filename);
  if (newsrc->newsgroup == NULL)
    newsrc->newsgroup = safe_strdup (group);
  if ((fp = fopen (filename, "r")) == NULL)
  {
    return (-1);
  }
#if 0
  /* Hmm, should we lock this file? */
  if (mx_lock_file (filename, fileno (fp), 0, 1, 1))
  {
    fclose (fp);
    return (-1);
  }
#endif
  stat (filename, &sb);
  newsrc->size = sb.st_size;
  newsrc->mtime = sb.st_mtime;

  while (fgets (buf, sizeof (buf), fp))
  {
    if (!strncmp (buf, group, grouplen) && 
	((buf[grouplen] == ':') || (buf[grouplen] == '!')))
    {
      nntp_parse_newsrc_line (newsrc, buf);
      break;
    }
  }
#if 0
  mx_unlock_file (filename, fileno (fp));
#endif
  fclose (fp);

  return 0;
}

/*
 * Automatically loads a newsrc into memory, if necessary.
 * Checks the size/mtime of a newsrc file, if it doesn't match, load
 * again.  Hmm, if a system has broken mtimes, this might mean the file
 * is reloaded every time, which we'd have to fix.
 *
 * a newsrc file is a line per newsgroup, with the newsgroup, then a 
 * ':' denoting subscribed or '!' denoting unsubscribed, then a 
 * comma separated list of article numbers and ranges.
 */
NEWSRC *select_newsrc (char *group, char *filename)
{
  char file[_POSIX_PATH_MAX];
  int x;

  strcpy (file, filename);
  gbuffy_expand_path (file, sizeof (file));

  for (x = 0; x < num_newsrc; x++)
  {
    if (!strcmp (file, newsrc_info[x].filename) &&
	!strcmp (group, newsrc_info[x].newsgroup))
    {
      struct stat sb;

      stat (file, &sb);
      if (newsrc_info[x].size == sb.st_size && 
	  newsrc_info[x].mtime == sb.st_mtime)
      {
	return &newsrc_info[x];
      }
      else
      {
	FREE (&newsrc_info[x].filename);

	FREE (&newsrc_info[x].line.entries);
	slurp_newsrc (&newsrc_info[x], group, file);
	return &newsrc_info[x];
      }
    }
  }

  /* New newsrc */
  x = num_newsrc;
  if (num_newsrc == 0)
  {
    num_newsrc++;
    newsrc_info = (NEWSRC *) safe_calloc (1, sizeof (NEWSRC));
  }
  else
  {
    num_newsrc++;
    safe_realloc ((void *)&newsrc_info, sizeof (NEWSRC) * num_newsrc);
    newsrc_info[num_newsrc-1].filename = NULL;
    newsrc_info[num_newsrc-1].mtime = 0;
    newsrc_info[num_newsrc-1].size = 0;
    newsrc_info[num_newsrc-1].newsgroup = NULL;
    newsrc_info[num_newsrc-1].line.entries = NULL;
    newsrc_info[num_newsrc-1].line.num = 0;
    newsrc_info[num_newsrc-1].line.max = 0;
    newsrc_info[num_newsrc-1].line.subscribed = 0;
  }
  slurp_newsrc (&newsrc_info[x], group, file);
  return &newsrc_info[x];
}

/* 
 * full status flags are not supported by nntp, but we can fake some
 * of them.  This is how:
 * Read = a read message number is in the .newsrc
 * New = a message is new since we last read this newsgroup
 * Old = anything else
 * So, Read is marked as such in the newsrc, old is anything that is 
 * "skipped" in the newsrc, and new is anything not in the newsrc.
 * By skipped, I mean before the last unread message
 * Returns 0 if not read, 1 if old, 2 if read
 */

int nntp_get_status (char *group, char *npath, int article_num)
{
  NEWSRC *newsrc;
  NEWSRC_LINE *data;
  int x;

  newsrc = select_newsrc (group, npath);

  data = &newsrc->line;

  if (data == NULL)
  {
    dprint (1, (debugfile, "newsgroup %s not found\n", group));
    return 0;
  }

  for (x = 0; x < data->num; x++)
  {
    if ((article_num >= data->entries[x].first) &&
	(article_num <= data->entries[x].last))
    {
      return 2;
    }
  }
  /* Old articles are articles which aren't read but an article after them 
   * has been read */
  if (article_num < data->entries[data->num - 1].last)
    return 1;
  return 0;
}

/* 
 * for nntp_context, we need the "oldest read" message, which one would
 * think would be the first in the newsrc line, but its actually the
 * last of the first pair (the first pair is usually 1-<first read>)
 */
int nntp_first_read (char *group, char *npath)
{
  NEWSRC *newsrc;
  NEWSRC_LINE *data;

  newsrc = select_newsrc (group, npath);

  data = &newsrc->line;

  if (data == NULL)
    return 0;

  if (data->num)
    return (data->entries[0].last);
  else 
    return 0;
}

/*
 * for buffy, we need the last read, which is the last entry on the line 
 */
int nntp_last_read (char *group, char *npath)
{
  NEWSRC *newsrc;
  NEWSRC_LINE *data;

  newsrc = select_newsrc (group, npath);

  data = &newsrc->line;

  if (data == NULL)
    return 0;

  if (data->num)
    return (data->entries[data->num-1].last);
  else 
    return 0;
}

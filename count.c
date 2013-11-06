
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <pwd.h>
#include <sys/time.h>
#include <dirent.h>
#include <errno.h>
#include <ctype.h>
#include <stdlib.h>

#include "gbuffy.h"
#include "rfc2047.h"

int parse_mime_header (BOX_INFO *mbox, GList *headers, FILE *fp, 
    int override_new)
{
  static char *buffer = NULL;
  static size_t buflen = STRING;
  char from[STRING] = "";
  char subject[STRING] = "";
  char xface[STRING] = "";
  int status = FALSE;
  int is_new = FALSE;

  if (buffer == NULL)
    buffer = (char *) malloc (buflen);

  while (*(buffer = read_rfc822_line (fp, buffer, &buflen)) != 0) 
  {
    if (!strncmp (buffer, "Status:", 7)) 
    {
      status = TRUE;
      if (!strchr (buffer, 'R') && !strchr (buffer, 'O')) 
      {
	is_new = TRUE;
      }
    }
    else if (headers)
    {
      if (!strncasecmp (buffer, "From:", 5))
      {
	rfc2047_decode (from, buffer, sizeof (from));
      }
      else if (!strncasecmp (buffer, "Subject:", 8))
      {
	rfc2047_decode (subject, buffer, sizeof (subject));
      }
      else if (!strncasecmp (buffer, "X-Face:", 7))
      {
	char *s;
	s = buffer + 7;
	while (*s && ISSPACE (*s)) s++;
	strfcpy (xface, s, sizeof (xface));
	if (strlen (s) > sizeof (xface))
	  g_print ("-E- xface header is larger than buffer\n");
      }
    }
  }
  if (status == FALSE) 
  {
    is_new = TRUE;
  }
  else 
    status = FALSE;

  if (headers && (is_new || override_new))
  {
    MESSAGE_INFO *m;

    m = (MESSAGE_INFO *) calloc (1, sizeof (MESSAGE_INFO));

    m->from = safe_strdup (from);
    m->subject = safe_strdup (subject);
    m->face = safe_strdup (xface);
    headers = g_list_append (headers, m);
  }

  return is_new;
}

/*
 * Return 0 on no change/failure, 1 on change
 */
int mbox_folder_count (BOX_INFO *mbox, int force, GList *headers)
{
  FILE *fp;
  char buffer[STRING];
  char garbage[STRING];
  char path[_POSIX_PATH_MAX];
  struct stat s;
  struct timeval t[2];
  
  if (mbox->path == NULL)
    return 0;

  strfcpy (path, mbox->path, sizeof (path));
  gbuffy_expand_path (path, sizeof (path));
/*  g_print ("-I- Trying mbox %s\n", path); */

  if (stat (path, &s) != 0)
  {
    mbox->file_size = 0;
    mbox->file_mtime = 0;
    if (mbox->num_messages)
    {
      mbox->num_messages = 0;
      mbox->new_messages = 0;
      return 1;
    }
    return 0;
  }
  
  if (!force && (s.st_size == mbox->file_size) && (s.st_mtime == mbox->file_mtime))
    return 0;

  if ((s.st_size == 0) || (!S_ISREG(s.st_mode))) 
  {
    mbox->file_size = 0;
    mbox->file_mtime = 0;
    mbox->num_messages = 0;
    mbox->new_messages = 0;
    return 1;
  }


  if ((fp = fopen (path, "r")) == NULL)
    return 0;

  /* Check if a folder by checking for From as first thing in file */
  fgets(buffer, sizeof (buffer), fp);
  if (!is_from(buffer, garbage, sizeof (garbage))) 
    return 0;

  mbox->num_messages = 1;
  mbox->new_messages = 0;
  mbox->file_mtime = s.st_mtime;
  mbox->file_size = s.st_size;

  if (parse_mime_header (mbox, headers, fp, 0)) 
  {
    mbox->new_messages++;
  }
  while (fgets (buffer, sizeof (buffer), fp) != 0)
  {
    if (is_from (buffer, garbage, sizeof (garbage)))
    {
      mbox->num_messages++;
      if (parse_mime_header (mbox, headers, fp, 0))
      {
	mbox->new_messages++;
      }
    } 
  }
  fclose (fp);

  /* Restore the access time of the mailbox for other checking programs */
  t[0].tv_sec = s.st_atime;
  t[0].tv_usec = 0;
  t[1].tv_sec = s.st_mtime;
  t[1].tv_usec = 0;

  utimes (path, t);
  return 1;
}

static void count_mh_sequences(BOX_INFO *mbox, GList *headers, FILE *fp)
{
  char *buff = NULL;
  char *t;
  char *p;
  size_t sz = 0;
  int first, last;
  char file[_POSIX_PATH_MAX];
  FILE *msg_fp;

  while ((buff = read_rfc822_line (fp, buff, &sz)))
  {
    if (!(t = strtok (buff, " \t:")))
      continue;
    
    if (strcmp (t, "unseen"))
      continue;
    
    while ((t = strtok (NULL, " \t:")))
    {
      if ((p = strchr(t, '-')))
      {
	*p++ = '\0';
	first = atoi(t);
	last = atoi(p);
      }
      else
      {
	first = last = atoi (t);
      }
      for (; first <= last; first++)
      {
	mbox->new_messages++;
	if (headers)
	{
	  snprintf (file, sizeof (file), "%s/%d", mbox->path, first);
	  msg_fp = fopen (file, "r");
	  if (msg_fp != NULL) 
	  {
	    parse_mime_header (mbox, headers, msg_fp, 1);
	    fclose(msg_fp);
	  }
	}
      }
    }
  }
  if (buff != NULL)
    free(&buff);
}

int dir_folder_count (BOX_INFO *mbox, int force, GList *headers)
{
  DIR *dp = 0;
  FILE *fp = 0;
  char path[_POSIX_PATH_MAX];
  char file[_POSIX_PATH_MAX];
  int mailfile = TRUE;
  struct dirent *de;
  struct stat s;
  struct stat ms;
  struct timeval t[2];
  time_t sylpheed_t = 0;

  if (mbox->path == NULL)
    return 0;

  if (mbox->type == GB_MAILDIR) 
  {
    snprintf (path, sizeof (path), "%s/new", mbox->path);
  } 
  else 
  {
    strfcpy (path, mbox->path, sizeof (path));
  }
  gbuffy_expand_path (path, sizeof (path));

  if (stat (path, &s) != 0)
  {
    mbox->file_size = 0;
    mbox->file_mtime = 0;
    if (mbox->num_messages)
    {
      mbox->num_messages = 0;
      mbox->new_messages = 0;
      return 1;
    }
    return 0;
  }
  
  if (!force && (s.st_size == mbox->file_size) && (s.st_mtime == mbox->file_mtime))
    return 0;

  if (mbox->type == GB_MH) 
  {
    snprintf(file, sizeof(file), "%s/.sylpheed_mark", mbox->path);
    if (stat(file, &ms) == 0)
    {
      sylpheed_t = ms.st_mtime;
    }
  }

  mbox->num_messages = 0;
  mbox->new_messages = 0;

  dp = opendir (path);
  if (dp == NULL)
    return 0;

  while ((de = readdir (dp)) != NULL)
  {
    mailfile = TRUE;
    if (mbox->type == GB_MH) 
    {
      char *p = NULL;

      strtol(de->d_name, &p, 10);
      if (p && *p != '\0')
	mailfile = FALSE;
    } 
    else if (mbox->type == GB_MAILDIR) 
    {
      if (*de->d_name == '.') 
	mailfile = FALSE;
    }
    if (mailfile)
    {
      mbox->num_messages++;
      if (mbox->type == GB_MH)
      {
	/* If this is a sylpheed_t mh folder, then anything newer than
	 * the .sylpheed_mark file is new... */
	if (sylpheed_t) 
	{
	  snprintf (file, sizeof (file), "%s/%s", path, de->d_name);
	  if (stat(file, &ms) == 0)
	  {
	    if (ms.st_mtime > sylpheed_t)
	      mbox->new_messages++;
	  }
	}
      }
      else if (mbox->type == GB_MAILDIR)
      {
	/* For maildir, if we aren't getting headers, we just count
	 * everything in this directory as new */
	mbox->new_messages++;
	if (headers != NULL)
	{
	  /* Ok, we need to get the From: and Subject: lines */
	  snprintf (file, sizeof (file), "%s/%s", path, de->d_name);
	  fp = fopen (file, "r");
	  if (fp != NULL) 
	  {
	    parse_mime_header (mbox, headers, fp, 1);
	    fclose(fp);
	  }
	}
      }
    }
  }
  closedir(dp);
  if (mbox->type == GB_MH && !sylpheed_t)
  {
    snprintf(file, sizeof (file),  "%s/.mh_sequences", mbox->path);
    fp = fopen(file, "r");
    if (fp != NULL)
    {
      count_mh_sequences(mbox, headers, fp);
      fclose(fp);
    }
  }

  /* Restore the access time of the mailbox for other checking programs */
  t[0].tv_sec = s.st_atime;
  t[0].tv_usec = 0;
  t[1].tv_sec = s.st_mtime;
  t[1].tv_usec = 0;

  utimes (path, t);

  return 1;
}

int external_folder_count (BOX_INFO *mbox, int force, GList *headers)
{
  return 0;
}

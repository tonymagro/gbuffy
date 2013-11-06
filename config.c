/*
 * config.c
 */

#include "gbuffy.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/stat.h>
#include <proplist.h>

static char *DefaultsFile; /* = "GBuffy";*/

static char *get_defaults_file ();

static proplist_t pl_get_dict_entry (proplist_t dict, char *key)
{
  proplist_t pkey;
  proplist_t entry;

  pkey = PLMakeString (key);
  entry = PLGetDictionaryEntry (dict, pkey);
  PLRelease (pkey);

  return entry;
}

static proplist_t pl_set_dict_entry (proplist_t dict, char *key, char *entry)
{
  proplist_t pkey;
  proplist_t pentry;
  proplist_t newdict;

  pkey = PLMakeString (key);
  pentry = PLMakeString (entry);
  newdict = PLInsertDictionaryEntry (dict, pkey, pentry);
  PLRelease (pkey);
  PLRelease (pentry);

  return newdict;
}

static char *configure_load_path_with_default (proplist_t dict, char *var, 
                                                 char *defval)
{
  proplist_t val;
  char *s;
  char path[_POSIX_PATH_MAX];

  val = pl_get_dict_entry (dict, var);
  if (val)
  {
    strfcpy (path, PLGetString (val), sizeof (path));
  }
  else
  {
    strfcpy (path, defval, sizeof (path));
  }

  s = safe_strdup (gbuffy_expand_path (path, sizeof (path)));
  return s;
}

static char *configure_load_string_with_default (proplist_t dict, char *var, 
                                                 char *defval)
{
  proplist_t val;
  char *s;

  val = pl_get_dict_entry (dict, var);
  if (val)
  {
    s = safe_strdup (PLGetString (val));
  }
  else
  {
    s = safe_strdup (defval);
  }

  return s;
}

static char *configure_load_string (proplist_t dict, char *var)
{
  proplist_t val;
  char *s;

  val = pl_get_dict_entry (dict, var);
  if (val)
  {
    s = safe_strdup (PLGetString (val));
  }
  else
  {
    s = NULL;
  }

  return s;
}

void gbuffy_save_conf ()
{
  proplist_t top;
  proplist_t file;
  proplist_t boxdict;
  proplist_t boxes;
  proplist_t mailboxes = PLMakeString ("mailboxes");
  proplist_t type = PLMakeString ("type");
  proplist_t title = PLMakeString ("title");
  proplist_t command = PLMakeString ("command");
  proplist_t path = PLMakeString ("path");
  proplist_t server = PLMakeString ("server");
  proplist_t port = PLMakeString ("port");
#ifdef HAVE_SSL
  proplist_t ssl = PLMakeString ("ssl");
#endif
  proplist_t login = PLMakeString ("login");
  proplist_t newsrc = PLMakeString ("newsrc");
  proplist_t tmp;
  BOX_INFO *box;
  char buf[STRING];

  top = PLMakeDictionaryFromEntries (NULL, NULL);
  boxes = PLMakeArrayFromElements(NULL);

  box = MailboxInfo;
  while (box != NULL)
  {
    boxdict = PLMakeDictionaryFromEntries (NULL, NULL);
    switch (box->type)
    {
      case GB_MMDF:
	tmp = PLMakeString ("mmdf");
	break;
      case GB_MAILDIR:
	tmp = PLMakeString ("maildir");
	break;
      case GB_MH:
	tmp = PLMakeString ("mh");
	break;
      case GB_IMAP:
	tmp = PLMakeString ("imap");
	break;
      case GB_NNTP:
	tmp = PLMakeString ("nntp");
	break;
      case GB_EXTERNAL:
	tmp = PLMakeString ("external");
	break;
      default:
	tmp = PLMakeString ("mbox");
	break;
    }

    boxdict = PLInsertDictionaryEntry (boxdict, type, tmp);
    PLRelease(tmp);
    tmp = PLMakeString (NONULL(box->title));
    boxdict = PLInsertDictionaryEntry (boxdict, title, tmp);
    PLRelease(tmp);
    tmp = PLMakeString (NONULL(box->path));
    boxdict = PLInsertDictionaryEntry (boxdict, path, tmp);
    PLRelease(tmp);
    tmp = PLMakeString (NONULL(box->command));
    boxdict = PLInsertDictionaryEntry (boxdict, command, tmp);
    PLRelease(tmp);
    if (box->server || box->login)
    {
      char buf[SHORT_STRING];

      if (box->server && box->server[0])
      {
	tmp = PLMakeString (box->server);
	boxdict = PLInsertDictionaryEntry (boxdict, server, tmp);
	PLRelease (tmp);
      }
      snprintf (buf, sizeof (buf), "%d", box->port);
      tmp = PLMakeString (buf);
      boxdict = PLInsertDictionaryEntry (boxdict, port, tmp);
      PLRelease (tmp);
#ifdef HAVE_SSL
      snprintf (buf, sizeof (buf), "%d", box->ssl_mode);
      tmp = PLMakeString (buf);
      boxdict = PLInsertDictionaryEntry (boxdict, ssl, tmp);
      PLRelease (tmp);
#endif
      if (box->login && box->login[0])
      {
	tmp = PLMakeString (box->login);
	boxdict = PLInsertDictionaryEntry (boxdict, login, tmp);
	PLRelease (tmp);
      }
    }
    if (box->newsrc && box->newsrc[0])
    {
      tmp = PLMakeString (NONULL (box->newsrc));
      boxdict = PLInsertDictionaryEntry (boxdict, newsrc, tmp);
      PLRelease (tmp);
    }

    boxes = PLAppendArrayElement (boxes, boxdict);

    box = box->next;
  }

  top = PLInsertDictionaryEntry (top, mailboxes, boxes);
  top = pl_set_dict_entry (top, "vertical", (Vertical ? "true" : "false"));
  snprintf (buf, sizeof (buf), "%d", PollTime);
  top = pl_set_dict_entry (top, "polltime", buf);
  snprintf (buf, sizeof (buf), "%d", Rows);
  top = pl_set_dict_entry (top, "rows", buf);
  top = pl_set_dict_entry (top, "maildir", NONULL(Maildir));

  file = PLMakeString (get_defaults_file());

  top = PLSetFilename (top, file);
  PLSave (top, TRUE);
  PLRelease(type);
  PLRelease(title);
  PLRelease(path);
  PLRelease(command);
  PLRelease(mailboxes);
  PLRelease(file);
  PLRelease(server);
  PLRelease(port);
#ifdef HAVE_SSL
  PLRelease(ssl);
#endif
  PLRelease(login);
  PLRelease(newsrc);
}

BOX_INFO *configure_load_boxes (proplist_t boxes)
{
  proplist_t boxdict;
  proplist_t type = PLMakeString ("type");
  proplist_t title = PLMakeString ("title");
  proplist_t command = PLMakeString ("command");
  proplist_t path = PLMakeString ("path");
  proplist_t tmp;
  BOX_INFO *box = NULL, *ret = NULL;
  char *s;
  unsigned int x, num;

  if (PLIsArray (boxes))
  {
    num = PLGetNumberOfElements (boxes);
    x = 0;
    while (x < num)
    {
      if (box == NULL)
      {
	ret = box = (BOX_INFO *) calloc (1, sizeof (BOX_INFO));
      } 
      else
      {
	box->next = (BOX_INFO *) calloc (1, sizeof (BOX_INFO));
	box = box->next;
      }
      boxdict = PLGetArrayElement (boxes, x);
      tmp = PLGetDictionaryEntry (boxdict, type);
      if (tmp != NULL)
      {
	s = PLGetString (tmp);
	if (!strcmp (s, "mbox"))
	  box->type = GB_MBOX;
	else if (!strcmp (s, "mmdf"))
	  box->type = GB_MMDF;
	else if (!strcmp (s, "maildir"))
	  box->type = GB_MAILDIR;
	else if (!strcmp (s, "mh"))
	  box->type = GB_MH;
	else if (!strcmp (s, "imap"))
	  box->type = GB_IMAP;
	else if (!strcmp (s, "nntp"))
	  box->type = GB_NNTP;
	else if (!strcmp (s, "external"))
	  box->type = GB_EXTERNAL;
      }
      tmp = PLGetDictionaryEntry (boxdict, path);
      if (tmp != NULL)
	box->path = safe_strdup (PLGetString (tmp));
      else
	box->path = safe_strdup ("mailbox");
      box->path = configure_load_string_with_default (boxdict, "path",
	  "mailbox");
      box->title = configure_load_string_with_default (boxdict, "title",
	  box->path);
      box->command = configure_load_string_with_default (boxdict, "command",
	  "mutt");
      box->server = configure_load_string (boxdict, "server");
      box->login = configure_load_string (boxdict, "login");
      if (box->type == GB_NNTP)
	box->newsrc = configure_load_path_with_default (boxdict, "newsrc", "~/.newsrc");
#ifdef HAVE_SSL
      s = configure_load_string (boxdict, "ssl");
      if (s)
      {
        box->ssl_mode = atoi(s);
        free (s);
      } else {
	box->ssl_mode = 0;
      }
#endif
      box->port = 0;
      s = configure_load_string (boxdict, "port");
      if (s)
      {
	box->port = atoi (s);
	free (s);
      }
      if (box->port == 0)
      {
	if (box->type == GB_IMAP)
	{
#ifdef HAVE_SSL
	  if (box->ssl_mode)
	    box->port = IMAPS_PORT;
	  else
	    box->port = IMAP_PORT;
#else
	  box->port = IMAP_PORT;
#endif
	}
	else if (box->type == GB_NNTP)
	{
	  box->port = NNTP_PORT;
	}
      }
      x++;
    }
  }
  else
  {
    fprintf (stderr, "-E- Hmm, the defaults file is strange, bailing\n");
    return NULL;
  }

  PLRelease(type);
  PLRelease(title);
  PLRelease(path);
  PLRelease(command);

  return ret;
}

BOX_INFO *gbuffy_configure_load ()
{
  proplist_t top;
  proplist_t boxes;
  proplist_t tmp;
  BOX_INFO *box;
  char *s;

  DefaultsFile = get_defaults_file ();
  top = PLGetProplistWithPath (DefaultsFile);
  if (top == NULL)
  {
    char path[_POSIX_PATH_MAX];

    fprintf (stderr, "-E- No defaults\n");
    strcpy (path, "~/Mail");
    Maildir = safe_strdup(gbuffy_expand_path (path, sizeof (path)));

    return NULL;
  }
  if (PLIsDictionary (top))
  {
    boxes = pl_get_dict_entry (top, "mailboxes");


    tmp = pl_get_dict_entry (top, "vertical");
    if (tmp)
    {
      s = PLGetString (tmp);
      if (!strcmp (s, "true"))
	Vertical = TRUE;
    }
    tmp = pl_get_dict_entry (top, "polltime");
    if (tmp)
    {
      s = PLGetString (tmp);
      sscanf (s, "%d", &PollTime);
    }
    tmp = pl_get_dict_entry (top, "rows");
    if (tmp)
    {
      s = PLGetString (tmp);
      sscanf (s, "%d", &Rows);
    }
    Maildir = configure_load_path_with_default (top, "maildir", "~/Mail");

    box = configure_load_boxes (boxes);
  }
  else 
  {
    fprintf (stderr, "-E- Hmm, the defaults file is strange, bailing\n");
    return NULL;
  }

  PLRelease(top);
  return box;
}

static char *get_defaults_file ()
{
  char path[_POSIX_PATH_MAX];
  char *defaults_dir;
  struct stat s;

  defaults_dir = getenv ("GNUSTEP_USER_PATH");
  if (defaults_dir == NULL)
    defaults_dir = "~/GNUstep";

  strfcpy (path, defaults_dir, sizeof (path));
  gbuffy_expand_path (path, sizeof (path));
  if (stat (path, &s) == -1)
  {
    if (errno != ENOENT)
    {
      fprintf (stderr, "-E- Error accessing path %s\n", path);
      perror ("stat");
    }
    strfcpy (path, "~/.gbuffyrc", sizeof (path));    
    gbuffy_expand_path (path, sizeof(path));     
    return safe_strdup(path);
  }
  else
  {
    strncat (path, "/Defaults", sizeof (path) - strlen (path));
    if (stat (path, &s) == -1)
    {
      if (errno != ENOENT)
      {
	fprintf (stderr, "-E- Error accessing path %s\n", path);
	perror ("stat");
      }
      else
      {
	if (mkdir (path, S_IRUSR|S_IWUSR|S_IXUSR|S_IRGRP|S_IXGRP|S_IROTH|S_IXOTH) == -1)
	{
	  fprintf (stderr, "-E- Error creating directory %s\n", path);
	  perror ("mkdir");
	}
      }
    }
    strncat (path, "/GBuffy", sizeof (path) - strlen (path));
  }
  return safe_strdup (path);
}

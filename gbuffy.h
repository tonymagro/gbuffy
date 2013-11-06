/*
 * gbuffy.h
 */

#ifndef _GBUFFY_H_
#define _GBUFFY_H_ 1

#include "config.h"

#include <stdio.h>
#include <sys/types.h>
#include <gtk/gtk.h>

#define IMAP_PORT 143
#define IMAPS_PORT 993
#define NNTP_PORT 119

#define SHORT_STRING    128
#define STRING          256
#define LONG_STRING    1024
#define HUGE_STRING    5120

/* What to show when a message has no subject */
#define NO_SUBJECT_STRING "<no subject>"

enum BOXTYPE {UNKNOWN = -1, GB_MBOX = 0, GB_MMDF, GB_MAILDIR, GB_MH, GB_IMAP, 
  GB_NNTP, GB_EXTERNAL, GB_MAX};

enum {
  GB_DETACH_PROCESS = 1
};

typedef struct _box_info
{
  GtkWidget *w;
  GtkWidget *button;
  GdkPixmap *pixmap;
  int selected;
  int leave;          /* denotes if mouse left window before release */

  enum BOXTYPE type;
  char *title;
  char *path;
  char *command;
  char *server;
  int port;
#ifdef HAVE_SSL
  int ssl_mode;
#endif
  char *login;
  char *pass;
  char *newsrc;

  GList *headers;
  int face;		/* denotes if headers list contains an X-Face: */

  time_t file_mtime;
  off_t file_size;

  int new_messages;
  int num_messages;

  int height;
  int display_num;
  int skip_num;

  struct _box_info *next;
} BOX_INFO;

typedef struct _box_class
{
  char *name;
  int (*count)(struct _box_info *, int, GList *);
/* void (*browse)(struct _box_info *); */
  int server;
} BOX_CLASS;

typedef struct _message_info
{
  char *from;
  char *subject;
  char *face;
} MESSAGE_INFO;


/* Global Variables */
extern int Vertical;
extern char *Username;
extern char *DefaultNewserver;
extern char *Homedir;
extern char *Maildir;
extern int PollTime;
extern int Rows;
extern BOX_INFO *MailboxInfo;
extern BOX_CLASS MailboxClass[];
#ifdef DEBUG
extern FILE *debugfile;
extern int debuglevel;
#endif
extern gint Width;
extern gint Height;


#define FOREVER while (1)
#define NONULL(x) x?x:""
#define FREE(x) safe_free((void **)x)
#define SKIPWS(c) while (*(c) && isspace ((unsigned char) *(c))) c++;
#define ISSPACE(c) isspace((unsigned char)c)
#define strfcpy(A,B,C) strncpy(A,B,C), *(A+(C)-1)=0
#ifdef DEBUG
#define dprint(N,X) if(debuglevel>=N) fprintf X
#else
#define dprint(N,X) 
#endif

int mbox_folder_count (BOX_INFO *mbox, int force, GList *headers);
int dir_folder_count (BOX_INFO *mbox, int force, GList *headers);
int imap_folder_count (BOX_INFO *mbox, int force, GList *headers);
int nntp_folder_count (BOX_INFO *mbox, int force, GList *headers);
int external_folder_count (BOX_INFO *mbox, int force, GList *headers);

void gbuffy_configure_dialog ();
BOX_INFO *gbuffy_configure_load ();
void gbuffy_display ();
void gbuffy_save_conf ();


/* from.c */
int is_from (const char *s, char *path, size_t pathlen);

/* lib.c */
void *safe_calloc (size_t nmemb, size_t size);
void safe_free (void **p);
char *safe_strdup (const char *s);
char *gbuffy_expand_path (char *s, size_t slen);
void safe_realloc (void **p, size_t siz);
char *read_rfc822_line (FILE *f, char *line, size_t *linelen);

/* nntp.c */
int nntp_get_status (char *group, char *npath, int article_num);
int nntp_last_read (char *group, char *npath);


/* system.c */
int gbuffy_system (const char *cmd, int flags);
#endif /* _GBUFFY_H_ */

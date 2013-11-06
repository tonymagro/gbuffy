/*
 * gbuffy.c
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <gtk/gtk.h>
#include <pwd.h>
#include <string.h>
#include <compface.h>

#include "gbuffy.h"

#define NEGATIVE -1
#define POSITIVE 1

#ifdef GNOME_APPLET
#include <applet-widget.h>
#endif

char *Homedir;
char *Username;
char *Maildir = NULL;
char *Spooldir = "/var/spool/mail";
char *DefaultNewserver;
int Vertical = FALSE;
int PollTime = 10;
int PollId = 0;
int DontExit = FALSE;
int Rows = 1;

struct 
{
  gint value;
  gint sign;
}  HorizPos, VertPos;

gint Width = -1;
gint Height = -1;
gint ScrH;
gint ScrW;
GtkWidget *MainWindow = NULL;

#ifdef DEBUG
FILE *debugfile = NULL;
int debuglevel = 0;
#endif

/* This might be an option someday */
int NoHeaderStay = TRUE;

BOX_INFO *MailboxInfo;
/* This currently needs to be in the same order as enum BOXTYPE */
BOX_CLASS MailboxClass[] =
{
  { "mbox",     mbox_folder_count,     0 },
  { "mmdf",     mbox_folder_count,     0 },
  { "maildir",  dir_folder_count,      0 },
  { "mh",       dir_folder_count,      0 },
  { "imap",     imap_folder_count,     1 },
  { "nntp",     nntp_folder_count,     1 },
  { "external", external_folder_count, 0 }
};

void gbuffy_exit (GtkWidget *widget, gpointer data)
{
  if (!DontExit)
    gtk_main_quit ();
  DontExit = FALSE;
}

gint gbuffy_display_expose (GtkWidget *da, GdkEventExpose *event, BOX_INFO *box)
{

  if (box->pixmap)
  {
    gdk_draw_pixmap (da->window, da->style->fg_gc[GTK_WIDGET_STATE (da)],
	box->pixmap, event->area.x, event->area.y, event->area.x, event->area.y,
	event->area.width, event->area.height); 
  }
  return FALSE;
}

GdkPixmap * pixmap_from_ikon (GdkWindow *w, char *buf)
{
  char data[300];
  unsigned int a = 0, b = 0, c = 0;
  int x = 0;
  char *line;
  char *p;

  line = buf;
  while (line)
  {
    p = strchr (line, '\n');
    if (p)
    {
      *p = '\0';
      p++;
    }
#define LO_BYTE(y) (0x00ff & y)
#define HI_BYTE(y) ((0x0ff00 & y)>>8)
#define SWITCH(y) (((1 & y) << 7) | ((2 & y) << 5) | \
                   ((4 & y) << 3) | ((8 & y) << 1) | \
		   ((16 & y) >> 1) | ((32 & y) >> 3) | \
		   ((64 & y) >> 5) | ((128 & y) >> 7))

    sscanf (line, "%i,%i,%i", &a, &b, &c);

    /* You'd think there would be an easier way, but apparently we 
     * have to flip the bits. */
    data[x++] = SWITCH(HI_BYTE(a));
    data[x++] = SWITCH(LO_BYTE(a));
    data[x++] = SWITCH(HI_BYTE(b));
    data[x++] = SWITCH(LO_BYTE(b));
    data[x++] = SWITCH(HI_BYTE(c));
    data[x++] = SWITCH(LO_BYTE(c));
    line = p;
  }

  /*
  for (a = 0; a < 48; a++)
  {
    printf ("%0x, %0x, %0x, %0x, %0x, %0x\n", 
	    data[a * 6 + 0],
	    data[a * 6 + 1],
	    data[a * 6 + 2],
	    data[a * 6 + 3],
	    data[a * 6 + 4],
	    data[a * 6 + 5]);
  }
  */
  return gdk_bitmap_create_from_data (w, data, 48, 48);
}

#include <gdk/gdkprivate.h>
void
gdk_draw_bitmap (GdkDrawable *drawable,
                 GdkGC       *gc,
                 GdkPixmap   *src,
                 gint         xsrc,
                 gint         ysrc,
                 gint         xdest,
                 gint         ydest,
                 gint         width,
                 gint         height)
{
  GdkWindowPrivate *drawable_private;
  GdkWindowPrivate *src_private;
  GdkGCPrivate *gc_private;
  g_return_if_fail (drawable != NULL);
  g_return_if_fail (src != NULL);
  g_return_if_fail (gc != NULL);

  drawable_private = (GdkWindowPrivate*) drawable;
  src_private = (GdkWindowPrivate*) src;
  if (drawable_private->destroyed || src_private->destroyed)
    return;
  gc_private = (GdkGCPrivate*) gc;

  if (width == -1)
    width = src_private->width;
  if (height == -1)
    height = src_private->height;

  XCopyPlane (drawable_private->xdisplay,
             src_private->xwindow,
             drawable_private->xwindow,
             gc_private->xgc,
             xsrc, ysrc,
             width, height,
             xdest, ydest, 1);
}

gint configure_event (GtkWidget *da, GdkEventConfigure *event, BOX_INFO *box)
{
  GList *headers;
  int y = box->height;
  int x, skip = 0;
  int face_display = FALSE;
  int any_display = FALSE;
  GdkPixmap *black;
  GdkGC *mask_gc;

  if (da->allocation.width == 1)
    return FALSE;

  if (box->pixmap)
    gdk_pixmap_unref (box->pixmap);

  box->pixmap = gdk_pixmap_new (da->window, da->allocation.width, 
      da->allocation.height, -1);

  black = gdk_pixmap_new (da->window, 48, 48, -1);
  gdk_draw_rectangle (black, da->style->black_gc, TRUE, 0, 0, 47, 47);
  mask_gc = gdk_gc_new (da->window);
  gdk_gc_copy (mask_gc, da->style->light_gc[GTK_WIDGET_STATE (da)]);

  gdk_draw_rectangle (box->pixmap, da->style->bg_gc[GTK_WIDGET_STATE (da)],
      TRUE, 0, 0, da->allocation.width, da->allocation.height);
  headers = g_list_first (box->headers);
  x = 2 + (50 * box->face);
  while (headers)
  {
    if (headers->data)
    {
      MESSAGE_INFO *m = (MESSAGE_INFO *)(headers->data);

      face_display = FALSE;
      any_display = FALSE;
      if (m->face)
      {
	if (skip < box->skip_num) skip++;
	else
	{
	  char buf[2048];
	  GdkPixmap *face;

	  strfcpy (buf, m->face, sizeof (buf));
	  if (uncompface (buf) >= 0)
	  {
	    face = pixmap_from_ikon (box->pixmap, buf); 
	    gdk_gc_set_clip_mask (mask_gc, face); 
	    gdk_gc_set_clip_origin (mask_gc, 2, y - box->height + 3);
	    gdk_draw_pixmap (box->pixmap, mask_gc, black, 0, 0, 2, y - box->height+3, -1, -1);
	    /*
	       gdk_draw_bitmap (box->pixmap, 
	       da->style->light_gc[GTK_WIDGET_STATE (da)],
	       face, 0, 0, 2, y - box->height + 3, -1, -1);
	     */
	    gdk_gc_set_clip_mask (mask_gc, NULL); 
	    gdk_pixmap_unref (face); 
	    face_display = TRUE;
	  }
	}
      }
      if (m->from)
      {
	if (skip < box->skip_num) skip++;
	else
	{
	  any_display = TRUE;
	  gdk_draw_string (box->pixmap, da->style->font, 
	      da->style->fg_gc[GTK_WIDGET_STATE (da)], 
	      x, y + (face_display * 3), 
	      m->from);
	  y += box->height;
	}
      }
      if (m->subject)
      {
	if (skip < box->skip_num) skip++;
	else
	{
	  any_display = TRUE;
	  gdk_draw_string (box->pixmap, da->style->font, 
	      da->style->fg_gc[GTK_WIDGET_STATE (da)], 
	      x, y + (face_display * 3), 
	      m->subject);
	  y += box->height;
	}
      }
      if (face_display)
	y += box->height;
      if (face_display || any_display)
      {
	gdk_draw_line (box->pixmap, da->style->dark_gc[GTK_WIDGET_STATE (da)],
	    /*	  (2 + (50 * !face_display)), y - box->height + 3,  */
	    x, y - box->height + 3,  
	    da->allocation.width - 18, 
	    y - box->height + 3);
      }
    }
    headers = headers->next;
  }

  return TRUE;
}

gint populate_clist (GtkWidget *clist, BOX_INFO *box)
{
  GList *headers;
  int x;
  char buf[STRING];
  int row = 0;
  GdkPixmap *black;


  black = gdk_pixmap_new (NULL, 48, 48, gdk_visual_get_best()->depth);

  headers = g_list_first (box->headers);
  x = 2 + (50 * box->face);
  while (headers)
  {
    if (headers->data)
    {
      MESSAGE_INFO *m = (MESSAGE_INFO *)(headers->data);

      gtk_clist_append (GTK_CLIST (clist), NULL);
      if (m->face)
      {
	char buf[2048];
	GdkPixmap *face;

	strfcpy (buf, m->face, sizeof (buf));
	if (uncompface (buf) >= 0)
	{
	  face = pixmap_from_ikon (box->pixmap, buf); 

	  snprintf (buf, sizeof (buf), "%s\n%s", m->from, m->subject);
	  gtk_clist_set_pixtext (GTK_CLIST (clist), row, 0, buf, 2, black, 
	      face);
	     
	  gdk_pixmap_unref (face); 
	}
	else 
	{
	  snprintf (buf, sizeof (buf), "%s\n%s", m->from, m->subject);
	  gtk_clist_set_text (GTK_CLIST (clist), row, 0, buf);
	}
      }
      else
      {
	snprintf (buf, sizeof (buf), "%s\n%s", m->from, m->subject);
	gtk_clist_set_text (GTK_CLIST (clist), row, 0, buf);
      }
    }
    headers = headers->next;
    row++;
  }

  return TRUE;
}

void gbuffy_display_box (BOX_INFO *box, int xpos, int ypos)
{
  GtkWidget *da;
  GList *headers;
  char buf[STRING];
  int height = 16, width = 0;
  int x, num = 0;
  int change;
  int screen_height;

  screen_height = gdk_screen_height();

  if (box->w == NULL)
  {
    headers = g_list_alloc ();

    change = (*(MailboxClass[box->type].count))(box, 1, headers);
    if (change)
    {
      char buf[STRING];

      snprintf (buf, sizeof (buf), "%s: %d", box->title, box->new_messages);
      gtk_label_set (GTK_LABEL (GTK_BUTTON (box->button)->child), buf); 
      if (box->new_messages)
	gtk_widget_set_state (box->button, GTK_STATE_SELECTED);
    }
    if (box->new_messages == 0)
      return;

    box->w = gtk_window_new (GTK_WINDOW_DIALOG);
    snprintf (buf, sizeof (buf), "%s:  %d / %d", box->title, box->new_messages, 
	box->num_messages);
    gtk_window_set_title (GTK_WINDOW (box->w), buf);
    gtk_window_position (GTK_WINDOW (box->w), GTK_WIN_POS_MOUSE);
    gtk_widget_realize (box->w);
    gtk_widget_set_uposition (box->w, xpos+5, ypos+5);
/*    headers = g_list_first (headers);  */
    box->headers = headers->next;
    if (headers->next)
    {
      g_list_free_1 (headers);
      headers = box->headers;
      headers->prev = NULL;
      while (headers)
      {
	if (headers->data)
	{
	  MESSAGE_INFO *m = (MESSAGE_INFO *)(headers->data);
	  if (m->from)
	  {
	    x = gdk_string_measure (box->w->style->font, m->from);
	    if (x > width)
	      width = x;
	    num++;
	  }
	  if (!(m->subject))
	  {
	    m->subject = safe_strdup(NO_SUBJECT_STRING);
	  }
	  if (m->subject)
	  {
	    x = gdk_string_measure (box->w->style->font, m->subject);
	    if (x > width)
	      width = x;
	    num++;
	  }
	  if (m->face)
	  {
	    box->face = TRUE;
	    num++;
	  }
	  /*
	     x = gdk_string_height (box->w->style->font, (char *)headers->data);
	     if (x > height)
	     height = x;
	   */
	}
	headers = headers->next;
      }
    }
    box->height = height;
    da = gtk_drawing_area_new (); 
    box->display_num = num;
    box->skip_num = 0;
    if (height * num > screen_height)
    {
      num = (screen_height / height) - 3;
    }
    if (num != box->display_num)
    {
      box->skip_num = box->display_num - num;
      box->display_num = num;
    }
    gtk_drawing_area_size (GTK_DRAWING_AREA (da), (box->face * 50) + width + 20,
	(height) * num + 4);
    gtk_signal_connect (GTK_OBJECT (da), "expose_event",
	(GtkSignalFunc) gbuffy_display_expose, box); 
    gtk_widget_set_events (da, GDK_EXPOSURE_MASK);
    gtk_container_add (GTK_CONTAINER (box->w), da); 
    gtk_widget_show (da);
    gtk_signal_connect (GTK_OBJECT(da),"configure_event",
	(GtkSignalFunc) configure_event, box);
    gtk_widget_set_usize (box->w, (box->face * 50) + width + 20,
	(height) * num + 4);
    /*
    clist = gtk_clist_new (1);
    gtk_clist_set_policy (GTK_CLIST (clist), GTK_POLICY_AUTOMATIC,
	            GTK_POLICY_AUTOMATIC);
    gtk_clist_set_column_width (GTK_CLIST (clist), 0, (box->face * 50) + width + 20);
    if (box->face)
      gtk_clist_set_row_height (GTK_CLIST (clist), 50);
    gtk_clist_freeze (GTK_CLIST (clist));
    populate_clist (clist, box);
    gtk_clist_thaw (GTK_CLIST (clist));
    gtk_container_add (GTK_CONTAINER (box->w), clist); 
    gtk_widget_show (clist);
	*/
    gtk_widget_show (box->w);
  }
}

gint gbuffy_button_callback (GtkWidget *widget, GdkEventButton *event, 
    BOX_INFO *box)
{
  switch (event->button) {
  case 1:
    box->leave = FALSE;
    gbuffy_display_box (box, event->x_root, event->y_root);
    break;
  case 2:
    if (box->command != NULL)
      gbuffy_system (box->command, GB_DETACH_PROCESS);
    break;
  case 3:
#ifdef GNOME_APPLET
    return FALSE;
#else
    gtk_timeout_remove (PollId);
    gbuffy_configure_dialog ();
    break;
#endif
  default:
    break;
  }
  return TRUE;
}

#ifdef GNOME_APPLET
static void
gbuffy_properties_callback (AppletWidget* applet, gpointer data)
{
  gtk_timeout_remove(PollId);
  gbuffy_configure_dialog();
}
#endif

gint gbuffy_leave_box (GtkWidget *widget, GdkEventButton *event, 
    BOX_INFO *box)
{
  box->leave = TRUE;
  if (box->new_messages)
    gtk_widget_set_state (box->button, GTK_STATE_SELECTED);
  return TRUE;
}

gint gbuffy_delete_box (GtkWidget *widget, GdkEventButton *event, 
    BOX_INFO *box)
{

  if (event->button == 1)
  {
    if (box->w != NULL && (!box->leave || NoHeaderStay))
    {
      GList *headers, *tmp;

      headers = g_list_first (box->headers);
      while (headers)
      {
	if (headers->data)
	{
	  safe_free ((void **)&((MESSAGE_INFO *)(headers->data))->from);
	  safe_free ((void **)&((MESSAGE_INFO *)(headers->data))->subject);
	  free (headers->data);
	}
	tmp = headers->next;
	g_list_free_1 (headers);
	headers = tmp;
      }
      gtk_widget_hide (box->w);
      gtk_widget_destroy (box->w);
      box->w = NULL;
    }
  }
  return TRUE;
}

gint gbuffy_poll_boxes (gpointer data)
{
  BOX_INFO *mbox;
  int change;
  gint cur_width;
  gint cur_height;
  static unsigned int beenhere = 0;

  mbox = MailboxInfo;
  while (mbox != NULL)
  {
    change = (*(MailboxClass[mbox->type].count))(mbox, 0, NULL);
    if (change)
    {
      char buf[STRING];

      snprintf (buf, sizeof (buf), "%s: %d", mbox->title, mbox->new_messages);
      gtk_label_set (GTK_LABEL (GTK_BUTTON (mbox->button)->child), buf); 
      if (mbox->new_messages)
	gtk_widget_set_state (mbox->button, GTK_STATE_SELECTED);
      else
	gtk_widget_set_state (mbox->button, GTK_STATE_NORMAL);

    }
    mbox = mbox->next;
  }

  /* This is the wrong way to do this, but its quick and dirty.  I
   * should figure out what event I should capture to receive and update
   * this information, but instead we just update it here. */

  gdk_window_get_size(MainWindow->window, &cur_width, &cur_height);
  if (cur_width != Width)
    Width = cur_width;
  if (cur_height != Height)
    Height = cur_height;
  
  HorizPos.sign = POSITIVE;
  VertPos.sign = POSITIVE;

  if (beenhere)
    gdk_window_get_position (MainWindow->window, &HorizPos.value, &VertPos.value);
  else
    beenhere = 1;

  return TRUE;
}

#ifdef GNOME_APPLET
/*this is when the panel orientation changes*/
static void applet_change_orient(GtkWidget *w, PanelOrientType o, gpointer data)
{
  int oldVertical = Vertical;

  switch(o) {
    case ORIENT_UP:
    case ORIENT_DOWN:
      Vertical = FALSE;
      break;
    case ORIENT_LEFT:
    case ORIENT_RIGHT:
      Vertical = TRUE;
      break;
  }
  if(Vertical != oldVertical)
    gbuffy_display();
}

static void about_cb (AppletWidget* applet, gpointer data) 
{
  const gchar *authors[] = {
    "Brandon Long",
    "Tim Danner",
    NULL
  };
  GtkWidget* a;

  a = gnome_about_new ("GBuffy", VERSION,
      "Copyright 1999 Brandon Long",
      (const gchar **) authors,
      _("GBuffy monitors mail in many formats, including "
	"IMAP, plus newsgroups."),
      NULL);
  gtk_widget_show (a);
}
#endif

void gbuffy_display ()
{
  GtkWidget *box;
  BOX_INFO *mbox;
  char buf[STRING];
  int box_count = 0, columns, rows;
#ifdef GNOME_APPLET
  GList* children;
#endif

  gint cur_width;
  gint cur_height;
  gint new_horizontal;
  gint new_vertical;

#ifdef GNOME_APPLET
  if (MainWindow != NULL) {
    children = gtk_container_children (GTK_CONTAINER (MainWindow));
    gtk_widget_destroy (GTK_WIDGET (children->data));
  } else {
    MainWindow = applet_widget_new ("gbuffy");
    applet_widget_register_stock_callback(APPLET_WIDGET(MainWindow),
		  "properties",
		  GNOME_STOCK_MENU_PROP,
		  _("Properties..."),
		  gbuffy_properties_callback,
		  NULL);
    applet_widget_register_stock_callback(APPLET_WIDGET(MainWindow),
		  "about",
		  GNOME_STOCK_MENU_ABOUT,
		  _("About..."),
		  about_cb,
		  NULL);
  }
#else
  if (MainWindow != NULL)
  {
    DontExit = TRUE;
    gtk_widget_destroy (MainWindow);
  }
  MainWindow = gtk_window_new (GTK_WINDOW_TOPLEVEL);
#endif

  gtk_widget_set_name (MainWindow, "main MainWindow");

  gtk_signal_connect (GTK_OBJECT (MainWindow), "delete_event",
      GTK_SIGNAL_FUNC (gbuffy_exit), NULL);
  gtk_signal_connect (GTK_OBJECT (MainWindow), "destroy",
      GTK_SIGNAL_FUNC (gbuffy_exit), NULL);

  for (mbox = MailboxInfo; mbox; mbox = mbox->next)
  {
    box_count++;
  }

  rows = Rows;

  columns = box_count / rows;
  while (rows * columns < box_count) 
  {
    columns++;
  }
  if (Vertical)
  {
    int tmp;
    tmp = rows;
    rows = columns;
    columns = tmp;
  }

  box = gtk_table_new (rows, columns, TRUE);

#ifdef GNOME_APPLET
  gtk_signal_connect(GTK_OBJECT(MainWindow), "change_orient",
		     GTK_SIGNAL_FUNC(applet_change_orient),
		     NULL);
  applet_widget_add (APPLET_WIDGET (MainWindow), box);
#else
  gtk_container_add (GTK_CONTAINER (MainWindow), box);
#endif

  mbox = MailboxInfo;
  box_count = 0;
  while (mbox != NULL)
  {
    int x, y;
    /* (*(MailboxClass[mbox->type].count))(mbox, 1, NULL); */
    snprintf (buf, sizeof (buf), "%s: %d", mbox->title, mbox->new_messages);
    mbox->button = gtk_button_new_with_label (buf);

    if (mbox->new_messages)
      gtk_widget_set_state (mbox->button, GTK_STATE_SELECTED);

    //gtk_box_pack_start (GTK_BOX (box), mbox->button, TRUE, TRUE, TRUE);
    x = box_count / rows;
    y = box_count % rows;
    gtk_table_attach (GTK_TABLE (box), mbox->button,
		      x, x + 1, y, y + 1, 
		      GTK_FILL | GTK_SHRINK,
		      GTK_FILL | GTK_SHRINK,
		      0, 0);
    gtk_widget_show (mbox->button);
    gtk_signal_connect_after (GTK_OBJECT (mbox->button), "button_press_event",
	GTK_SIGNAL_FUNC (gbuffy_button_callback), mbox);
    gtk_signal_connect (GTK_OBJECT (mbox->button), "button_release_event",
	GTK_SIGNAL_FUNC (gbuffy_delete_box), mbox);
    gtk_signal_connect_after (GTK_OBJECT (mbox->button), "leave_notify_event",
	GTK_SIGNAL_FUNC (gbuffy_leave_box), mbox);
    mbox = mbox->next;
    box_count++;
  }

  gtk_widget_show (box);

  gtk_widget_show (MainWindow);
  gdk_window_get_size(MainWindow->window, &cur_width, &cur_height);
  Width = (Width == -1) ? cur_width : Width;
  Height = (Height == -1) ? cur_height : Height;
  gdk_window_resize(MainWindow->window, Width, Height);
  
  gbuffy_poll_boxes (NULL);

  PollId = gtk_timeout_add (PollTime * 1000, gbuffy_poll_boxes, NULL);

  if (!(HorizPos.sign != 0 && VertPos.sign != 0))
    return;
  new_horizontal = (HorizPos.sign == POSITIVE) ? HorizPos.value :
    ScrW - Width - HorizPos.value;
  new_vertical = (VertPos.sign == POSITIVE) ? VertPos.value :
    ScrH - Height - VertPos.value;
  gdk_window_move(MainWindow->window, new_horizontal, new_vertical);

}

void parse_geometry(int *argc, char ***argv)
{
  gint i, remove=0;
  gchar *p;

  HorizPos.value=-1;
  HorizPos.sign=0;
  VertPos.value=-1;
  VertPos.sign=0;

  ScrH = gdk_screen_height();
  ScrW = gdk_screen_width();

  for (i=0; i<*argc; i++) 
  {
    if (strcmp("--geometry", (*argv)[i]) == 0 ||
	strncmp("--geometry", (*argv)[i], 10) == 0 ||
	strcmp("-geometry", (*argv)[i]) == 0 ||
	strncmp("-geometry", (*argv)[i], 9) == 0) 
    {
      if((i+1) == *argc) 
      {  /* '-(-)geometry' was the _last_ argument */
	(*argv)[i] = NULL;
	remove = -1;
	break;
      }
      p = (*argv)[i+1];
      while(*p) 
      {
	switch(*p) 
	{
	  case 'x': 
	    Height = strtol(p + 1, &p, 10);
	    break;
	  case '-':
	  case '+':
	    if (HorizPos.value == -1) 
	    {
	      HorizPos.sign = (*p == '-') ? NEGATIVE : POSITIVE;
	      HorizPos.value = strtol(p+1, &p, 10);
	    }
	    else 
	    {
	      VertPos.sign = (*p == '-') ? NEGATIVE : POSITIVE;
	      VertPos.value = strtol(p+1, &p, 10);
	    }
	    break;
	  default:
	    Width = strtol(p, &p, 10);
	    break;
	}
      }	    /* while(p) */
      (*argv)[i] = NULL;
      (*argv)[i+1] = NULL;
      break; /* We are done here */
    }
  }
  if (remove == -1) 
  {
    argc--;
  }
  else 
  {
    for(i=1;i<*argc;i++) 
    { /* Remove NULL:ed argv entries */
      if ((*argv)[i] == NULL) 
      {
	(*argv)[i] = (*argv)[i+2];
	remove++;
      }
    }
  }
  *argc-=remove;
}        

int main (int argc, char *argv[])
{
  char path[_POSIX_PATH_MAX];
  char *p;
  struct passwd *pw;

#ifdef GNOME_APPLET
  applet_widget_init("gbuffy", VERSION, argc, argv, NULL, 0, NULL);
#else
  gtk_init (&argc, &argv);
#endif

  if ((p = getenv ("HOME")))
    Homedir = safe_strdup (p);

  if ((p = getenv ("NNTPSERVER")))
    DefaultNewserver = safe_strdup (p);
  else
    DefaultNewserver = safe_strdup ("news");

  if ((pw = getpwuid (getuid ())))
  {
    Username = safe_strdup (pw->pw_name);

    if (!Homedir)
      Homedir = safe_strdup (pw->pw_dir);
  }
  parse_geometry(&argc, &argv); 
  
#ifdef DEBUG
  if (argc == 3) 
  {
    if (!strcmp (argv[1], "-d"))
    {
      debuglevel = atoi (argv[2]);
      strfcpy (path, "~/.gbuffydebug", sizeof (path));
      gbuffy_expand_path (path, sizeof (path));
      debugfile = fopen (path, "w");
      if (debugfile == NULL)
      {
	g_print ("-E- Unable to open debugfile %s\n", path);
	gtk_exit (-1);
      }
      setvbuf(debugfile,NULL,_IONBF,0);
      g_print ("-I- Debugging at level %d\n", debuglevel);
    }
  }
  g_print("Will set:\nWidth: %d\nHeight: %d\nXOFF: %d:%d\nYOFF: %d:%d\n",
	  Width, Height, HorizPos.sign, HorizPos.value, VertPos.sign, 
	  VertPos.value);
#endif


  MailboxInfo = gbuffy_configure_load ();
  if (MailboxInfo == NULL)
  {
    MailboxInfo = (BOX_INFO *) calloc (1, sizeof (BOX_INFO));
    MailboxInfo->type = GB_MBOX;
    if (getenv ("MAIL"))
      MailboxInfo->path = safe_strdup (getenv ("MAIL"));
    else
    {
      snprintf (path, sizeof (path), "%s/%s", Spooldir, getenv ("USER"));
      MailboxInfo->path = safe_strdup (path);
    }
    MailboxInfo->title = safe_strdup ("Spool");
    MailboxInfo->command = safe_strdup ("xterm -e sh -c 'echo Sorry, no command configured.  Press enter.; read'");
  }

  gbuffy_display ();

#ifdef GNOME_APPLET
  applet_widget_gtk_main();
#else
  gtk_main ();
#endif

  return 0;
}

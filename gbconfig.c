/*
 * gbconfig.c
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <gtk/gtk.h>
#include <proplist.h>

#include "gbuffy.h"

#include "up.xpm"
#include "down.xpm"

/*
 * Ugly.  I'm not sure exactly how the changed stuff works, so I read
 * the value of the entry fields when they would switch of the window
 * closes, and update the corresponding boxes.
 */
static GtkWidget *ConfigureWindow = NULL;
static GtkWidget *TitleEntry;
static GtkWidget *PathEntry;
static GtkWidget *CommandEntry;
static GtkWidget *PollTimeEntry;
static GtkWidget *RowsEntry;
static GtkWidget *MaildirEntry;
static GtkWidget *MailboxCList;
static GtkWidget *TypeOptionMenu;
static GtkWidget *ServerEntry;
static GtkWidget *ServerLabel;
#ifdef HAVE_SSL
static GtkWidget *SSLLabel;
static GtkWidget *SSLCheckbox;
#endif
static GtkWidget *PortEntry;
static GtkWidget *PortLabel;
static GtkWidget *LoginEntry;
static GtkWidget *LoginLabel;
static GtkWidget *NewsrcEntry;
static GtkWidget *NewsrcLabel;

/*
 * Helper function and struct for making an option menu.  This should be
 * this hard.
 */
typedef struct _OptionMenuItem
{
  gchar *name;
  GtkSignalFunc func;
  gpointer data;
} OptionMenuItem;

static GtkWidget * create_option_menu (OptionMenuItem items[], gint num_items)
{
  GtkWidget *menu;
  GtkWidget *menuitem;
  GtkWidget *optionmenu;
  int x;

  optionmenu = gtk_option_menu_new ();
  menu = gtk_menu_new ();
  gtk_option_menu_set_menu (GTK_OPTION_MENU (optionmenu), menu);

  for (x = 0; x < num_items; x++)
  {
    menuitem = gtk_menu_item_new_with_label (items[x].name);
    gtk_signal_connect (GTK_OBJECT (menuitem), "activate",
	(GtkSignalFunc) items[x].func, items[x].data);
    gtk_menu_append (GTK_MENU (menu), menuitem);
    gtk_widget_show (menuitem);
  }
  gtk_option_menu_set_history (GTK_OPTION_MENU (optionmenu), 0);

  return optionmenu;
}

static void configure_select_mailbox (GtkWidget *clist, gint row, gint column,
                               GdkEventButton *event, gpointer data)
{
  BOX_INFO *box;

/*  g_print ("-I- Select received for row %d\n", row); */
  box = (BOX_INFO *) gtk_clist_get_row_data (GTK_CLIST (clist), row);
  box->selected = TRUE;
  if (box->type == GB_IMAP || box->type == GB_NNTP)
  {
    gtk_widget_show (ServerLabel);
    gtk_widget_show (ServerEntry);
    gtk_entry_set_text (GTK_ENTRY (ServerEntry), 
	box->server ? box->server : 
	(box->type == GB_IMAP) ? "imap" : DefaultNewserver);
#ifdef HAVE_SSL
    gtk_widget_show (SSLLabel);
    gtk_widget_show (SSLCheckbox);
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (SSLCheckbox), box->ssl_mode ? TRUE : FALSE);
#endif
    gtk_widget_show (PortLabel);
    gtk_widget_show (PortEntry);
    if (box->port)
    {
      char buf[SHORT_STRING];
      snprintf (buf, sizeof (buf), "%d", box->port);
      gtk_entry_set_text (GTK_ENTRY (PortEntry), buf);
    }
    else
#ifdef HAVE_SSL
      gtk_entry_set_text (GTK_ENTRY (PortEntry), 
	  (box->type == GB_IMAP) ? (box->ssl_mode ? "993" : "143") : "119");
#else
      gtk_entry_set_text (GTK_ENTRY (PortEntry), 
	  (box->type == GB_IMAP) ? "143" : "119");
#endif
    gtk_widget_show (LoginLabel);
    gtk_widget_show (LoginEntry);
    gtk_entry_set_text (GTK_ENTRY (LoginEntry), 
	box->login ? box->login : NONULL (Username));
  }
  else
  {
    gtk_widget_hide (ServerLabel);
    gtk_widget_hide (ServerEntry);
    gtk_widget_hide (SSLLabel);
    gtk_widget_hide (SSLCheckbox);
    gtk_widget_hide (PortLabel);
    gtk_widget_hide (PortEntry);
    gtk_widget_hide (LoginLabel);
    gtk_widget_hide (LoginEntry);
    gtk_widget_hide (NewsrcLabel);
    gtk_widget_hide (NewsrcEntry);
  }
  if (box->type == GB_NNTP)
  {
    gtk_widget_show (NewsrcLabel);
    gtk_widget_show (NewsrcEntry);
    gtk_entry_set_text (GTK_ENTRY (NewsrcEntry), 
	box->newsrc ? box->newsrc : "~/.newsrc");
  }
  gtk_entry_set_text (GTK_ENTRY (TitleEntry), NONULL (box->title));
  gtk_entry_set_text (GTK_ENTRY (PathEntry), NONULL (box->path));
  gtk_entry_set_text (GTK_ENTRY (CommandEntry), NONULL (box->command));
  /* 
   * FIXME: currently, this assumes a lot about the type corresponding
   * to the index of the option menu
   */
  gtk_option_menu_set_history (GTK_OPTION_MENU (TypeOptionMenu), box->type);
}

static void configure_set_box (BOX_INFO *box)
{
  if (box->title)
    free (box->title);
  box->title = strdup (gtk_entry_get_text (GTK_ENTRY (TitleEntry)));
  if (box->path)
    free (box->path);
  box->path = strdup (gtk_entry_get_text (GTK_ENTRY (PathEntry)));
  if (box->command)
    free (box->command);
  box->command = strdup (gtk_entry_get_text (GTK_ENTRY (CommandEntry)));

  if (box->newsrc)
    FREE (&box->newsrc);
  if (box->server)
    FREE (&box->server);
  if (box->newsrc)
    FREE (&box->newsrc);
  box->port = 0;
  if (box->type == GB_IMAP || box->type == GB_NNTP)
  {
    box->login = strdup (gtk_entry_get_text (GTK_ENTRY (LoginEntry)));
    box->server = strdup (gtk_entry_get_text (GTK_ENTRY (ServerEntry)));
    if (box->type == GB_NNTP)
      box->newsrc = strdup (gtk_entry_get_text (GTK_ENTRY (NewsrcEntry)));
#ifdef HAVE_SSL
    else
      box->ssl_mode = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (SSLCheckbox)) == TRUE;
#endif
    box->port = atoi (gtk_entry_get_text (GTK_ENTRY (PortEntry)));
  }
}

static void configure_unselect_mailbox (GtkWidget *clist, gint row, gint column,
                                 GdkEventButton *event, gpointer data)
{
  BOX_INFO *box;

/*  g_print ("-I- Unselect received for row %d\n", row); */
  box = (BOX_INFO *) gtk_clist_get_row_data (GTK_CLIST (clist), row);
  box->selected = FALSE;

  configure_set_box (box);

  gtk_clist_set_text (GTK_CLIST (clist), row, 0, box->title);
}

static void configure_set_type (GtkWidget *widget, gpointer data)
{
  BOX_INFO *box;
  enum BOXTYPE type;

  type = GPOINTER_TO_INT (data);
  if (type == GB_IMAP || type == GB_NNTP)
  {
    gtk_widget_show (ServerLabel);
    gtk_widget_show (ServerEntry);
    gtk_entry_set_text (GTK_ENTRY (ServerEntry), 
	(type == GB_IMAP) ? "imap" : "news");
#ifdef HAVE_SSL
    gtk_widget_show (SSLLabel);
    gtk_widget_show (SSLCheckbox);
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (SSLCheckbox), FALSE);
#endif
    gtk_widget_show (PortLabel);
    gtk_widget_show (PortEntry);
    gtk_entry_set_text (GTK_ENTRY (PortEntry), 
	(type == GB_IMAP) ? "143" : "119");
    gtk_widget_show (LoginLabel);
    gtk_widget_show (LoginEntry);
    gtk_entry_set_text (GTK_ENTRY (LoginEntry), NONULL (Username));
  }
  else
  {
    gtk_widget_hide (ServerLabel);
    gtk_widget_hide (ServerEntry);
    gtk_widget_hide (SSLLabel);
    gtk_widget_hide (SSLCheckbox);
    gtk_widget_hide (PortLabel);
    gtk_widget_hide (PortEntry);
    gtk_widget_hide (LoginLabel);
    gtk_widget_hide (LoginEntry);
    gtk_widget_hide (NewsrcLabel);
    gtk_widget_hide (NewsrcEntry);
  }
  if (type == GB_NNTP)
  {
    gtk_widget_show (NewsrcLabel);
    gtk_widget_show (NewsrcEntry);
    gtk_entry_set_text (GTK_ENTRY (NewsrcEntry), "~/.newsrc");
  }
  box = MailboxInfo;
  while (box != NULL)
  {
    if (box->selected)
    {
      box->type = type;
      if (type == GB_IMAP || type == GB_NNTP)
      {
	if (box->login)
	  gtk_entry_set_text (GTK_ENTRY (LoginEntry), box->login);
	if (box->server)
	  gtk_entry_set_text (GTK_ENTRY (ServerEntry), box->server);
#ifdef HAVE_SSL
	if (box->ssl_mode)
	  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (SSLCheckbox), TRUE);
#endif
	if (box->port)
	{
	  char buf[SHORT_STRING];
	  snprintf (buf, sizeof (buf), "%d", box->port);
	  gtk_entry_set_text (GTK_ENTRY (PortEntry), buf);
	}
	if (type == GB_NNTP)
	{
	  if (box->newsrc)
	    gtk_entry_set_text (GTK_ENTRY (NewsrcEntry), box->newsrc);
	}
      }
    }
    box = box->next;
  }
}

static void configure_insert (GtkWidget *button, gpointer data)
{
  BOX_INFO *box;
  int x;

  box = MailboxInfo;
  while (box && box->next)
    box = box->next;

  if (box)
  {
    box->next = (BOX_INFO *) calloc (1, sizeof (BOX_INFO));
    box = box->next;
  }
  else
    box = (BOX_INFO *) calloc (1, sizeof (BOX_INFO));

  if (MailboxInfo == NULL)
    MailboxInfo = box;

  box->title = strdup ("New Box");
  x = gtk_clist_append (GTK_CLIST (MailboxCList), &(box->title));
  gtk_clist_set_row_data (GTK_CLIST (MailboxCList), x, box);
  gtk_clist_select_row (GTK_CLIST (MailboxCList), x, 0);
}

static void configure_delete (GtkWidget *button, gpointer data)
{
  BOX_INFO *box, *last;
  int row = 0;

  last = box = MailboxInfo;
  while (box != NULL)
  {
    if (box->selected)
    {
      gtk_clist_remove (GTK_CLIST (MailboxCList), row); 
      if (box->title)
	free (box->title);
      if (box->path)
	free (box->path);
      if (box->command)
	free (box->command);
      last->next = box->next;
      box = last->next;
    }
    else
    {
      last = box;
      box = box->next;
    }
    row++;
  }
  gtk_entry_set_text (GTK_ENTRY (TitleEntry), "");
  gtk_entry_set_text (GTK_ENTRY (PathEntry), "");
  gtk_entry_set_text (GTK_ENTRY (CommandEntry), "");
}

static void configure_close (GtkWidget *button, gpointer data)
{
  BOX_INFO *box;
  int row = 0;

  box = MailboxInfo;
  while (box != NULL)
  {
    if (box->selected)
    {
      configure_set_box (box);
      gtk_clist_set_text (GTK_CLIST (MailboxCList), row, 0, box->title);
    }
    row++;
    box = box->next;
  }
  safe_free ((void *)&Maildir);
  Maildir = safe_strdup (gtk_entry_get_text (GTK_ENTRY (MaildirEntry)));
  PollTime = atoi (gtk_entry_get_text (GTK_ENTRY (PollTimeEntry)));
  row = atoi (gtk_entry_get_text (GTK_ENTRY (RowsEntry)));
  if (row != Rows)
  {
    Rows = row;
    Height = -1;
    Width = -1;
  }

  gtk_widget_hide (ConfigureWindow);
  gbuffy_display ();
}

static void configure_swap_entries (GtkWidget *button, gpointer data)
{
  BOX_INFO *box, *last, *n, *p;
  int selected = 0;
  int next;

  box = last = p = MailboxInfo;
  while (box != NULL)
  {
    if (box->selected)
    {
      break;
    }
    p = last;
    last = box;
    box = box->next;
    selected++;
  }
  g_print ("-I- Received swap %s\n", (data ? "down" : "up"));
  if (box == NULL)
    return;

  /* True is down, false is up */
  if (GPOINTER_TO_INT(data))
  {
    /* Down */
    if (box->next != NULL)
    {
      next = selected + 1;
      g_print ("-I- Swap %d, %d\n", selected, next);
      gtk_clist_swap_rows (GTK_CLIST (MailboxCList), selected, next);
      if (box == MailboxInfo)
      {
	n = box->next;
	last = n->next;
	n->next = box;
	box->next = last;
	MailboxInfo = n;
      }
      else
      {
	n = box->next;
	last->next = box->next;
	box->next = n->next;
	n->next = box;
      }
    }
  }
  else
  {
    /* Up */
    if ((selected > 0) && (box != MailboxInfo))
    {
      next = selected - 1;
      gtk_clist_swap_rows (GTK_CLIST (MailboxCList), selected, next);
      g_print ("-I- Swap %d, %d\n", selected, next);
      if (last == MailboxInfo)
      {
	p = box->next;
	MailboxInfo = box;
	MailboxInfo->next = last;
	last->next = p;
      }
      else
      {
	p->next = box;
	last->next = box->next;
	box->next = last;
      }
    }
  }
}

static void configure_save (GtkWidget *button, gpointer data)
{
  BOX_INFO *box;
  int row = 0;

  box = MailboxInfo;
  while (box != NULL)
  {
    if (box->selected)
    {
      configure_set_box (box);
      gtk_clist_set_text (GTK_CLIST (MailboxCList), row, 0, box->title);
    }
    row++;
    box = box->next;
  }
  safe_free ((void *)&Maildir);
  Maildir = safe_strdup (gtk_entry_get_text (GTK_ENTRY (MaildirEntry)));
  PollTime = atoi (gtk_entry_get_text (GTK_ENTRY (PollTimeEntry)));
  row = atoi (gtk_entry_get_text (GTK_ENTRY (RowsEntry)));
  if (row != Rows)
  {
    Rows = row;
    Height = -1;
    Width = -1;
  }

  gbuffy_save_conf ();
}

static void toggle_var_callback (GtkWidget *button, gpointer data)
{
  int *var = (int *)data;

  *var = (GTK_TOGGLE_BUTTON (button)->active) ? 1 : 0;
}

GtkWidget *create_pixmap_button (GtkWidget *w, gchar **xpm)
{
  GtkWidget *button;
  GtkWidget *pixmapwid;
  GdkBitmap *mask;
  GdkPixmap *pixmap;
  GtkStyle *style;

  button = gtk_button_new ();

  style = gtk_widget_get_style (button);

  pixmap = gdk_pixmap_create_from_xpm_d (w->window, &mask,
      &style->fg[GTK_STATE_NORMAL], xpm);
  pixmapwid = gtk_pixmap_new (pixmap, mask);
/*  gtk_container_border_width (GTK_CONTAINER (button), 2); */
  gtk_container_add (GTK_CONTAINER (button), pixmapwid);
  gtk_widget_show (pixmapwid);
  gdk_pixmap_unref (pixmap); 
  gdk_pixmap_unref (mask); 

  return button;
}

void gbuffy_configure_dialog ()
{
  GtkWidget *hbox;
  GtkWidget *hbox2;
  GtkWidget *vbox;
  GtkWidget *table;
  GtkWidget *label;
  GtkWidget *button;
  GtkWidget *frame;
  GtkWidget *scroll;
  char *titles[] = {"Mailboxes", NULL};
  BOX_INFO *box;
  int x;
  int row;
  static OptionMenuItem *types = NULL;
  char buf[STRING];

  if (ConfigureWindow == NULL)
  {
    ConfigureWindow = gtk_dialog_new ();
    gtk_widget_realize (ConfigureWindow);

    gtk_signal_connect (GTK_OBJECT (ConfigureWindow), "destroy",
	GTK_SIGNAL_FUNC(gtk_widget_destroyed),
	&ConfigureWindow);

    gtk_window_set_title (GTK_WINDOW (ConfigureWindow), "GBuffy: Configuration");

    frame = gtk_widget_new (gtk_frame_get_type (),
		      "GtkFrame::shadow", GTK_SHADOW_ETCHED_IN,
		      "GtkFrame::label_xalign", 0.1,
		      "GtkFrame::label", "Mailbox Configuration",
		      "GtkContainer::border_width", 10,
		      "GtkWidget::visible", TRUE,
		      NULL); 

    gtk_box_pack_start (GTK_BOX (GTK_DIALOG (ConfigureWindow)->vbox), frame, TRUE, TRUE, 0);
    hbox = gtk_hbox_new (TRUE, 0);
    gtk_container_add (GTK_CONTAINER (frame), hbox);
    vbox = gtk_vbox_new (FALSE, 0);
    gtk_box_pack_start (GTK_BOX (hbox), vbox, TRUE, TRUE, 0);
    MailboxCList = gtk_clist_new_with_titles (1, titles);
    scroll = gtk_scrolled_window_new (NULL, NULL);
    gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scroll),
	GTK_POLICY_AUTOMATIC, 
	GTK_POLICY_AUTOMATIC);
    gtk_container_add (GTK_CONTAINER (scroll), MailboxCList);
    gtk_clist_set_column_width (GTK_CLIST (MailboxCList), 0, 100);

    box = MailboxInfo;
    x = 0;
    while (box != NULL)
    {
      gtk_clist_append (GTK_CLIST (MailboxCList), &(box->title));
      gtk_clist_set_row_data (GTK_CLIST (MailboxCList), x, box);
      x++;
      box = box->next;
    }
    gtk_box_pack_start (GTK_BOX (vbox), scroll, TRUE, TRUE, 0);
    gtk_signal_connect(GTK_OBJECT(MailboxCList), "select_row",
	GTK_SIGNAL_FUNC(configure_select_mailbox), NULL);
    gtk_signal_connect(GTK_OBJECT(MailboxCList), "unselect_row",
	GTK_SIGNAL_FUNC(configure_unselect_mailbox), NULL);
    gtk_widget_show (MailboxCList);
    gtk_widget_show (scroll);
    hbox2 = gtk_hbox_new (TRUE, 0);
    gtk_box_pack_start (GTK_BOX (vbox), hbox2, FALSE, FALSE, 0); 
    button = create_pixmap_button (ConfigureWindow, up);
    gtk_box_pack_start (GTK_BOX (hbox2), button, TRUE, FALSE, 0);
    gtk_signal_connect (GTK_OBJECT (button), "clicked",
	GTK_SIGNAL_FUNC (configure_swap_entries), GINT_TO_POINTER(FALSE));
    gtk_widget_show (button);
    button = create_pixmap_button (ConfigureWindow, down);
    gtk_box_pack_start (GTK_BOX (hbox2), button, TRUE, FALSE, 0);
    gtk_signal_connect (GTK_OBJECT (button), "clicked",
	GTK_SIGNAL_FUNC (configure_swap_entries), GINT_TO_POINTER(TRUE));
    gtk_widget_show (button);
    gtk_widget_show (hbox2);
    gtk_widget_show (vbox);

#ifdef HAVE_SSL
    table = gtk_table_new (3, 9, FALSE);
#else
    table = gtk_table_new (3, 8, FALSE);
#endif
    gtk_table_set_row_spacings (GTK_TABLE (table), 5);
    gtk_table_set_col_spacings (GTK_TABLE (table), 5);
    gtk_container_border_width (GTK_CONTAINER (table), 10);
    gtk_box_pack_start (GTK_BOX (hbox), table, TRUE, TRUE, 0);

    if (types == NULL)
    {
      types = (OptionMenuItem *) calloc (GB_MAX, sizeof (OptionMenuItem));
      for (x = 0; x < GB_MAX; x++)
      {
	types[x].name = MailboxClass[x].name;
	types[x].func = GTK_SIGNAL_FUNC (configure_set_type);
	types[x].data = GINT_TO_POINTER (x);
      }
    }
    row = 1;

    label = gtk_label_new ("Type:");
    gtk_table_attach (GTK_TABLE (table), label, 0, 1, row - 1, row, 
	GTK_EXPAND | GTK_FILL, GTK_EXPAND, 0, 0);
    gtk_widget_show (label);
    TypeOptionMenu = create_option_menu (types, GB_MAX);
    gtk_table_attach (GTK_TABLE (table), TypeOptionMenu, 1, 2, row - 1, row, 
	GTK_EXPAND | GTK_FILL, GTK_EXPAND, 0, 0);
    gtk_widget_show (TypeOptionMenu);
    row++;

    label = gtk_label_new ("Title:");
    gtk_table_attach (GTK_TABLE (table), label, 0, 1, row - 1, row, 
	GTK_EXPAND | GTK_FILL, GTK_EXPAND, 0, 0);
    gtk_widget_show (label);
    TitleEntry = gtk_entry_new ();
    gtk_table_attach (GTK_TABLE (table), TitleEntry, 1, 3, row - 1, row, 
	GTK_EXPAND | GTK_FILL, GTK_EXPAND, 0, 0);
    gtk_widget_show (TitleEntry);
    row++;

    ServerLabel = gtk_label_new ("Server:");
    gtk_table_attach (GTK_TABLE (table), ServerLabel, 0, 1, row - 1, row, 
	GTK_EXPAND | GTK_FILL, GTK_EXPAND, 0, 5);
    ServerEntry = gtk_entry_new ();
    gtk_table_attach (GTK_TABLE (table), ServerEntry, 1, 3, row - 1, row, 
	GTK_EXPAND | GTK_FILL, GTK_EXPAND, 0, 5);
    row++;

#ifdef HAVE_SSL
    SSLLabel = gtk_label_new ("SSL:");
    gtk_table_attach (GTK_TABLE (table), SSLLabel, 0, 1, row - 1, row,
	GTK_EXPAND | GTK_FILL, GTK_EXPAND, 0, 5);
    SSLCheckbox = gtk_check_button_new();
    gtk_table_attach (GTK_TABLE (table), SSLCheckbox, 1, 3, row - 1, row,
	GTK_EXPAND | GTK_FILL, GTK_EXPAND, 0, 5);
    row++;
#endif

    PortLabel = gtk_label_new ("Port:");
    gtk_table_attach (GTK_TABLE (table), PortLabel, 0, 1, row - 1, row, 
	GTK_EXPAND | GTK_FILL, GTK_EXPAND, 0, 5);
    PortEntry = gtk_entry_new ();
    gtk_table_attach (GTK_TABLE (table), PortEntry, 1, 3, row - 1, row, 
	GTK_EXPAND | GTK_FILL, GTK_EXPAND, 0, 5);
    row++;

    LoginLabel = gtk_label_new ("Login:");
    gtk_table_attach (GTK_TABLE (table), LoginLabel, 0, 1, row - 1, row, 
	GTK_EXPAND | GTK_FILL, GTK_EXPAND, 0, 5);
    LoginEntry = gtk_entry_new ();
    gtk_table_attach (GTK_TABLE (table), LoginEntry, 1, 3, row - 1, row, 
	GTK_EXPAND | GTK_FILL, GTK_EXPAND, 0, 5);
    row++;

    label = gtk_label_new ("Path:");
    gtk_table_attach (GTK_TABLE (table), label, 0, 1, row - 1, row, 
	GTK_EXPAND | GTK_FILL, GTK_EXPAND, 0, 5);
    gtk_widget_show (label);
    PathEntry = gtk_entry_new ();
    gtk_table_attach (GTK_TABLE (table), PathEntry, 1, 3, row - 1, row, 
	GTK_EXPAND | GTK_FILL, GTK_EXPAND, 0, 5);
    gtk_widget_show (PathEntry);
    row++;

    NewsrcLabel = gtk_label_new ("Newsrc:");
    gtk_table_attach (GTK_TABLE (table), NewsrcLabel, 0, 1, row - 1, row, 
	GTK_EXPAND | GTK_FILL, GTK_EXPAND, 0, 5);
    NewsrcEntry = gtk_entry_new ();
    gtk_table_attach (GTK_TABLE (table), NewsrcEntry, 1, 3, row - 1, row, 
	GTK_EXPAND | GTK_FILL, GTK_EXPAND, 0, 5);
    row++;

    label = gtk_label_new ("Command:");
    gtk_table_attach (GTK_TABLE (table), label, 0, 1, row - 1, row, 
	GTK_EXPAND | GTK_FILL, GTK_EXPAND, 0, 5);
    gtk_widget_show (label);
    CommandEntry = gtk_entry_new ();
    gtk_table_attach (GTK_TABLE (table), CommandEntry, 1, 3, row - 1, row, 
	GTK_EXPAND | GTK_FILL, GTK_EXPAND, 0, 5);
    gtk_widget_show (CommandEntry);
    row++;

    gtk_widget_show (table);
    gtk_widget_show (hbox);

    table = gtk_table_new (3, 5, FALSE);
    gtk_table_set_row_spacings (GTK_TABLE (table), 5);
    gtk_table_set_col_spacings (GTK_TABLE (table), 5);
    gtk_container_border_width (GTK_CONTAINER (table), 10);
    gtk_box_pack_start (GTK_BOX (GTK_DIALOG (ConfigureWindow)->vbox), table, TRUE, TRUE, 0);
#ifndef GNOME_APPLET
    /* We don't need to configure this here, the applet detects
     * orientation changes automatically. */
    button = gtk_check_button_new_with_label ("Vertical");
    gtk_table_attach (GTK_TABLE (table), button, 0, 1, 0, 1, 
	GTK_FILL, GTK_EXPAND, 0, 5);
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button), Vertical);
    gtk_signal_connect(GTK_OBJECT(button), "toggled",
	GTK_SIGNAL_FUNC(toggle_var_callback), &Vertical);
    gtk_widget_show (button);
#endif

    label = gtk_label_new ("Mail Directory:");
    gtk_table_attach (GTK_TABLE (table), label, 1, 2, 0, 1, 
	GTK_FILL, GTK_EXPAND, 0, 5);
    gtk_widget_show (label);
    MaildirEntry = gtk_entry_new ();
    gtk_entry_set_text (GTK_ENTRY (MaildirEntry), NONULL (Maildir));
    gtk_table_attach (GTK_TABLE (table), MaildirEntry, 3, 4, 0, 1, 
	GTK_EXPAND | GTK_FILL, GTK_EXPAND, 0, 5);
    gtk_widget_show (MaildirEntry);

    label = gtk_label_new ("Poll Time:");
    gtk_table_attach (GTK_TABLE (table), label, 1, 2, 1, 2, 
	GTK_FILL, GTK_EXPAND, 0, 5);
    gtk_widget_show (label);
    PollTimeEntry = gtk_entry_new ();
    snprintf (buf, sizeof (buf), "%d", PollTime);
    gtk_entry_set_text (GTK_ENTRY (PollTimeEntry), buf);
    gtk_table_attach (GTK_TABLE (table), PollTimeEntry, 3, 4, 1, 2, 
	GTK_EXPAND | GTK_FILL, GTK_EXPAND, 0, 5);
    gtk_widget_show (PollTimeEntry);
    gtk_widget_show (table);

    label = gtk_label_new ("Rows:");
    gtk_table_attach (GTK_TABLE (table), label, 1, 2, 2, 3, 
	GTK_FILL, GTK_EXPAND, 0, 5);
    gtk_widget_show (label);
    RowsEntry = gtk_entry_new ();
    snprintf (buf, sizeof (buf), "%d", Rows);
    gtk_entry_set_text (GTK_ENTRY (RowsEntry), buf);
    gtk_table_attach (GTK_TABLE (table), RowsEntry, 3, 4, 2, 3, 
	GTK_EXPAND | GTK_FILL, GTK_EXPAND, 0, 5);
    gtk_widget_show (RowsEntry);
    gtk_widget_show (table);

    button = gtk_button_new_with_label ("Insert");
    gtk_box_pack_start (GTK_BOX (GTK_DIALOG (ConfigureWindow)->action_area), 
	button, TRUE, TRUE, 0);
    gtk_signal_connect(GTK_OBJECT(button), "clicked",
	GTK_SIGNAL_FUNC(configure_insert), NULL);
    gtk_widget_show (button);
    button = gtk_button_new_with_label ("Delete");
    gtk_box_pack_start (GTK_BOX (GTK_DIALOG (ConfigureWindow)->action_area), 
	button, TRUE, TRUE, 0);
    gtk_signal_connect(GTK_OBJECT(button), "clicked",
	GTK_SIGNAL_FUNC(configure_delete), NULL);
    gtk_widget_show (button);
    button = gtk_button_new_with_label ("Save");
    gtk_box_pack_start (GTK_BOX (GTK_DIALOG (ConfigureWindow)->action_area), 
	button, TRUE, TRUE, 0);
    gtk_signal_connect(GTK_OBJECT(button), "clicked",
	GTK_SIGNAL_FUNC(configure_save), NULL);
    gtk_widget_show (button);
    button = gtk_button_new_with_label ("Close");
    gtk_box_pack_start (GTK_BOX (GTK_DIALOG (ConfigureWindow)->action_area), 
	button, TRUE, TRUE, 0);
    gtk_signal_connect(GTK_OBJECT(button), "clicked",
	GTK_SIGNAL_FUNC(configure_close), &ConfigureWindow);
    gtk_widget_show (button);
  }
  if (!GTK_WIDGET_VISIBLE (ConfigureWindow))
    gtk_widget_show (ConfigureWindow);
  else
    gtk_widget_hide (ConfigureWindow);
}


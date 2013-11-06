
#include <gtk/gtk.h>
#include <stdio.h>
#include <string.h>
#include "gtklib.h"
#include "gbuffy.h"

GtkWidget * create_option_menu (OptionMenuItem items[], gint num_items)
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

typedef struct _fileselect
{
  GtkWidget *filew;
  char *file;
} FILESELECT;

void file_select_ok (GtkWidget *w, FILESELECT *fs)
{
  strcpy (fs->file, gtk_file_selection_get_filename (GTK_FILE_SELECTION (fs->filew)));
  gtk_main_quit ();
}

void quit_dialog (GtkWidget *widget, gpointer data)
{
  gtk_main_quit ();
}

char * file_select_dialog (char *type, char *filename)
{
  GtkWidget *filew;
  FILESELECT fs;
  guint destroy_sig;
  
  filew = gtk_file_selection_new (type);
  fs.filew = filew;
  fs.file = filename;
  destroy_sig = gtk_signal_connect (GTK_OBJECT (filew), "destroy", 
		      (GtkSignalFunc) quit_dialog, &filew);

  /* Connect the ok_button to file_ok_sel function */
  gtk_signal_connect (GTK_OBJECT (GTK_FILE_SELECTION (filew)->ok_button),
		      "clicked", (GtkSignalFunc) file_select_ok, &fs);
  gtk_signal_connect (GTK_OBJECT (GTK_FILE_SELECTION (filew)->cancel_button),
		      "clicked", (GtkSignalFunc) quit_dialog,
		      GTK_OBJECT (filew));

  /* Lets set the filename, as if this were a save dialog, and we are giving
   *            a default filename */
  gtk_file_selection_set_filename (GTK_FILE_SELECTION(filew), filename);
  gtk_widget_show (filew);
  gtk_main ();
  gtk_signal_disconnect (GTK_OBJECT (filew), destroy_sig);
  gtk_widget_destroy (filew);
  return filename;
}

GdkGC * copy_gc_with_color (GdkPixmap *pixmap, GdkGC *gc, char *color)
{
  GdkGC *new_gc;
  GdkColor gcolor;

  new_gc = gdk_gc_new (pixmap);
  gdk_gc_copy (new_gc, gc);
  gdk_color_parse (color, &gcolor);
  gdk_color_alloc (gtk_widget_get_default_colormap(), &gcolor);
  gdk_gc_set_foreground (new_gc, &gcolor);

  return new_gc;
}

typedef struct _password_dialog
{
  GtkWidget *dialog;
  GtkWidget *user_entry;
  GtkWidget *pass_entry;
  char *user;
  char *pass;
  int ok;
} PDIALOG;

void pass_select_ok (GtkWidget *w, PDIALOG *pd)
{
  pd->user = gtk_entry_get_text (GTK_ENTRY (pd->user_entry));
  pd->pass = gtk_entry_get_text (GTK_ENTRY (pd->pass_entry));
  pd->ok = 1;
  gtk_main_quit ();
}

/* Returns 1 on ok, 0 on cancel */
int password_prompt_dialog (char *prompt, char *user, size_t userlen, 
    char *pass, size_t passlen)
{
  GtkWidget *button;
  GtkWidget *label;
  GtkWidget *table;
  PDIALOG pd;
  guint destroy_sig;
  
  pd.dialog = gtk_dialog_new ();
  pd.ok = 0;
  pd.user = NULL;
  pd.pass = NULL;
  destroy_sig = gtk_signal_connect (GTK_OBJECT (pd.dialog), "destroy", 
		      (GtkSignalFunc) quit_dialog, &pd.dialog);

  button = gtk_button_new_with_label ("Ok");
  gtk_box_pack_start (GTK_BOX (GTK_DIALOG (pd.dialog)->action_area),
              button, TRUE, TRUE, 0);
  gtk_signal_connect (GTK_OBJECT (button),
		      "clicked", (GtkSignalFunc) pass_select_ok, &pd);
  gtk_widget_show (button);

  button = gtk_button_new_with_label ("Cancel");
  gtk_box_pack_start (GTK_BOX (GTK_DIALOG (pd.dialog)->action_area),
              button, TRUE, TRUE, 0);
  gtk_signal_connect (GTK_OBJECT (button),
		      "clicked", (GtkSignalFunc) quit_dialog,
		      GTK_OBJECT (pd.dialog));
  gtk_widget_show (button);

  gtk_window_set_title (GTK_WINDOW (pd.dialog), "Login Prompt");
  
  label = gtk_label_new (prompt);
  gtk_box_pack_start (GTK_BOX (GTK_DIALOG (pd.dialog)->vbox), label, TRUE, 
      TRUE, 0);
  gtk_widget_show (label);
  
  table = gtk_table_new (2, 2, FALSE);
  gtk_table_set_row_spacings (GTK_TABLE (table), 5);
  gtk_table_set_col_spacings (GTK_TABLE (table), 5);
  gtk_container_border_width (GTK_CONTAINER (table), 10);
  gtk_box_pack_start (GTK_BOX (GTK_DIALOG (pd.dialog)->vbox), table, TRUE, 
      TRUE, 0);
  label = gtk_label_new ("Login:");
  gtk_table_attach (GTK_TABLE (table), label, 0, 1, 0, 1,
              GTK_EXPAND | GTK_FILL, GTK_EXPAND, 0, 0);
  gtk_widget_show (label);
  pd.user_entry = gtk_entry_new_with_max_length (userlen);
  gtk_entry_set_text (GTK_ENTRY (pd.user_entry), user);
  gtk_table_attach (GTK_TABLE (table), pd.user_entry, 1, 2, 0, 1,
      GTK_EXPAND | GTK_FILL, GTK_EXPAND, 0, 0);
  gtk_widget_show (pd.user_entry);
  label = gtk_label_new ("Password:");
  gtk_table_attach (GTK_TABLE (table), label, 0, 1, 1, 2,
              GTK_EXPAND | GTK_FILL, GTK_EXPAND, 0, 0);
  gtk_widget_show (label);
  pd.pass_entry = gtk_entry_new_with_max_length (passlen);
  gtk_entry_set_visibility (GTK_ENTRY (pd.pass_entry), FALSE);
  gtk_entry_set_text (GTK_ENTRY (pd.pass_entry), pass);
  gtk_table_attach (GTK_TABLE (table), pd.pass_entry, 1, 2, 1, 2,
      GTK_EXPAND | GTK_FILL, GTK_EXPAND, 0, 0);
  gtk_widget_show (pd.pass_entry);
  gtk_widget_show (table);
  gtk_widget_show (pd.dialog);
  gtk_main ();
  gtk_signal_disconnect (GTK_OBJECT (pd.dialog), destroy_sig);
  if (pd.user)
  {
    strfcpy (user, pd.user, userlen);
  }
  if (pd.pass)
  {
    strfcpy (pass, pd.pass, passlen);
  }
  gtk_widget_destroy (pd.dialog);
  return pd.ok;
}

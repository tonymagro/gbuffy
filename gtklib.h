
#ifndef _GTKLIB_H_
#define _GTKLIB_H_ 1

typedef struct _OptionMenuItem
{
  gchar *name;
  GtkSignalFunc func;
  gpointer data;
} OptionMenuItem;

GtkWidget * create_option_menu (OptionMenuItem items[], gint num_items);
char * file_select_dialog (char *type, char *filename);
GdkGC * copy_gc_with_color (GdkPixmap *pixmap, GdkGC *gc, char *color);
int password_prompt_dialog (char *prompt, char *user, size_t userlen, 
    char *pass, size_t passlen);

#endif /* _GTKLIB_H_ */

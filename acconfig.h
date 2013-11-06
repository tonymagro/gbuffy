/*
 * config file for xbuffy.  
 */

@TOP@

/* if you want Xbuffy to try to use Content-Length: headers uncomment the
 * following 
 */
#undef USE_CONTENT_LENGTH

/* Version Information */
#undef VERSION

/* enable NNTP mailboxes? */
#undef USE_NNTP

/* Default NNTP Server */ 
#undef NNTP_SERVER

/* enable keyboard LED blinking? */
#undef USE_LED

/* build as a GNOME Applet */
#undef GNOME_APPLET

/* Does your system have the snprintf() call? */
#undef HAVE_SNPRINTF

/* Does your system have the vsnprintf() call? */
#undef HAVE_VSNPRINTF

/* program to use for shell commands */
#define EXECSHELL "/bin/sh"

/* Enable SSL? */
#undef HAVE_SSL

/* Enable debugging info */
#undef DEBUG


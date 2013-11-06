/* 
 * utimes (BSD Equivalent of utime() 
 * - set file mod and access times)
 * (No attempt to reproduce same error code expect that they 
 * both do return -1 on error and 0 on success) 
 * 
 * From: corrigan@weber.ucsd.edu (Michael J. Corrigan) 
 */

#include <sys/types.h>
#include <sys/time.h>
#include <utime.h>
    
int utimes(file,tvp) char *file; struct timeval *tvp; 
{ 
    struct utimbuf ut;
    time_t now;
	
	now = time((time_t *)NULL);
	if (tvp == (struct timeval *)NULL) {
	    ut.actime = now;
	    ut.modtime = now; 
	} else {
	    ut.actime = tvp++->tv_sec;
	    ut.modtime = tvp->tv_sec; 
	} return(utime(file,&ut)); 
}

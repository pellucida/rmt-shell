/*
// @(#) access.h
*/

# if	!defined(ACCESS_H)
# define	ACCESS_H

# if 	!defined( RESTRICT_ACCESS)
# define        RESTRICT_ACCESS "/opt/rmt/etc/access"
# endif

int	access_init ();
int	access_check (char* devstr);

# endif

/*
// @(#) access.h
*/

# if	!defined(ACCESS_H)
# define	ACCESS_H

# include	<stdbool.h>
# include	"util.h"

# if	defined(USE_ACCESS)
#  if 	!defined( RESTRICT_ACCESS)
#     define        RESTRICT_ACCESS "/opt/rmt/etc/access"
#  endif

int	access_init ();
int	access_check (char* devstr);
# else
static inline int access_init() { return ok; }
static inline int access_check (char* devstr) { return true; }

# endif
# endif

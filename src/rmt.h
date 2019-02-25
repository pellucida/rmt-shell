/*
// @(#) rmt.h
*/

# if	!defined(RMT_H)
# define	RMT_H

typedef	struct arg	{
	int	input;
	int	output;
	int	tape;
	int	magtape;
	struct	{
		size_t	size;
		char*	buffer;
	}	record;
}	arg_t;

int	do_rmt (arg_t*, char*, int, char**);
# endif

/*
// @(#) rmt.h
*/

# if	!defined(RMT_H)
# define	RMT_H
# include	<stdio.h>

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

ssize_t safe_read (int fd, char* buf, ssize_t len);
ssize_t full_write (int fd, char* buf, ssize_t len);

void	reply_ext (arg_t* arg, int status, char* more);
void	reply_ext_nostatus (arg_t*, char*);
void	report_error (arg_t* arg, int error);

# endif

/*
//  @(#) rmt.c
*/
# include	<string.h>
# include	<stdio.h>
# include	<unistd.h>
# include	<limits.h>
# include	<stdlib.h>
# include	<stdbool.h>
# include	<errno.h>

# include	<sys/types.h>
# include	<sys/stat.h>
# include	<fcntl.h>

# include	<sys/socket.h>

# include	"util.h"
# include	"access.h"
# include	"rmt.h"
# include	"cmd_extend.h"

enum	{
	FD_NONE	= -1,
};
ssize_t safe_read (int fd, char* buf, ssize_t len) {
	ssize_t	n	= 0;
	if (len <= 0)
		return len;
	do {
		n	= read (fd, buf, len);
	} while (n < 0 && errno == EINTR);
	return n;
}

ssize_t full_write (int fd, char* buf, ssize_t len) {
	ssize_t	n	= 0;

	while (len > 0) {
		ssize_t	nw	= write (fd, buf, len);
		if (nw < 0) {
			if (errno == EINTR)
				continue;
			return nw;
		}
		n	+= nw;
		buf	+= nw;
		len	-= nw;
	}
	return n;
}

static	void	report_error_msg (arg_t* arg, int errcode, char* msg){
	char	buf [BUFSIZ];
	snprintf (buf, sizeof(buf), "E%d\n%s\n", errcode, msg);
	full_write (arg->output, buf, strlen (buf));
}
void	report_error (arg_t* arg, int error) {
	report_error_msg (arg, error, strerror(error));
}

void	reply (arg_t* arg, long status) {
	char	buf [BUFSIZ];
	snprintf (buf, sizeof(buf), "A%ld\n", status);
	full_write (arg->output, buf, strlen (buf));
}
void	reply_ext (arg_t* arg, long status, char* more) {
	char	buf [BUFSIZ];
	snprintf (buf, sizeof(buf), "A%ld\n%s\n", status, more);
	full_write (arg->output, buf, strlen (buf));
}
void	reply_ext_nostatus (arg_t* arg, char* more) {
	char	buf [BUFSIZ];
	snprintf (buf, sizeof(buf), "%s\n", more);
	full_write (arg->output, buf, strlen (buf));
}

static	int	read_str (int fd, char* buf, size_t size) {
	int	result	= err;
	size_t	n	= 0;
	size_t	i	= 0;
	size_t	j	= size-1;
	while (i!=j) {
		if (safe_read (fd, &buf[i], 1) == 1) {
			if (buf[i] == '\n' || buf[i] == '\r') {
				buf[i]	= '\0';
				result	= ok;
				j	= i;
			}
			else	{
				++i;
			}
		}
		else	{
			j	= i;
		}
	}
	return	result;
}

static void prepare_record_buffer (arg_t* arg, size_t size) {
	if (size >= arg->record.size) {
		char*	new	= realloc (arg->record.buffer, size);
		if (new) {
			arg->record.buffer	= new;
			arg->record.size	= size;
//	Probably silly
# ifdef SO_RCVBUF
			while (size > 1024 && (setsockopt(arg->input, SOL_SOCKET, SO_RCVBUF,
			      (char *) &size, sizeof size) < 0)) {
					size -= 1024;
			}
# endif
		}
		else	{
			report_error (arg, ENOMEM);
			exit (EXIT_FAILURE);
		}
	}

}


long	get_number (int fd) {
	long	result	= -1L;
	char	number [(sizeof(long)*CHAR_BIT)/3];
	int	r	= read_str (fd, number, sizeof(number));
	if (r == ok){
		result	= strtol (number, 0, 10);
	}
	return	result;
}
/* ------------------ */
struct	openflags_t	{
	char*	name;
	int	flag;
};
typedef	struct	openflags_t	openflags_t;

# define	FLAG_ENTRY(x)	{ .name=#x, .flag=x,}
static	openflags_t openflags[]	= {
 	FLAG_ENTRY( O_RDONLY),
 	FLAG_ENTRY( O_WRONLY),
 	FLAG_ENTRY( O_RDWR),
 	FLAG_ENTRY( O_CREAT),
 	FLAG_ENTRY( O_TRUNC),
 	FLAG_ENTRY( O_APPEND),
 	FLAG_ENTRY( O_NONBLOCK),
};
static	size_t	N_OPENFLAGS	= sizeof(openflags)/sizeof(openflags[0]);
# undef	FLAG_ENTRY
static	int	openflag_lookup (char* name, int* mode) {
	int	result	= err;
	int	i	= 0;
	int	j	= N_OPENFLAGS;
	while (i!=j){
		if (strcmp (name, openflags[i].name)==0){
			j	= i;
			*mode	= openflags[i].flag;
			result	= ok;
		}
		else	{
			++i;
		}
	}
	return	result;
}
static	inline	int	isflag(char* f) { return f[0] == 'O' && f[1] == '_'; }
static	inline	int	iswhite(int ch) { return ch == ' ' || ch == '\t'; }

static	int	get_open_mode (char* rmtflags, int* rwmodep) {
	int	result	= ok;
	int	rend	= err;
	int	ch	= 0;
	char*	flag	= 0;
	int	rwmode	= 0;
	int	inflag	= false;
	while (result != rend) {
		ch	= *rmtflags;
		if (inflag) {
			if (!isalpha(ch)){
				int	mode	= 0;
				*rmtflags++	= '\0';
				inflag	= false;
				result	= openflag_lookup (flag, &mode);
				if (result == ok) {
					rwmode	|= mode;
				}
				if (ch == '\0') {
					rend	= result;
				}
			}
			else	++rmtflags;
		}
		else {
			if (ch=='\0') {
				rend	= result;
			}
			if (iswhite(ch)){
				++rmtflags;
			}
			else if (isflag (rmtflags)) {
				flag	= rmtflags;
				rmtflags= &rmtflags[2];
				inflag	= true;
			}
			else	result	= err;
		}
	}
	if (result == ok) {
		*rwmodep	= rwmode;
	}
	return	result;
}

/* --------------- */
/*
// Rmt v0 assumes the file/(tape)device exists
// but if dumping to a new file need to CREAT file
// rmt v1 can use symbolic modes O_RDWR|O_CREAT
// but for v0 clients we can use heuristic below
// to provide sensible flags to open(2).
//
// [ -e device -a -f device ]
//	"w" 		==> O_WRONLY | O_TRUNC
// 	"r" "rw"	==> O_RDWR
// [ -e device -a -c device ]
//	"r" "rw" "w"	==> requested
// [ ! -e device ]
//	"w" "rw"	==> requested | O_CREAT
//	"r"		==> ENOENT
*/
int	heuristic_mode (char* device, int* modep) {
	int	mode	= *modep;
	int	result	= ok;
	struct	stat	sb;
	if (lstat (device, &sb)==ok) {
		// File
		if (S_ISREG (sb.st_mode) || S_ISLNK (sb.st_mode) ) {
			if (mode==O_WRONLY) {
				mode	|= O_TRUNC;
				*modep	= mode;
			}	
		}
		// Tape?
		else if (S_ISCHR (sb.st_mode)) {
			;
		}
		// Refuse anything else
		else	{
			errno	= ENOTSUP;
			result	= err;
		}
	}
	else if (mode != O_RDONLY ) {
		mode	|= O_CREAT;
		*modep	= mode;
	}
	else	{
		errno	= ENOENT;
		result	= err;
	}
	return	result;
}
/* ----------------- */
static	void	cmd_write (arg_t* arg) {
	size_t	size	= get_number (arg->input);
	int	result	= ok;
	ssize_t	n	= 0;
	ssize_t	j	= size;
	char*	buffer	= 0;

	prepare_record_buffer (arg, size);
	buffer	= arg->record.buffer;

	while (n!=j) {
		ssize_t	nw = safe_read (arg->input, &buffer[n], size - n);
		if (nw > 0) {
			n	+= nw;
		}
		else	{
			report_error (arg, EIO);
			result	= err;
			j	= n;
		}	
	}
	if (result==ok) {
		n	= full_write (arg->tape, buffer, size);
		if (n >=0) {
			reply (arg, n);
		}
		else	{
			report_error (arg, errno);
		}
	}
}

static	void cmd_read (arg_t* arg) {
	size_t	size	= get_number (arg->input);
	ssize_t	n	= 0;
	char*	buffer	= 0;

	prepare_record_buffer (arg, size);
	buffer	= arg->record.buffer;

	n	= safe_read (arg->tape, buffer, size);
	if (n >= 0) {
		reply (arg, n);
		full_write (arg->output, buffer, n); 
	}
	else	{
		report_error (arg, errno);
	}
}

static	void	cmd_close (arg_t* arg) {
	char	device [PATH_MAX];
/*
//	Basically ignore the fd passed in Cn and close
//	the 'tape' device fd - there is only one.
*/
	int	result	= read_str (arg->input, device, sizeof(device));
	if (arg->tape >= 0) {
		result	= close (arg->tape); 
		if (result==ok) {
			arg->tape	= FD_NONE;
			arg->magtape	= false;
			reply (arg, 0);
		}	
		else	{
			report_error (arg, errno);
		}
	}
	else	{
		report_error (arg, EBADF);
	}
}
static	void	cmd_quit (arg_t* arg) {
	if (arg->tape != FD_NONE) {
		close (arg->tape);
	}
	reply (arg, 0);
	close (arg->input);
	close (arg->output);
	exit (EXIT_SUCCESS);
}
static	void	cmd_version (arg_t* arg) {
	char	dummy [PATH_MAX];
	int	result	= read_str (arg->input, dummy, sizeof(dummy));
	reply (arg, arg->vers);
}
/*
// Note: replies with the fd number ie if open()->7 then A7
*/
static	void	cmd_open (arg_t* arg) {
	char	device [PATH_MAX];
	char	rwmode [PATH_MAX];
	int	result	= read_str (arg->input, device, sizeof(device));
	if (result == ok) {
		result	= read_str (arg->input, rwmode, sizeof(rwmode));
		if (result==ok) {
			if (arg->tape != FD_NONE) {
				close (arg->tape);
				arg->tape	= FD_NONE;
			}
			
			if (access_check (device) == true) {
				int	mode	= 0;
				int	tape	= FD_NONE;
				char*	symmode	= rwmode;

				if (isdigit (rwmode[0])) {
					mode	= strtoul (rwmode, &symmode, 10);
				}
				if (arg->clntvers < 1) {
					result	= heuristic_mode (device, &mode);
				}
				else if (symmode[0] != '\0') {
					result	= get_open_mode (symmode, &mode);
				}
				if (result == ok) {
					// Open NONBLOCK so tape status can be returned
					mode	|= O_NONBLOCK;
					if ((mode & O_CREAT)!=0) {
						tape	= open (device, mode, 0660);
					}
					else	{
						tape	= open (device, mode);
					}
				}
				if (tape >= 0) {
					int	flags	= fcntl (tape, F_GETFL, 0);
					struct	stat	sbuf;
					if (flags >= 0) { // Don't need this after open()
						flags	&= ~O_NONBLOCK;
						fcntl (tape, F_SETFL, flags);
					}
					if (is_tape_dev (tape)) {
						arg->magtape	= true;
					}
					arg->tape	= tape;
					reply (arg, tape);
				}
				else	{
					report_error (arg, errno);
				}
			}
			else	{
				report_error (arg, EACCES);
			}	
		}
	}
}
static	void	cmd_seek (arg_t* arg) {
	int	result	= ok;
	long	where	= get_number (arg->input);
	off_t	offset	= get_number (arg->input);
	int	whence	= SEEK_SET;

	switch (where) {
	case	0:
		whence	= SEEK_SET;
	break;
	case	1:
		whence	= SEEK_CUR;
	break;
	case	2:
		whence	= SEEK_END;
	break;
	default:
		report_error (arg, EINVAL);
		result	= err;
	}
	if (result==ok) {
		off_t	to	= lseek (arg->tape, offset, whence);
		if (to != (off_t)(-1)) {
			reply (arg, to);
		}
		else	{
			report_error (arg, errno);
		}
	}
}

//
// Doesn't consume a terminal '\n'
// Not very useful - binary return
static	void	cmd_mtio_status (arg_t* arg) {
	mtio_status (arg);
}
static	void	cmd_mtio_status_ext (arg_t* arg) {
	mtio_status_ext (arg);
}

static	void	cmd_mtioctop (arg_t* arg) {
	mtioctop (arg);
}
static	void	cmd_mtioctop_ext (arg_t* arg) {
	mtioctop_ext (arg);
}

static	const	char null_cmd_ok[] = "Ss";
static	void	cmd_nop (arg_t* arg){
	if (strchr (null_cmd_ok, arg->lastcmd) != 0) {
		;	// Just ignore extraneous '\n'
	}
	else	{
		report_error_msg (arg, 0, "Null Command");
	}
}
typedef	void	(*cmd_t)(arg_t*);

static	cmd_t	CMDS [256] =	{
	['Q'] = cmd_quit,
	['q'] = cmd_quit,

	['O'] = cmd_open,
	['R'] = cmd_read,
	['W'] = cmd_write,
	['L'] = cmd_seek,
	['C'] = cmd_close,

	['I'] = cmd_mtioctop,
	['i'] = cmd_mtioctop_ext,
	['S'] = cmd_mtio_status,
	['s'] = cmd_mtio_status_ext,
	['v'] = cmd_version,
	['\n'] = cmd_nop,
};


int	do_rmt (arg_t* arg, char* base, int argc, char* argv[]) {
	int	result	= ok;
	char	command	= 0;
	while (1) {
		cmd_t	cmd	= 0;
		
		if (safe_read (arg->input, &command, 1) != 1)
			cmd_quit (arg);
		cmd	= CMDS [command];

		if (cmd) {
			cmd (arg);
			arg->lastcmd	= command;
		}
		else {
		      report_error (arg, ENOTSUP);
		      exit (EXIT_FAILURE);
		}
	}
	return	result;
}

int main (int argc, char **argv) {
	arg_t	arg	= {
		.input = STDIN_FILENO, .output = STDOUT_FILENO,
		.tape = FD_NONE,
		.vers = 1,
		.clntvers = 0,
		.lastcmd = 'Q',
		.record = { .buffer = 0, .size = 0, }
	};
	if (access_init () == err) {
		report_error (&arg, EACCES);
	  	exit (EXIT_FAILURE);
	}
//
//	Either invoke as a rmt process (argc==1)
//	or as "shell" -c "cmd" (argc==3)
//
//
//	Handles the commands (in cmd_extend.c) 
//
	if (argc == 3) {  // argv[0] "-c" "command"	
		int	result	= err;
		char*	cmd	= getenv ("SSH_ORIGINAL_COMMAND");
		if (cmd) {		// ?authorized_keys command=".. used
			argv [2]	= cmd;
		}
		cmd_extend (&arg, argc, argv); // doesn't return
	}
//
//	Handles the rmt V0.0 commands
//
	else if (argc == 1) {
		do_rmt (&arg, "rmt", argc, argv);
	}
	report_error (&arg, ENOTSUP);
	exit (EXIT_FAILURE);
}

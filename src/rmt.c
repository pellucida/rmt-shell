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
# include	<sys/mtio.h>

# include	<sys/socket.h>

# include	"constant.h"
# include	"util.h"
# include	"access.h"
# include	"rmt.h"
# include	"cmd_extend.h"

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

void	report_error (arg_t* arg, int error) {
	char	buf [BUFSIZ];
	snprintf (buf, sizeof(buf), "E%d\n%s\n", error, strerror (error));
	full_write (arg->output, buf, strlen (buf));
}


static	void	reply (arg_t* arg, int status) {
	char	buf [BUFSIZ];
	snprintf (buf, sizeof(buf), "A%ld\n", status);
	full_write (arg->output, buf, strlen (buf));
}
void	reply_ext (arg_t* arg, int status, char* more) {
	char	buf [BUFSIZ];
	snprintf (buf, sizeof(buf), "A%ld\n%s\n", status, more);
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
static	int	cmd_write (arg_t* arg) {
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
	return	result;
}

static	int	cmd_read (arg_t* arg) {
	size_t	size	= get_number (arg->input);
	int	result	= err;
	ssize_t	n	= 0;
	char*	buffer	= 0;

	prepare_record_buffer (arg, size);
	buffer	= arg->record.buffer;

	n	= safe_read (arg->tape, buffer, size);
	if (n >= 0) {
		reply (arg, n);
		full_write (arg->output, buffer, n); 
		result	= ok;
	}
	else	{
		report_error (arg, errno);
	}
	return	result;
}

static	int	cmd_close (arg_t* arg) {
	char	device [PATH_MAX];
	int	result	= read_str (arg->input, device, sizeof(device));
	if (arg->tape >= 0) {
		result	= close (arg->tape); 
		if (result==ok) {
			arg->tape	= -1;
			reply (arg, 0);
		}	
		else	{
			report_error (arg, errno);
		}
	}
	else	{
		report_error (arg, EBADF);
	}
	return	result;
}
static	int	cmd_quit (arg_t* arg) {
	if (arg->tape >= 0) {
		close (arg->tape);
	}
	reply (arg, 0);
	close (arg->input);
	close (arg->output);
	exit (EXIT_SUCCESS);
}

static	int	cmd_open (arg_t* arg) {
	char	device [PATH_MAX];
	char	rwmode [PATH_MAX];

	int	result	= read_str (arg->input, device, sizeof(device));
	if (result == ok) {
		result	= read_str (arg->input, rwmode, sizeof(rwmode));
		if (result==ok) {
			if (arg->tape) {
				close (arg->tape);
				arg->tape	= -1;
			}
			
			if (access_check (device) == true) {
				struct	stat	sb;
				int	mode	= strtoul (rwmode, 0, 10);
				int	tape	= -1;

// exists(device) && file(device) 
//	"w" 		==> TRUNC
// 	"r" "rw"	==>
// !exists(device) 
//	"w" "rw"	==> CREAT
//	"r"		==> ENOENT
				if (lstat (device, &sb)==ok) {
					// File
					if (S_ISREG (sb.st_mode) || S_ISLNK (sb.st_mode) ) {
						if (mode==O_WRONLY) {
							mode	|= O_TRUNC;
						}	
						tape	= open (device, mode);
						arg->magtape	= 0;
					}
					// Tape?
					else if (S_ISCHR (sb.st_mode)) {
						tape	= open (device, mode);
						arg->magtape	= 1;
					}
					// Refuse anything else
					else	{
						errno	= ENOTSUP;
					}
				}
				else if (mode != O_RDONLY ) {
					mode	|= O_CREAT;
					tape	= open (device, mode, 0660);
				}
				else	{
					errno	= ENOENT;
				}
				if (tape >= 0) {
					arg->tape	= tape;
					reply (arg, 0);
					result	= ok;
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
static	int	cmd_seek (arg_t* arg) {
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
			result	= err;
		}
	}
	return	result;
}

// Not very useful - binary return
static	int	cmd_mtio_status (arg_t* arg) {
	char	dummy [PATH_MAX];
	int	result	= ok;
//	read_str (arg->input, dummy, sizeof(dummy));

	
#ifdef MTIOCGET
	struct mtget	op;
	if (ioctl (arg->tape, MTIOCGET, (char *) &op) >= 0) {
		reply (arg, sizeof (op));
		full_write (arg->output, (char *) &op, sizeof (op));
	}
	else	{
		report_error (arg, ENOTTY);
		result	= err;
	}
#endif
	return	result;
}
static	int	cmd_mtioctop (arg_t* arg) {
	int	result	= ok;
	long	opreq	= get_number (arg->input);
	long	count	= get_number (arg->input);

#ifdef MTIOCTOP
	struct mtop	mtop	= { .mt_op	= opreq, .mt_count	= count, };
	if (ioctl (arg->tape, MTIOCTOP, (char *) &mtop)==ok) {
		reply (arg, count);
	}
	else	{
		report_error (arg, errno);
		result	= err;
	}
# endif
	return	result;
}

typedef	int	(*cmd_t)(arg_t*);

static	cmd_t	CMDS [256] =	{
	['Q'] = cmd_quit,
	['q'] = cmd_quit,

	['O'] = cmd_open,
	['R'] = cmd_read,
	['W'] = cmd_write,
	['L'] = cmd_seek,
	['C'] = cmd_close,

	['I'] = cmd_mtioctop,
	['S'] = cmd_mtio_status,
};


int	do_rmt (arg_t* arg, char* base, int argc, char* argv[]) {
	char	command	= 0;
	while (1) {
		cmd_t	cmd	= 0;
		
		if (safe_read (arg->input, &command, 1) != 1)
			cmd_quit (arg);
		cmd	= CMDS [command];

		if (cmd) {
			cmd (arg);
		}
		else {
		      report_error (arg, ENOTSUP);
		      exit (EXIT_FAILURE);
		}
	}
}

int main (int argc, char **argv) {
	arg_t	arg	= {
		.input = STDIN_FILENO, .output = STDOUT_FILENO,
		.tape = -1,
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

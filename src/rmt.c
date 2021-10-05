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


static	void	reply (arg_t* arg, long status) {
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
/*
//	Basically ignore the fd passed in Cn and close
//	the 'tape' device fd - there is only one.
*/
	int	result	= read_str (arg->input, device, sizeof(device));
	if (arg->tape >= 0) {
		result	= close (arg->tape); 
		if (result==ok) {
			arg->tape	= FD_NONE;
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
	if (arg->tape != FD_NONE) {
		close (arg->tape);
	}
	reply (arg, 0);
	close (arg->input);
	close (arg->output);
	exit (EXIT_SUCCESS);
}
static	int	cmd_version (arg_t* arg) {
	char	dummy [PATH_MAX];
	int	result	= read_str (arg->input, dummy, sizeof(dummy));
	reply (arg, arg->vers);
	return	result;
}
/*
// Note: replies with the fd number ie if open()->7 then A7
*/
static	int	cmd_open (arg_t* arg) {
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
					if (fstat (tape, &sbuf)==0) {
						if (S_ISCHR(sbuf.st_mode)) { 
							struct mtop	mtop	= { .mt_op = MTNOP, .mt_count = 1, };
							if (ioctl (tape, MTIOCTOP, (char *) &mtop)==ok) {
								arg->magtape	= 1;
							}
						}
					}
					arg->tape	= tape;
					reply (arg, tape);
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

//
// Doesn't consume a terminal '\n'
// Not very useful - binary return
static	int	cmd_mtio_status (arg_t* arg) {
	int	result	= err;
# ifdef MTIOCGET
	struct mtget	op;
	if (arg->magtape && ioctl (arg->tape, MTIOCGET, (char *) &op) >= 0) {
		reply (arg, sizeof (op));
		full_write (arg->output, (char *) &op, sizeof (op));
		result	= ok;
	}
	else	{
		report_error (arg, ENOTTY);
	}
# else
	report_error (arg, ENOTTY);
# endif
	return	result;
}
enum	{			// struct mtget field 
	MT_TYPE		= 'T',	// .mt_type
	MT_DSREG	= 'D',	// .mt_dsret
	MT_ERREG	= 'E',	// .mt_erreg
	MT_RESID	= 'R',	// .mt_resid
	MT_FILENO	= 'F',	// .mt_fileno
	MT_BLKNO	= 'B',	// .mt_blkno
	MT_FLAGS	= 'f',	// .mt_flags	- not linux
	MT_BF		= 'b',	// .mt_bf	- not linux
};
//
// 's' like 'S' command DOESN'T consume a '\n' [rmt(1)]
//
static	int	cmd_mtio_status_ext (arg_t* arg) {
	int	result	= ok;
	if (arg->clntvers > 0) {
#ifdef MTIOCGET
		char	subcmd [1];
		struct mtget	op;
		if (safe_read (arg->input, &subcmd[0], 1) == 1) {
			if (!arg->magtape) {
				report_error (arg, ENOTTY);
				result	= err;
			}
			else if (ioctl (arg->tape, MTIOCGET, (char *) &op) >= 0) {
				switch (subcmd[0]) {
				case	MT_TYPE:
					reply (arg, op.mt_type);
				break;
				case	MT_DSREG:
					reply (arg, op.mt_dsreg);
				break;
				case	MT_ERREG:
					reply (arg, op.mt_erreg);
				break;
				case	MT_RESID:
					reply (arg, op.mt_resid);
				break;
				case	MT_FILENO:
					reply (arg, op.mt_fileno);
				break;
				case	MT_BLKNO:
					reply (arg, op.mt_blkno);
				break;
				default:
					report_error (arg, EINVAL);
					result	= err;
				break;
				}
			}
			else	{
				report_error (arg, errno);
				result	= err;
			}
		}
		else	{
			report_error (arg, EINVAL);
			result	= err;
		}
	}
	else	{
		report_error (arg, ENOTSUP);
		result	= err;
	}
# else
		report_error (arg, ENOTSUP);
		result	= err;
	}		
#endif
	return	result;
}


#ifdef MTIOCTOP
/*
// Translation table for RMT V1.
*/
enum	{
	V1_WEOF = 0, V1_FSF = 1, V1_BSF = 2, V1_FSR = 3, 
	V1_BSR = 4, V1_REW = 5, V1_OFFL = 6, V1_NOP = 7,
	V1_RETEN = 8, V1_ERASE = 9, V1_EOM = 10, 
};
static	int mtio_table [] = {
	[V1_WEOF]	= MTWEOF,
	[V1_FSF]	= MTFSF,
	[V1_BSF]	= MTBSF,
	[V1_FSR]	= MTFSR,
	[V1_BSR]	= MTBSR,
	[V1_REW]	= MTREW,
	[V1_OFFL]	= MTOFFL,
	[V1_NOP]	= MTNOP,
	[V1_RETEN]	= MTRETEN,
	[V1_ERASE]	= MTERASE,
	[V1_EOM]	= MTEOM,
};
enum	{
	MAX_XLATE	= sizeof(mtio_table)/sizeof(mtio_table[0]),
};
static	inline	int	mtio_translate (int op) {
	int	result	= err;
	if (op >= 0 && op < MAX_XLATE) {
		result	= mtio_table [op];
	}
	return	result;
}
# endif
static	int	cmd_mtioctop (arg_t* arg) {
	int	result	= ok;
	long	opreq	= get_number (arg->input);
	long	count	= get_number (arg->input);

	if (opreq == -1L && count == 0) {
		arg->clntvers	= 1;
		reply (arg, 1);
	}
	else	{
#ifdef MTIOCTOP
		struct mtop	mtop	= { .mt_op	= opreq, .mt_count	= count, };
		if (arg->clntvers > 0) {
			int	xlate	= mtio_translate (opreq);
			if (xlate != err) {
				mtop.mt_op	= xlate;
			}
		}
		if (ioctl (arg->tape, MTIOCTOP, (char *) &mtop)==ok) {
			reply (arg, count);
		}
		else	{
			report_error (arg, errno);
			result	= err;
		}
# else
		report_error (arg, ENOTSUP);
		result		= err;
# endif
	}
	return	result;
}
#ifdef MTIOCTOP
enum	{
	V1i_CACHE	= 0,
	V1i_NOCACHE	= 1,
	V1i_RETEN	= 2,
	V1i_ERASE	= 3,
	V1i_EOM		= 4,
	V1i_NBSF	= 5,
};
static	int	mtio_ext_table[]	= {
# if	defined(MTCACHE)
	[V1i_CACHE]	= MTCACHE,
# else
	[V1i_CACHE]	= err,
# endif
# if	defined(MTNOCACHE)
	[V1i_NOCACHE]	= MTNOCACHE,
# else
	[V1i_NOCACHE]	= err,
# endif
# if	defined(MTRETEN)
	[V1i_RETEN]	= MTRETEN,
# else
	[V1i_RETEN]	= err,
# endif
# if	defined(MTERASE)
	[V1i_ERASE]	= MTERASE,
# else
	[V1i_ERASE]	= err,
# endif
# if	defined(MTEOM)
	[V1i_EOM]	= MTEOM,
# else
	[V1i_EOM]	= err,
# endif
# if	defined(MTNBSF)
	[V1i_NBSF]	= MTNBSF,
# else
	[V1i_NBSF]	= err,
# endif
};

enum	{
	MAX_EXT_XLATE	= sizeof(mtio_ext_table)/sizeof(mtio_ext_table[0]),
};
# endif

static	int	cmd_mtioctop_ext (arg_t* arg) {
	int	result	= ok;
	long	opreq	= get_number (arg->input);
	long	count	= get_number (arg->input);

# ifdef MTIOCTOP
	if (arg->clntvers >= 1) {
		struct mtop	mtop	= { .mt_op = opreq, .mt_count = count, };

		if (opreq < MAX_EXT_XLATE) {
			int	xlate	= mtio_ext_table [opreq];
			if (xlate != err) {
				mtop.mt_op	= xlate;
				if (ioctl (arg->tape, MTIOCTOP, (char *) &mtop)==ok) {
					reply (arg, count);
				}
				else	{
					report_error (arg, errno);
					result	= err;
				}
			}
			else	{
					report_error (arg, ENOTSUP);
					result	= err;
			}
		}
	}
	else	{
		report_error (arg, ENOTSUP);
		result	= err;
	}
# else
	report_error (arg, ENOTSUP);
	result	= err;
# endif
	return	result;
}

//
// Permit null command after 'S' 's'
//
static	const	char null_cmd_ok[] = "Ss";
static	int	cmd_nop (arg_t* arg){
	int	result	= ok;
	if (strchr (null_cmd_ok, arg->lastcmd) != 0) {
		;	// Just ignore extraneous '\n'
	}
	else	{
		report_error_msg (arg, 0, "Null Command");
		result	= err;
	}
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
	['i'] = cmd_mtioctop_ext,
	['S'] = cmd_mtio_status,
	['s'] = cmd_mtio_status_ext,
	['v'] = cmd_version,
	['\n'] = cmd_nop,
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
			arg->lastcmd	= command;
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

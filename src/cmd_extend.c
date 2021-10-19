# include	<stdlib.h>
# include	<string.h>
# include	<limits.h>
# include	<stdbool.h>
# include	<errno.h>
# include	<sys/types.h>
# include	<sys/stat.h>
# include	<sys/utsname.h>

# include	"util.h"
# include	"rmt.h"
# include	"access.h"
# include	"cmd_extend.h"

struct  entry { 
        char*   name;   
        int    (*action)(arg_t* arg, char*,int,char**);
};
typedef	struct	entry	entry_t;

# if	defined(RMT_MKDIR)
static  int    do_mkdir (arg_t* arg, char* base, int argc, char* argv[]) {
	int	result	= EXIT_FAILURE;
        if (argc == 2) { 
                if (access_check (argv[1]) == true) {
                	if (mkdir (argv[1], 0770)==ok) {
				reply_ext (arg, 0, "mkdir");
				result	= EXIT_SUCCESS;
			}
			else	{
				report_error (arg, errno);
			}
		}
		else {
			report_error (arg, EACCES);
		}
	}	
	else	{
		report_error (arg, EINVAL);
	}
	return	result;
}
# endif
# if	defined(RMT_STAT)
static  int    do_stat (arg_t* arg, char* base, int argc, char* argv[]) {
	int	result	= EXIT_FAILURE;
	
        if (argc == 2) { 
                if (access_check (argv[1]) == true) {
			struct	stat	sb;
			char	buf [PATH_MAX];
			if (stat (argv[1], &sb)==ok) {
				int	ftype	= decode_filetype (sb.st_mode);
				char*	owner	= getusername (sb.st_uid);
				char*	group	= getgroupname (sb.st_gid);
				int	len	= snprintf (buf, sizeof (buf), "%c %o %s %s %ld %s", ftype, sb.st_mode & 03777,
						owner, group, sb.st_size, argv[1]);
				reply_ext_nostatus (arg, buf);
				result	= EXIT_SUCCESS;
			}
			else	{
				report_error (arg, errno);
			}
		}
		else {
			report_error (arg, EACCES);
		}
	}	
	else	{
		report_error (arg, EINVAL);
	}
	return	result;
}
# endif
# if	defined(RMT_UNAME)
static  int    do_uname (arg_t* arg, char* base, int argc, char* argv[]) {
	struct	utsname	u;
	int	result	= EXIT_FAILURE;
	if (uname (&u) == ok) {
		size_t	len	= strlen (u.sysname);
		char	buf [len+1];
		memcpy (buf, u.sysname, len);
		buf[len]	= '\0';
		reply_ext_nostatus (arg, buf);
		result	= EXIT_SUCCESS;
	}
	else	{
		full_write (arg->output, "\n", 1);
	}
	return	result;
}
# endif

static	entry_t   table[] = {
	{ "rmt", do_rmt },

# if	defined(RMT_MKDIR)
        { "mkdir", do_mkdir }, 
# endif
# if	defined(RMT_STAT)
        { "stat", do_stat }, 
# endif
# if	defined(RMT_UNAME)
        { "uname", do_uname }, 
# endif
};
static  size_t  tablesize       = sizeof(table)/sizeof(table[0]);

static	int	lookup (char* base, struct entry** ep) {
	int	i	= 0;
	int	j	= tablesize;
	int	result	= false;
	struct	entry*	e	= 0;
	while (i!=j) {
		e	= &table [i];
		if (strcmp (base, e->name)==0) {
			*ep	= e;
			j	= i;
			result	= true;
		}
		else	{
			++i;
		}
	}
	return	result;
}

void	cmd_extend (arg_t* arg, int argc, char* argv[]) {
	int	result	= err;

	if (strcmp (argv[1], "-c")==0) {
		int	nargc	= 0;
		char**	nargv	= 0;
		char*	cmd	= 0;
		char*	base	= 0;
		struct	entry*	e	= 0;
		if (parse_args (argv[2], &nargc, &nargv) == ok) {
			cmd	= nargv[0];
			base	= basename (cmd);
			if (lookup (base, &e)==true) {
				exit (e->action (arg, base, nargc, nargv));
			}
			else	report_error (arg, ENOTSUP);
		}
		else	{
			report_error (arg, errno);
		}
	}
	else	report_error (arg, ENOTSUP);
	exit (EXIT_FAILURE);
}

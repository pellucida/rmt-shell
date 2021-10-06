/*
//  @(#) util.c
*/
# include	<stdio.h>
# include	<string.h>
# include	<stdlib.h>
# include	<sys/types.h>
# include	<sys/stat.h>
# include	<errno.h>
# include	<pwd.h>
# include	<grp.h>

# include	"util.h"



int	fgetline (FILE* input, char* line, size_t size) {
	int	result	= EOF;
	int	ch	= EOF;
	int	i	= 0;
	int	j	= size-1;

	while (i!=j) {
		ch	= fgetc (input);
		if (ch == EOF || ch == '\n') {
			j	= i;
		}
		else	{
			line [i++]	= ch;
		}
	}
	line [i]	= '\0';
	if (ch!=EOF || i!=0) {
		result	= i;
	}
	return	result;
}

char*	basename (char* path) {
	char*	result	= path;
	char*	t	= strrchr (path, '/');
	if (t && t[1] != '\0') {
		result	= t+1;
	}
	return	result;
}

int	decode_filetype (int st_mode) {
	int	type	= st_mode & S_IFMT;
	int	ftype	= '?';
	switch (type) {
	case	S_IFREG:
		ftype	= 'f';
	break;
	case	S_IFDIR:
		ftype	= 'd';
	break;
	case	S_IFSOCK:
		ftype	= 's';
	break;
	case	S_IFLNK:
		ftype	= 'l';
	break;
	case	S_IFIFO:
		ftype	= 'p';
	break;
	case	S_IFBLK:
		ftype	= 'b';
	break;
	case	S_IFCHR:
		ftype	= 'c';
	break;
	default:
		ftype	= '?';
	}
	return	ftype;
}

char*	getusername (uid_t uid) {
	static	char	buf[32];
	char*	result	= buf;

	struct	passwd*	pw	= getpwuid (uid);
	if (pw) {
		result	= pw->pw_name;
	}
	else	{
		snprintf (buf,sizeof(buf), "%ld", uid);
	}
	return	result;
}
char*	getgroupname (gid_t gid) {
	static	char	buf[32];
	char*	result	= buf;

	struct	group*	gp	= getgrgid (gid);
	if (gp) {
		result	= gp->gr_name;
	}
	else	{
		snprintf (buf,sizeof(buf), "%ld", gid);
	}
	return	result;
}

// create dir + "/" + last
static	char*	compose_path (char* dir, char* last) {
	size_t	len	= strlen (dir);
	size_t	more	= strlen (last);
	size_t	newsize	= len+1+more;
	char*	result	= realloc (dir, newsize);
	if (result) {
		result [len]   = '/';
		memcpy (&result[len+1], last, more);
	}
	return	result;
}

// resolve the path with realpath(3) for access checking
// but the last component may not yet exist
// ie program wants to create it.

char*	resolve_path (char* path) {
	size_t	len	= strlen (path);
	char	copy [len+1];
	memcpy (copy, path, len+1);
	char*	result	= realpath (copy, 0);	// malloc'd by realpath
	if (result==0) {
		if (errno == ENOENT) {		// assume last component doesn't exist
			char*	t	= strrchr (copy, '/');
			if (t) {
				size_t	more	= strlen (t+1);
				*t	= '\0';
				result	= realpath (copy, 0);	// try again with path prefix
				if (result) {	// put the bits back together
					result	= compose_path (result, t+1);
				}
			}
		}
	}
	return	result;
}

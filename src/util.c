/*
//  @(#) util.c
*/
# include	<stdio.h>
# include	<string.h>
# include	<sys/types.h>
# include	<sys/stat.h>
# include	<pwd.h>
# include	<grp.h>

# include	"constant.h"
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

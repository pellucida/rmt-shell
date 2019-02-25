/*
//	@(#) util.h
*/

# if	!defined(UTIL_H)
#	define	UTIL_H
# include	<stdio.h>

int	fgetline (FILE* input, char* line, size_t size);
char*	basename (char* path);
int	parse_args (char* line, int* nargcp, char*** nargvp);
int	decode_filetype (int st_mode);

char*	getusername (uid_t uid);
char*	getgroupname (gid_t uid);
char*	resolve_path (char* path);

# endif

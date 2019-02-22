/*
// @(#) access.c
*/
# include	<stdio.h>
# include	<string.h>
# include	<stdlib.h>
# include	<stdbool.h>
# include	<ctype.h>


# include	"constant.h"
# include	"util.h"
# include	"access.h"

enum	{
	DEFAULT_LISTSIZE	= 64,
};
static	struct	{
	size_t	size;
	size_t	used;
	char**	list;
} access 	= {.size = 0, .used = 0, .list = 0 };

static	int	access_grow (size_t size) {
	int	result	= err;
	if (size==0) {
		if (access.size == 0) {
			size	= DEFAULT_LISTSIZE;
		}
		else	{
			size	= access.size * 2;
		}
	}
	if (size > access.size) {
		char** new	= realloc (access.list, size);
		if (new) {
			access.list	= new;
			access.size	= size;
			result		= ok;
		}
	}
	return	result;
}
static	int	access_insert (char* path) {
	int	result	= err;
	if (access.used < access.size) {
		char*	copy	= strdup (path);
		if (copy) {
			access.list [access.used++]	= copy;
			result	= ok;
		}
	}
	else	{
		result	= access_grow (0);
		if (result == 0) {
			result	= access_insert (path);
		}
	}
	return	result;
}
	
static	int	access_readconfig (FILE* config) {
	char	line [BUFSIZ];
	int	result	= ok;
	while (fgetline (config, line, sizeof(line))!= EOF && result==ok) {
		char*	s	= strchr (line, '#');
		char*	start	= 0;
		if (s)
			*s	= '\0';
		start	= strchr (line, '/');
		if (start) {	// ==> start[0] == '/'
			int	ch	= 0;
			s	= start;
			while ((ch = *s) != '\0' && (isalnum (ch) || ispunct(ch))) 
				++s;
			*s	= '\0';
			if (start[0] != '\0') {
				result	= access_insert (start);
			}
		}
		else	{
			;
		}
	} 
	return	result;
}

int	access_init () {
	int	result	= err;
	FILE*	config	= fopen (RESTRICT_ACCESS, "r");
	if (config) {
		result	= access_readconfig (config);
		if (result==ok) {
			fclose (config);
		}
	}
	return	result;
}

int	access_check (char* devstr) {
	int	result	= false;
	int	i	= 0;
	int	j	= access.used;
	while (i!=j) {
		char* 	entry	= access.list [i];
		size_t	len	= strlen (entry);
		if (len > 0 && entry[len-1] == '/') {
			if (strncmp (entry, devstr, len)==0) {
				char*	t	= strstr (&devstr[len-1], "/..");
				if (t==0 || (t[3] != '\0' && t[3] != '/')) {
					result	= true;
				}
			}
		}
		else if (strcmp (entry, devstr)==0) {
			result	= true;
		}
		if (result == true) {
			j	= i;
		}
		else	{
			++i;
		}
	}
	return	result;
}


/*
// @(#) access.c
*/
# include	<stdio.h>
# include	<string.h>
# include	<stdlib.h>
# include	<stdbool.h>
# include	<ctype.h>
# include	<fnmatch.h>

# include	"util.h"
# include	"access.h"

# if	defined(USE_ACCESS)
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

static	int	check_simple (char* path) {
	int	result	= false;
	int	i	= 0;
	int	j	= access.used;
	while (i!=j) {
		char* 	entry	= access.list [i];
		if (fnmatch (entry, path, FNM_NOESCAPE|FNM_PATHNAME)==0) {
			result	= true;
			j	= i;
		}
		else	{
			++i;
		}
	}
	return	result;
}

// where devstr and resolved path differ check they are both permitted
// eg /tmp/null -> /dev/null
// check /tmp/null is ok, resolve /tmp/null into /dev/null check that too is ok

int	access_check (char* devstr) {
	int	result	= check_simple (devstr);;
	if (result) {
		char*	canonical	= resolve_path (devstr);
		if (canonical) {
			if (strcmp (devstr, canonical)!=0) {
				result	= check_simple (canonical);
			}
		}
	}
	return	result;
}
# endif

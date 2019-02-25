/*
//	parse_arg.c
*/
# include	<string.h>
# include	<stdlib.h>
// # include	<stdio.h>
# include	<stdbool.h>
# include	<errno.h>

# include	"constant.h"
# include	"util.h"

/*
//  [state] x [type] ==> [state, action]
//        [type] SPACE  ESCAPE QUOTE   SQUOT   OTHER  END
// [state]
//  START        START  WORD   STRING  WORD    WORD   FIN
//  WORD         START  WORD   STRING  WORD    WORD   FIN
//  STRING       STRING STRING WORD    STRING  STRING SERR
//  SSTRIN       SSTRIN SSTRIN SSTRIN  WORD    SSTRIN SERR
*/

enum state { FIN = 0, START = 1, WORD = 2, STRING = 3, SSTRIN = 4, OTHER = 5, SERR = 6, NSTATES };
typedef	enum state	state_t;

enum ctype { END = 0, ESCAPE = 1, SPACE = 2, QUOTE = 3, SQUOT = 4, };
typedef	enum ctype	ctype_t;
	
struct	word	{
	char*	start;
	char*	finish;
};
typedef	struct	word	word_t;

struct	args	{
	int	argc;
	char**	argv;
};
typedef	struct	args	args_t;

struct	category {
	ctype_t	type;
	int	ch;
};
typedef	struct	category categ_t;

static	categ_t	classify (char** sp) {
	char*	s	= *sp;
	ctype_t	type	= END;
	int	ch	= *s++;
	if (ch=='\\') {
		if (s[0] != '\0') {
			type	= ESCAPE;
			ch	= *s++;
		}
		else	{
			ch	= '\0';
		}
	}
	else if (ch=='"') {
		type	= QUOTE;
	}
	else if (ch=='\'') {
		type	= SQUOT;
	}
	else if (ch==' ' || ch=='\t' || ch=='\n') {
		type	= SPACE;
	}
	else if (ch=='\0') {
		type	= END;
	}
	else	{
		type	= OTHER;
	}
	*sp		= s;
	return	(categ_t) { .type = type, .ch = ch };
}

typedef	int	(*action_t)(args_t* args, word_t* word,  categ_t x);

// append char to the current argv string
static	int	copy (args_t* args, word_t* word, categ_t x) {
	*(word->finish++)	= x.ch;
}
// terminate the current argv string; start next
static	int	argv (args_t* args, word_t* word, categ_t x) {
	*(word->finish++)	= '\0';
	
	args->argv [args->argc++]	= word->start;
	word->start			= word->finish;
}
static	int	skip (args_t* args, word_t* word, categ_t x) {
	;
}

struct	state_action {
	state_t	next;
	action_t action;
};
typedef struct	state_action table_t;

static	table_t	table[][NSTATES]	= {
    [START] = {
	[SPACE]	 = { .next = START, .action = skip },
	[ESCAPE] = { .next = WORD,  .action = copy },
	[QUOTE]  = { .next = STRING,.action = skip },
	[SQUOT]  = { .next = SSTRIN,.action = skip },
	[OTHER]  = { .next = WORD,  .action = copy },
	[END]    = { .next = FIN,   .action = skip },
    },
    [WORD] = {
	[SPACE]	 = { .next = START, .action = argv },
	[ESCAPE] = { .next = WORD,  .action = copy },
	[QUOTE]  = { .next = STRING,.action = skip },
	[SQUOT]  = { .next = SSTRIN,.action = skip },
	[OTHER]  = { .next = WORD,  .action = copy },
	[END]    = { .next = FIN,   .action = argv },
    },
    [STRING]= {
	[SPACE]	 = { .next = STRING,.action = copy },
	[ESCAPE] = { .next = STRING,.action = copy },
	[QUOTE]  = { .next = WORD,  .action = skip },
	[SQUOT]  = { .next = STRING,.action = copy },
	[OTHER]  = { .next = STRING,.action = copy },
	[END]    = { .next = SERR,  .action = argv },
    },
    [SSTRIN]= {
	[SPACE]	 = { .next = SSTRIN,.action = copy },
	[ESCAPE] = { .next = SSTRIN,.action = copy },
	[QUOTE]  = { .next = SSTRIN,.action = copy },
	[SQUOT]  = { .next = WORD,  .action = skip },
	[OTHER]  = { .next = SSTRIN,.action = copy },
	[END]    = { .next = SERR,  .action = argv },
    },
};

//
// Take a line of text and break into words honoring quoting (")(')
// and escape (\) processing
// Create and return argc, argv[]
//
int	parse_args (char* line, int* argcp,char*** argvp) {
	int	result	= err;
	int	len	= strlen (line);
	args_t	args	= {
			.argc = 0, 
			.argv = calloc (len/2, sizeof (*args.argv)),
	};
	char*	copy	=  calloc (len+1,sizeof(*copy));
	word_t	word	= { .start = copy, .finish = copy, };	
	char*	s	= line;
	table_t	entry	= { .next = START, .action = skip };

	while (entry.next != FIN && entry.next != SERR) {
		categ_t	x = classify (&s);
		entry	= table [entry.next][x.type];
		entry.action (&args, &word, x);
	}
	if (entry.next == FIN) {
		*argcp	= args.argc;
		*argvp	= args.argv;
		result	= ok;
	}
	else	{
		errno	= EINVAL;
	}
	return	result;
}	

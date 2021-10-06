/*
// @(#) rmt_mtio.h - tape specific operations
*/
# if HAVE_MTIO_H

# include	<sys/mtio.h>
# endif
# include	<errno.h>
# include	<stdbool.h>
# include       <sys/types.h>
# include       <sys/stat.h>


# include	"util.h"
# include	"rmt.h"

# if 	!defined(MTIOCGET)

// Don't have mtio.h just stub out.

int	is_tape_dev (int tape) { return false; }
void	mtio_status (arg_t* arg) { report_error (arg, ENOTSUP); }
void	mtio_status_ext (arg_t* arg) { report_error (arg, ENOTSUP);}

// Support I-1\n0 for librmt to communicate protocol version
void	mtioctop (arg_t* arg) {
	long	opreq	= get_number (arg->input);
	long	count	= get_number (arg->input);
	if (opreq == -1L && count == 0) {
		arg->clntvers	= 1;
		reply (arg, 1);
	}
	else	report_error (arg, ENOTSUP);
}

// Just swallow 'i' args.
void	mtioctop_ext (arg_t* arg) {
	long	opreq	= get_number (arg->input);
	long	count	= get_number (arg->input);
	report_error (arg, ENOTSUP);
}
	
# else

int	is_tape_dev (int tape) {
	struct	stat sbuf;
	int	result	= false;
	if (fstat (tape, &sbuf)==0) {
		if (S_ISCHR(sbuf.st_mode)) { 
			struct mtop	mtop	= { .mt_op = MTNOP, .mt_count = 1, };
			if (ioctl (tape, MTIOCTOP, (char *) &mtop)==ok) {
				result	= true;
			}
		}
	}
	errno	= 0;
	return	result;
}

void	mtio_status (arg_t* arg) {
	struct mtget	op;
	if (arg->magtape && ioctl (arg->tape, MTIOCGET, (char *) &op) >= 0) {
		reply (arg, sizeof (op));
		full_write (arg->output, (char *) &op, sizeof (op));
	}
	else	{
		report_error (arg, ENOTTY);
	}
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
void	mtio_status_ext (arg_t* arg) {
	char	subcmd [1];
	struct mtget	op;
	if (safe_read (arg->input, &subcmd[0], 1) == 1) {
		if (!arg->magtape) {
			report_error (arg, ENOTTY);
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
			break;
			}
		}
		else	{
			report_error (arg, errno);
		}
	}
	else	{
		report_error (arg, EINVAL);
	}
}

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
void	mtioctop (arg_t* arg) {
	long	opreq	= get_number (arg->input);
	long	count	= get_number (arg->input);

	if (opreq == -1L && count == 0) {
		arg->clntvers	= 1;
		reply (arg, 1);
	}
	else	{
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
		}
	}
}
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
void	mtioctop_ext (arg_t* arg) {
	long	opreq	= get_number (arg->input);
	long	count	= get_number (arg->input);

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
				}
			}
			else	{
				report_error (arg, ENOTSUP);
			}
		}
	}
	else	{
		report_error (arg, ENOTSUP);
	}
}
# endif

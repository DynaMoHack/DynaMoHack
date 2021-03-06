/* Copyright (c) Stichting Mathematisch Centrum, Amsterdam, 1985. */
/* Copyright (c) M. Stephenson, 1990, 1991.			  */
/* Copyright (c) Dean Luick, 1990.				  */
/* DynaMoHack may be freely redistributed.  See license for details. */

#define MAKEDEFS_C	/* use to conditionally include file sections */
/* #define DEBUG */	/* uncomment for debugging info */

#ifdef __GNUC__
#define NORETURN __attribute__((noreturn))
#else
#define NORETURN
#endif

#include "config.h"
#include "permonst.h"
#include "objclass.h"
#include "monsym.h"
#include "artilist.h"
#include "dungeon.h"
#include "obj.h"
#include "monst.h"
#include "you.h"
#include "flag.h"
#include "dlb.h"

/* version information */
#include "dynamohack.h"
#include "patchlevel.h"

#define rewind(fp) fseek((fp),0L,SEEK_SET)	/* guarantee a return value */

static const char
    *Dont_Edit_Code =
	"/* This source file is generated by 'makedefs'.  Do not edit. */\n",
    *Dont_Edit_Data =
	"#\tThis data file is generated by 'makedefs'.  Do not edit. \n";

static struct version_info version;


static char	in_line[256];

#ifdef FILE_PREFIX
		/* if defined, a first argument not starting with - is
		 * taken as a text string to be prepended to any
		 * output filename generated */
char *file_prefix="";
#endif

int main(int,char **);

void do_objs(const char *);
void do_data(const char *,const char *);
void do_dungeon(const char *,const char *);
void do_date(const char *,boolean);
void do_monstr(const char *);
void do_permonst(const char *);
void do_questtxt(const char *,const char *);
void do_rumors(const char *,const char *,const char *);
void do_oracles(const char *,const char *);

extern void objects_init(void);	/* objects.c */

static void make_version(void);
static char *version_string(char *);
static char *version_id_string(char *,const char *);
static char *xcrypt(const char *);
static int check_control(char *);
static char *without_control(char *);
static boolean d_filter(char *);
static boolean h_filter(char *);
static boolean ranged_attk(const struct permonst*);
static int mstrength(const struct permonst *);

static boolean qt_comment(char *);
static boolean qt_control(char *);
static int get_hdr(char *);
static boolean new_id(char *);
static boolean known_msg(int,int);
static void new_msg(char *,int,int);
static void do_qt_control(char *);
static void do_qt_text(char *);
static void adjust_qt_hdrs(void);
static void put_qt_hdrs(void);

static char *tmpdup(const char *);
static char *limit(char *,int);
/* input, output, tmp */
static FILE *ifp, *ofp, *tfp;



static const char *usage_info[] = {
    "usage: %s [MODE] [FILENAMES]\n",
    "       %s -o [OUT (onames.h)]\n",
    "       %s -d [IN (data.base)] [OUT (data)]\n",
    "       %s -e [IN (dungeon.def)] [OUT (dungeon.pdf)]\n",
    "       %s -m [OUT (monstr.c)]\n",
    "       %s -v [OUT (date.h)] [OUT (options)]\n",
    "       %s -w [OUT (verinfo.h)] [OUT (options)]\n",
    "       %s -p [OUT (permonst.h)]\n",
    "       %s -q [IN (quest.txt)] [OUT (quest.dat)]\n",
    "       %s -r [IN (rumors.tru)] [IN (rumors.fal)] [OUT (rumors)]\n",
    "       %s -h [IN (oracles.txt)] [OUT (oracles)]\n",
};

static NORETURN void usage(char *argv0, char mode, int expected)
{
	int i;
	if (expected)
	    fprintf(stderr, "Error: incorrect number of args for mode -%c: %d args required\n", mode, expected);
	
	for (i = 0; i < sizeof(usage_info)/sizeof(usage_info[0]); i++)
	    fprintf(stderr, usage_info[i], argv0);
	
	exit(1);
}


int main(int argc, char	*argv[])
{
	if (argc < 3)
	    usage(argv[0], 0, 0);

	init_objlist();
	
	/* construct the current version number */
	make_version();
	
	switch (argv[1][1]) {
	    case 'o':
	    case 'O':
		if (argc != 3) usage(argv[0], argv[1][1], 3);
		do_objs(argv[2]);
		    break;
		    
	    case 'd':
	    case 'D':
		if (argc != 4) usage(argv[0], argv[1][1], 4);
		do_data(argv[2], argv[3]);
		    break;
		    
	    case 'e':
	    case 'E':	
		if (argc != 4) usage(argv[0], argv[1][1], 4);
		do_dungeon(argv[2], argv[3]);
		    break;
		    
	    case 'm':
	    case 'M':
		if (argc != 3) usage(argv[0], argv[1][1], 3);
		do_monstr(argv[2]);
		    break;
		    
	    case 'v':
	    case 'V':
		if (argc != 3) usage(argv[0], argv[1][1], 3);
		do_date(argv[2], TRUE);
		    break;

	    case 'w':
	    case 'W':
		if (argc != 3) usage(argv[0], argv[1][1], 3);
		do_date(argv[2], FALSE);
		    break;
		    
	    case 'p':
	    case 'P':
		if (argc != 3) usage(argv[0], argv[1][1], 3);
		do_permonst(argv[2]);
		    break;
		    
	    case 'q':
	    case 'Q':
		if (argc != 4) usage(argv[0], argv[1][1], 4);
		do_questtxt(argv[2], argv[3]);
		    break;
		    
	    case 'r':
	    case 'R':
		if (argc != 5) usage(argv[0], argv[1][1], 5);
		do_rumors(argv[2], argv[3], argv[4]);
		    break;
		    
	    case 'h':
	    case 'H':
		if (argc != 4) usage(argv[0], argv[1][1], 4);
		do_oracles(argv[2], argv[3]);
		    break;

	    default:
		fprintf(stderr, "Error: unknown mode -%c\n", argv[1][1]);
		usage(argv[0], 0, 0);
		return 1;
	}
	
	free(objects);
	return 0;
}


/* trivial text encryption routine which can't be broken with `tr' */
/* duplicated in src/hacklib.c */
static char *xcrypt(const char *str)
{
	static char buf[BUFSZ];
	const char *p;
	char *q;
	int bitmask;

	for (bitmask = 1, p = str, q = buf; *p; q++) {
		*q = *p++;
		if (*q & (32|64)) *q ^= bitmask;
		if ((bitmask <<= 1) >= 32) bitmask = 1;
	}
	*q = '\0';
	return buf;
}

void do_rumors(const char *in_tru, const char *in_false, const char *outfile)
{
	long	true_rumor_size;

	if (!(ofp = fopen(outfile, WRTMODE))) {
		perror(outfile);
		exit(EXIT_FAILURE);
	}
	fprintf(ofp, "%s", Dont_Edit_Data);

	if (!(ifp = fopen(in_tru, RDTMODE))) {
		perror(in_tru);
		fclose(ofp);
		unlink(outfile);	/* kill empty output file */
		exit(EXIT_FAILURE);
	}

	/* get size of true rumors file */
	fseek(ifp, 0L, SEEK_END);
	true_rumor_size = ftell(ifp);
	fprintf(ofp,"%06lx\n", true_rumor_size);
	fseek(ifp, 0L, SEEK_SET);

	/* copy true rumors */
	while (fgets(in_line, sizeof in_line, ifp) != 0)
		fputs(xcrypt(in_line), ofp);

	fclose(ifp);

	if (!(ifp = fopen(in_false, RDTMODE))) {
		perror(in_false);
		fclose(ofp);
		unlink(outfile);	/* kill incomplete output file */
		exit(EXIT_FAILURE);
	}

	/* copy false rumors */
	while (fgets(in_line, sizeof in_line, ifp) != 0)
		fputs(xcrypt(in_line), ofp);

	fclose(ifp);
	fclose(ofp);
	return;
}



static void make_version(void)
{
	int i;

	/*
	 * integer version number
	 */
	version.incarnation = ((unsigned long)VERSION_MAJOR << 24) |
				((unsigned long)VERSION_MINOR << 16) |
				((unsigned long)PATCHLEVEL << 8) |
				((unsigned long)EDITLEVEL);
	/*
	 * encoded feature list
	 * Note:  if any of these magic numbers are changed or reassigned,
	 * EDITLEVEL in patchlevel.h should be incremented at the same time.
	 * The actual values have no special meaning, and the category
	 * groupings are just for convenience.
	 */
	version.feature_set = (unsigned long)(0L
		/* levels and/or topology (0..4) */
		/* monsters (5..9) */
#ifdef KOPS
			| (1L <<  6)
#endif
		/* objects (10..14) */
		/* flag bits and/or other global variables (15..26) */
			);
	/*
	 * Value used for object & monster sanity check.
	 *    (NROFARTIFACTS<<24) | (NUM_OBJECTS<<12) | (NUMMONS<<0)
	 */
	for (i = 1; artifact_names[i]; i++) continue;
	version.entity_count = (unsigned long)(i - 1);
	for (i = 1; objects[i].oc_class != ILLOBJ_CLASS; i++) continue;
	version.entity_count = (version.entity_count << 12) | (unsigned long)i;
	for (i = 0; mons[i].mlet; i++) continue;
	version.entity_count = (version.entity_count << 12) | (unsigned long)i;

	return;
}

static char *version_string(char *outbuf)
{
    sprintf(outbuf, "%d.%d.%d", VERSION_MAJOR, VERSION_MINOR, PATCHLEVEL);
    return outbuf;
}

static char *version_id_string(char *outbuf, const char *build_date)
{
    char subbuf[64], versbuf[64];

    subbuf[0] = '\0';
#ifdef PORT_SUB_ID
    subbuf[0] = ' ';
    strcpy(&subbuf[1], PORT_SUB_ID);
#endif
#ifdef BETA
    strcat(subbuf, " Beta");
#endif

    sprintf(outbuf, "%s DynaMoHack%s Version %s - last build %s.",
	    PORT_ID, subbuf, version_string(versbuf), build_date);
    return outbuf;
}


void do_date(const char *outfile, boolean printdates)
{
	time_t clocktim = 0;
	char *c, cbuf[60], buf[BUFSZ];

	if (!(ofp = fopen(outfile, WRTMODE))) {
		perror(outfile);
		exit(EXIT_FAILURE);
	}
	fprintf(ofp, "%s", Dont_Edit_Code);

	time(&clocktim);
	strcpy(cbuf, ctime((time_t *)&clocktim));
	for (c = cbuf; *c; c++) if (*c == '\n') break;
	*c = '\0';	/* strip off the '\n' */

	if (printdates) {
	    fprintf(ofp,"#define BUILD_DATE \"%s\"\n", cbuf);
	    fprintf(ofp,"#define BUILD_TIME (%ldL)\n", clocktim);
	    fprintf(ofp,"\n");
	}

	fprintf(ofp,"#define VERSION_NUMBER 0x%08xU\n",
		version.incarnation);
	fprintf(ofp,"#define VERSION_FEATURES 0x%08xU\n",
		version.feature_set);
#ifdef IGNORED_FEATURES
	fprintf(ofp,"#define IGNORED_FEATURES 0x%08xU\n",
		(unsigned int) IGNORED_FEATURES);
#endif
	fprintf(ofp,"#define VERSION_SANITY1 0x%08xU\n",
		version.entity_count);
	fprintf(ofp,"\n");
	fprintf(ofp,"#define VERSION_STRING \"%s\"\n", version_string(buf));
	fprintf(ofp,"#define VERSION_ID \\\n \"%s\"\n",
		version_id_string(buf, cbuf));
	fprintf(ofp,"\n");
	fclose(ofp);
	return;
}


/* routine to decide whether to discard something from data.base */
static boolean d_filter(char *line)
{
    if (*line == '#') return TRUE;	/* ignore comment lines */
    return FALSE;
}

   /*
    *
	New format (v3.1) of 'data' file which allows much faster lookups [pr]
"do not edit"		first record is a comment line
01234567		hexadecimal formatted offset to text area
name-a			first name of interest
123,4			offset to name's text, and number of lines for it
name-b			next name of interest
name-c			multiple names which share same description also
456,7			share a single offset,count line
.			sentinel to mark end of names
789,0			dummy record containing offset, count of EOF
text-a			4 lines of descriptive text for name-a
text-a			at file position 0x01234567L + 123L
text-a
text-a
text-b/text-c		7 lines of text for names-b and -c
text-b/text-c		at fseek(0x01234567L + 456L)
...
    *
    */

void do_data(const char *infile, const char *outfile)
{
	char	tempfile[256];
	boolean ok;
	long	txt_offset;
	int	entry_cnt, line_cnt;

	sprintf(tempfile, "%s.%s", outfile, "tmp");

	if (!(ifp = fopen(infile, RDTMODE))) {		/* data.base */
		perror(infile);
		exit(EXIT_FAILURE);
	}
	if (!(ofp = fopen(outfile, WRTMODE))) {	/* data */
		perror(outfile);
		fclose(ifp);
		exit(EXIT_FAILURE);
	}
	if (!(tfp = fopen(tempfile, WRTMODE))) {	/* data.tmp */
		perror(tempfile);
		fclose(ifp);
		fclose(ofp);
		unlink(outfile);
		exit(EXIT_FAILURE);
	}

	/* output a dummy header record; we'll rewind and overwrite it later */
	fprintf(ofp, "%s%08lx\n", Dont_Edit_Data, 0L);

	entry_cnt = line_cnt = 0;
	/* read through the input file and split it into two sections */
	while (fgets(in_line, sizeof in_line, ifp)) {
	    if (d_filter(in_line)) continue;
	    if (*in_line > ' ') {	/* got an entry name */
		/* first finish previous entry */
		if (line_cnt)  fprintf(ofp, "%d\n", line_cnt),  line_cnt = 0;
		/* output the entry name */
		fputs(in_line, ofp);
		entry_cnt++;		/* update number of entries */
	    } else if (entry_cnt) {	/* got some descriptive text */
		/* update previous entry with current text offset */
		if (!line_cnt)  fprintf(ofp, "%ld,", ftell(tfp));
		/* save the text line in the scratch file */
		fputs(in_line, tfp);
		line_cnt++;		/* update line counter */
	    }
	}
	/* output an end marker and then record the current position */
	if (line_cnt)  fprintf(ofp, "%d\n", line_cnt);
	fprintf(ofp, ".\n%ld,%d\n", ftell(tfp), 0);
	txt_offset = ftell(ofp);
	fclose(ifp);		/* all done with original input file */

	/* reprocess the scratch file; 1st format an error msg, just in case */
	sprintf(in_line, "rewind of \"%s\"", tempfile);
	if (rewind(tfp) != 0)  goto dead_data;
	/* copy all lines of text from the scratch file into the output file */
	while (fgets(in_line, sizeof in_line, tfp))
	    fputs(in_line, ofp);

	/* finished with scratch file */
	fclose(tfp);
	unlink(tempfile);	/* remove it */

	/* update the first record of the output file; prepare error msg 1st */
	sprintf(in_line, "rewind of \"%s\"", outfile);
	ok = (rewind(ofp) == 0);
	if (ok) {
	   sprintf(in_line, "header rewrite of \"%s\"", outfile);
	   ok = (fprintf(ofp, "%s%08lx\n", Dont_Edit_Data, txt_offset) >= 0);
	}
	if (!ok) {
dead_data:  perror(in_line);	/* report the problem */
	    /* close and kill the aborted output file, then give up */
	    fclose(ofp);
	    unlink(outfile);
	    exit(EXIT_FAILURE);
	}

	/* all done */
	fclose(ofp);

	return;
}

/* routine to decide whether to discard something from oracles.txt */
static boolean h_filter(char *line)
{
    static boolean skip = FALSE;
    char tag[sizeof in_line];

    if (*line == '#') return TRUE;	/* ignore comment lines */
    if (sscanf(line, "----- %s", tag) == 1) {
	skip = FALSE;
    } else if (skip && !strncmp(line, "-----", 5))
	skip = FALSE;
    return skip;
}

static const char *special_oracle[] = {
	"\"...it is rather disconcerting to be confronted with the",
	"following theorem from [Baker, Gill, and Solovay, 1975].",
	"",
	"Theorem 7.18  There exist recursive languages A and B such that",
	"  (1)  P(A) == NP(A), and",
	"  (2)  P(B) != NP(B)",
	"",
	"This provides impressive evidence that the techniques that are",
	"currently available will not suffice for proving that P != NP or          ",
	"that P == NP.\"  [Garey and Johnson, p. 185.]"
};

/*
   The oracle file consists of a "do not edit" comment, a decimal count N
   and set of N+1 hexadecimal fseek offsets, followed by N multiple-line
   records, separated by "---" lines.  The first oracle is a special case.
   The input data contains just those multi-line records, separated by
   "-----" lines.
 */

void do_oracles(const char *infile, const char *outfile)
{
	char	tempfile[256];
	boolean in_oracle, ok;
	long	txt_offset, offset, fpos;
	int	oracle_cnt;
	int i;

	sprintf(tempfile, "%s.%s", outfile, "tmp");

	if (!(ifp = fopen(infile, RDTMODE))) {
		perror(infile);
		exit(EXIT_FAILURE);
	}
	if (!(ofp = fopen(outfile, WRTMODE))) {
		perror(outfile);
		fclose(ifp);
		exit(EXIT_FAILURE);
	}
	if (!(tfp = fopen(tempfile, WRTMODE))) {	/* oracles.tmp */
		perror(tempfile);
		fclose(ifp);
		fclose(ofp);
		unlink(outfile);
		exit(EXIT_FAILURE);
	}

	/* output a dummy header record; we'll rewind and overwrite it later */
	fprintf(ofp, "%s%5d\n", Dont_Edit_Data, 0);

	/* handle special oracle; it must come first */
	fputs("---\n", tfp);
	fprintf(ofp, "%05lx\n", ftell(tfp));  /* start pos of special oracle */
	for (i = 0; i < SIZE(special_oracle); i++) {
	    fputs(xcrypt(special_oracle[i]), tfp);
	    fputc('\n', tfp);
	}

	oracle_cnt = 1;
	fputs("---\n", tfp);
	fprintf(ofp, "%05lx\n", ftell(tfp));	/* start pos of first oracle */
	in_oracle = FALSE;

	while (fgets(in_line, sizeof in_line, ifp)) {

	    if (h_filter(in_line)) continue;
	    if (!strncmp(in_line, "-----", 5)) {
		if (!in_oracle) continue;
		in_oracle = FALSE;
		oracle_cnt++;
		fputs("---\n", tfp);
		fprintf(ofp, "%05lx\n", ftell(tfp));
		/* start pos of this oracle */
	    } else {
		in_oracle = TRUE;
		fputs(xcrypt(in_line), tfp);
	    }
	}

	if (in_oracle) {	/* need to terminate last oracle */
	    oracle_cnt++;
	    fputs("---\n", tfp);
	    fprintf(ofp, "%05lx\n", ftell(tfp));	/* eof position */
	}

	/* record the current position */
	txt_offset = ftell(ofp);
	fclose(ifp);		/* all done with original input file */

	/* reprocess the scratch file; 1st format an error msg, just in case */
	sprintf(in_line, "rewind of \"%s\"", tempfile);
	if (rewind(tfp) != 0)  goto dead_data;
	/* copy all lines of text from the scratch file into the output file */
	while (fgets(in_line, sizeof in_line, tfp))
	    fputs(in_line, ofp);

	/* finished with scratch file */
	fclose(tfp);
	unlink(tempfile);	/* remove it */

	/* update the first record of the output file; prepare error msg 1st */
	sprintf(in_line, "rewind of \"%s\"", outfile);
	ok = (rewind(ofp) == 0);
	if (ok) {
	    sprintf(in_line, "header rewrite of \"%s\"", outfile);
	    ok = (fprintf(ofp, "%s%5d\n", Dont_Edit_Data, oracle_cnt) >=0);
	}
	if (ok) {
	    sprintf(in_line, "data rewrite of \"%s\"", outfile);
	    for (i = 0; i <= oracle_cnt; i++) {
		if (!(ok = (fpos = ftell(ofp)) >= 0)) break;
		if (!(ok = (fseek(ofp, fpos, SEEK_SET) >= 0))) break;
		if (!(ok = (fscanf(ofp, "%5lx", &offset) == 1))) break;
		if (!(ok = (fseek(ofp, fpos, SEEK_SET) >= 0))) break;
		if (!(ok = (fprintf(ofp, "%05lx\n", offset + txt_offset) >= 0)))
		    break;
	    }
	}
	if (!ok) {
dead_data:  perror(in_line);	/* report the problem */
	    /* close and kill the aborted output file, then give up */
	    fclose(ofp);
	    unlink(outfile);
	    exit(EXIT_FAILURE);
	}

	/* all done */
	fclose(ofp);

	return;
}


static	struct deflist {

	const char	*defname;
	boolean	true_or_false;
} deflist[] = {
	      {	"REINCARNATION", TRUE },
	      { 0, 0 } };

static int check_control(char *s)
{
	int	i;

	if (s[0] != '%') return -1;

	for (i = 0; deflist[i].defname; i++)
	    if (!strncmp(deflist[i].defname, s+1, strlen(deflist[i].defname)))
		return i;

	return -1;
}

static char *without_control(char *s)
{
	return s + 1 + strlen(deflist[check_control(in_line)].defname);
}

void do_dungeon(const char *infile, const char *outfile)
{
	int rcnt = 0;

	if (!(ifp = fopen(infile, RDTMODE))) {
		perror(infile);
		exit(EXIT_FAILURE);
	}
	if (!(ofp = fopen(outfile, WRTMODE))) {
		perror(outfile);
		exit(EXIT_FAILURE);
	}
	
	fprintf(ofp, "%s", Dont_Edit_Data);

	while (fgets(in_line, sizeof in_line, ifp) != 0) {
	    rcnt++;
	    if (in_line[0] == '#') continue;	/* discard comments */
recheck:
	    if (in_line[0] == '%') {
		int i = check_control(in_line);
		if (i >= 0) {
		    if (!deflist[i].true_or_false)  {
			while (fgets(in_line, sizeof in_line, ifp) != 0)
			    if (check_control(in_line) != i) goto recheck;
		    } else
			fputs(without_control(in_line),ofp);
		} else {
		    fprintf(stderr, "Unknown control option '%s' in file %s at line %d.\n",
			    in_line, infile, rcnt);
		    exit(EXIT_FAILURE);
		}
	    } else
		fputs(in_line,ofp);
	}
	fclose(ifp);
	fclose(ofp);

	return;
}

/* returns TRUE if monster can attack at range */
static boolean ranged_attk(const struct permonst *ptr)
{
	int	i, j;
	int atk_mask = (1<<AT_BREA) | (1<<AT_SPIT) | (1<<AT_GAZE);

	for (i = 0; i < NATTK; i++) {
	    if ((j=ptr->mattk[i].aatyp) >= AT_WEAP || (atk_mask & (1<<j)))
		return TRUE;
	}

	return FALSE;
}

/* This routine is designed to return an integer value which represents
 * an approximation of monster strength.  It uses a similar method of
 * determination as "experience()" to arrive at the strength.
 */
static int mstrength(const struct permonst *ptr)
{
	int	i, tmp2, n, tmp = ptr->mlevel;

	if (tmp > 49)		/* special fixed hp monster */
	    tmp = 2*(tmp - 6) / 4;

/*	For creation in groups */
	n = (!!(ptr->geno & G_SGROUP));
	n += (!!(ptr->geno & G_LGROUP)) << 1;

/*	For ranged attacks */
	if (ranged_attk(ptr)) n++;

/*	For higher ac values */
	n += (ptr->ac < 4);
	n += (ptr->ac < 0);

/*	For very fast monsters */
	n += (ptr->mmove >= 18);

/*	For each attack and "special" attack */
	for (i = 0; i < NATTK; i++) {

	    tmp2 = ptr->mattk[i].aatyp;
	    n += (tmp2 > 0);
	    n += (tmp2 == AT_MAGC);
	    n += (tmp2 == AT_WEAP && (ptr->mflags2 & M2_STRONG));
	}

/*	For each "special" damage type */
	for (i = 0; i < NATTK; i++) {

	    tmp2 = ptr->mattk[i].adtyp;
	    if ((tmp2 == AD_DRLI) || (tmp2 == AD_STON) || (tmp2 == AD_DRST)
		|| (tmp2 == AD_DRDX) || (tmp2 == AD_DRCO) || (tmp2 == AD_WERE))
			n += 2;
	    else if (strcmp(ptr->mname, "grid bug")) n += (tmp2 != AD_PHYS);
	    n += ((int) (ptr->mattk[i].damd * ptr->mattk[i].damn) > 23);
	}

/*	Leprechauns are special cases.  They have many hit dice so they
	can hit and are hard to kill, but they don't really do much damage. */
	if (!strcmp(ptr->mname, "leprechaun")) n -= 2;

/*	Finally, adjust the monster level  0 <= n <= 24 (approx.) */
	if (n == 0) tmp--;
	else if (n >= 6) tmp += ( n / 2 );
	else tmp += ( n / 3 + 1);

	return (tmp >= 0) ? tmp : 0;
}

void do_monstr(const char *outfile)
{
    const struct permonst *ptr;
    int i, j;

    /*
     * create the source file, "monstr.c"
     */
    if (!(ofp = fopen(outfile, WRTMODE))) {
	perror(outfile);
	exit(EXIT_FAILURE);
    }
    
    fprintf(ofp, "%s", Dont_Edit_Code);
    fprintf(ofp,"#include \"config.h\"\n");
    fprintf(ofp,"\nconst int monstr[] = {\n");
    for (ptr = &mons[0], j = 0; ptr->mlet; ptr++) {
	i = mstrength(ptr);
	fprintf(ofp,"%2d,%c", i, (++j & 15) ? ' ' : '\n');
    }
    /* might want to insert a final 0 entry here instead of just newline */
    fprintf(ofp,"%s};\n", (j & 15) ? "\n" : "");

    fprintf(ofp,"\n/*monstr.c*/\n");

    fclose(ofp);
    return;
}

void do_permonst(const char *outfile)
{
	int	i;
	char	*c, *nam;

	if (!(ofp = fopen(outfile, WRTMODE))) {
		perror(outfile);
		exit(EXIT_FAILURE);
	}
	
	fprintf(ofp,"%s", Dont_Edit_Code);
	fprintf(ofp,"#ifndef PM_H\n#define PM_H\n");

	if (strcmp(mons[0].mname, "playermon") != 0)
		fprintf(ofp,"\n#define\tPM_PLAYERMON\t(-1)");

	for (i = 0; mons[i].mlet; i++) {
		fprintf(ofp,"\n#define\tPM_");
		if (mons[i].mlet == S_HUMAN &&
				!strncmp(mons[i].mname, "were", 4))
		    fprintf(ofp, "HUMAN_");
		for (nam = c = tmpdup(mons[i].mname); *c; c++)
		    if (*c >= 'a' && *c <= 'z') *c -= (char)('a' - 'A');
		    else if (*c < 'A' || *c > 'Z') *c = '_';
		fprintf(ofp,"%s\t%d", nam, i);
	}
	fprintf(ofp,"\n\n#define\tNUMMONS\t%d\n", i);
	fprintf(ofp,"\n#endif /* PM_H */\n");
	fclose(ofp);
	return;
}


/*	Start of Quest text file processing. */
#include "qtext.h"

static struct qthdr	qt_hdr;
static struct msghdr	msg_hdr[N_HDR];
static struct qtmsg	*curr_msg;

static int	qt_line;

static boolean	in_msg;
#define NO_MSG	1	/* strlen of a null line returned by fgets() */

static boolean qt_comment(char *s)
{
	if (s[0] == '#') return TRUE;
	return (boolean)(!in_msg  && strlen(s) == NO_MSG);
}

static boolean qt_control(char *s)
{
	return (boolean)(s[0] == '%' && (s[1] == 'C' || s[1] == 'E'));
}

static int get_hdr (char *code)
{
	int	i;

	for (i = 0; i < qt_hdr.n_hdr; i++)
	    if (!strncmp(code, qt_hdr.id[i], LEN_HDR)) return ++i;

	return 0;
}

static boolean new_id (char *code)
{
	if (qt_hdr.n_hdr >= N_HDR) {
	    fprintf(stderr, OUT_OF_HEADERS, qt_line);
	    return FALSE;
	}

	strncpy(&qt_hdr.id[qt_hdr.n_hdr][0], code, LEN_HDR);
	msg_hdr[qt_hdr.n_hdr].n_msg = 0;
	qt_hdr.offset[qt_hdr.n_hdr++] = 0L;
	return TRUE;
}

static boolean known_msg(int num, int id)
{
	int i;

	for (i = 0; i < msg_hdr[num].n_msg; i++)
	    if (msg_hdr[num].qt_msg[i].msgnum == id) return TRUE;

	return FALSE;
}


static void new_msg(char *s, int num, int id)
{
	struct	qtmsg	*qt_msg;

	if (msg_hdr[num].n_msg >= N_MSG) {
		fprintf(stderr, OUT_OF_MESSAGES, qt_line);
	} else {
		qt_msg = &(msg_hdr[num].qt_msg[msg_hdr[num].n_msg++]);
		qt_msg->msgnum = id;
		qt_msg->delivery = s[2];
		qt_msg->offset = qt_msg->size = 0L;

		curr_msg = qt_msg;
	}
}

static void do_qt_control(char *s)
{
	char code[BUFSZ];
	int num, id = 0;

	switch(s[1]) {

	    case 'C':	if (in_msg) {
			    fprintf(stderr, CREC_IN_MSG, qt_line);
			    break;
			} else {
			    in_msg = TRUE;
			    if (sscanf(&s[4], "%s %5d", code, &id) != 2) {
			    	fprintf(stderr, UNREC_CREC, qt_line);
			    	break;
			    }
			    num = get_hdr(code);
			    if (!num && !new_id(code))
			    	break;
			    num = get_hdr(code)-1;
			    if (known_msg(num, id))
			    	fprintf(stderr, DUP_MSG, qt_line);
			    else new_msg(s, num, id);
			}
			break;

	    case 'E':	if (!in_msg) {
			    fprintf(stderr, END_NOT_IN_MSG, qt_line);
			    break;
			} else in_msg = FALSE;
			break;

	    default:	fprintf(stderr, UNREC_CREC, qt_line);
			break;
	}
}

static void do_qt_text(char *s)
{
	if (!in_msg) {
	    fprintf(stderr, TEXT_NOT_IN_MSG, qt_line);
	}
	curr_msg->size += strlen(s);
	return;
}

static void adjust_qt_hdrs(void)
{
	int	i, j;
	long count = 0L, hdr_offset = sizeof(int) +
			(sizeof(char)*LEN_HDR + sizeof(long)) * qt_hdr.n_hdr;

	for (i = 0; i < qt_hdr.n_hdr; i++) {
	    qt_hdr.offset[i] = hdr_offset;
	    hdr_offset += sizeof(int) + sizeof(struct qtmsg) * msg_hdr[i].n_msg;
	}

	for (i = 0; i < qt_hdr.n_hdr; i++)
	    for (j = 0; j < msg_hdr[i].n_msg; j++) {

		msg_hdr[i].qt_msg[j].offset = hdr_offset + count;
		count += msg_hdr[i].qt_msg[j].size;
	    }
	return;
}

static void put_qt_hdrs(void)
{
	int	i, count;

	/*
	 *	The main header record.
	 */
#ifdef DEBUG
	fprintf(stderr, "%ld: header info.\n", ftell(ofp));
#endif
	count = fwrite(&(qt_hdr.n_hdr), sizeof(int), 1, ofp);
	if (count != 1) goto err_out;
	
	count = fwrite(&(qt_hdr.id[0][0]), sizeof(char)*LEN_HDR, qt_hdr.n_hdr, ofp);
	if (count != qt_hdr.n_hdr) goto err_out;
	
	count = fwrite(&(qt_hdr.offset[0]), sizeof(long), qt_hdr.n_hdr, ofp);
	if (count != qt_hdr.n_hdr) goto err_out;
#ifdef DEBUG
	for (i = 0; i < qt_hdr.n_hdr; i++)
		fprintf(stderr, "%c @ %ld, ", qt_hdr.id[i], qt_hdr.offset[i]);

	fprintf(stderr, "\n");
#endif

	/*
	 *	The individual class headers.
	 */
	for (i = 0; i < qt_hdr.n_hdr; i++) {

#ifdef DEBUG
	    fprintf(stderr, "%ld: %c header info.\n", ftell(ofp),
		    qt_hdr.id[i]);
#endif
	    count = fwrite(&(msg_hdr[i].n_msg), sizeof(int), 1, ofp);
	    if (count != 1) goto err_out;
	    count = fwrite(&(msg_hdr[i].qt_msg[0]),
			    sizeof(struct qtmsg), msg_hdr[i].n_msg, ofp);
	    if (count != msg_hdr[i].n_msg) goto err_out;
#ifdef DEBUG
	    { int j;
	      for (j = 0; j < msg_hdr[i].n_msg; j++)
		fprintf(stderr, "msg %d @ %ld (%ld)\n",
			msg_hdr[i].qt_msg[j].msgnum,
			msg_hdr[i].qt_msg[j].offset,
			msg_hdr[i].qt_msg[j].size);
	    }
#endif
	}
	return;
	
err_out:
	fprintf(stderr, "Error writing record\n");
	exit(1);
}

void do_questtxt(const char *infile, const char *outfile)
{
	if (!(ifp = fopen(infile, RDTMODE))) {
		perror(infile);
		exit(EXIT_FAILURE);
	}

	if (!(ofp = fopen(outfile, WRBMODE))) {
		perror(outfile);
		fclose(ifp);
		exit(EXIT_FAILURE);
	}

	qt_hdr.n_hdr = 0;
	qt_line = 0;
	in_msg = FALSE;

	while (fgets(in_line, 80, ifp) != 0) {
	    qt_line++;
	    if (qt_control(in_line)) do_qt_control(in_line);
	    else if (qt_comment(in_line)) continue;
	    else		    do_qt_text(in_line);
	}

	rewind(ifp);
	in_msg = FALSE;
	adjust_qt_hdrs();
	put_qt_hdrs();
	while (fgets(in_line, 80, ifp) != 0) {

		if (qt_control(in_line)) {
		    in_msg = (in_line[1] == 'C');
		    continue;
		} else if (qt_comment(in_line)) continue;
#ifdef DEBUG
		fprintf(stderr, "%ld: %s", ftell(stdout), in_line);
#endif
		fputs(xcrypt(in_line), ofp);
	}
	fclose(ifp);
	fclose(ofp);
	return;
}


static	char	temp[42];

/* limit a name to 40 characters length */
static char *limit(char *name, int pref)
{
	strncpy(temp, name, pref ? 36 : 40);
	temp[pref ? 36 : 40] = 0;
	return temp;
}

void do_objs(const char *outfile)
{
	int i, sum = 0;
	char *c, *objnam;
	int nspell = 0;
	int prefix = 0;
	char class = '\0';
	boolean	sumerr = FALSE;

	if (!(ofp = fopen(outfile, WRTMODE))) {
		perror(outfile);
		exit(EXIT_FAILURE);
	}
	
	fprintf(ofp,"%s",Dont_Edit_Code);
	fprintf(ofp,"#ifndef ONAMES_H\n#define ONAMES_H\n\n");

	for (i = 0; !i || objects[i].oc_class != ILLOBJ_CLASS; i++) {
		objects[i].oc_name_idx = objects[i].oc_descr_idx = i;	/* init */
		if (!(objnam = tmpdup(OBJ_NAME(objects[i])))) continue;

		/* make sure probabilities add up to 1000 */
		if (objects[i].oc_class != class) {
			if (sum && sum != 1000) {
			    fprintf(stderr, "prob error for class %d (%d%%)",
				    class, sum);
			    fflush(stderr);
			    sumerr = TRUE;
			}
			class = objects[i].oc_class;
			sum = 0;
		}

		for (c = objnam; *c; c++)
		    if (*c >= 'a' && *c <= 'z') *c -= (char)('a' - 'A');
		    else if (*c < 'A' || *c > 'Z') *c = '_';

		switch (class) {
		    case WAND_CLASS:
			fprintf(ofp,"#define\tWAN_"); prefix = 1; break;
		    case RING_CLASS:
			fprintf(ofp,"#define\tRIN_"); prefix = 1; break;
		    case POTION_CLASS:
			fprintf(ofp,"#define\tPOT_"); prefix = 1; break;
		    case SPBOOK_CLASS:
			fprintf(ofp,"#define\tSPE_"); prefix = 1; nspell++; break;
		    case SCROLL_CLASS:
			fprintf(ofp,"#define\tSCR_"); prefix = 1; break;
		    case AMULET_CLASS:
			/* avoid trouble with stupid C preprocessors */
			fprintf(ofp,"#define\t");
			if (objects[i].oc_material == PLASTIC) {
			    fprintf(ofp,"FAKE_AMULET_OF_YENDOR\t%d\n", i);
			    prefix = -1;
			    break;
			}
			break;
		    default:
			fprintf(ofp,"#define\t");
		}
		if (prefix >= 0)
			fprintf(ofp,"%s\t%d\n", limit(objnam, prefix), i);
		prefix = 0;

		sum += objects[i].oc_prob;
	}

	/* check last set of probabilities */
	if (sum && sum != 1000) {
	    fprintf(stderr, "prob error for class %d (%d%%)", class, sum);
	    fflush(stderr);
	    sumerr = TRUE;
	}

	fprintf(ofp,"#define\tLAST_GEM\t(JADE)\n");
	fprintf(ofp,"#define\tMAXSPELL\t%d\n", nspell+1);
	fprintf(ofp,"#define\tNUM_OBJECTS\t%d\n", i);

	fprintf(ofp, "\n/* Artifacts (unique objects) */\n\n");

	for (i = 1; artifact_names[i]; i++) {
		for (c = objnam = tmpdup(artifact_names[i]); *c; c++)
		    if (*c >= 'a' && *c <= 'z') *c -= (char)('a' - 'A');
		    else if (*c < 'A' || *c > 'Z') *c = '_';

		if (!strncmp(objnam, "THE_", 4))
			objnam += 4;
		/* fudge _platinum_ YENDORIAN EXPRESS CARD */
		if (!strncmp(objnam, "PLATINUM_", 9))
			objnam += 9;
		fprintf(ofp,"#define\tART_%s\t%d\n", limit(objnam, 1), i);
	}

	fprintf(ofp, "#define\tNROFARTIFACTS\t%d\n", i-1);
	fprintf(ofp,"\n#endif /* ONAMES_H */\n");
	fclose(ofp);
	if (sumerr) exit(EXIT_FAILURE);
	return;
}

static char *tmpdup(const char *str)
{
	static char buf[128];

	if (!str) return NULL;
	strncpy(buf, str, 127);
	return buf;
}


#ifdef STRICT_REF_DEF
struct flag flags;
# ifdef ATTRIB_H
struct attribs attrmax, attrmin;
# endif
#endif /* STRICT_REF_DEF */

/*makedefs.c*/

/*
 * osh.c - original source code for the V6 Thompson shell as found in V7 UNIX
 *
 *	From: Version 7 (V7) UNIX /usr/src/cmd/osh.c
 *
 *	NOTE: The first 42 lines of this file have been added by
 *	      Jeffrey Allen Neitzel <jan (at) etsh (dot) nl> to comply
 *	      with the license.  The file is otherwise unmodified.
 */
/*-
 * Copyright (C) Caldera International Inc.  2001-2002.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code and documentation must retain the above
 *    copyright notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed or owned by Caldera
 *      International, Inc.
 * 4. Neither the name of Caldera International, Inc. nor the names of other
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 *
 * USE OF THE SOFTWARE PROVIDED FOR UNDER THIS LICENSE BY CALDERA
 * INTERNATIONAL, INC. AND CONTRIBUTORS ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL CALDERA INTERNATIONAL, INC. BE LIABLE FOR ANY DIRECT,
 * INDIRECT INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 */

#include <setjmp.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <signal.h>

#define	INTR	2
#define	QUIT	3
#define LINSIZ 1000
#define ARGSIZ 50
#define TRESIZ 100

#define QUOTE 0200
#define FAND 1
#define FCAT 2
#define FPIN 4
#define FPOU 8
#define FPAR 16
#define FINT 32
#define FPRS 64
#define TCOM 1
#define TPAR 2
#define TFIL 3
#define TLST 4
#define DTYP t_dtyp
#define DLEF t_dlef.t_dltr
#define DRIT t_drit.t_drtr
#define DFLG t_dflg
#define DARR t_dcom.t_darr
#define DPTR t_dcom.t_dptr
#define DSPT t_dspr.t_dptr
#define DSTR t_dspr.t_dtre
#define DLPT t_dlef.t_dlpt
#define DRPT t_drit.t_drpt
#define	ENOMEM	12
#define	ENOEXEC 8

struct tree {
  int t_dtyp;
  int t_dflg;
  union {
    struct tree *t_dltr;
    char *t_dlpt;
  } t_dlef;
  union {
    struct tree *t_drtr;
    char *t_drpt;
  } t_drit;
  union {
    char *t_dptr;
    struct tree *t_dtre;
  } t_dspr;
  union {
    char *t_dptr;
    char *t_darr[TRESIZ];
  } t_dcom;
} trebuf[TRESIZ];
int treec;
int errval;
char	*dolp;
char	pidp[6];
char	**dolv;
jmp_buf	jmpbuf;
int	dolc;
char	*promp;
char	*linep;
char	*elinep;
char	**argp;
char	**eargp;
char	peekc;
char	gflg;
char	error;
char	uid;
char	setintr;
char	*arginp;
int	onelflg;
int	stoperr;

#define	NSIG	sizeof mesg / sizeof *mesg
char	*mesg[] = {
	0,
	"Hangup",
	0,
	"Quit",
	"Illegal instruction",
	"Trace/BPT trap",
	"IOT trap",
	"EMT trap",
	"Floating exception",
	"Killed",
	"Bus error",
	"Memory fault",
	"Bad system call",
	0,
	"Alarm clock",
	"Terminated",
};

char	line[LINSIZ];
char	*args[ARGSIZ];

static int main1();
static int word();
static struct tree *tree();
static int getc();
static int readc();
static struct tree *syntax(char **p1, char **p2);
static struct tree *syn1(char **p1, char **p2);
static struct tree *syn2(char **p1, char **p2);
static struct tree *syn3(char **p1, char **p2);
static int scan(struct tree *at, int (*f)());
static int tglob(int c);
static int trim(int c);
static int execute(struct tree *t, int *pf1, int *pf2);
static int texec(char *f, struct tree *at);
static int err(char *s, int exitno);
static int prs(char *as);
static int putc(int c);
static int prn(int n);
static int any(int c, char *as);
static int equal(char *as1, char *as2);
static int pwait(int i);

int main(int c, char **av)
{
	register int f;
	register char *acname, **v;
	int execflg;

	for(f=3; f<15; f++)
		close(f);
	dolc = getpid();
	for(f=4; f>=0; f--) {
		pidp[f] = dolc%10 + '0';
		dolc = dolc/10;
	}
	v = av;
	acname = "<none>";
	promp = "% ";
	if((uid = getuid()) == 0)
		promp = "# ";
	stoperr = 0;
	if(c>1 && v[1][0]=='-' && v[1][1]=='e') {
		++stoperr;
		v[1] = v[0];
		++v;
		--c;
	}
	arginp = 0;
	execflg = onelflg = 0;
	if(c > 1) {
		promp = 0;
		if (*v[1]=='-') {
			execflg = 1;
			if (v[1][1]=='c' && c>2)
				arginp = v[2];
			else if (v[1][1]=='t')
				onelflg = 2;
		} else {
			close(0);
			f = open(v[1], 0);
			if(f < 0) {
				prs(v[1]);
				err(": cannot open",255);
			}
		}
	}
	setintr = 0;
	if(execflg) {
		signal(SIGQUIT, SIG_DFL);
		signal(SIGINT, SIG_DFL);
		if (arginp==0&&onelflg==0)
			setintr++;
	}
	dolv = v;
	dolc = c;

loop:
	if(promp != 0)
		prs(promp);
	peekc = getc();
	main1();
	goto loop;
}

static int main1()
{
	register char  *cp;
	register struct tree *t;

	argp = args;
	eargp = args+ARGSIZ-1;
	linep = line;
	elinep = line+LINSIZ-1;
	error = 0;
	gflg = 0;
	do {
		cp = linep;
		word();
	} while(*cp != '\n');
	treec = 0;
	if(gflg == 0) {
		if(error == 0) {
			setjmp(jmpbuf);
			if (error)
				return 1;
			t = syntax(args, argp);
		}
		if(error != 0)
			err("syntax error",255); else
			execute(t, 0, 0);
	}
}

static int word()
{
	register char c, c1;

	*argp++ = linep;

loop:
	switch(c = getc()) {

	case ' ':
	case '\t':
		goto loop;

	case '\'':
	case '"':
		c1 = c;
		while((c=readc()) != c1) {
			if(c == '\n') {
				error++;
				peekc = c;
				return 1;
			}
			*linep++ = c|QUOTE;
		}
		goto pack;

	case '&':
	case ';':
	case '<':
	case '>':
	case '(':
	case ')':
	case '|':
	case '^':
	case '\n':
		*linep++ = c;
		*linep++ = '\0';
		return 1;
	}

	peekc = c;

pack:
	for(;;) {
		c = getc();
		if(any(c, " '\"\t;&<>()|^\n")) {
			peekc = c;
			if(any(c, "\"'"))
				goto loop;
			*linep++ = '\0';
			return 1;
		}
		*linep++ = c;
	}
}

static struct tree *tree()
{
	if(treec == TRESIZ) {
		prs("Command line overflow\n");
		error++;
		longjmp(jmpbuf, 1);
	}
	return(&trebuf[treec++]);
}

static int getc()
{
	register char c;

	if(peekc) {
		c = peekc;
		peekc = 0;
		return(c);
	}
	if(argp > eargp) {
		argp -= 10;
		while((c=getc()) != '\n');
		argp += 10;
		err("Too many args",255);
		gflg++;
		return(c);
	}
	if(linep > elinep) {
		linep -= 10;
		while((c=getc()) != '\n');
		linep += 10;
		err("Too many characters",255);
		gflg++;
		return(c);
	}
getd:
	if(dolp) {
		c = *dolp++;
		if(c != '\0')
			return(c);
		dolp = 0;
	}
	c = readc();
	if(c == '\\') {
		c = readc();
		if(c == '\n')
			return(' ');
		return(c|QUOTE);
	}
	if(c == '$') {
		c = readc();
		if(c>='0' && c<='9') {
			if(c-'0' < dolc)
				dolp = dolv[c-'0'];
			goto getd;
		}
		if(c == '$') {
			dolp = pidp;
			goto getd;
		}
	}
	return(c&0177);
}

static int readc()
{
	int rdstat;
	char cc;
	register int c;

	if (arginp) {
		if (*arginp == 1)
			exit(errval);
		if ((c = *arginp++) == 0) {
			*arginp = 1;
			c = '\n';
		}
		return(c);
	}
	if (onelflg==1)
		exit(255);
	if((rdstat = read(0, &cc, 1)) != 1) {
		if(rdstat==0) exit(errval); /* end of file*/
		else exit(255); /* error */
	}
	if (cc=='\n' && onelflg)
		onelflg--;
	return(cc);
}

/*
 * syntax
 *	empty
 *	syn1
 */

static struct tree *syntax(char **p1, char **p2)
{
	while(p1 != p2) {
		if(any(**p1, ";&\n"))
			p1++; else
			return(syn1(p1, p2));
	}
	return(0);
}

/*
 * syn1
 *	syn2
 *	syn2 & syntax
 *	syn2 ; syntax
 */

static struct tree *syn1(char **p1, char **p2)
{
	register char **p;
	register struct tree *t;
	int l;

	l = 0;
	for(p=p1; p!=p2; p++)
	switch(**p) {

	case '(':
		l++;
		continue;

	case ')':
		l--;
		continue;

	case '&':
	case ';':
	case '\n':
		if(l == 0) {
			register struct tree *t1;

			l = **p;
			t = tree();
			t->DTYP = TLST;
			t->DLEF = syn2(p1, p);
			t->DFLG = 0;
			if(l == '&') {
				t1 = t->DLEF;
				t->DFLG |= FAND|FPRS|FINT;
			}
			if((t1 = syntax(p+1, p2)))
				t->DRIT = t1; else
				t->DRIT = 0;
			return(t);
		}
	}
	if(l == 0)
		return(syn2(p1, p2));
	error++;
	return(0);
}

/*
 * syn2
 *	syn3
 *	syn3 | syn2
 */

static struct tree *syn2(char **p1, char **p2)
{
	register char **p;
	register int l;
	register struct tree *t;

	l = 0;
	for(p=p1; p!=p2; p++)
	switch(**p) {

	case '(':
		l++;
		continue;

	case ')':
		l--;
		continue;

	case '|':
	case '^':
		if(l == 0) {
			t = tree();
			t->DTYP = TFIL;
			t->DLEF = syn3(p1, p);
			t->DRIT = syn2(p+1, p2);
			t->DFLG = 0;
			return(t);
		}
	}
	return(syn3(p1, p2));
}

/*
 * syn3
 *	( syn1 ) [ < in  ] [ > out ]
 *	word word* [ < in ] [ > out ]
 */

static struct tree *syn3(char **p1, char **p2)
{
	register char **p;
	char **lp, **rp, *i, *o;
	register struct tree *t;
	int n, l, c, flg;

	flg = 0;
	if(**p2 == ')')
		flg |= FPAR;
	lp = 0;
	rp = 0;
	i = 0;
	o = 0;
	n = 0;
	l = 0;
	for(p=p1; p!=p2; p++)
	switch(c = **p) {

	case '(':
		if(l == 0) {
			if(lp != 0)
				error++;
			lp = p+1;
		}
		l++;
		continue;

	case ')':
		l--;
		if(l == 0)
			rp = p;
		continue;

	case '>':
		p++;
		if(p!=p2 && **p=='>')
			flg |= FCAT; else
			p--;

	case '<':
		if(l == 0) {
			p++;
			if(p == p2) {
				error++;
				p--;
			}
			if(any(**p, "<>("))
				error++;
			if(c == '<') {
				if(i != 0)
					error++;
				i = *p;
				continue;
			}
			if(o != 0)
				error++;
			o = *p;
		}
		continue;

	default:
		if(l == 0)
			p1[n++] = *p;
	}
	if(lp != 0) {
		if(n != 0)
			error++;
		t = tree();
		t->DTYP = TPAR;
		t->DSTR = syn1(lp, rp);
		goto out;
	}
	if(n == 0)
		error++;
	p1[n++] = 0;
	t = tree();
	t->DTYP = TCOM;
	for(l=0; l<n; l++)
		t->DARR[l] = p1[l];
out:
	t->DFLG = flg;
	t->DLPT = i;
	t->DRPT = o;
	return(t);
}

static int scan(struct tree *at, int (*f)())
{
	register char *p, **t, c;

	t = at->DARR;
	while((p = *t++))
		while((c = *p))
			*p++ = (*f)(c);
}

static int tglob(int c)
{

	if(any(c, "[?*"))
		gflg = 1;
	return(c);
}

static int trim(int c)
{

	return(c&0177);
}

static int execute(struct tree *t, int *pf1, int *pf2)
{
	int i, f, pv[2];
	register struct tree *t1;
	register char *cp1, *cp2;

	if(t != 0)
	switch(t->DTYP) {

	case TCOM:
		cp1 = *t->DARR;
		if(equal(cp1, "chdir")) {
			if(t->DARR[1] != 0) {
				if(chdir(t->DARR[1]) < 0)
					err("chdir: bad directory",255);
			} else
				err("chdir: arg count",255);
			return 1;
		}
		if(equal(cp1, "shift")) {
			if(dolc < 1) {
				prs("shift: no args\n");
				return 1;
			}
			dolv[1] = dolv[0];
			dolv++;
			dolc--;
			return 1;
		}
		if(equal(cp1, "login")) {
			if(promp != 0) {
				execv("/bin/login", t->DARR);
			}
			prs("login: cannot execute\n");
			return 1;
		}
		if(equal(cp1, "newgrp")) {
			if(promp != 0) {
				execv("/bin/newgrp", t->DARR);
			}
			prs("newgrp: cannot execute\n");
			return 1;
		}
		if(equal(cp1, "wait")) {
			pwait(-1);
			return 1;
		}
		if(equal(cp1, ":"))
			return 1;

	case TPAR:
		f = t->DFLG;
		i = 0;
		if((f&FPAR) == 0)
			i = fork();
		if(i == -1) {
			err("try again",255);
			return 1;
		}
		if(i != 0) {
			if((f&FPIN) != 0) {
				close(pf1[0]);
				close(pf1[1]);
			}
			if((f&FPRS) != 0) {
				prn(i);
				prs("\n");
			}
			if((f&FAND) != 0)
				return 1;
			if((f&FPOU) == 0)
				pwait(i);
			return 1;
		}
		if(t->DLEF != 0) {
			close(0);
			i = open(t->DLPT, 0);
			if(i < 0) {
				prs(t->DLPT);
				err(": cannot open",255);
				exit(255);
			}
		}
		if(t->DRIT != 0) {
			if((f&FCAT) != 0) {
				i = open(t->DRPT, 1);
				if(i >= 0) {
					lseek(i, 0L, 2);
					goto f1;
				}
			}
			i = creat(t->DRPT, 0666);
			if(i < 0) {
				prs(t->DRPT);
				err(": cannot create",255);
				exit(255);
			}
		f1:
			close(1);
			dup(i);
			close(i);
		}
		if((f&FPIN) != 0) {
			close(0);
			dup(pf1[0]);
			close(pf1[0]);
			close(pf1[1]);
		}
		if((f&FPOU) != 0) {
			close(1);
			dup(pf2[1]);
			close(pf2[0]);
			close(pf2[1]);
		}
		if((f&FINT)!=0 && t->DLEF==0 && (f&FPIN)==0) {
			close(0);
			open("/dev/null", 0);
		}
		if((f&FINT) == 0 && setintr) {
			signal(SIGINT, SIG_IGN);
			signal(SIGQUIT, SIG_IGN);
		}
		if(t->DTYP == TPAR) {
			if((t1 = t->DSTR))
				t1->DFLG |= f&FINT;
			execute(t1, pf1, pf2);
			exit(255);
		}
		gflg = 0;
		scan(t, tglob);
		if(gflg) {
			t->DSPT = "/etc/glob";
			execv(t->DSPT, &t->DSPT);
			prs("glob: cannot execute\n");
			exit(255);
		}
		{
			int p = 0;

			scan(t, trim);
			*linep = 0;
			texec(t->DPTR, t);
			cp1 = linep;
			cp2 = getenv("PATH");
			while((*cp1 = *cp2++)) {
				p++;
				if(*cp1 == ':') {
					*cp1++ = '/';
					cp2 = *t->DARR;
					while((*cp1++ = *cp2++));
					texec(linep, t);
					cp1 = linep;
					cp2 = &getenv("PATH")[p];
					continue;
				}
				cp1++;
			}
			*cp1++ = '/';
			cp2 = *t->DARR;
			while((*cp1++ = *cp2++));
			texec(linep, t);
			prs(*t->DARR);
			err(": not found",255);
			exit(255);
		}

	case TFIL:
		f = t->DFLG;
		pipe(pv);
		t1 = t->DLEF;
		t1->DFLG |= FPOU | (f&(FPIN|FINT|FPRS));
		execute(t1, pf1, pv);
		t1 = t->DRIT;
		t1->DFLG |= FPIN | (f&(FPOU|FINT|FAND|FPRS));
		execute(t1, pv, pf2);
		return 1;

	case TLST:
		f = t->DFLG&FINT;
		if((t1 = t->DLEF))
			t1->DFLG |= f;
		execute(t1, pf1, pf2);
		if((t1 = t->DRIT))
			t1->DFLG |= f;
		execute(t1, pf1, pf2);
		return 1;

	}
}

static int texec(char *f, struct tree *at)
{
	register struct tree *t;

	t = at;
	execv(f, t->DARR);
	if (errno==ENOEXEC) {
		if (*linep)
			t->DPTR = linep;
		t->DSPT = "/usr/bin/osh";
		execv(t->DSPT, &t->DSPT);
		prs("No shell!\n");
		exit(255);
	}
	if (errno==ENOMEM) {
		prs(*t->DARR);
		err(": too large",255);
		exit(255);
	}
}

static int err(char *s, int exitno)
{

	prs(s);
	prs("\n");
	if(promp == 0) {
		lseek(0, 0L, 2);
		exit(exitno);
	}
}

static int prs(char *as)
{
	register char *s;

	s = as;
	while(*s)
		putc(*s++);
}

static int putc(int c)
{
	char cc;

	cc = c;
	write(2, &cc, 1);
}

static int prn(int n)
{
	register int a;

	if ((a = n/10))
		prn(a);
	putc(n%10 + '0');
}

static int any(int c, char *as)
{
	register char *s;

	s = as;
	while(*s)
		if(*s++ == c)
			return(1);
	return(0);
}

static int equal(char *as1, char *as2)
{
	register char *s1, *s2;

	s1 = as1;
	s2 = as2;
	while(*s1++ == *s2)
		if(*s2++ == '\0')
			return(1);
	return(0);
}

static int pwait(int i)
{
	register int p, e;
	int s;

	if(i != 0)
	for(;;) {
		p = wait(&s);
		if(p == -1)
			break;
		e = s&0177;
		if (e>=NSIG || mesg[e]) {
			if(p != i) {
				prn(p);
				prs(": ");
			}
			if (e < NSIG)
				prs(mesg[e]);
			else {
				prs("Signal ");
				prn(e);
			}
			if(s&0200)
				prs(" -- Core dumped");
		}
		if (e || (s&&stoperr))
			err("", (s>>8)|e );
		errval |= (s>>8);
	}
}

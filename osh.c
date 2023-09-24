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
#define DTYP 0
#define DLEF 1
#define DRIT 2
#define DFLG 3
#define DSPR 4
#define DCOM 5
#define	ENOMEM	12
#define	ENOEXEC 8

struct tree {
  int t_dtyp,
      t_dflg;
  char t_dlef[100],
      t_drit[100],
      *t_dspr,
      *t_dcom[100];
} t;
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
int	trebuf[TRESIZ];

int main(c, av)
int c;
char **av;
{
	register f;
	register char *acname, **v;

	for(f=3; f<15; f++)
		close(f);
	dolc = getpid();
	for(f=4; f>=0; f--) {
		dolc = dolc/10;
		pidp[f] = dolc%10 + '0';
	}
	v = av;
	acname = "<none>";
	promp = "% ";
	if((uid = getuid()) == 0)
		promp = "# ";
	if(c>1 && v[1][0]=='-' && v[1][1]=='e') {
		++stoperr;
		v[1] = v[0];
		++v;
		--c;
	}
	if(c > 1) {
		promp = 0;
		if (*v[1]=='-') {
			**v = '-';
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
	if(**v == '-') {
		signal(QUIT, 1);
		f = signal(INTR, 1);
		if ((arginp==0&&onelflg==0) || (f&01)==0)
			setintr++;
	}
	dolv = v+1;
	dolc = c-1;

loop:
	if(promp != 0)
		prs(promp);
	peekc = getc();
	main1();
	goto loop;
}

int main1()
{
	register char  *cp;
	register struct tree *t;
	extern struct tree *syntax();

	argp = args;
	eargp = args+ARGSIZ-5;
	linep = line;
	elinep = line+LINSIZ-5;
	error = 0;
	gflg = 0;
	do {
		cp = linep;
		word();
	} while(*cp != '\n');
	if(gflg == 0) {
		if(error == 0) {
			setjmp(jmpbuf);
			if (error)
				return 1;
			t = syntax(args, argp);
		}
		if(error != 0)
			err("syntax error",255); else
			execute(t);
	}
}

int word()
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

struct tree *tree()
{
	return(&t);
}

int getc()
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

int readc()
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
	if((rdstat = read(0, &cc, 1)) != 1)
		if(rdstat==0) exit(errval); /* end of file*/
		else exit(255); /* error */
	if (cc=='\n' && onelflg)
		onelflg--;
	return(cc);
}

/*
 * syntax
 *	empty
 *	syn1
 */

struct tree *syntax(p1, p2)
char **p1, **p2;
{
	extern struct tree *syn1();

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

struct tree *syn1(p1, p2)
char **p1, **p2;
{
	register char **p;
	register struct tree *t = tree();
	int l;
	extern struct tree *syn2();

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
			l = **p;
			t->t_dtyp = TLST;
			syn2(p1, p);
			t->t_dflg = 0;
			if(l == '&')
				t->t_dflg |= FAND|FPRS|FINT;
			syntax(p+1, p2);
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

struct tree *syn2(p1, p2)
char **p1, **p2;
{
	register char **p;
	register int l;
	register struct tree *t = tree();
	extern struct tree *syn3();

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
			t->t_dtyp = TFIL;
			syn3(p1, p);
			syn2(p+1, p2);
			t->t_dflg = 0;
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

struct tree *syn3(p1, p2)
char **p1, **p2;
{
	register char **p;
	char **lp, **rp;
	register struct tree *t = tree();
	int n, l, i, o, c, flg;

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
				i = **p;
				continue;
			}
			if(o != 0)
				error++;
			o = **p;
		}
		continue;

	default:
		if(l == 0)
			p1[n++] = *p;
		if(n == sizeof t->t_dcom / sizeof *t->t_dcom) {
			prs("Command line overflow\n");
			error++;
			longjmp(jmpbuf, 1);
		}
	}
	if(lp != 0) {
		if(n != 0)
			error++;
		t->t_dtyp = TPAR;
		syn1(lp, rp);
		goto out;
	}
	if(n == 0)
		error++;
	p1[n++] = 0;
	t->t_dtyp = TCOM;
	for(l=0; l<n; l++)
		t->t_dcom[l] = p1[l];
out:
	t->t_dflg = flg;
	*t->t_dlef = i;
	*t->t_drit = o;
	return(t);
}

int scan(at, f)
struct tree *at;
int (*f)();
{
	register char *p, **t, c;

	t = at->t_dcom;
	while((p = *t++))
		while((c = *p))
			*p++ = (*f)(c);
}

int tglob(c)
int c;
{

	if(any(c, "[?*"))
		gflg = 1;
	return(c);
}

int trim(c)
int c;
{

	return(c&0177);
}

int execute(t, pf1, pf2)
struct tree *t;
int *pf1, *pf2;
{
	int i, f, pv[2];
	register struct tree *t1;
	register char *cp1, *cp2;
	extern int errno;
	extern char *getenv();

	if(t != 0)
	switch(t->t_dtyp) {

	case TCOM:
		cp1 = *t->t_dcom;
		if(equal(cp1, "chdir")) {
			if(t->t_dcom[1] != 0) {
				if(chdir(t->t_dcom[1]) < 0)
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
				execv("/bin/login", t->t_dcom);
			}
			prs("login: cannot execute\n");
			return 1;
		}
		if(equal(cp1, "newgrp")) {
			if(promp != 0) {
				execv("/bin/newgrp", t->t_dcom);
			}
			prs("newgrp: cannot execute\n");
			return 1;
		}
		if(equal(cp1, "wait")) {
			pwait(-1, 0);
			return 1;
		}
		if(equal(cp1, ":"))
			return 1;

	case TPAR:
		f = t->t_dflg;
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
				pwait(i, t);
			return 1;
		}
		if(*t->t_dlef != 0) {
			close(0);
			i = open(t->t_dlef, 0);
			if(i < 0) {
				prs(t->t_dlef);
				err(": cannot open",255);
				exit(255);
			}
		}
		if(*t->t_drit != 0) {
			if((f&FCAT) != 0) {
				i = open(t->t_drit, 1);
				if(i >= 0) {
					lseek(i, 0L, 2);
					goto f1;
				}
			}
			i = creat(t->t_drit, 0666);
			if(i < 0) {
				prs(t->t_drit);
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
		if((f&FINT)!=0 && *t->t_dlef==0 && (f&FPIN)==0) {
			close(0);
			open("/dev/null", 0);
		}
		if((f&FINT) == 0 && setintr) {
			signal(INTR, 0);
			signal(QUIT, 0);
		}
		if(t->t_dtyp == TPAR) {
			if((t1 = t))
				t1->t_dflg |= f&FINT;
			execute(t1);
			exit(255);
		}
		gflg = 0;
		scan(t, tglob);
		if(gflg) {
			t->t_dspr = "/etc/glob";
			execv(t->t_dspr, t->t_dspr);
			prs("glob: cannot execute\n");
			exit(255);
		}
		{
			int p = 0;

			scan(t, trim);
			*linep = 0;
			texec(t->t_dcom, t);
			cp1 = linep;
			cp2 = getenv("PATH");
			while((*cp1 = *cp2++)) {
				p++;
				if(*cp1 == ':') {
					*cp1++ = '/';
					cp2 = *t->t_dcom;
					while((*cp1++ = *cp2++));
					texec(linep, t);
					cp1 = linep;
					cp2 = &getenv("PATH")[p];
					continue;
				}
				cp1++;
			}
			*cp1++ = '/';
			cp2 = *t->t_dcom;
			while((*cp1++ = *cp2++));
			texec(linep, t);
			prs(*t->t_dcom);
			err(": not found",255);
			exit(255);
		}

	case TFIL:
		f = t->t_dflg;
		pipe(pv);
		t1 = t;
		t1->t_dflg |= FPOU | (f&(FPIN|FINT|FPRS));
		execute(t1, pf1, pv);
		t1 = t;
		t1->t_dflg |= FPIN | (f&(FPOU|FINT|FAND|FPRS));
		execute(t1, pv, pf2);
		return 1;

	case TLST:
		f = t->t_dflg&FINT;
		if((*(char **) &t1->t_dlef = t->t_dlef))
			t1->t_dflg |= f;
		execute(t1);
		if((*(char **) &t1->t_drit = t->t_drit))
			t1->t_dflg |= f;
		execute(t1);
		return 1;

	}
}

int texec(f, at)
char *f;
struct tree *at;
{
	extern int errno;
	register struct tree *t;

	t = at;
	execv(f, t->t_dcom);
	if (errno==ENOEXEC) {
		if (*linep)
			*(char **) &t->t_dcom = linep;
		t->t_dspr = "/usr/bin/osh";
		execv(t->t_dspr, t->t_dspr);
		prs("No shell!\n");
		exit(255);
	}
	if (errno==ENOMEM) {
		prs(*t->t_dcom);
		err(": too large",255);
		exit(255);
	}
}

int err(s, exitno)
char *s;
int exitno;
{

	prs(s);
	prs("\n");
	if(promp == 0) {
		lseek(0, 0L, 2);
		exit(exitno);
	}
}

int prs(as)
char *as;
{
	register char *s;

	s = as;
	while(*s)
		putc(*s++);
}

int putc(c)
int c;
{
	char cc;

	cc = c;
	write(2, &cc, 1);
}

int prn(n)
int n;
{
	register int a;

	if ((a = n/10))
		prn(a);
	putc(n%10 + '0');
}

int any(c, as)
int c;
char *as;
{
	register char *s;

	s = as;
	while(*s)
		if(*s++ == c)
			return(1);
	return(0);
}

int equal(as1, as2)
char *as1, *as2;
{
	register char *s1, *s2;

	s1 = as1;
	s2 = as2;
	while(*s1++ == *s2)
		if(*s2++ == '\0')
			return(1);
	return(0);
}

int pwait(i, t)
int i, *t;
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
		if (e || s&&stoperr)
			err("", (s>>8)|e );
		errval |= (s>>8);
	}
}

/* $Id$ */

#include <sys/param.h>
#include <sys/sysctl.h>

#include <err.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sysexits.h>

struct evcnt {
	int		*ec_scnam;
	u_int		 ec_scdep;
	struct evcnt	*ec_next;
};

struct evcnt	*lookupec(char *);
void		 getsubec(struct evcnt *, struct evcnt **,
			  struct evcnt **);
void		 intrdump(char *);
void		 shorten(char **, char *);
void __dead	 usage(void);

static int recursive = 1;

int
main(int argc, char *argv[])
{
	int c;

	while ((c = getopt(argc, argv, "D")) != -1)
		switch (c) {
		case 'D':
			recursive = 0;
			break;
		default:
			usage();
			/* NOTREACHED */
		}

	if (argc == 1) {
		intrdump(NULL);
	} else
		while (*++argv != NULL)
			intrdump(*argv);
	exit(EXIT_SUCCESS);
}

void
intrdump(char *name)
{
	struct evcnt *head, *next, *last;
	u_int64_t cnt;
	size_t siz;
	char dev[128];
	int n, i;

	head = lookupec(name);
	for (; head != NULL; head = next) {
		head->ec_scnam[head->ec_scdep - 1] = KERN_INTRCNT_CNT;
		siz = sizeof(cnt);
		if (sysctl(head->ec_scnam, head->ec_scdep, &cnt, &siz,
		    NULL, 0) == -1)
			err(EX_OSERR, "sysctl");
		head->ec_scnam[head->ec_scdep - 1] = KERN_INTRCNT_NAME;
		siz = sizeof(dev);
		if (sysctl(head->ec_scnam, head->ec_scdep, &dev, &siz,
		    NULL, 0) == -1)
			err(EX_OSERR, "sysctl");

		n = 0;
		for (i = 0; i < head->ec_scdep; i++)
			n += printf("  ");
		n += printf("%s", dev);
#define INTRCOL (8 * 4)
		n &= ~7;
		for (n &= ~7; n < INTRCOL; n += 8)
			(void)printf("\t");
		(void)printf("%llu", cnt);

		if (recursive) {
			getsubec(head, &next, &last);
			if (next != NULL) {
				last->ec_next = head->ec_next;
				head->ec_next = next;
			}
		}
		next = head->ec_next;
		free(head->ec_scnam);
		free(head);
	}
}

void
shorten(char **a, char *b)
{
	if (strncmp(*a, b, strlen(b)) == 0 &&
	    (*a)[strlen(b)] == '.')
		*a += strlen(b) + 1;
}

struct evcnt *
lookupec(char *dev)
{
	char *p, *np, buf[128];
	struct evcnt *ec;
	int i, j, n;
	size_t siz;

	if ((ec = malloc(sizeof(*ec))) == NULL)
		return (NULL);
	ec->ec_scnam = NULL;
	ec->ec_next = NULL;

	if (dev != NULL) {
		struct ctlname names[] = CTL_NAMES;
		shorten(&dev, names[CTL_KERN].ctl_name);
	}
	if (dev != NULL) {
		struct ctlname names[] = CTL_KERN_NAMES;
		shorten(&dev, names[KERN_INTRCNT].ctl_name);
	}

	ec->ec_scdep = 3; /* kern.intr.<whatever>.intrcnt */
	for (p = dev; p != NULL && *p != '\0'; p++)
		if (*p == '.')
			/* XXX: check for overflows. */
			ec->ec_scdep++;
	if ((ec->ec_scnam = calloc(ec->ec_scdep,
	     sizeof(*ec->ec_scnam))) == NULL)
		err(EX_OSERR, "calloc");
	i = 0;
	ec->ec_scnam[i++] = CTL_KERN;
	ec->ec_scnam[i++] = KERN_INTRCNT;
	for (p = dev; p != NULL; p = np, i++) {
		if ((np = strchr(p, '.')) != NULL)
			*np++ = '\0';

		ec->ec_scnam[i] = KERN_INTRCNT_NUM;
		siz = sizeof(n);
		if (sysctl(ec->ec_scnam, i + 1, &n, &siz, NULL, 0) ==
		    -1)
			err(EX_OSERR, "sysctl");
		ec->ec_scnam[i + 1] = KERN_INTRCNT_NAME;
		siz = sizeof(buf);
		for (j = 0; j < n; j++) {
			ec->ec_scnam[i] = j;
			if (sysctl(ec->ec_scnam, i + 2, &buf, &siz,
			    NULL, 0) != -1)
				break;
			if (errno != EOPNOTSUPP)
				err(EX_OSERR, "sysctl");
		}
	}
	return (ec);
}

void
getsubec(struct evcnt *parent, struct evcnt **first, struct evcnt **last)
{
	struct evcnt *ec, **ecp;
	size_t siz;
	int i, n;

	parent->ec_scnam[parent->ec_scdep - 1] = KERN_INTRCNT_NUM;
	siz = sizeof(n);
	if (sysctl(parent->ec_scnam, parent->ec_scdep, &n, &siz, NULL,
	    0) == -1)
		err(EX_OSERR, "sysctl");
	ecp = first;
	*ecp = ec = NULL;
	for (i = 0; i < n; i++) {
		if ((ec = malloc(sizeof(**ecp))) == NULL)
			err(EX_OSERR, "malloc");
		if ((ec->ec_scnam = calloc(parent->ec_scdep + 1,
		     sizeof(*ec->ec_scnam))) == NULL)
			err(EX_OSERR, "calloc");
		(void)memcpy(ec->ec_scnam, parent->ec_scnam,
		    parent->ec_scdep * sizeof(*ec->ec_scnam));
		ec->ec_scdep = parent->ec_scdep + 1;
		ec->ec_scnam[ec->ec_scdep - 2] = i;

		*ecp = ec;
		ecp = &ec->ec_next;
	}
	if (ec != NULL)
		ec->ec_next = NULL;
	*last = ec;
}

void __dead
usage(void)
{
	extern char *__progname;

	(void)fprintf(stderr, "usage: %s [-D] [device]\n", __progname);
	exit(EX_USAGE);
}

# $Id$

PROG = intrdump
SRCS = intrdump.c
MAN = intrdump.1
CFLAGS += -Wall -g

.include <bsd.prog.mk>

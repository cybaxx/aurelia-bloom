CC      ?= cc
DESTDIR ?= /opt/lab/bin
NASM    ?= nasm

BASIC_CFLAGS   = -fno-stack-protector -no-pie -g -O0
HARDENED_CFLAGS = -fstack-protector-all -fPIE -pie -g -O0

SRCS_FUND = \
	00-fundamentals/01-stack-overflow/vulnerable.c \
	00-fundamentals/02-format-string/vulnerable.c \
	00-fundamentals/03-heap-overflow/vulnerable.c

SRCS_EXPLOIT = \
	02-exploitation/01-ret2libc/vulnerable.c \
	02-exploitation/02-rop-chains/vulnerable.c \
	02-exploitation/03-pledge-bypass/vulnerable.c

ALL_SRCS = $(SRCS_FUND) $(SRCS_EXPLOIT)

.PHONY: basic hardened clean install

basic:
	@mkdir -p $(DESTDIR)
	@for src in $(ALL_SRCS); do \
		name=$$(echo $$src | sed 's|.*/||; s|\.c$$||'); \
		dir=$$(dirname $$src); \
		echo "CC (basic) $$src -> $(DESTDIR)/$$name"; \
		$(CC) $(BASIC_CFLAGS) -o $(DESTDIR)/$$name $$src; \
	done

hardened:
	@mkdir -p $(DESTDIR)
	@for src in $(ALL_SRCS); do \
		name=$$(echo $$src | sed 's|.*/||; s|\.c$$||'); \
		echo "CC (hardened) $$src -> $(DESTDIR)/$$name"; \
		$(CC) $(HARDENED_CFLAGS) -o $(DESTDIR)/$$name $$src; \
	done

clean:
	rm -f $(DESTDIR)/vulnerable

install: basic
	@echo "Installed to $(DESTDIR)"

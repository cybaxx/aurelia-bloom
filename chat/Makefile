CC      ?= cc
CFLAGS  ?= -Wall -Wextra -O2
LDFLAGS ?= -lsodium

SRCDIR  = src
OBJDIR  = obj
BINDIR  = .

CHATD_SRCS = $(SRCDIR)/chatd.c $(SRCDIR)/crypto.c $(SRCDIR)/peer.c
CHAT_SRCS  = $(SRCDIR)/chat.c $(SRCDIR)/crypto.c
IRCGW_SRCS = $(SRCDIR)/irc-gateway.c

CHATD_OBJS = $(CHATD_SRCS:$(SRCDIR)/%.c=$(OBJDIR)/%.o)
CHAT_OBJS  = $(CHAT_SRCS:$(SRCDIR)/%.c=$(OBJDIR)/%.o)

# Deduplicate objects (crypto.o shared between both)
ALL_SRCS = $(sort $(CHATD_SRCS) $(CHAT_SRCS))
ALL_OBJS = $(ALL_SRCS:$(SRCDIR)/%.c=$(OBJDIR)/%.o)

.PHONY: all clean install

all: $(BINDIR)/chatd $(BINDIR)/chat $(BINDIR)/irc-gateway

$(OBJDIR)/%.o: $(SRCDIR)/%.c | $(OBJDIR)
	$(CC) $(CFLAGS) -I$(SRCDIR) -c -o $@ $<

$(BINDIR)/chatd: $(CHATD_OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

$(BINDIR)/chat: $(CHAT_OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

$(BINDIR)/irc-gateway: $(SRCDIR)/irc-gateway.c
	$(CC) $(CFLAGS) -I$(SRCDIR) -o $@ $< $(LDFLAGS)

$(OBJDIR):
	mkdir -p $(OBJDIR)

install: all
	install -d /opt/lab/bin
	install -m 755 $(BINDIR)/chatd /opt/lab/bin/chatd
	install -m 755 $(BINDIR)/chat /opt/lab/bin/chat
	install -m 755 $(BINDIR)/irc-gateway /opt/lab/bin/irc-gateway

clean:
	rm -rf $(OBJDIR) $(BINDIR)/chatd $(BINDIR)/chat $(BINDIR)/irc-gateway

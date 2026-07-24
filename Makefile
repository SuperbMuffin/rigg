PREFIX ?= /usr/local
DESTDIR ?=

GRAY  = \033[0;90m
RESET = \033[0m

CORE_SRC_DIR = core
CORE_DST_DIR = $(DESTDIR)$(PREFIX)/share/rigg/core

CORE_LIBS = core_io core_str
CORE_FILES = $(shell find $(CORE_SRC_DIR) -type f)

RIGG_DIR  = rigg
RIGGC_DIR = riggc

all: rigg riggc

build: rigg riggc

rigg:
	@printf '%b--- rigg ---%b\n' '$(GRAY)' '$(RESET)'
	@$(MAKE) --no-print-directory -s -C $(RIGG_DIR) all PREFIX=$(PREFIX) DESTDIR=$(DESTDIR)

riggc:
	@printf '%b--- riggc ---%b\n' '$(GRAY)' '$(RESET)'
	@$(MAKE) --no-print-directory -s -C $(RIGGC_DIR) all PREFIX=$(PREFIX) DESTDIR=$(DESTDIR)

core-install:
	@printf '%b--- core install ---%b\n' '$(GRAY)' '$(RESET)'
	mkdir -p $(DESTDIR)$(PREFIX)/share/rigg
	cp -R $(CORE_SRC_DIR)/. $(CORE_DST_DIR)

core-uninstall:
	@printf '%b--- core uninstall ---%b\n' '$(GRAY)' '$(RESET)'
	rm -rf $(DESTDIR)$(PREFIX)/share/rigg/core

rigg-install:
	@printf '%b--- rigg install ---%b\n' '$(GRAY)' '$(RESET)'
	@$(MAKE) --no-print-directory -C $(RIGG_DIR) install PREFIX=$(PREFIX) DESTDIR=$(DESTDIR)

riggc-install:
	@printf '%b--- riggc install ---%b\n' '$(GRAY)' '$(RESET)'
	@$(MAKE) --no-print-directory -C $(RIGGC_DIR) install PREFIX=$(PREFIX) DESTDIR=$(DESTDIR)

install: core-install rigg-install riggc-install

clean:
	@printf '%b--- rigg ---%b\n' '$(GRAY)' '$(RESET)'
	@$(MAKE) --no-print-directory -C $(RIGG_DIR) clean
	@printf '%b--- riggc ---%b\n' '$(GRAY)' '$(RESET)'
	@$(MAKE) --no-print-directory -C $(RIGGC_DIR) clean

test:
	@printf '%b--- riggc ---%b\n' '$(GRAY)' '$(RESET)'
	@$(MAKE) --no-print-directory -s -C $(RIGGC_DIR) test

run:
	@printf '%b--- rigg ---%b\n' '$(GRAY)' '$(RESET)'
	@$(MAKE) --no-print-directory -s -C $(RIGG_DIR) run

uninstall:
	@printf '%b--- rigg uninstall ---%b\n' '$(GRAY)' '$(RESET)'
	@$(MAKE) --no-print-directory -C $(RIGG_DIR) uninstall PREFIX=$(PREFIX) DESTDIR=$(DESTDIR)
	@printf '%b--- riggc uninstall ---%b\n' '$(GRAY)' '$(RESET)'
	@$(MAKE) --no-print-directory -C $(RIGGC_DIR) uninstall PREFIX=$(PREFIX) DESTDIR=$(DESTDIR)
	@$(MAKE) --no-print-directory core-uninstall PREFIX=$(PREFIX) DESTDIR=$(DESTDIR)

.PHONY: all build rigg riggc core-install core-uninstall rigg-install riggc-install install clean test run uninstall

PREFIX ?= $(HOME)/.local
CFLAGS ?= -O2 -Wall -Wextra
BINS = bin/sk-vdisplay-common.sh bin/sk-vdisplay-screen bin/sk-vdisplay-layout bin/sk-sunshine bin/sk-cam bin/sk-cam-recv

all: build/sk-vdisplay

build/sk-vdisplay: src/sk-vdisplay.c
	mkdir -p build
	$(CC) $(CFLAGS) -o $@ $< -levdi -lm

install: build/sk-vdisplay
	install -Dm755 build/sk-vdisplay $(PREFIX)/bin/sk-vdisplay
	for b in $(BINS); do install -Dm755 $$b $(PREFIX)/bin/$$(basename $$b); done
	install -Dm644 -t $(HOME)/.config/systemd/user systemd/user/*.service

check:
	sh -n bin/sk-vdisplay-screen bin/sk-vdisplay-layout bin/sk-sunshine bin/sk-cam bin/sk-cam-recv install.sh
	./tests/test-config.sh

clean:
	rm -rf build

.PHONY: all install check clean

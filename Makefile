PREFIX ?= /usr/local
BIN_DIR = $(PREFIX)/bin
MAN_DIR = $(PREFIX)/share/man/man1
DIST    = dist/janus

.PHONY: build install uninstall clean test

# Build the standalone single-file janus into dist/janus.
build:
	@./scripts/build-janus $(DIST)

# Install the standalone janus onto PATH as `janus`, plus the man page.
install: build
	mkdir -p $(BIN_DIR)
	cp $(DIST) $(BIN_DIR)/janus
	chmod +x $(BIN_DIR)/janus
	@echo "Installed $(BIN_DIR)/janus"
	@if [ -f doc/janus-run.1 ]; then \
		mkdir -p $(MAN_DIR); \
		cp doc/janus-run.1 $(MAN_DIR)/janus-run.1; \
		chmod 644 $(MAN_DIR)/janus-run.1; \
		ln -sf janus-run.1 $(MAN_DIR)/janus.1; \
		echo "Installed man page to $(MAN_DIR)/janus-run.1 (janus.1 -> janus-run.1)"; \
	fi

uninstall:
	rm -f $(BIN_DIR)/janus
	rm -f $(MAN_DIR)/janus-run.1 $(MAN_DIR)/janus.1
	@echo "Removed $(BIN_DIR)/janus and man pages"

clean:
	rm -rf dist
	@echo "Cleaned dist/"

# Run the engine test against the development tree.
test:
	@bash test/test-engine.sh

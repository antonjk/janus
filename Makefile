PREFIX ?= /usr/local
BIN_DIR = $(PREFIX)/bin
MAN_DIR = $(PREFIX)/share/man/man1
DIST    = dist/janus
DEV_BIN ?= $(HOME)/.dev/bin

.PHONY: build install uninstall clean test dev dev-clean

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

# Run the full test suite against the development tree.
test:
	@bash test/run-all.sh

# Symlink the live source transpiler (bin/janus-run) into the dev-override dir
# ($(DEV_BIN), kept first on PATH) as `janus`, shadowing any installed janus. Because
# it links to the SOURCE (not the built bundle), edits to bin/janus-run and
# include/*.inc are picked up immediately — no rebuild. `make dev-clean` removes it.
dev:
	@mkdir -p "$(DEV_BIN)"
	@ln -sf "$(abspath bin/janus-run)" "$(DEV_BIN)/janus"
	@echo "Dev janus linked (live source): $(DEV_BIN)/janus -> $(abspath bin/janus-run)"
	@case ":$$PATH:" in *":$(DEV_BIN):"*) ;; *) echo "WARNING: $(DEV_BIN) is not on PATH; add it (first) so the dev build is picked up." ;; esac

dev-clean:
	@rm -f "$(DEV_BIN)/janus"
	@echo "Removed dev override $(DEV_BIN)/janus (installed janus, if any, is active again)."

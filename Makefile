PREFIX ?= /usr/local
BIN_DIR = $(PREFIX)/bin
DIST    = dist/janus

.PHONY: build install uninstall clean test

# Build the standalone single-file janus into dist/janus.
build:
	@./scripts/build-janus $(DIST)

# Install the standalone janus onto PATH as `janus`.
install: build
	mkdir -p $(BIN_DIR)
	cp $(DIST) $(BIN_DIR)/janus
	chmod +x $(BIN_DIR)/janus
	@echo "Installed $(BIN_DIR)/janus"

uninstall:
	rm -f $(BIN_DIR)/janus
	@echo "Removed $(BIN_DIR)/janus"

clean:
	rm -rf dist
	@echo "Cleaned dist/"

# Run the engine test against the development tree.
test:
	@bash test/test-engine.sh

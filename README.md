# Janus

Run an ordinary shell script, or annotate it with `#@` comment directives and run
the same file as a tree of **groups** and **steps** executed concurrently with a
live progress UI. The run mode is chosen by the script's shebang — one file, two
faces.

## Why "Janus"?

Janus is the two-faced Roman god of doorways and transitions. A single annotated
script has two faces, selected by its shebang:

- Under `bash`, the `#@` directives are inert comments — the script runs plainly,
  top-to-bottom (great for debugging).
- Under `janus-run`, those directives become groups and steps that run concurrently
  with grouped output, aligned status, and a spinner.

Group **barriers** echo Janus's gates; step state changes echo his transitions.

## How it works

The annotated source never contains runtime calls. `janus-run` is a transpiler and
shebang interpreter: it rewrites the `#@` directives into runtime calls, sources the
engine, and runs the result. Steps are launched concurrently; a nested group acts as
a barrier between batches.

```bash
#!/usr/bin/env janus-run
#@ group async: build
    #@ step: compile
    make -j4
    #@ end
    #@ step: assets
    npm run build:assets
    #@ end
#@ end
```

Running this executes `compile` and `assets` at the same time, waits for both, and
shows each one's status. Change the shebang to `#!/usr/bin/env bash` and the exact
same file runs the two commands sequentially with the `#@` lines ignored.

## Installation

### Homebrew (recommended)

```bash
brew install antonjk/tap/janus
```

This also installs [`cprintf`](https://github.com/antonjk/bash-cprintf) for colored
output.

### From source (development tree)

```bash
git clone https://github.com/antonjk/janus.git
cd janus
./bin/janus-run examples/nested-async.sh      # run directly from the tree
```

### Standalone single-file build

Janus bundles into one self-contained script (the runtime is inlined, no `include/`
directory needed):

```bash
make build                 # produces dist/janus
make install               # installs dist/janus -> /usr/local/bin/janus (+ man page)
make install PREFIX=~/.local
make uninstall
make clean
```

`make install` also installs the man page (`man janus-run`, with `man janus` as an
alias).

## Usage

```bash
# Run an annotated script through the engine
janus-run script.sh [args...]

# As a shebang (#!/usr/bin/env janus-run), just run the file
./script.sh [args...]

# Inspect the transpiled output without running it
janus-run --emit script.sh

# Help
janus-run --help
```

Arguments after the script path are forwarded to the script as its own `$@`.

## Annotation grammar

Directives are shell comments beginning with `#@`. Every `group` and `step` is
closed by its own `#@ end`. Groups nest; steps are leaves.

```
#@ group [async[=N]]: <header>
#@ end

#@ step [id=X] [depends=a,b] [success=N] [fail=N] [ok=WORD] [err=WORD]: <header>
<body ...>
#@ end
```

- **`async`** — without it, a group's steps run sequentially. A bare `async` runs
  them concurrently with the width in `JANUS_ASYNC` (default `unbounded`, i.e. up to
  one process per CPU core; set it to a number, or `1` to force sync); `async=N` fixes the concurrency at `N`.
- **Headers and bodies expand at registration time** — `$(...)`, `${var}`, and loop
  variables resolve in the lexical context where the directive appears, so a `#@ step`
  inside a `for` loop registers one step per iteration.

### Step attributes

| Attribute   | Meaning                                                      | Default            |
|-------------|-------------------------------------------------------------|--------------------|
| `id=X`      | Explicit step id                                            | auto               |
| `depends=…` | Declared sibling dependencies (parsed, not yet enforced)    | none               |
| `success=N` | Trailing output lines shown on success                      | `JANUS_CONTEXT_SUCCESS` (0) |
| `fail=N`    | Trailing output lines shown on failure                      | `JANUS_CONTEXT_FAIL` (25)   |
| `ok=WORD`   | Status word on success (single word)                        | `JANUS_OK_TEXT` (OK)        |
| `err=WORD`  | Status word on failure (single word)                        | `JANUS_ERR_TEXT` (ERR)      |

## Execution semantics

A group processes its children in registration order. A maximal run of adjacent
steps forms one concurrent batch (bounded by `async=N`; a sync group runs each step
singly). A nested group is a barrier: the preceding batch finishes before the group
starts, and the group finishes before the following batch starts. Within a batch,
steps launch concurrently but are reported one at a time in registration order; the
spinner animates whichever step is currently being waited on.

Each step body runs in a **forked subshell of your script**, so a step can call
functions you defined in the script, use `source`d custom includes, and read the
script's variables. The subshell inherits the script's `set` options and
environment; a step's own changes stay isolated from the engine and other steps.
Loop-varying values are captured per iteration (a `#@ step` inside a `for` loop
registers one step per pass with that pass's values baked in).

## Environment variables

| Variable                 | Purpose                                   | Default     |
|--------------------------|-------------------------------------------|-------------|
| `JANUS_ASYNC`            | Concurrency for a bare-`async` group (`unbounded` = up to CPU cores) | `unbounded` |
| `JANUS_CONTEXT_SUCCESS`  | Default trailing lines on success         | `0`         |
| `JANUS_CONTEXT_FAIL`     | Default trailing lines on failure         | `25`        |
| `JANUS_OK_TEXT`          | Default success status word               | `OK`        |
| `JANUS_ERR_TEXT`         | Default failure status word               | `ERR`       |
| `JANUS_INDENT`           | Spaces of indent per nesting level        | `2`         |
| `JANUS_SPINNER`          | Spinner glyph set                         | `wave-bars` |
| `JANUS_SPINNER_AREA`     | Spinner width in columns                  | `5`         |

## Requirements

- The runtime requires **Bash 4+** (`wait -n`, etc.). The transpiler itself also runs
  under Bash 3.2. On macOS, install a modern Bash (e.g. `brew install bash`).
- Color is provided by [`cprintf`](https://github.com/antonjk/bash-cprintf) when
  available on `PATH`; otherwise Janus uses a vendored single-file `cprintf-lib`, and
  failing that falls back to plain text with markup stripped. Output never hard-fails
  on formatting.

## Project layout

```
bin/janus-run          transpiler + interpreter
include/janus.inc      runtime: registry + group/step engine
include/*.inc          trap / temp / spinner / cprintf wrapper
include/cprintf-lib    vendored single-file cprintf (MIT; see cprintf-lib.LICENSE)
scripts/build-janus    bundler -> dist/janus
examples/              annotated examples
test/test-engine.sh    engine ordering test
doc/janus-run.1        man page
DESIGN.md              design notes
```

## Testing

```bash
make test        # runs the engine ordering test
```

## License

MIT License — see `LICENSE`. The vendored `cprintf-lib` is MIT (see
`include/cprintf-lib.LICENSE`).

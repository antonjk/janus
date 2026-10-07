# Janus — Annotated Script Executor

## 1. Goal

Write an ordinary Bash script, annotate it with `#@` comment directives, and get
**two** ways to run it. The run mode is selected by the script's **shebang**, so the
invocation is always the same (`./script.sh`) and the intent lives in the file.

| Shebang                        | `./script.sh` does                                      |
|--------------------------------|---------------------------------------------------------|
| `#!/usr/bin/env janus-run`   | transpile + run with the janus engine (concurrent, UI)|
| `#!/usr/bin/env bash` (or `#!/bin/bash`) | plain sequential run; `#@` lines inert        |

1. **Direct run** — shebang is `bash`. The `#@` lines are inert comments. The script
   executes top-to-bottom, sequentially. This is the debugging / plain-shell mode.
   Behaviour must match the janus run for the common cases. To debug an annotated
   script, flip the one shebang line to `#!/usr/bin/env bash`.

2. **Janus run** — shebang is `janus-run`. The kernel execs
   `janus-run /abs/path/script.sh [args...]`; `janus-run` acts as an
   **interpreter** (see §5): it transpiles the directives into a tree of **groups**
   (branches) and **steps** (leaves), then runs them with concurrency, progress UI,
   and per-step output capture.

The annotated source file never contains janus runtime calls. The runtime wiring
is added by the transpiler.

## 2. Vocabulary

- **Group** — a branch node. Runs its children in registration order. May be
  `async` (adjacent steps batch concurrently) or sync (children one at a time).
- **Step** — a leaf node. A captured block of shell that the engine runs as one unit.
- **Registration pass** — janus executes the (transpiled) script once. All non-step
  code runs immediately; it is the "driver" that decides which groups/steps exist
  (loops, command substitution, etc.). `janus-step` only *captures* its body; it
  does not run it yet.
- **Execution pass** — after registration, `janus-exec` walks the tree and runs the
  captured step bodies according to group semantics.

## 3. Annotation grammar

```
#@ group [async[=N]]: <header text>
#@ end

#@ step [id=<id>] [depends=<id>,...] [success=<N>] [fail=<N>] [ok=<word>] [err=<word>]: <header text>
<body ...>
#@ end
```

- Ends are **explicit**. Every `group` and `step` is closed by its own `#@ end`.
- Groups **nest**. Steps are leaves and never nest.
- `async` with no `=N` means unbounded concurrency within a batch; `async=N` caps it.
- Per-step display overrides (all optional; fall back to global env defaults):
  - `success=<N>` / `fail=<N>` — context lines to show on success / failure
    (override `JANUS_CONTEXT_SUCCESS` / `JANUS_CONTEXT_FAIL`).
  - `ok=<word>` / `err=<word>` — status word shown on success / failure
    (override `JANUS_OK_TEXT` / `JANUS_ERR_TEXT`, default `OK` / `ERR`).
    Single word only (the attribute list is space-separated).
- Header text is a normal string; it is **expanded at registration time** (so
  `$(...)`, `${var}` in a header resolve in the lexical context where the directive
  appears).
- `depends` is parsed in v1 but **not yet enforced** (see §7). Intended semantics,
  when implemented: intra-group only.

## 4. Execution semantics

A group processes its children **in registration order**. Within a group:

- A **maximal run of adjacent steps** forms one **concurrent batch**. In an `async`
  group the batch runs concurrently (bounded by `N`); in a sync group each batch is
  effectively size 1 (sequential).
- A **child group is a barrier**: the preceding batch must finish before the child
  group starts, and the child group must finish before the following batch starts.

### Worked example

```
#@ group async: outer
  #@ step: 1   #@ end
  #@ step: 2   #@ end
  #@ group async: nested
    #@ step: 3 #@ end
    #@ step: 4 #@ end
  #@ end
  #@ step: 5   #@ end
#@ end
```

Execution order:

1. Run steps **1 and 2** concurrently; wait for both.
2. Run nested group: steps **3 and 4** concurrently; wait for both.
3. Run step **5**.

A sync group (`#@ group:` with no `async`) runs 1, then 2, then nested(3 then 4), then 5.

## 5. Transpilation

`janus-run` rewrites the annotated source into an executable script, then runs it
(or prints it with `--emit`).

### 5.0 Interpreter contract

`janus-run` is designed to be used as a shebang interpreter
(`#!/usr/bin/env janus-run`). When the kernel execs an annotated script it calls:

```
janus-run /abs/path/script.sh [script-args...]
```

- **File-vs-flag detection.** `janus-run` inspects its first argument. If it is a
  known flag (e.g. `--emit`), it is consumed by `janus-run`; the **first
  non-flag argument** is the target script path. This lets the same binary serve both
  as an interpreter (`janus-run script.sh ...`) and as an explicit debug command
  (`janus-run --emit script.sh`).
- **Argument forwarding (required).** Everything after the target script path is
  forwarded into the transpiled script as its own `$@`. So an annotated script can
  take its own CLI arguments (as `whisper-build` forwards `brazil-build-options`).
  Under `bash script.sh args` this is free; as an interpreter `janus-run` must do it
  explicitly.
- **No flags in the shebang.** `#!/usr/bin/env` passes **at most one** argument
  (the interpreter name) portably; `#!/usr/bin/env janus-run --emit` is NOT reliable
  across kernels (notably macOS). Therefore `--emit` and any other flags are
  **explicit-invocation only** — never placed in a shebang. `--emit` is a debugging
  action (print the transpiled script and exit without running), not a run mode.
- `janus-run` itself is a plain Bash script (`#!/usr/bin/env bash`); it must be on
  `PATH` for `/usr/bin/env` to resolve it, and the annotated script must be
  executable for `./script.sh` to work.

| Directive                                   | Emitted code                                             |
|---------------------------------------------|----------------------------------------------------------|
| `#@ group: T`                               | `janus-group "T"`                                      |
| `#@ group async: T`                         | `janus-group --async "T"`                             |
| `#@ group async=N: T`                       | `janus-group --async N "T"`                           |
| `#@ step: T` ... `#@ end`                   | `janus-step "T" <<STEP_BODY_<uid>` / body / `STEP_BODY_<uid>` |
| `#@ step id=X depends=a,b: T` ... `#@ end`  | `janus-step --id X --depends a,b "T" <<STEP_BODY_<uid>` ... |
| `#@ end` closing a group                    | `janus-group-end`                                     |

Rules:

- Heredoc delimiter is **unquoted** (`<<STEP_BODY_<uid>`), so the body is **expanded
  at registration** in its lexical context (loop variables, `$(...)`). A literal `$`
  in a body must be escaped (`\$`).
- `<uid>` is **unique per step** so adjacent or driver-authored heredocs never collide.
- The transpiler maintains an **open-block stack** to decide whether a given `#@ end`
  closes a step (close heredoc) or a group (`janus-group-end`).
- **Prologue**: after the shebang, inject `source <dir>/include/janus.inc`.
- **Epilogue**: append `janus-exec` at the end of the file.
- The original annotated file is never modified; transpilation targets a temp file
  (or stdout under `--emit`).

### Transpiled form of the rebase example

The annotated source begins with `#!/usr/bin/env janus-run`. `janus-run`
transpiles it to the following Bash and executes that (the shebang is rewritten to
`bash`, the runtime is sourced, `janus-exec` is appended, and the script's own
args are available as `$@`):

```bash
#!/usr/bin/env bash
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "${SCRIPT_DIR}/include/cprintf.inc"
source "${SCRIPT_DIR}/include/janus.inc"   # injected

cd "$(brazil-context workspace root)/src"

janus-group "Sync Versionset Metadata"
  janus-step "$(cat ../packageInfo | grep "versionSet" | sed 's/[^=]*=[^"]*"\([^"]*\)";/\1/g')" <<STEP_BODY_1
brazil ws sync --md
STEP_BODY_1
janus-group-end

janus-group --async 10 "Rebase Packages..."
for file in */; do
  janus-step "${file//\/}" <<STEP_BODY_2
cd "${file}"
git-stash-and-rebase.sh
STEP_BODY_2
done
janus-group-end

janus-exec   # injected
```

## 6. Runtime (registry + engine)

State lives in a **spool directory** on disk, not shell arrays, so it survives
subshells (`( ... )`). Reuse `temp.inc` (`temp_file`, EXIT cleanup of `/tmp/PID-$$`).

```
/tmp/PID-$$/janus/
  tree                 # append-only, one record per node
  step.<uid>.body      # captured (already expanded) step body
  step.<uid>.out       # runtime stdout+stderr for that step
```

`tree` record format (one line): `TYPE|depth|id|meta|header`

```
GROUP|0|g1|async=10|Rebase Packages...
STEP|1|s2|depends=|release-Foo
GROUP_END|0|g1||
```

Append-only + depth reconstructs the tree without nested shell data structures and is
immune to subshell scoping.

### Runtime API (in `include/janus.inc`)

- `janus-group [--async[=N]] "<header>"` — append a GROUP record, push onto a depth stack.
- `janus-group-end` — append a GROUP_END record, pop the depth stack.
- `janus-step [--id X] [--depends a,b] "<header>"` — read stdin (the heredoc body)
  into `step.<uid>.body`, append a STEP record. **Does not execute.**
- `janus-exec` — parse `tree`, walk it per §4, run step bodies with concurrency,
  spinner (`spinner.inc`), and colored status (`cprintf.inc`); print per-step
  success/fail context (tail of `step.<uid>.out`).

### Engine (janus-exec) — pseudocode

```
walk(group):
  batch = []
  for child in children(group) in order:
    if child is STEP:
      batch.append(child)
    else:                      # child is a GROUP (barrier)
      run_batch(batch); batch = []
      walk(child)
  run_batch(batch)

run_batch(steps):
  if group is sync: cap = 1 else cap = N (or unbounded)
  launch up to `cap` step bodies as background children, capturing to step.<uid>.out
  as each finishes, show status; keep launching until all done
  wait for all in the batch before returning
```

Interrupt handling: on SIGINT/EXIT, `kill_descendant_processes` (from `trap.inc`)
reaps running step children; `temp.inc` removes the spool dir.

## 7. v1 scope / later

**v1 ships:**
- Grammar (`group`, `step`, `async[=N]`, explicit `#@ end`, nested groups).
- Transpiler with `--emit`.
- Registry + engine with the batch/barrier model of §4.
- Spool-dir state; spinner + cprintf status UI; output capture + context tail.

**v1 parses but does NOT enforce:**
- `depends` — accepted in grammar, recorded in the tree, currently a no-op with a
  warning. Intra-group launch-gating is a later iteration.

**Later iterations:**
- Enforce `depends` (intra-group DAG gate).
- Group-level `depends`.
- Concurrency across nested groups (currently a barrier).
- Reconcile step-body quoting so direct-run and janus-run agree in more edge cases.

## 8. Proposed workspace layout

```
/Users/akoekemo/workplace/Personal/janus/
  DESIGN.md
  bin/
    janus-run          # transpiler + driver (--emit to print transpiled script)
    janus-exec         # (optional) standalone engine entry if run outside janus-run
  include/
    janus.inc            # runtime: janus-group/step/group-end/exec
    temp.inc             # spool dir + cleanup        (ported/reused)
    trap.inc             # trap_add + kill_descendant (ported/reused)
    spinner.inc          # spinner                    (ported/reused)
    cprintf.inc          # colored output             (ported/reused)
  examples/
    rebase.sh            # annotated version of whisper-rebase
    nested-async.sh      # the §4 worked example
  test/
    ...                  # transpiler + engine tests
```

## 9. Open items for sign-off

1. ~~`depends` in v1~~ — **DECIDED: (a) parse-but-ignore** for v1 (see §7).
2. ~~Includes: port vs. source~~ — **DECIDED:** `temp.inc`, `trap.inc`, `spinner.inc`
   are **ported/copied** into `include/`. Color is provided by **vendoring upstream
   `cprintf`** (antonjk/bash-cprintf, MIT, commit `bf33ebf`) into `include/cprintf-lib`
   (single-file `lib` build + its LICENSE). Janus's own thin
   `include/cprintf.inc` wrapper resolves color in this order: (1) a `cprintf` on
   `PATH`, (2) the vendored `include/cprintf-lib`, (3) a plain-`printf` stub that
   strips `<...>` markup — so the engine never hard-fails on formatting and the
   workspace runs anywhere (it is not currently installed on this machine).
3. ~~`command`-builtin shadowing~~ — **MOOT:** legacy emitters are dropped. `janus.inc`
   is a clean runtime (`janus-group` / `janus-step` / `janus-group-end` /
   `janus-exec`) with no `h1/h2/h3/command` functions, so nothing shadows the builtin.

# pfind & spfind

Unix-style utilities: **pfind** recurses a directory tree and prints files matching a given permission string (e.g. `rw-r--r--`); **spfind** runs pfind and sorts the output via a pipe, then prints the sorted paths and total match count.

## Features

- **pfind**: Recursive permission-based file search using `lstat`, `opendir`/`readdir`; validates 9-character `rwx` permission strings; getopt for `-d`, `-p`, `-h`.
- **spfind**: Composes pfind with the system `sort` using **fork**, **exec**, and **pipes**; multiplexes pfind’s stdout and stderr in the parent with `select()` so errors appear on stderr and sorted paths on stdout; reports total matches.

## Build

Requires a C99-capable compiler (gcc or clang) and a POSIX environment (Linux, macOS, etc.).

```bash
make all
```

This produces `pfind` and `spfind` in the project root. **Run both from the repo root** so that `spfind` can execute `./pfind`. Clean with:

```bash
make clean
```

## Usage

### pfind

```bash
./pfind -d <directory> -p <permissions string> [-h]
```

- `-d` — directory to search (required).
- `-p` — 9-character permission string, e.g. `rwxr-xr-x` or `rw-r--r--` (required).
- `-h` — print usage and exit.

**Examples:**

```bash
./pfind -h
./pfind -d . -p rw-r--r--
./pfind -d /tmp -p rwxrwxrwx
```

### spfind

```bash
./spfind -d <directory> -p <permissions string> [-h]
```

Same options as pfind. Output is sorted and a “Total matches: N” line is printed at the end. Errors from pfind go to stderr and are not counted in the total.

**Examples:**

```bash
./spfind -h
./spfind -d ./tests/fixtures -p rw-r--r--
./spfind -d . -p ---------
```

## Design and architecture

### pfind

- Parses options with `getopt`; validates the permission string (exactly 9 chars, each `-` or `r`/`w`/`x`).
- Recurses with `opendir`/`readdir`; uses `lstat` (not `stat`) so symlinks are not followed.
- Builds a 9-char permission string from `st_mode` (user/group/other read/write/execute) and compares to the target; prints the full path of each match to stdout.
- Errors (invalid dir, permission denied, etc.) go to stderr with descriptive messages.

### spfind: fork/exec and data flow

- **Three processes**: parent, pfind child, sort child.
- **Pipes**:
  - `pfind_to_sort`: pfind’s stdout → sort’s stdin (matches flow to sort).
  - `sort_to_parent`: sort’s stdout → parent (parent reads sorted lines).
  - `error_pipe`: pfind’s stderr → parent (parent forwards errors to stderr).
- **Child 1 (pfind)**: `dup2` so stdout → write end of `pfind_to_sort`, stderr → write end of `error_pipe`; close unused FDs; `execv("./pfind", argv)`.
- **Child 2 (sort)**: `dup2` so stdin ← read end of `pfind_to_sort`, stdout → write end of `sort_to_parent`; close unused FDs; `execlp("sort", "sort", NULL)`.
- **Parent**: Closes all write ends used by children, then reads from `sort_to_parent` (sorted paths) and `error_pipe` (pfind errors) using **select()** to avoid blocking on one stream. Forwards sort output to stdout and error output to stderr; counts newlines in sort output for “Total matches”; `waitpid`s both children and exits with failure if either child failed or any error was seen.

This keeps stderr and stdout separate and ensures sorted output plus a correct match count.

## Error handling and edge cases

- **pfind**: Unknown option, missing `-d`/`-p`, invalid permission string, nonexistent or non-directory path, permission denied, stat/readdir failures — all produce clear stderr messages and `EXIT_FAILURE` where appropriate.
- **spfind**: No getopt; forwards pfind’s argv. Fails on pipe/fork/dup2/exec failures; if pfind or sort exec fails, prints “Error: pfind failed.” or “Error: sort failed.” to stderr. If pfind writes to stderr (e.g. permission denied), that is forwarded to stderr and spfind still exits with failure; “Total matches” counts only lines from sort (stdout).

**Important**: `spfind` runs `./pfind` in the current working directory. Run `spfind` from the directory that contains the `pfind` binary (e.g. repo root after `make`).

## Testing

A test script builds the project, sets up a fixture directory tree with known permissions, runs pfind and spfind, and shows sample output:

```bash
make test
```

Or run manually:

```bash
./scripts/run_tests.sh
```

The script creates `tests/fixtures` with subdirs and files and sets permissions so that specific `-p` values produce predictable matches. It runs from the repo root so `./pfind` is available to `spfind`.

## License

MIT (see [LICENSE](LICENSE)).

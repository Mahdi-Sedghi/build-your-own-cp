# build-your-own-cp

> Rebuild `cp(1)` from scratch using only four POSIX system calls — and learn exactly
> what the user/kernel boundary costs you along the way.

---

## Table of Contents

- [Why this exists](#why-this-exists)
- [Features](#features)
- [Requirements](#requirements)
- [Build](#build)
- [Usage](#usage)
- [How it works (OS concepts)](#how-it-works-os-concepts)
- [Error handling](#error-handling)
- [Testing](#testing)
- [Project layout](#project-layout)
- [Roadmap](#roadmap)
- [Contributing](#contributing)
- [License](#license)

---

## Why this exists

Most "copy a file" tutorials hide the interesting parts behind `fopen` / `fread` / `fwrite`.
This project exposes the actual interface the kernel offers you:

```c
open() → read() → write() (in a loop) → close()
```

It is deliberately small, deliberately un-magical, and deliberately correct about the
parts that most code gets wrong — **partial writes**, **`EINTR` retries**, **deferred
`close()` errors**, and **cleanup on every failure path**.

If you are learning Operating Systems, this is a program you can read top-to-bottom in
one sitting and understand completely.

---

## Features

- ✅ **Zero `<stdio.h>`** — no `fopen`, `fread`, `fwrite`, `printf`, no buffering.
- ✅ **Raw POSIX system calls only** — `open(2)`, `read(2)`, `write(2)`, `close(2)`, `unlink(2)`.
- ✅ **Binary-safe** — works on ELF binaries, images, `/dev/urandom`, arbitrary bytes.
- ✅ **4 KB page-sized static buffer** — no heap, no stack pressure.
- ✅ **Correct partial-write loop** — drains the buffer until `off == nread`.
- ✅ **`EINTR` handling** — retries interrupted syscalls instead of aborting.
- ✅ **Deferred-write detection** — checks `close()` on the destination for writeback errors.
- ✅ **Deterministic cleanup** — closes descriptors and removes half-written output on any failure.
- ✅ **Permission-controlled output** — new files created `0644` (masked by your `umask`).

---

## Requirements

- A POSIX system (Linux, macOS, BSD, WSL).
- `gcc` or `clang` with C99 or later.
- No external libraries.

---

## Build

```bash
gcc -Wall -Wextra -Wconversion -O2 -o syscopy syscopy.c
```

For a debug build with AddressSanitizer (recommended while hacking on it):

```bash
gcc -Wall -Wextra -Wconversion -g -O1 \
    -fsanitize=address,undefined \
    -o syscopy syscopy.c
```

---

## Usage

```bash
./syscopy <source_file> <destination_file>
```

Examples:

```bash
# Copy a regular file
./syscopy notes.txt notes.backup

# Copy a binary — prove it's byte-identical
./syscopy /bin/ls ls.copy
cmp /bin/ls ls.copy && echo "IDENTICAL"

# Copy from a device (yes, this works)
./syscopy /dev/urandom random.bin   # Ctrl-C to stop, or pick a small device
```

Exit status: `0` on success, `1` on any failure. Errors are written to **stderr**.

---

## How it works (OS concepts)

### 1. `open()` returns a file descriptor, not a file

`open()` traps into the kernel. The kernel walks the path through the VFS, checks your
`uid`/`gid` against the inode's permission bits, and installs a `struct file *` in your
process's **descriptor table**. The `int` you get back is just an **index** into that table
(`0` = stdin, `1` = stdout, `2` = stderr, so the first user file is typically `3`).

```c
src_fd = open(src_path, O_RDONLY);
dst_fd = open(dst_path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
```

> **Mode `0644`** is the *requested* permission. The kernel ANDs it with your `umask`,
> so the effective mode is `0644 & ~umask`. Never trust the literal — check with `ls -l`.

### 2. `read()` and `write()` move bytes across the user/kernel boundary

- `read()` copies bytes **from the page cache** into your user buffer (`copy_to_user`).
- `write()` copies bytes **into the page cache** (`copy_from_user`) and returns immediately —
  **not** when the data reaches disk.

Every one of these is a **mode switch**: user mode → kernel mode → user mode.

### 3. Partial writes are not a bug — they are the contract

`write()` may legally write **fewer bytes than you asked for**. Reasons include signal
delivery mid-syscall (`EINTR`), disk quota, or a full page. The kernel guarantees only one
thing: **the return value tells you exactly how far it got.**

That is why the inner loop looks like this:

```c
ssize_t off = 0;
while (off < nread) {
    ssize_t nw = write(dst_fd, buf + off, nread - off);
    if (nw < 0) {
        if (errno == EINTR) continue;   /* retry the SAME bytes */
        /* real error */
    }
    off += nw;                          /* advance by what actually got written */
}
```

### 4. `close()` can fail — and you should check it

Writeback happens asynchronously. A disk error that occurs after `write()` returns may
surface **at `close()`**. Ignoring `close()` on an output file is a real data-integrity bug.

### 5. On failure, clean up deterministically

The `fail:` label closes both descriptors (in case they are still open) and `unlink()`s
the half-written destination so you never leave silent corruption behind.

---

## Error handling

Every syscall whose return value can fail is checked:

| Syscall    | Handled failures                                        |
|------------|---------------------------------------------------------|
| `open()`   | `ENOENT`, `EACCES`, `EISDIR`, `ELOOP`, `EMFILE`, …      |
| `read()`   | `EINTR` (retry), `EIO`, `EISDIR`, …                     |
| `write()`  | `EINTR` (retry), `ENOSPC`, `EDQUOT`, `EPIPE`, `EIO`, …  |
| `close()`  | deferred writeback errors (`EIO`, `ENOSPC`, …)          |
| `unlink()` | best-effort (ignored if it fails)                       |

All diagnostics go to **stderr** via `write(2)` — never `stdio` — so nothing is lost if the
process dies before flushing. If your course permits `<stdio.h>`, you can replace the
`report_error()` body with a single `perror()`.

Try to break it:

```bash
./syscopy does_not_exist.bin out.bin     # cannot open source: No such file or directory
./syscopy /etc/shadow out.bin            # cannot open source: Permission denied
./syscopy source.bin /root/out.bin       # cannot open destination: Permission denied
./syscopy source.bin                     # usage: syscopy <source_file> <destination_file>
```

---

## Testing

A minimal manual test suite:

```bash
# 1. Round-trip integrity on a large binary
dd if=/dev/urandom of=big.bin bs=1M count=64
./syscopy big.bin big.copy
cmp big.bin big.copy && echo "OK: identical"

# 2. Empty file
: > empty
./syscopy empty empty.copy
cmp empty empty.copy && echo "OK: empty copied"

# 3. Source is a directory (should fail cleanly)
mkdir -p somedir
./syscopy somedir out.bin; echo "exit=$?"   # expect exit=1, no out.bin left

# 4. Interrupt mid-copy (partial write / EINTR sanity)
./syscopy big.bin slow.copy & sleep 0.01; kill -INT $!; wait

# 5. Descriptor leak check (run once, then watch /proc)
./syscopy big.bin /dev/null
```

If you have `valgrind`, run under it to confirm zero leaks and clean syscall usage:

```bash
valgrind --leak-check=full --track-fds=yes ./syscopy big.bin big.copy
```

---

## Project layout

```
.
├── syscopy.c   # the entire program — one file, no dependencies
├── README.md   # this file
└── LICENSE     # MIT (or your choice)
```

---

## Roadmap

Ideas for making this a fuller, more instructive project — pick what interests you:

- [ ] **`sendfile(2)` variant** — kernel-to-kernel copy, zero user-space buffer.
- [ ] **`splice(2)` / `copy_file_range(2)`** — modern zero-copy paths; add a `--fast` flag.
- [ ] **`O_DIRECT` experiment** — bypass the page cache, and document the alignment rules.
- [ ] **Preserve metadata** — copy mode, owner, timestamps (`fchmod`, `fchown`, `utimensat`).
- [ ] **Atomic replace** — write to `dest.tmp`, `fsync()`, then `rename(2)` over the target.
- [ ] **Progress reporting** — print bytes copied via `write(STDERR_FILENO, …)` and `clock_gettime`.
- [ ] **Sparse-file support** — detect zero blocks with `lseek(SEEK_HOLE)` and skip them.
- [ ] **Configurable buffer size** — `--buffer=1M` to observe throughput vs. buffer size.
- [ ] **Benchmark script** — compare `syscopy` against `cp` and `dd` on the same file.
- [ ] **Unit tests** — a shell script or `pytest`-style harness that exercises every error path.
- [ ] **Detailed write-up** — a `docs/` folder explaining page cache, writeback, and `EINTR` in depth.

---

## Contributing

This project is intentionally tiny, so keep contributions focused and readable.

1. Fork the repo and create a branch: `git checkout -b feature/sendfile-variant`
2. Follow the existing style: no `stdio`, explicit error checks on every syscall, comments
   that link each step to an OS concept.
3. Add a test case (shell snippet in `README.md` or a new `tests/` file) for anything you change.
4. Open a pull request describing **what OS concept** your change demonstrates.

Bug reports are very welcome — especially if you can reproduce a partial-write or
`EINTR` scenario. That is the whole point of the project.

---

## License

[MIT](LICENSE) — do what you want, attribution appreciated.

---

*Built as a teaching artifact for an Operating Systems course. If it helped you understand
the user/kernel boundary a little better, ⭐ the repo.*

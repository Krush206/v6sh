# UNIX V6 Shell
The original Shell from Version 6 UNIX.

Also contains `pwb.c`, the PWB/UNIX shell (Bell Labs, 2.44, 5/26/77),
ported to modern Unix/Linux.

## Notes
Compiled with `-ansi`, `-pedantic` and `-Wall` flags.

## Building

    cc -ansi -pedantic -Wall -o pwb pwb.c      # PWB shell
    cc -ansi -pedantic -Wall -o osh osh.c      # V6 shell

Both also build warning-free under `-Wall -Wextra -O2` (except one
intentional warning in `pwb.c` where a global points into `main1`'s
stack frame, as in the original design).

## PWB shell port notes

`pwb.c` was ported from V6-era K&R C to ISO C while keeping the
original structure and behavior:

- Numeric word-offset macros (`#define DCOM 5`, `t[DCOM]`) replaced
  with real struct member access (`t->DARR[i]`, `t->DLEF`, ...);
  word-offset punning is not portable to 64-bit layouts.
- `struct tree` fields hold either tree links or strings exactly as
  before; explicit casts mark the punning sites.
- `texec()` now takes `char **argv` instead of relying on struct
  adjacency to synthesize an argv for the ENOEXEC fallback.
- V6 syscalls updated: `seek`/`tell` -> `lseek`, integer `signal()`
  values -> `SIG_IGN`/`SIG_DFL`, wait status decoded with
  `WIFEXITED`/`WTERMSIG`/`WCOREDUMP`, `times(2)` mapped through
  `struct tms`.
- K&R implicit int declarations, missing returns, and deprecated
  `register` usage cleaned up; builds clean with `-ansi -pedantic -Wall`.
- The site-specific accounting file (`/etc/sha`) and `.path` file are
  used if present, else sensible defaults (same as the original).


# BSD vs Linux: Key Differences for Exploit Development

## Syscall Conventions (x86-64)

### Syscall Invocation
Both use the `syscall` instruction, but register usage differs slightly:

| Register | Linux | BSD |
|----------|-------|-----|
| Syscall # | `rax` | `rax` |
| Arg 1 | `rdi` | `rdi` |
| Arg 2 | `rsi` | `rsi` |
| Arg 3 | `rdx` | `rdx` |
| Arg 4 | `r10` | `r10` (FreeBSD) / `rcx` varies |
| Arg 5 | `r8` | `r8` |
| Arg 6 | `r9` | `r9` |
| Return | `rax` | `rax` |

### Common Syscall Numbers
| Syscall | Linux | FreeBSD/HBSD |
|---------|-------|-------------|
| read | 0 | 3 |
| write | 1 | 4 |
| open | 2 | 5 |
| close | 3 | 6 |
| execve | 59 | 59 |
| exit | 60 | 1 |
| socket | 41 | 97 |
| bind | 49 | 104 |
| listen | 50 | 106 |
| accept | 43 | 30 |
| dup2 | 33 | 90 |
| fork | 57 | 2 |
| mmap | 9 | 477 |

## libc Differences
- BSD uses **BSD libc**, not glibc
- Function addresses differ — ret2libc exploits need per-target analysis
- ROP gadgets in BSD libc are different from glibc
- Use `objdump -d /lib/libc.so.7` or `r2 /lib/libc.so.7` to find gadgets

## Memory Layout
Both are similar at a high level:
```
High:  [Stack]     ← grows down
       [mmap/libs]
       [Heap]      ← grows up
       [BSS]
       [Data]
Low:   [Text]
```
But ASLR implementations differ — BSD randomizes more aggressively by default.

## Compiler Differences
- BSD default compiler: **Clang/LLVM** (not GCC)
- `-fstack-protector` uses different canary symbols
- PIE is default on HardenedBSD
- RELRO is full by default

## Process Inspection
| Task | Linux | BSD |
|------|-------|-----|
| Memory map | `cat /proc/pid/maps` | `procstat -v pid` |
| Open files | `ls /proc/pid/fd` | `procstat -f pid` |
| Binary info | `readelf -a` | `readelf -a` or `objdump` |
| Debugger | `gdb` | `gdb` or `lldb` |

## /proc Filesystem
Linux relies heavily on `/proc`. FreeBSD/HardenedBSD has a limited `/proc` (must be mounted explicitly) — prefer `sysctl` and `procstat` instead.

## Practical Impact on Exploits
1. **Shellcode must use BSD syscall numbers** — Linux shellcode won't work
2. **ret2libc needs BSD libc analysis** — different gadgets
3. **ASLR is stronger** — SEGVGUARD prevents brute force
4. **No `/proc/self/maps`** by default — use `procstat` for address leaks during debugging

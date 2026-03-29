# Pledge & Unveil (Sandboxing Concepts)

## Overview
OpenBSD introduced `pledge()` and `unveil()` as lightweight sandboxing primitives. While HardenedBSD doesn't implement these directly, it uses FreeBSD's Capsicum framework for similar capability-based sandboxing.

Understanding both approaches is essential for BSD security work.

## pledge() (OpenBSD)
Restricts which syscall groups a process can use after a certain point.

```c
#include <unistd.h>

int main(void) {
    /* After this call, only stdio + read paths allowed */
    pledge("stdio rpath", NULL);

    /* Any syscall outside these groups kills the process */
    FILE *f = fopen("/etc/passwd", "r");  /* OK: rpath */
    unlink("/tmp/test");                   /* SIGABRT: not pledged */
}
```

### Common Promise Groups
| Promise | Allows |
|---------|--------|
| `stdio` | read, write, close, dup, mmap (basic I/O) |
| `rpath` | open for reading, stat, readlink |
| `wpath` | open for writing |
| `cpath` | create, unlink, rename |
| `inet`  | socket, connect, bind (TCP/UDP) |
| `proc`  | fork, wait, kill |
| `exec`  | execve |

## unveil() (OpenBSD)
Restricts filesystem visibility to explicitly named paths.

```c
unveil("/var/www", "r");     /* can only read /var/www */
unveil("/tmp", "rwc");       /* read/write/create in /tmp */
unveil(NULL, NULL);          /* lock — no more unveil calls */
```

## Capsicum (FreeBSD / HardenedBSD)
FreeBSD's capability mode is more fine-grained:

```c
#include <sys/capsicum.h>

cap_enter();  /* enter capability mode — no new file descriptors */

/* Only pre-opened file descriptors can be used */
/* All global namespace access (paths, PIDs, etc.) is denied */
```

## Security Implications for Exploitation
- Even if you get code execution, sandboxed processes can't:
  - Open new files outside allowed paths
  - Make network connections (if not pledged)
  - Spawn processes (if not pledged)
- Exploits must work within the sandbox constraints
- This is explored in `lab/02-exploitation/03-pledge-bypass/`

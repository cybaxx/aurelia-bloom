# Filesystem Security

## Mount Options
Restrict what can happen on each filesystem in `/etc/fstab`:

| Option | Effect |
|--------|--------|
| `noexec` | Cannot execute binaries |
| `nosuid` | Ignore setuid/setgid bits |
| `nodev` | Ignore device nodes |

Recommended layout:
```
/tmp    noexec,nosuid,nodev
/var    nosuid,nodev
/home   nosuid,nodev
```

## File Flags (BSD-specific)
Beyond Unix permissions, BSD has file flags:

```sh
chflags schg /etc/passwd        # system immutable
chflags sunlnk /var/log/auth.log  # system undeletable (append-only)
ls -lo /etc/passwd              # view flags
```

At securelevel >= 1, even root cannot clear `schg` or `sunlnk` flags.

## UFS vs ZFS

### UFS (Unix File System)
- Default on FreeBSD/HardenedBSD
- Journaling via Soft Updates
- Simple and reliable

### ZFS
- Copy-on-write, checksums, snapshots
- Excellent for data integrity
- Snapshot before exploit testing:
  ```sh
  zfs snapshot lab@clean
  zfs rollback lab@clean    # restore after testing
  ```

## Secure Partitioning Strategy
Separate partitions limit the blast radius of an attack:
- `/` — minimal, read-only where possible
- `/tmp` — noexec (prevents shellcode staging)
- `/var` — nosuid (logs, mail, tmp files)
- `/home` — nosuid,nodev (user data)
- `/usr/local` — third-party software

## chroot and Jails
```sh
# Basic chroot
chroot /var/empty /bin/sh

# FreeBSD jail (lightweight container)
jail -c name=webapp path=/jail/webapp host.hostname=webapp \
    ip4.addr=192.168.56.50 command=/bin/sh
```

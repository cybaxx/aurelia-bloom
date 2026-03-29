# Filesystem & Permissions

## Unix Filesystem Hierarchy (BSD)
```
/               root
/bin            essential user binaries
/sbin           essential system binaries
/etc            configuration files
/usr/local      third-party software (not /opt on BSD)
/usr/src        kernel source (if installed)
/usr/ports      ports tree (FreeBSD/HardenedBSD)
/var/log        system logs
/tmp            temporary files (cleared on reboot)
/home           user home directories
```

## File Permissions
Every file has owner, group, and other permissions:
```
-rwxr-xr--  1 root wheel  4096 Jan 1 00:00 file
│├─┤├─┤├─┤
│ │   │  └── other: read only
│ │   └───── group: read + execute
│ └───────── owner: read + write + execute
└─────────── type: - file, d directory, l symlink
```

## chmod
```sh
chmod 755 file      # rwxr-xr-x
chmod 644 file      # rw-r--r--
chmod u+x file      # add execute for owner
chmod go-w file     # remove write for group and other
```

## chown
```sh
chown user:group file
chown -R labuser:labuser /opt/lab   # recursive
```

## Special Permissions
```sh
chmod 4755 file     # setuid — runs as file owner
chmod 2755 dir      # setgid — new files inherit group
chmod 1777 /tmp     # sticky bit — only owner can delete
```

## Security-Relevant Mount Options
In `/etc/fstab`:
```
/dev/da0p4  /tmp  ufs  rw,noexec,nosuid,nodev  2  2
```
- `noexec` — prevent execution of binaries
- `nosuid` — ignore setuid/setgid bits
- `nodev` — ignore device files

## BSD-Specific: File Flags
```sh
chflags schg file       # system immutable (even root can't modify at securelevel > 0)
chflags noschg file     # remove immutable flag
ls -lo file             # show flags
```

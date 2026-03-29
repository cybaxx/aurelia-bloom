# Shell Basics

## The Shell
BSD systems use `/bin/sh` (Bourne shell) by default. It follows POSIX standards and is simpler than bash.

## Navigation
```sh
pwd                 # print working directory
cd /usr/local       # change directory
cd ..               # go up one level
cd ~                # go to home directory
ls -la              # list all files with details
```

## File Operations
```sh
cp source dest      # copy
mv old new          # move/rename
rm file             # remove (no undo!)
mkdir -p a/b/c      # create nested directories
touch file          # create empty file / update timestamp
```

## Viewing Files
```sh
cat file            # print entire file
head -20 file       # first 20 lines
tail -f /var/log/messages  # follow log output
less file           # page through file (q to quit)
```

## Pipes and Redirection
```sh
cmd > file          # redirect stdout to file
cmd >> file         # append to file
cmd 2>&1            # redirect stderr to stdout
cmd1 | cmd2         # pipe output of cmd1 into cmd2
```

## Examples
```sh
# Find all .c files under /opt/lab
find /opt/lab -name "*.c"

# Count lines in a file
wc -l /etc/passwd

# Search for a pattern
grep "root" /etc/passwd

# Sort and deduplicate
sort file | uniq
```

## BSD Differences from Linux
- `ls -G` for colors (not `--color`)
- No `ip` command — use `ifconfig`
- Init scripts in `/etc/rc.d/`, not systemd
- Default shell is `/bin/sh`, not bash

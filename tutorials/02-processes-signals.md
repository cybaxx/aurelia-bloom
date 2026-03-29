# Processes & Signals

## Viewing Processes
```sh
ps aux              # all processes, detailed
ps -ef              # alternative format
top                 # interactive process monitor (q to quit)
```

## Process Control
```sh
command &           # run in background
jobs                # list background jobs
fg %1              # bring job 1 to foreground
bg %1              # resume job 1 in background
Ctrl+Z             # suspend current process
Ctrl+C             # send SIGINT (interrupt)
```

## Signals
| Signal  | Number | Default Action | Use |
|---------|--------|---------------|-----|
| SIGHUP  | 1      | Terminate     | Reload config |
| SIGINT  | 2      | Terminate     | Ctrl+C |
| SIGKILL | 9      | Terminate     | Force kill (cannot catch) |
| SIGSEGV | 11     | Core dump     | Segmentation fault |
| SIGTERM | 15     | Terminate     | Graceful shutdown |
| SIGSTOP | 17     | Stop          | Cannot catch |

```sh
kill -15 <pid>      # send SIGTERM
kill -9 <pid>       # force kill
kill -HUP <pid>     # reload config
pkill -f "name"     # kill by process name
```

## Process Inspection (useful for exploitation)
```sh
procstat -v <pid>   # virtual memory map (BSD equivalent of /proc/pid/maps)
procstat -b <pid>   # binary path
procstat -e <pid>   # environment
```

## Core Dumps
```sh
sysctl kern.coredump=1              # enable core dumps
ulimit -c unlimited                 # allow unlimited core size
# After crash:
gdb ./program /path/to/core        # analyze core dump
```

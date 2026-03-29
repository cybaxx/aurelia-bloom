# Shellcode: Bind Shell

## Objective
Create shellcode that binds a shell to TCP port 4444, allowing remote access.

## How It Works
1. `socket()` — create a TCP socket
2. `bind()` — bind to 0.0.0.0:4444
3. `listen()` — wait for connections
4. `accept()` — accept incoming connection
5. `dup2()` — redirect stdin/stdout/stderr to the socket
6. `execve("/bin/sh")` — spawn a shell

## BSD vs Linux
The syscall **numbers** differ significantly:
| Syscall  | Linux | BSD |
|----------|-------|-----|
| socket   | 41    | 97  |
| bind     | 49    | 104 |
| listen   | 50    | 106 |
| accept   | 43    | 30  |
| dup2     | 33    | 90  |

## Test
```sh
# Terminal 1 (target):
./shellcode

# Terminal 2 (attacker):
nc target-basic 4444
```

## Exercises
1. Change the port number
2. Add a connect-back (reverse shell) variant
3. Test on target-hardened — observe W^X enforcement

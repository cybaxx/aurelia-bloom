# Kernel Security Features

## ASLR (Address Space Layout Randomization)
Randomizes the base addresses of the stack, heap, shared libraries, and (with PIE) the executable itself.

```sh
# Check status
sysctl hardening.pax.aslr.status

# 0 = disabled, 1 = opt-in, 2 = opt-out (default on HardenedBSD), 3 = force
```

### Per-binary control
```sh
# Disable ASLR for a specific binary (for debugging)
hbsdcontrol pax disable aslr /opt/lab/bin/vulnerable
```

## CFI (Control Flow Integrity)
Prevents code-reuse attacks (ROP, JOP) by validating that indirect calls and jumps target valid function entries.

HardenedBSD compiles base system binaries with Clang's CFI.

## SEGVGUARD
Detects repeated segfaults from the same binary and temporarily blocks execution — defeats brute-force ASLR bypass.

```sh
sysctl hardening.pax.segvguard.status
```

## Retpoline
Mitigates Spectre v2 (branch target injection) by replacing indirect branches with a "return trampoline."

```sh
sysctl hw.ibrs_active
```

## Securelevel
A kernel security level that restricts what even root can do:

| Level | Restrictions |
|-------|-------------|
| -1    | Permanently insecure (development) |
| 0     | Insecure (normal boot) |
| 1     | Secure: no module loading, no raw disk writes, no debugger attach |
| 2     | Highly secure: + no time changes, no pf rule changes |

```sh
sysctl kern.securelevel          # check current level
sysctl kern.securelevel=1        # raise (cannot lower without reboot)
```

## W^X (Write XOR Execute)
Memory pages cannot be both writable and executable simultaneously. This prevents classic shellcode injection where you write code to the stack/heap and execute it.

HardenedBSD enforces strict W^X on all processes by default.

## Implications for Exploitation
These features create a layered defense:
1. **ASLR** — attacker can't predict addresses → needs an info leak
2. **W^X** — can't inject shellcode → must use code reuse (ret2libc, ROP)
3. **CFI** — validates call targets → limits available ROP gadgets
4. **SEGVGUARD** — blocks brute force → can't spray and pray
5. **Securelevel** — even with root, kernel is locked down

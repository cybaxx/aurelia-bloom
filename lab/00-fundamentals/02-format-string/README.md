# Level 2: Format String Vulnerability

## Objective
Use a format string bug to overwrite the `auth` variable and gain access.

## Background
When user input is passed directly as the format string to `printf`/`snprintf`, an attacker can:
- **Read memory** with `%x`, `%p`, `%s`
- **Write memory** with `%n` (writes the number of characters printed so far)

## Steps

1. **Leak stack values:**
   ```
   ./vulnerable '%p.%p.%p.%p.%p.%p.%p.%p'
   ```

2. **Find the address of `auth`** (printed by the program, or via gdb).

3. **Craft the write:**
   Place the target address in the format string and use `%n` to write to it.

4. **Verify:** The program prints "Access granted!" if `auth != 0`.

## On target-basic vs target-hardened
- **target-basic**: Fixed addresses — straightforward `%n` write
- **target-hardened**: ASLR + PIE — address of `auth` changes every run; requires an info leak first

## Solution
See `exploit.c`.

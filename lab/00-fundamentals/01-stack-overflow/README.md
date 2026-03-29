# Level 1: Stack Buffer Overflow

## Objective
Overwrite the saved return address on the stack to redirect execution to `secret()`.

## Background
When `strcpy()` copies more data than the destination buffer can hold, it overwrites adjacent stack memory — including the saved frame pointer and return address.

## Steps

1. **Find the buffer size and offset to return address:**
   ```
   gdb ./vulnerable
   (gdb) disas vulnerable
   (gdb) p &secret
   ```

2. **Calculate the payload:**
   - Buffer: 64 bytes
   - Saved RBP: 8 bytes
   - Return address: next 8 bytes
   - Total padding: 72 bytes of junk + 8 bytes for the address of `secret()`

3. **Run the exploit:**
   ```
   ./vulnerable $(python3 -c "print('A'*72 + '<addr>')")
   ```

## On target-basic vs target-hardened
- **target-basic**: ASLR off, no canaries — exploit works reliably
- **target-hardened**: ASLR + stack canaries — the program crashes with `*** stack smashing detected ***` before your overwritten return address is reached

## Solution
See `exploit.c` for a working exploit generator.

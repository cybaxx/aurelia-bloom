# Phase 2 Tasks — Low to High Impact

Ordered by impact toward the jail-based single-VM architecture.
Current state: 4 separate VMs, no jails, 20 GB disk, ~5 GB RAM.

---

## Low Impact (cleanup, prep work)

### 1. Remove compiled .o files from chat/obj/
Build artifacts checked into repo. Clean up and .gitignore them.
- Files: `chat/obj/*.o`
- Effort: 5 min

### 2. Remove redundant pip install in attacker.sh
Line 10 pip-installs pwntools, but `py311-pwntools` is already pkg-installed on line 6.
- File: `provisioning/attacker.sh:10`
- Effort: 1 min

### 3. Remove duplicate curl/wget in base.sh
Both installed; keep `curl`, drop `wget`.
- File: `packer/scripts/base.sh:11`
- Effort: 1 min

### 4. Strip debug symbols from lab binaries
Remove `-g` flag and add `strip` post-compile. Smaller binaries for jail deployment later.
- Files: `lab/Makefile:5-6`, `provisioning/lab-setup.sh`
- Effort: 10 min

### 5. Add base box cleanup (man pages, locale, docs)
Add removal of `/usr/share/man`, `/usr/share/info`, `/usr/share/locale`, `/usr/share/examples` to `packer/scripts/base.sh`. Shrinks base box ~20-30 MB.
- File: `packer/scripts/base.sh` (before dd zero-fill)
- Effort: 10 min

---

## Medium Impact (package reduction, Vagrantfile simplification)

### 6. Remove lldb from common.sh
~100-120 MB savings. gdb is sufficient for all lab exercises.
- File: `provisioning/common.sh:10`
- Effort: 1 min

### 7. Remove bash, vim, tmux, git from common.sh
Not needed on target VMs. `vi` is built-in, `sh` is the convention.
Move `git` to attacker-only if needed.
- File: `provisioning/common.sh:10-11`
- Savings: ~70 MB per VM
- Effort: 10 min

### 8. Move gdb, python3 to attacker-only installs
Targets don't need debuggers or Python. Reduces target VM footprint.
- Files: `provisioning/common.sh:10`, `provisioning/attacker.sh:6`
- Savings: ~100 MB per target VM
- Effort: 10 min

### 9. Reduce Packer disk_size from 20 GB to 6 GB
Current 20 GB is excessive. 6 GB is plenty for host + jails.
- File: `packer/hardenedbsd.pkr.hcl:27`
- Effort: 1 min

### 10. Reduce VM memory in Vagrantfile (interim)
Drop targets from 1024 to 512 MB. Prep for single-VM where this goes away entirely.
- File: `Vagrantfile:40,56,72`
- Effort: 5 min

---

## High Impact (architectural — jail migration)

### 11. Install BusyBox in Packer base box
Add `pkg install -y busybox-freebsd` to `packer/scripts/base.sh`.
Foundation for minimal jail filesystems.
- File: `packer/scripts/base.sh`
- Effort: 5 min

### 12. Create jail filesystem skeleton script
New `provisioning/jails.sh` that:
- Creates `/jails/{attacker,basic,hardened,service}` directory trees
- Populates BusyBox + symlinks in target jails
- Extracts minimal libc/ld-elf.so into target jail `/lib/`
- Creates `/etc/passwd`, `/etc/hosts`, etc. per jail
- File: NEW `provisioning/jails.sh`
- Effort: 2-3 hours

### 13. Create jail.conf with vnet networking
Write `/etc/jail.conf` with bridge0 + epair interfaces per jail.
Test vnet support on HardenedBSD 15-STABLE.
- File: NEW `etc/jail.conf` (or template in `provisioning/`)
- Effort: 2-3 hours (includes network debugging)

### 14. Install Dropbear SSH into target jails
Static ~110 KB SSH daemon per jail. Allows direct SSH to jail IPs.
Configure labuser key/password auth.
- Affects: jail filesystem setup, provisioning
- Effort: 1-2 hours

### 15. Rewrite Vagrantfile for single VM
Replace 4 VM definitions with 1. Single VM boots, runs jail provisioning.
- Old `Vagrantfile` → `Vagrantfile.multi-vm` (backup)
- New `Vagrantfile`: 1 VM, 2 GB RAM, 2 CPUs, single private network IP
- Provisioner chain: base → jails → attacker → targets → services
- File: `Vagrantfile`
- Effort: 1 hour

### 16. Refactor provisioning scripts for jail context
Rewrite `attacker.sh`, `target-basic.sh`, `target-hardened.sh`, `services.sh` to:
- Install into jail filesystems instead of root `/`
- Use `jexec` or chroot for in-jail commands
- Handle sysctl/securelevel per-jail
- Move `chat.sh` into attacker jail context
- Files: all `provisioning/*.sh`
- Effort: 3-4 hours

### 17. Refactor lab-setup.sh for cross-jail installs
Compile on host, install binaries into each jail's `/opt/lab/bin/`.
- File: `provisioning/lab-setup.sh`
- Effort: 30 min

### 18. Update Packer provisioner chain
Add `jails.sh` to Packer build. Pre-bake jail skeletons into the box
so `vagrant up` only needs to populate and start jails.
- File: `packer/hardenedbsd.pkr.hcl`
- Effort: 1 hour

### 19. End-to-end testing and documentation
- Verify all 4 jails start and get correct IPs
- Test SSH to each jail IP
- Run all lab exercises against jailed targets
- Test chat system in attacker jail
- Update CLAUDE.md build instructions
- Update README
- Effort: 2-3 hours

---

## Summary

| Impact | Tasks | Est. Total Effort |
|--------|-------|-------------------|
| Low    | 1-5   | ~30 min           |
| Medium | 6-10  | ~30 min           |
| High   | 11-19 | ~15-20 hours      |

Low and medium tasks are worth doing now regardless — they shrink the current
box and simplify the codebase. High-impact tasks are the actual jail migration
and should be done sequentially (11 → 19).

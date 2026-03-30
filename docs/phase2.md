# Phase 2: Single-VM Jail Architecture

## Overview

Consolidate the 4-VM cyber range into a single HardenedBSD VM using FreeBSD jails
with vnet networking. Students interact with the same IPs and services — the
difference is invisible at the network layer.

## Target Architecture

```
┌─ VirtualBox VM (HardenedBSD host) ──────────────────────┐
│                                                          │
│  Kernel: HardenedBSD 15-STABLE                          │
│  Host tools: jail management, bridge/epair networking    │
│                                                          │
│  bridge0 ─── 192.168.56.0/24                            │
│    ├── epair0b → jail:attacker   .10  (full tooling)    │
│    ├── epair1b → jail:basic      .20  (busybox minimal) │
│    ├── epair2b → jail:hardened   .30  (busybox minimal) │
│    └── epair3b → jail:service    .40  (busybox + nginx) │
│                                                          │
│  Shared: kernel, /usr/local/share/lab (nullfs mounts)   │
└──────────────────────────────────────────────────────────┘
```

## Size Targets

| Component          | Current (4 VMs)  | Phase 2 (1 VM + jails) |
|--------------------|------------------|------------------------|
| Vagrant box file   | ~1.5-2 GB        | ~300-500 MB            |
| Disk allocation    | 20 GB            | 4-6 GB                 |
| Runtime RAM        | 5 GB             | 1.5-2 GB               |
| Boot time          | ~3-5 min (4 VMs) | ~30-60 sec             |

## Jail Filesystem Layout

### Attacker Jail (full userland)
```
/jails/attacker/
├── bin/, sbin/, lib/, libexec/   # FreeBSD base (or subset)
├── usr/local/                    # radare2, pwntools, gdb, gcc, nasm
├── opt/exploits/                 # exploit workspace
├── opt/lab/ → nullfs from host   # shared lab materials
├── etc/                          # jail-specific config
└── home/labuser/
```
~500-800 MB — this jail carries the heavy tools.

### Target Jails (BusyBox minimal)
```
/jails/target-basic/
├── bin/busybox              # ~1.5 MB, symlinks for sh, ls, cat, etc.
├── lib/                     # libc.so, ld-elf.so, libgcc_s.so (minimal)
├── etc/passwd, group, hosts
├── opt/lab/bin/             # vulnerable binaries (~50 KB each)
├── tmp/
├── dev/                     # devfs mount
└── home/labuser/
```
~5-15 MB per jail.

### Service Jail (BusyBox + nginx)
```
/jails/target-service/
├── bin/busybox
├── lib/                     # minimal libs
├── usr/local/sbin/nginx     # static or minimal nginx
├── usr/local/www/cgi-bin/   # vulnerable CGI scripts
├── etc/
└── opt/lab/bin/
```
~15-30 MB.

## Networking Design

### Bridge + epair (vnet jails)

```sh
# Host setup
ifconfig bridge0 create
ifconfig bridge0 inet 192.168.56.1/24

# Per jail
ifconfig epair0 create
ifconfig epair0a up
ifconfig bridge0 addm epair0a

# Inside jail (via jail.conf exec.start or vnet config)
ifconfig epair0b inet 192.168.56.10/24
route add default 192.168.56.1
```

### jail.conf

```conf
# /etc/jail.conf

path = "/jails/$name";
host.hostname = "$name";
vnet;
vnet.interface = "epair${id}b";
mount.devfs;
allow.raw_sockets;
exec.clean;

exec.prestart  = "ifconfig epair${id} create";
exec.prestart += "ifconfig bridge0 addm epair${id}a";
exec.prestart += "ifconfig epair${id}a up";

exec.start     = "/bin/sh /etc/rc";
exec.stop      = "/bin/sh /etc/rc.shutdown";

exec.poststop  = "ifconfig bridge0 deletem epair${id}a";
exec.poststop += "ifconfig epair${id}a destroy";

attacker {
    $id = 0;
    vnet.interface = "epair0b";
    exec.start += "ifconfig epair0b inet 192.168.56.10/24";
}

basic {
    $id = 1;
    vnet.interface = "epair1b";
    exec.start += "ifconfig epair1b inet 192.168.56.20/24";
    securelevel = -1;  # allow disabling ASLR inside
}

hardened {
    $id = 2;
    vnet.interface = "epair2b";
    exec.start += "ifconfig epair2b inet 192.168.56.30/24";
    securelevel = 1;
}

service {
    $id = 3;
    vnet.interface = "epair3b";
    exec.start += "ifconfig epair3b inet 192.168.56.40/24";
    exec.start += "service nginx start";
}
```

## Provisioning Changes

### Packer Template (hardenedbsd.pkr.hcl)
- `disk_size`: 20480 → 6144
- Add `scripts/jails.sh` provisioner to base build
- Install BusyBox package into base box
- Pre-create jail filesystem skeletons

### New: provisioning/jails.sh (host-level jail setup)
- Create `/jails/{attacker,basic,hardened,service}` trees
- Install BusyBox into target jail trees
- Extract minimal libc/ld-elf into target jail libs
- Configure `/etc/jail.conf`
- Configure bridge0 networking
- Enable jails at boot via `jail_enable="YES"` and `jail_list`

### Vagrantfile
- Single VM definition (replaces 4)
- 2 GB RAM, 2 CPUs
- Single provisioner chain: common → jails → attacker → targets → services
- Private network on host VM: 192.168.56.1 (bridge gateway)
- No per-VM network config needed (jails handle it)

### common.sh
- Becomes host-only: no more per-VM package installs
- Heavy packages (gcc, gdb, radare2, etc.) install once on host
- Nullfs-mount shared tools into attacker jail
- Target jails get only BusyBox + lab binaries

### attacker.sh
- Provisions attacker jail filesystem
- Installs radare2, pwntools, gdb, capstone into jail or via nullfs
- Sets up exploit workspace inside jail
- Chat system runs inside attacker jail

### target-basic.sh / target-hardened.sh
- Write jail-specific sysctl configs
- Copy lab binaries into jail tree
- BusyBox symlink setup
- SSH inside jails via host sshd forwarding or per-jail dropbear (~110 KB)

### services.sh
- Install nginx into service jail (static binary or minimal pkg)
- Deploy CGI scripts into jail tree
- Configure nginx inside jail

### lab-setup.sh
- Cross-compile lab binaries on host
- Install into each jail's `/opt/lab/bin/`

## SSH Access Strategy

Two options:

**Option A: Host sshd + port forwarding**
```
Host sshd listens on 22
  Port 2220 → jail exec attacker
  Port 2221 → jail exec basic
  Port 2222 → jail exec hardened
  Port 2223 → jail exec service
```
Students: `ssh labuser@192.168.56.10` → host forwards to jail.

**Option B: Dropbear per jail (~110 KB static binary)**
Each jail runs its own lightweight SSH daemon. More realistic, minimal overhead.

Recommended: **Option B** — keeps the "separate machines" illusion intact.

## Build Process

```sh
cd packer && packer build hardenedbsd.pkr.hcl
vagrant box add hardenedbsd-15stable packer/hardenedbsd-15stable.box
vagrant up        # boots single VM, starts all jails
vagrant ssh       # lands on host
jexec attacker sh # enter attacker jail
```

## Migration Path

1. Keep current 4-VM Vagrantfile as `Vagrantfile.multi-vm` (fallback)
2. New `Vagrantfile` uses single-VM + jail provisioning
3. All lab exercises, IPs, and workflows remain identical
4. Students can still `ssh labuser@192.168.56.20` etc.

## Risks and Mitigations

| Risk | Mitigation |
|------|-----------|
| Shared kernel — jail escape = full compromise | Feature, not bug: add jail escape lab exercise |
| BusyBox behavior differs from FreeBSD userland | Only targets use BusyBox; lab binaries are native ELF |
| vnet complexity on HardenedBSD | Test epair/bridge support early; fallback to IP alias jails |
| Per-jail sysctl (ASLR toggle) | Use `security.jail.sysctl_allowed` or per-jail securelevel |
| Dropbear vs OpenSSH compatibility | Test key auth and labuser password auth with dropbear |

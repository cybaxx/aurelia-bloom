# CLAUDE.md

## Project
HardenedBSD Cyber Range — IaC security learning platform using Packer + Vagrant.
Single VM with FreeBSD jails for lightweight isolation.

## Architecture
Single HardenedBSD VM hosts 4 jails connected via bridge0 + vnet epair networking:
- `attacker` (.10) — Full FreeBSD userland, exploit tools (radare2, gdb, pwntools)
- `basic` (.20) — BusyBox minimal, protections disabled (ASLR off)
- `hardened` (.30) — BusyBox minimal, full protections (ASLR, securelevel=1, pf)
- `service` (.40) — BusyBox + nginx, vulnerable CGI services

## Structure
- `packer/` — Packer template builds HardenedBSD ISO into a Vagrant box
- `Vagrantfile` — Single VM, provisions host + jails (legacy multi-VM: `Vagrantfile.multi-vm`)
- `provisioning/` — Shell provisioners (POSIX sh, not bash — BSD compatibility)
  - `common.sh` — Host-level setup (pkg, labuser, build tools)
  - `jails.sh` — Creates jail filesystems, bridge networking, jail.conf
  - `jail-attacker.sh` — Attacker jail packages and tools
  - `jail-targets.sh` — Target jail config, lab binary compilation + install
  - `jail-services.sh` — Service jail nginx + vulnerable CGI
- `lab/` — Vulnerable C source, exploit code, READMEs. Makefile has `basic` and `hardened` targets
- `chat/` — Encrypted P2P chat over Tor hidden services
- `tutorials/` — Markdown Unix tutorials
- `guides/` — Markdown HardenedBSD security guides
- `docs/` — Architecture docs (phase2.md, phase2-tasks.md)

## Conventions
- Provisioning scripts use `/bin/sh` (not bash) for BSD portability
- Lab binaries install to `/opt/lab/bin/` inside target jails
- Base box: `hardenedbsd-15stable`
- Private network: 192.168.56.0/24 (bridge0 on host at .1)
- Jail IPs: .10=attacker, .20=basic, .30=hardened, .40=service
- Lab user: `labuser` (password: `labuser`)
- Vagrant user: `vagrant` (insecure keypair)
- Target jails use BusyBox + Dropbear SSH (~5-15 MB each)
- Attacker jail uses full FreeBSD userland + OpenSSH

## Building
```sh
cd packer && packer build hardenedbsd.pkr.hcl
vagrant box add hardenedbsd-15stable packer/hardenedbsd-15stable.box
vagrant up        # boots single VM, creates and starts all jails
vagrant ssh       # lands on host
jexec attacker sh # enter attacker jail
```

## Testing
```sh
sh tests/test_provisioning.sh  # validate scripts, configs, structure
```

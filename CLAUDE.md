# CLAUDE.md

## Project
HardenedBSD Cyber Range — IaC security learning platform using Packer + Vagrant.

## Structure
- `packer/` — Packer template builds HardenedBSD ISO into a Vagrant box
- `Vagrantfile` — 4 VMs: attacker, target-basic, target-hardened, target-service
- `provisioning/` — Shell provisioners (POSIX sh, not bash — BSD compatibility)
- `lab/` — Vulnerable C source, exploit code, READMEs. Makefile has `basic` and `hardened` targets
- `tutorials/` — Markdown Unix tutorials
- `guides/` — Markdown HardenedBSD security guides

## Conventions
- Provisioning scripts use `/bin/sh` (not bash) for BSD portability
- Lab binaries install to `/opt/lab/bin/` on targets
- All VMs use the same base box: `hardenedbsd-15stable`
- Private network: 192.168.56.0/24 (.10=attacker, .20=basic, .30=hardened, .40=service)
- Lab user: `labuser` (password: `labuser`)
- Vagrant user: `vagrant` (insecure keypair)

## Building
```sh
cd packer && packer build hardenedbsd.pkr.hcl
vagrant box add hardenedbsd-15stable packer/hardenedbsd-15stable.box
vagrant up
```

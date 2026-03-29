# aurelia-bloom

HardenedBSD Cyber Range — a self-contained security learning platform with 100% Infrastructure as Code.

```
112 145 154 154 171 106 151 163 150 103 162 145 167 :: 112 145 154 154 171 106 151 163 150 103 162 145 167 ::

                                                                                                                  
    @@@ @@@@@@@@ @@@      @@@      @@@ @@@ @@@@@@@@ @@@  @@@@@@ @@@  @@@  @@@@@@@ @@@@@@@  @@@@@@@@ @@@  @@@  @@@ 
    @@! @@!      @@!      @@!      @@! !@@ @@!      @@! !@@     @@!  @@@ !@@      @@!  @@@ @@!      @@!  @@!  @@! 
    !!@ @!!!:!   @!!      @!!       !@!@!  @!!!:!   !!@  !@@!!  @!@!@!@! !@!      @!@!!@!  @!!!:!   @!!  !!@  @!@ 
.  .!!  !!:      !!:      !!:        !!:   !!:      !!:     !:! !!:  !!! :!!      !!: :!!  !!:       !:  !!:  !!  
::.::   : :: ::  : ::.: : : ::.: :   .:     :       :   ::.: :   :   : :  :: :: :  :   : : : :: ::    ::.:  :::   
                                                                                                                  
                                                                                                                               
112 145 154 154 171 106 151 163 150 103 162 145 167 :: 112 145 154 154 171 106 151 163 150 103 162 145 167 ::                                                                                                                               
```

```
git clone <repo> && cd aurelia-bloom
cd packer && packer build hardenedbsd.pkr.hcl   # build base box from ISO
cd .. && vagrant up                               # boot all 4 VMs
vagrant ssh attacker                              # start hacking
```

## Architecture

```
┌─────────────────────────────────────────────────────────────┐
│                  Private Network 192.168.56.0/24            │
│                                                             │
│  ┌─────────────-┐  ┌──────────────┐  ┌───────────────────┐  │
│  │   attacker   │  │ target-basic │  │  target-hardened  │  │
│  │  .10  2G/2C  │  │  .20  1G/1C  │  │   .30  1G/1C      │  │
│  │  radare2     │  │  ASLR=off    │  │   ASLR=on         │  │
│  │  gdb/pwntools│  │  no canaries │  │   pf firewall     │  │
│  └─────────────-┘  └──────────────┘  │   securelevel=1   │  │
│                                      └───────────────────┘  │
│  ┌────────────────┐                                         │
│  │ target-service │                                         │
│  │  .40  1G/1C    │                                         │
│  │  nginx + CGI   │                                         │
│  │  vuln services │                                         │
│  └────────────────┘                                         │
└─────────────────────────────────────────────────────────────┘
```

## Components

| Directory | What |
|-----------|------|
| `packer/` | Packer template: HardenedBSD ISO → Vagrant box |
| `provisioning/` | Shell scripts that configure each VM |
| `lab/` | Vulnerable C programs + exploits + writeups |
| `tutorials/` | Unix fundamentals (shell, permissions, networking, pf) |
| `guides/` | HardenedBSD security deep-dives (ASLR, CFI, pledge, pf) |
| `chat/` | Tor-based E2E encrypted P2P chat between players |

## Lab Exercises

### Tier 0 — Fundamentals
- `lab/00-fundamentals/01-stack-overflow/` — Classic buffer overflow
- `lab/00-fundamentals/02-format-string/` — Format string read/write
- `lab/00-fundamentals/03-heap-overflow/` — Heap-based function pointer overwrite

### Tier 1 — Shellcode
- `lab/01-shellcode/01-execve/` — BSD execve shellcode
- `lab/01-shellcode/02-bind-shell/` — TCP bind shell

### Tier 2 — Exploitation
- `lab/02-exploitation/01-ret2libc/` — Return-to-libc (bypass W^X)
- `lab/02-exploitation/02-rop-chains/` — ROP chain construction
- `lab/02-exploitation/03-pledge-bypass/` — Exploit after privilege drop

## Prerequisites

- [Packer](https://packer.io) >= 1.9
- [Vagrant](https://vagrantup.com) >= 2.3
- [VirtualBox](https://virtualbox.org) >= 7.0
- ~8 GB free RAM, ~40 GB disk

## Quick Start

1. **Build the base box:**
   ```sh
   cd packer
   packer init hardenedbsd.pkr.hcl
   packer build hardenedbsd.pkr.hcl
   vagrant box add hardenedbsd-15stable hardenedbsd-15stable.box
   cd ..
   ```

2. **Launch the range:**
   ```sh
   vagrant up              # all VMs
   vagrant up attacker     # just the attacker
   ```

3. **Connect:**
   ```sh
   vagrant ssh attacker
   ssh labuser@target-basic    # password: labuser
   ```

4. **Run a lab:**
   ```sh
   # On target-basic:
   /opt/lab/bin/vulnerable $(python3 -c "print('A'*80)")
   ```

## Player Chat

The attacker VM includes an E2E encrypted P2P chat system over Tor. Players on separate machines (even behind NAT) can communicate securely:

```sh
vagrant ssh attacker
chat id                       # show your .onion address
chat add-peer <other.onion>   # connect to another player
chat                          # interactive mode
```

Messages are encrypted with XChaCha20-Poly1305 (libsodium). Peers authenticate via a shared secret (defaults to `aurelia-bloom`). See `chat/README.md` for details.

## Workflow: basic vs hardened

Every lab exercise compiles on both targets:
- **target-basic**: Protections off — learn the technique
- **target-hardened**: Protections on — see why modern defenses work

---

*Becoming jellyfish of the hydrosphere, the isomorph between human and machine existence of the mechanosphere, and the breathing together in the cold air of the stratosphere.*

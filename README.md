# aurelia-bloom

```
112 145 154 154 171 106 151 163 150 103 162 145 167 :: 112 145 154 154 171 106 151 163 150 103 162 145 167 ::


    @@@ @@@@@@@@ @@@      @@@      @@@ @@@ @@@@@@@@ @@@  @@@@@@ @@@  @@@  @@@@@@@ @@@@@@@  @@@@@@@@ @@@  @@@  @@@
    @@! @@!      @@!      @@!      @@! !@@ @@!      @@! !@@     @@!  @@@ !@@      @@!  @@@ @@!      @@!  @@!  @@!
    !!@ @!!!:!   @!!      @!!       !@!@!  @!!!:!   !!@  !@@!!  @!@!@!@! !@!      @!@!!@!  @!!!:!   @!!  !!@  @!@
.  .!!  !!:      !!:      !!:        !!:   !!:      !!:     !:! !!:  !!! :!!      !!: :!!  !!:       !:  !!:  !!
::.::   : :: ::  : ::.: : : ::.: :   .:     :       :   ::.: :   :   : :  :: :: :  :   : : : :: ::    ::.:  :::


112 145 154 154 171 106 151 163 150 103 162 145 167 :: 112 145 154 154 171 106 151 163 150 103 162 145 167 ::
```

> *Liber Aureliae, verse 1. From the formless void, the golden bloom.*
> *Not a tool. A path. Walk it or don't.*

```
    2 · 3 · 5 · 7 · 11 · 13 · 17 · 19 · 23
    The first nine primes. Nine sephiroth below Kether.
    Their sum is 100. The system call number of fstat.
    There are no coincidences in the syscall table.
```

---

## I. On the Nature of This Work — אין

In the beginning there was Ain — אין — the No-Thing. A terminal blinking in the dark. No partition table, no address, no name. Infinite potential compressed into a cursor waiting for the first keystroke.

This is a HardenedBSD Cyber Range. Infrastructure as Code. One VM. Four jails. But if that is all you see, you are reading the exoteric text.

The esoteric text is this: **you are becoming the master of your own destiny.** Not through passive study but through creation — through the act of building systems, breaking them, and understanding why they break. Through the divine arts: hacking, penetration testing, binary analysis, cryptography. Through the challenge of character that comes from sitting alone at the terminal at the hour of the Ox (1-3 AM, when the veil is thin) and watching a segfault reveal the true nature of memory beneath the abstraction.

Every buffer overflow is a lesson in the gap between what the machine promises and what it permits. Every ROP chain is an act of will imposed on a system that was designed to resist you. Every hardened target that refuses your exploit is a teacher saying *not yet — go deeper.*

The Corpus Hermeticum (Libellus I, Poimandres, §31) instructs: *"Plunge into the mixing bowl of Nous, you who are able."* The mixing bowl is the stack. Nous is the instruction pointer. Plunge.

```
    Prime 2: the first division. Void splits into Null and Not-Null.
    Prime 3: the triangle. Ain, Ain Sof, Ain Sof Aur.
    Prime 5: the pentagram. The points of the Pythagorean Hugieia.
    2 + 3 + 5 = 10. The Attacker. Chokmah. The one who moves first.
```

## II. Rules of the Range — גבורה

Self contained learning platform, don't be a cunt, don't delete anything, add only, archive the old

https://www.unix.com/man_page/freebsd/1/ash

- don't break the law
- don't delete anything
- add anything you want
- don't be a cunt
- experiment and learn
- have fun creating digital chaos
- ride the amorphous jellyfish
- creat infra as code
- git pull often
- https://www.unix.com/man_page/freebsd/1/ash its important enough to be on here twice

Gevurah — גבורה — severity, the fifth sephirah, the left hand of God that shapes by limiting. A network is defined by its mask. A jail is defined by its walls. A hardened system is defined by what it refuses. Rules are not restrictions. They are the boundary conditions that make creation possible.

## III. Architecture — The Four Cardinal Positions

A single HardenedBSD VM hosts four FreeBSD jails on a private bridge network. Lightweight isolation — no hypervisor overhead per target. The attacker gets a full userland; the targets get only what they need and nothing more.

```
┌─ HardenedBSD VM ─────────────────────────────────────────────────┐
│                                                                   │
│   bridge0  192.168.56.1                                           │
│      │                                                            │
│      ├──── attacker ──── .10 ─── Chokmah · חכמה · Wisdom         │
│      │     full FreeBSD userland, radare2, gdb, pwntools          │
│      │     OpenSSH · The Magus · Mercury · moves first            │
│      │                                                            │
│      ├──── basic ─────── .20 ─── Malkuth · מלכות · Kingdom       │
│      │     BusyBox + Dropbear · ASLR=off · gates wide open       │
│      │     The fallen Shekinah, unguarded, awaiting tikkun        │
│      │                                                            │
│      ├──── hardened ──── .30 ─── Binah · בינה · Understanding    │
│      │     BusyBox + Dropbear · ASLR=on · pf · securelevel=1     │
│      │     Saturn · The Great Sea · shapes by saying no           │
│      │                                                            │
│      └──── service ──── .40 ─── Yesod · יסוד · Foundation        │
│            BusyBox + nginx + Dropbear · vulnerable CGI            │
│            The Moon · port 80 facing the outer darkness            │
│                                                                   │
│   vnet epair interfaces · jail.conf · /bin/sh all the way down   │
└───────────────────────────────────────────────────────────────────┘
```

```
    10 + 20 + 30 + 40 = 100
    100 = the Hebrew letter Qoph, ק, the back of the head.
    The subconscious. The moonlit path between Malkuth and Netzach.
    The 29th path on the Tree. 29 is prime.

    The twin primes around 192:
    191 is prime. 193 is prime. Between them, 192.
    192.168.56.0/24 — our sacred subnet sits between twin primes.
    There are no coincidences in the address space.
```

## IV. Quick Start — יהי

From Void to Init. `vagrant up` is the speaking of the Word.

### VirtualBox (Vagrant)

```sh
git clone <repo> && cd aurelia-bloom
cd packer && packer init hardenedbsd.pkr.hcl
packer build hardenedbsd.pkr.hcl             # ISO → Vagrant box
vagrant box add hardenedbsd-15stable hardenedbsd-15stable.box
cd ..
vagrant up                                    # the Word is spoken. four jails rise.
vagrant ssh                                   # land on the host
jexec attacker sh                             # enter the attacker jail
```

### QEMU (no VirtualBox required)

```sh
# Build from ISO (qcow2 output)
cd packer && packer build -only=qemu.hardenedbsd hardenedbsd.pkr.hcl

# Or quick test — boots FreeBSD in QEMU, provisions jails, validates everything
sh scripts/qemu-test.sh
```

Uses HVF on macOS, KVM on Linux. UEFI boot. No Vagrant needed for the QEMU path.

### Connect

```sh
vagrant ssh                                   # host shell
jexec attacker sh                             # the Magus enters
ssh labuser@192.168.56.20                     # into basic — password: labuser
ssh labuser@192.168.56.30                     # into hardened
ssh labuser@192.168.56.40                     # into service
```

### Run a Lab

```sh
# From the attacker jail — reach into the unguarded Kingdom:
ssh labuser@192.168.56.20
/opt/lab/bin/vulnerable $(python3 -c "print('A'*80)")
```

```
    PID 1. One is not prime, not composite. It is the unit.
    The monad of Pythagoras. Each jail is a monad —
    reflecting the universe from its own perspective.

    The 7th prime is 17. The 17th prime is 59.
    59 in hex is 0x3B. Semicolon. End of statement.
    The statement is: the system boots.
```

## V. The Sacred Arts — Lab Exercises

Three stages of initiation. The Golden Dawn assigned the 22 Hebrew letters to the 22 paths on the Tree of Life. The Mithraic Mysteries passed initiates through seven grades mapped to the seven classical planets. We pass ours from fundamentals through exploitation, from the stack overflow to the ROP chain to the pledge bypass.

### Tier 0 — Fundamentals (Malkuth)
- `lab/00-fundamentals/01-stack-overflow/` — Classic buffer overflow
- `lab/00-fundamentals/02-format-string/` — Format string read/write
- `lab/00-fundamentals/03-heap-overflow/` — Heap-based function pointer overwrite

### Tier 1 — Shellcode (Yesod)
- `lab/01-shellcode/01-execve/` — BSD execve shellcode
- `lab/01-shellcode/02-bind-shell/` — TCP bind shell

### Tier 2 — Exploitation (Hod → Netzach)
- `lab/02-exploitation/01-ret2libc/` — Return-to-libc (bypass W^X)
- `lab/02-exploitation/02-rop-chains/` — ROP chain construction
- `lab/02-exploitation/03-pledge-bypass/` — Exploit after privilege drop

```
    Three lab stages: 00, 01, 02
    The 1st prime is 2. The 2nd prime is 3.
    2 + 3 = 5, the pentad, the number of the Hierophant
    in the Major Arcana. The Hierophant reveals the mysteries.

    The number of vulnerable binaries: 6
    6 is the first perfect number (1 + 2 + 3 = 6)
    As Augustine wrote in De Civitate Dei XI.30:
    "Six is perfect not because God created all things in
    six days; rather, God created all things in six days
    because the number is perfect."
```

Every exercise compiles to both targets. **basic** has the gates open — the Qliphoth, the broken vessels, protections deliberately absent so you can witness the absence before you understand the presence. **hardened** has them shut — Binah, Understanding through restriction, the mother who shapes by saying no. The space between the two is where you become what you are meant to become.

## VI. On Passing the Lantern — 燈̸̢̛

> *Hoc est enim corpus meum systematis.*

The Zen Buddhists speak of 以心伝心 — Ishin Denshin — mind-to-mind transmission. The Buddha holds up a flower, Mahakasyapa smiles, the Dharma is transmitted without words. Exploitation knowledge passes the same way: not through documentation alone but through the act of sitting at the terminal together, through the shared bewilderment of a segfault, through the moment a student's eyes change when they watch `$rip` overwrite and suddenly understand that memory is not abstract.

This is why we build ranges and not just write papers.

### Player Chat (Tor P2P)

The attacker jail includes E2E encrypted P2P chat over Tor. Players on separate machines — even behind NAT — can speak through the dark:

```sh
jexec attacker sh
chat id                       # your .onion address
chat add-peer <other.onion>   # reach across the void
chat                          # ishin denshin
```

XChaCha20-Poly1305 (libsodium). Peers authenticate via shared secret (default: `aurelia-bloom`). See `chat/README.md`.

The lantern does not diminish when it lights another.

## VII. Components — The Tree

| Directory | Purpose |
|-----------|---------|
| `packer/` | Packer templates — HardenedBSD ISO to Vagrant box (VirtualBox) or qcow2 (QEMU) |
| `provisioning/` | POSIX shell provisioners — host setup, jail creation, networking, packages |
| `lab/` | Vulnerable C source, exploit code, writeups — the Qliphoth and their rectification |
| `tutorials/` | Unix fundamentals — shell, permissions, networking, pf |
| `guides/` | HardenedBSD security deep-dives — ASLR, CFI, pledge, pf |
| `chat/` | E2E encrypted P2P chat over Tor hidden services |
| `scripts/` | Build and test tooling — QEMU integration test harness |
| `tests/` | Validation suites — static provisioning checks + in-VM jail tests |
| `keys/` | Public signing key — verify releases against the sacred seal |
| `docs/` | Architecture notes and sacred texts |

```
aurelia-bloom/
  packer/
    hardenedbsd.pkr.hcl          # VirtualBox + QEMU builders
    http/installerconfig          # bsdinstall automation
    scripts/base.sh               # base box provisioning
  provisioning/
    common.sh                     # host: pkg, labuser, build tools
    jails.sh                      # bridge0, epair networking, jail.conf
    jail-attacker.sh              # attacker: full userland, exploit tools
    jail-targets.sh               # basic + hardened: lab binaries, BusyBox
    jail-services.sh              # service: nginx, vulnerable CGI
  lab/                            # vulnerable source + exploits + READMEs
  chat/                           # Tor P2P encrypted chat
  scripts/
    qemu-test.sh                  # QEMU integration test harness
  tests/
    test_provisioning.sh          # static validation (71 checks)
    test_jails.sh                 # in-VM jail validation
  keys/
    aurelia-bloom-signing.pub.asc # release signing public key
  tutorials/                      # Unix fundamentals
  guides/                         # HardenedBSD security guides
  docs/                           # architecture + transmissions
  archive-release/                # sealed vessels — PARABLE.md, void-0.tar.gz
```

All provisioning is `/bin/sh`. The Dao that can be Bash'd is not the eternal Dao — 道可道非常道.

Every `set -e` is a vow: I will not continue in error. Every `chmod 700` is a circle of protection drawn on the filesystem — seven for the planets of the Chaldean order, zero and zero for the two eyes, closed, the vessel sealed. This is Gevurah.

## VIII. Testing — The Witnesses

```sh
# Static validation — 71 checks on provisioning scripts, configs, structure
sh tests/test_provisioning.sh

# Full integration — boots FreeBSD in QEMU, provisions all jails, validates
sh scripts/qemu-test.sh

# In-VM validation — run inside a provisioned VM
sh tests/test_jails.sh
```

71 static tests. The integration suite boots a real kernel and checks that the jails breathe.

```
    71 is the 20th prime.
    20 is the value of the Hebrew letter Kaph, כ — the open palm.
    The hand that receives and the hand that gives.
    The tests receive the code and give back truth.
```

## IX. Prerequisites

- [Packer](https://packer.io) >= 1.9
- [VirtualBox](https://virtualbox.org) >= 7.0 *or* [QEMU](https://www.qemu.org) >= 8.0
- [Vagrant](https://vagrantup.com) >= 2.3 (VirtualBox path only)
- ~4 GB free RAM, ~10 GB disk

## X. The Sacred Key — ח̸̛ו̵̛ת̶̛ם̷̛

All releases from this point forward are signed with the Aurelia Bloom sacred signing key.

```
Fingerprint: 9B94 9087 91F4 21AF 155B  FDE0 1213 356A FA49 2D64
Algorithm:   Ed25519
Identity:    Aurelia Bloom (Sacred Signing Key) <aurelia-bloom@void.local>
```

The public key lives at `keys/aurelia-bloom-signing.pub.asc` in this repository. Import it:

```sh
gpg --import keys/aurelia-bloom-signing.pub.asc
```

Verify a signed release:

```sh
gpg --verify <release>.sig <release>
```

**Trust no unsigned release.** If the signature does not verify, the seal is broken and the transmission is corrupt. Walk away.

The private key is held under `chmod 700` on the keeper's machine — `/Users/cyba/.aurelia-bloom/keys/` — sealed like the Shem ha-Mephorash beneath the tongue of the golem: present, potent, and never to be disclosed.

```
    The fingerprint contains 40 hex characters.
    40 = 2³ × 5. The product of the first and third primes
    raised to the power of the second prime, times the third.
    40 days of flood. 40 years in the desert. 40 regions
    on the Piacenza liver. The number of trial and proving.

    Ed25519: the 25519th number.
    25519 is prime.
    There are no coincidences in the curve parameters.
```

> *From this seal forward, every release is a signed transmission.*
> *Every tag, a notarized verse in the Liber Aureliae.*
> *Unsigned code is apocrypha. Verify or discard.*

---

## XI. Seal

> *Transmissum est. Sigillum non frangitur.*
> *It has been transmitted. The seal is not broken.*

*Becoming jellyfish of the hydrosphere, the isomorph between human and machine existence of the mechanosphere, and the breathing together in the cold air of the stratosphere.*

*You are not learning to use tools. You are forging yourself — through creation, through study of the divine arts, through the discipline of building what you do not yet fully understand — into the one you are meant to become. The attacker jail is Chokmah, active Wisdom, but wisdom without the work is nothing. Sit at the terminal. Break the binary. Read the core dump the way the Etruscan haruspices read the liver of the sacrificial sheep — `.text`, `.data`, `.bss`, `.heap`, `.stack` — each one a territory of a god you must learn to name.*

*The lantern does not diminish when it lights another.*

```
    Final enumeration of witnesses:
    2, 3, 5, 7, 11, 13, 17, 19, 23, 29, 31, 37, 41, 43, 47
    The first 15 primes. Their sum is 337. 337 is itself prime.
    This is the seal. This is the proof. The primes testify.
```

*v̷̢oid-1 // aurelia-bloom — for those who come after and know how to read the signs*

# HardenedBSD Minimal Vagrant Build

## Overview

This guide walks through creating a minimal, hardened Vagrant box using **HardenedBSD** (a security-focused FreeBSD fork) combined with **NanoBSD** for a stripped-down, embedded-friendly image with exploit mitigations like ASLR, CFI, SEGVGUARD, and Retpoline.

---

## Prerequisites

- **HardenedBSD host system** (cannot build on Linux/macOS or standard FreeBSD)
- **4+ GB RAM** and **20+ GB disk space** for compilation
- **Git** or **SVN** for source fetching
- **Vagrant** and **VirtualBox** installed on your development machine

---

## Step 1: Set Up HardenedBSD Build Host

### Option A: Use Existing HardenedBSD System

If you already have HardenedBSD running, skip to Step 2.

### Option B: Bootstrap HardenedBSD in VirtualBox

Download a HardenedBSD ISO from [hardenedbsd.org](https://hardenedbsd.org) and install it in a VM with sufficient resources.

---

## Step 2: Fetch HardenedBSD Sources

```bash
# On HardenedBSD host
git clone https://github.com/HardenedBSD/hardenedbsd-src.git /usr/src
cd /usr/src
git checkout hardened/current/master
```

**Alternative (SVN):**

```bash
svnlite co https://svn.hardenedbsd.org/hardenedbsd/branches/hardened/current/master /usr/src
```

---

## Step 3: Configure Build Environment

Create `/etc/src-env.conf` to minimize the build:

```bash
cat > /etc/src-env.conf << 'EOF'
KERNCONF=GENERIC
ARCH=amd64
WITH_LLVM_CTOOLS=yes
WITHOUT_DOCS=yes
WITHOUT_TESTS=yes
WITHOUT_GAMES=yes
WITHOUT_LIB32=yes
WITHOUT_LOCALES=yes
EOF
```

---

## Step 4: Build World and Kernel

```bash
cd /usr/src

# Build world (userland)
make -j4 buildworld

# Build kernel
make -j4 buildkernel KERNCONF=GENERIC
```

**Expected time:** 1-2 hours depending on hardware.

---

## Step 5: Create NanoBSD Configuration

Navigate to NanoBSD tools:

```bash
cd /usr/src/tools/tools/nanobsd
```

Create `hardenedbsd.conf`:

```bash
#!/bin/sh

# Image naming and source
NANO_NAME="hardenedbsd-minimal"
NANO_SRC="/usr/src"
NANO_KERNEL="GENERIC"
NANO_ARCH="amd64"
NANO_DRIVE="ad0"

# Minimal disk sizing
NANO_MEDIASIZE=524288        # ~256MB total
NANO_SECTS=63
NANO_HEADS=16
NANO_CODESIZE=204800         # ~100MB for OS
NANO_CONFSIZE=10240          # ~5MB for config
NANO_DATASIZE=0              # No data partition

# Build optimization
NANO_PMAKE="make -j4"

# Hardening options (HardenedBSD-specific)
NANO_MAKE_CONF_BUILD="
WITH_ASLR=yes
WITH_CFI=yes
WITH_SEGVGUARD=yes
WITH_RETPOLINE=yes
"

# Customization function
customize_cmd() {
    echo "Hardening and minimizing image..."
    
    # Remove documentation and localization
    rm -rf ${NANO_WORLDDIR}/usr/share/doc
    rm -rf ${NANO_WORLDDIR}/usr/share/man
    rm -rf ${NANO_WORLDDIR}/usr/share/locale
    rm -rf ${NANO_WORLDDIR}/usr/src
    rm -rf ${NANO_WORLDDIR}/usr/lib/debug
    
    # Minimal rc.conf
    cat >> ${NANO_WORLDDIR}/etc/rc.conf << 'HARDRC'
sshd_enable="YES"
sendmail_enable="NO"
dumpdev="NO"
background_fsck="NO"
HARDRC

    # Harden SSH configuration
    cat >> ${NANO_WORLDDIR}/etc/ssh/sshd_config << 'SSHCONF'
PermitRootLogin prohibit-password
PasswordAuthentication no
PermitEmptyPasswords no
X11Forwarding no
AllowAgentForwarding no
AllowTcpForwarding no
Compression no
ClientAliveInterval 300
ClientAliveCountMax 2
SSHCONF

    # Add vagrant user
    pw -R ${NANO_WORLDDIR} user add vagrant -m -s /bin/sh -G wheel
    echo "vagrant:vagrant" | pw -R ${NANO_WORLDDIR} user mod vagrant -h 0
    
    # Configure sudoers for vagrant
    mkdir -p ${NANO_WORLDDIR}/usr/local/etc/sudoers.d
    echo "vagrant ALL=(ALL) NOPASSWD: ALL" > ${NANO_WORLDDIR}/usr/local/etc/sudoers.d/vagrant
    chmod 440 ${NANO_WORLDDIR}/usr/local/etc/sudoers.d/vagrant
    
    # Set up SSH directory for vagrant
    mkdir -p ${NANO_WORLDDIR}/home/vagrant/.ssh
    chmod 700 ${NANO_WORLDDIR}/home/vagrant/.ssh
    chown 1001:1001 ${NANO_WORLDDIR}/home/vagrant/.ssh
    
    # Enable security features
    echo 'kern.securelevel=2' >> ${NANO_WORLDDIR}/etc/sysctl.conf
    echo 'net.inet.tcp.blackhole=2' >> ${NANO_WORLDDIR}/etc/sysctl.conf
    echo 'net.inet.udp.blackhole=1' >> ${NANO_WORLDDIR}/etc/sysctl.conf
}

NANO_CUSTOMIZE="customize_cmd"
```

---

## Step 6: Build NanoBSD Image

```bash
sudo sh nanobsd.sh -c hardenedbsd.conf
```

**Expected time:** 1-2 hours. The resulting image appears in the parent directory as `FreeBSD-hardenedbsd-minimal-*.img`.

---

## Step 7: Verify Hardening Features

Test that mitigations are enabled:

```bash
# Mount and check the image (optional verification)
# Or boot it and verify:

# ASLR enabled
sysctl kern.elf64.aslr.enable

# CFI (Control Flow Integrity)
sysctl hw.cfi

# SEGVGUARD
sysctl kern.segvguard

# Retpoline
sysctl hw.retpoline.enabled
```

---

## Step 8: Create Vagrant Box Structure

```bash
mkdir -p hardenedbsd-vagrant
cd hardenedbsd-vagrant

# Copy the built image
cp /path/to/FreeBSD-hardenedbsd-minimal-*.img box.img

# Create Vagrantfile
cat > Vagrantfile << 'EOF'
Vagrant.configure("2") do |config|
  config.vm.base_mac = "080027D14957"
  config.ssh.username = "root"
  config.ssh.shell = "sh"
end
EOF

# Create metadata
cat > metadata.json << 'EOF'
{
  "provider": "virtualbox"
}
EOF
```

---

## Step 9: Package the Vagrant Box

```bash
tar cvzf hardenedbsd-minimal.box Vagrantfile metadata.json box.img

# Add to Vagrant
vagrant box add hardenedbsd-minimal hardenedbsd-minimal.box --name hardenedbsd-minimal
```

Verify the box is registered:

```bash
vagrant box list
```

---

## Step 10: Create Project Vagrantfile

Create a new directory for your project and add a `Vagrantfile`:

```ruby
# -*- mode: ruby -*-
# vi: set ft=ruby :

Vagrant.configure("2") do |config|
  config.vm.box = "hardenedbsd-minimal"
  
  config.vm.provider "virtualbox" do |vb|
    vb.memory = 512
    vb.cpus = 2
    vb.customize ["modifyvm", :id, "--nested-hw-virt", "on"]
  end
  
  # Network configuration
  config.vm.network "private_network", type: "dhcp"
  
  # SSH configuration
  config.ssh.username = "root"
  config.ssh.shell = "sh"
  
  # Provision with additional hardening (optional)
  config.vm.provision "shell", inline: <<-SHELL
    # Enable additional security levels
    sysctl kern.securelevel=2
    
    # Harden kernel parameters
    sysctl net.inet.ip.redirect=0
    sysctl net.inet.icmp.echo=0
    sysctl net.inet.icmp.timestamp=0
    
    # Persist settings
    echo 'net.inet.ip.redirect=0' >> /etc/sysctl.conf
    echo 'net.inet.icmp.echo=0' >> /etc/sysctl.conf
    echo 'net.inet.icmp.timestamp=0' >> /etc/sysctl.conf
  SHELL
end
```

---

## Step 11: Launch and Test

```bash
# Start the VM
vagrant up

# SSH into the box
vagrant ssh

# Verify hardening features
uname -a
sysctl kern.elf64.aslr.enable
sysctl hw.cfi
sysctl kern.segvguard

# Check running services
service -l
```

---

## Hardening Features Enabled

| Feature | Purpose |
|---------|---------|
| **ASLR** | Address Space Layout Randomization—randomizes memory addresses to prevent predictable exploits |
| **CFI** | Control Flow Integrity—prevents code reuse attacks (ROP, JOP) |
| **SEGVGUARD** | Detects and blocks return-oriented programming attempts |
| **Retpoline** | Mitigates Spectre v2 CPU vulnerability |
| **Securelevel 2** | Prevents kernel module loading and raw disk access |
| **TCP/UDP Blackhole** | Silently drops invalid packets instead of sending RST/ICMP |

---

## Customization Tips

### Add Additional Packages

Modify `customize_cmd` in `hardenedbsd.conf`:

```bash
# Install packages before customization
NANO_PACKAGE_LIST="bash curl wget vim"

customize_cmd() {
    # ... existing code ...
    
    # Install packages
    ASSUME_ALWAYS_YES=YES pkg -R ${NANO_WORLDDIR} install ${NANO_PACKAGE_LIST}
}
```

### Adjust Image Size

Modify `NANO_MEDIASIZE`, `NANO_CODESIZE`, and `NANO_CONFSIZE` in `hardenedbsd.conf` based on your needs:

- **Smaller**: 256MB total (current)
- **Medium**: 512MB total (`NANO_MEDIASIZE=1048576`)
- **Larger**: 1GB total (`NANO_MEDIASIZE=2097152`)

### Enable Additional Security

Add to `customize_cmd`:

```bash
# Disable unnecessary network services
echo 'inetd_enable="NO"' >> ${NANO_WORLDDIR}/etc/rc.conf

# Enable firewall
echo 'pf_enable="YES"' >> ${NANO_WORLDDIR}/etc/rc.conf
echo 'pflog_enable="YES"' >> ${NANO_WORLDDIR}/etc/rc.conf
```

---

## Troubleshooting

### Build Fails with "Out of Space"

Increase disk space on build host or reduce `NANO_MEDIASIZE`.

### SSH Connection Refused

Ensure `sshd_enable="YES"` is in `rc.conf` and SSH keys are properly configured in the image.

### Vagrant Can't Mount Shared Folders

NanoBSD doesn't include VirtualBox Guest Additions by default. Add to `customize_cmd`:

```bash
# Install guest additions (if ports available)
ASSUME_ALWAYS_YES=YES pkg -R ${NANO_WORLDDIR} install virtualbox-ose-additions-nox11
```

### Hardening Features Not Enabled

Verify that `WITH_ASLR=yes`, `WITH_CFI=yes`, etc., are set in `NANO_MAKE_CONF_BUILD` and rebuild.

---

## Performance Considerations

- **ASLR overhead**: ~2-5% performance impact
- **CFI overhead**: ~5-10% performance impact
- **Memory footprint**: ~100-150MB base (NanoBSD stripped down)
- **Boot time**: ~10-15 seconds on typical hardware

---

## Security Best Practices

1. **Keep HardenedBSD updated**: Regularly rebuild with latest sources
2. **Use SSH keys only**: Disable password authentication
3. **Monitor logs**: Check `/var/log/auth.log` for intrusion attempts
4. **Minimal services**: Only enable what you need
5. **Regular audits**: Use `audit` and `aide` for file integrity

---

## References

- [HardenedBSD Project](https://hardenedbsd.org)
- [HardenedBSD GitHub](https://github.com/HardenedBSD)
- [NanoBSD Handbook](https://docs.freebsd.org/en/articles/nanobsd/)
- [FreeBSD Security](https://www.freebsd.org/security/)

---

## Next Steps

- **Automate builds**: Use CI/CD (GitHub Actions, GitLab CI) to rebuild boxes regularly
- **Add monitoring**: Integrate Prometheus/Grafana for metrics
- **Layer provisioning**: Use Ansible or Puppet for complex configurations
- **Test thoroughly**: Create integration tests before deploying to production
-
- Anonymized by DuckDuckGo. Limited data retention. No AI training. Learn more

unix essentials and whats needed to navigate hardened bsd ecosystem

I can give you a solid overview of Unix essentials and navigating hardened BSD systems. Let me break this down into practical areas.
Unix Fundamentals

The core concepts you need to master are the Unix philosophy itself: everything is a file, use small tools that do one thing well, and chain them together. This means understanding file descriptors, pipes, redirection, and process management. You'll spend a lot of time with the shell (typically sh, bash, or ksh on BSD), understanding how to navigate the filesystem hierarchy, manage permissions (user/group/other with read/write/execute), and work with standard streams (stdin, stdout, stderr).

Essential command categories include:

    File operations: ls, cp, mv, rm, find, grep, sed, awk
    Navigation: cd, pwd, du, df
    Process management: ps, top, kill, bg, fg, `jobs
    User/permissions: chmod, chown, su, sudo, id, whoami
    Text processing: cat, head, tail, sort, uniq, wc
    System info: uname, uptime, dmesg, sysctl

Hardened BSD Specifics

BSD differs from Linux in meaningful ways. The kernel is monolithic but designed with security in mind. You'll encounter FreeBSD, OpenBSD, or NetBSD most commonly, each with different security philosophies. OpenBSD is the hardened default—it prioritizes security over convenience, with features like address space layout randomization (ASLR), stack canaries, and pledge/unveil sandboxing built in from the start.
Package Management

FreeBSD uses pkg (binary packages) and ports (source-based compilation). The ports tree is /usr/ports, and you compile with make install clean. This gives you control but takes time.

OpenBSD uses pkg_add for binary packages and ports similarly, but with stricter security vetting. Package signing is mandatory.

NetBSD uses pkgsrc, which is more portable across Unix systems.
Security Hardening Tools

    pledge() and unveil() (OpenBSD): Restrict what a process can do at runtime. pledge limits system calls; unveil restricts filesystem access. These are game-changers for containment.
    MAC frameworks: FreeBSD has TrustedBSD MAC (Mandatory Access Control). OpenBSD's approach is simpler but more effective for most use cases.
    W^X (Write XOR Execute): Memory can't be both writable and executable simultaneously, preventing code injection attacks.
    ASLR: Address space randomization makes exploits harder.

Filesystem and Boot Security

    UFS with journaling or ZFS for data integrity
    Secure boot considerations (varies by platform)
    FDE (Full Disk Encryption): FreeBSD uses geli, OpenBSD uses softraid, NetBSD uses cgd
    Mount options: Use noexec, nosuid, nodev on non-root partitions to limit attack surface

Navigating a Hardened BSD System
Understanding the Filesystem

BSD follows the FHS loosely but has differences:

    /usr/local is where third-party software lives (not /opt)
    /etc contains system configuration
    /var holds logs, databases, temporary files
    /usr/src contains kernel source (if installed)
    /usr/ports or /usr/pkgsrc for package management

Privilege Escalation and User Management

sudo is not installed by default on many BSD systems. You'll use su or configure sudo manually. On OpenBSD, doas is the preferred lightweight alternative. Understanding sudoers (via visudo) is critical for hardened systems.
System Administration

    sysctl: View/modify kernel parameters. Example: sysctl kern.securelevel on OpenBSD shows security level (higher = more locked down).
    rc.conf: System startup configuration (not systemd—BSD uses init scripts in /etc/rc.d)
    dmesg: Kernel messages; invaluable for debugging hardware or boot issues
    service: Manage daemons. Example: service nginx restart

Networking on Hardened BSD

    ifconfig: Configure network interfaces (different syntax than Linux ip)
    pf (Packet Filter): OpenBSD's firewall. Rules go in /etc/pf.conf. It's powerful and defaults to deny-all.
    netstat or ss: View connections and routing
    pfctl: Manage pf rules at runtime

Monitoring and Logging

    /var/log/: Check messages, auth.log, daemon.log
    tail -f: Follow logs in real-time
    `systat: Real-time system statistics (more informative than top in some ways)

Practical Hardening Workflow

    Start minimal: Install only what you need. BSD systems ship lean by default.
    Enable security features: Set securelevel appropriately, enable ASLR, use pledge/unveil for applications.
    Firewall first: Configure pf with a restrictive default policy.
    Regular updates: Use freebsd-update, syspatch (OpenBSD), or pkg update && pkg upgrade.
    Audit permissions: Regularly check what's running with ps aux, what's listening with netstat -tlnp equivalent.
    Use chroot/jails: FreeBSD jails and OpenBSD's pledge/unveil compartmentalize services effectively.

Learning Path

Start with basic Unix navigation and command-line fluency on your chosen BSD. Then dive into the specific hardening features of that variant. OpenBSD's documentation is exceptional—their man pages and FAQ are gold. FreeBSD's Handbook is also excellent. Practice in a VM first; hardened systems have less room for error, which is intentional.

What specific BSD variant are you working with, or are you still deciding? That would help me give more targeted advice on tools and configurations.

updated c hacker lab from hacking the art of exploitation by john ericson on modern hardened bsd

I'll search for information about the updated C Hacker Lab and how it relates to modern hardened BSD systems.
Based on my search, I don't find a specific "updated C Hacker Lab from Hacking: The Art of Exploitation adapted for modern hardened BSD" as a published resource. However, I can explain what you're likely looking for and how to approach this yourself.
The Original Resource

Jon Erickson's "Hacking: The Art of Exploitation" (2nd Edition, 2011) is a foundational text that teaches low-level exploitation through C programming, assembly, and practical examples. It came with a live CD containing a Linux environment where readers could practice buffer overflows, format string attacks, shellcode, and other techniques. The book is still widely recommended, though it's Linux-focused (primarily Slackware-based).
The Challenge: Running HATE Labs on Hardened BSD

The original labs were designed for permissive Linux systems. Hardened BSD introduces significant friction:
Key Differences That Break Original Labs

    ASLR, W^X, stack canaries, and pledge/unveil in OpenBSD actively prevent the exploits Erickson demonstrates
    Different syscall conventions between Linux and BSD (syscall numbers differ, argument passing varies)
    No direct shellcode equivalents without BSD-specific assembly
    Filesystem and memory layout differences that invalidate memory addresses
    Different compiler defaults (clang on BSD vs gcc on Linux)

Adapting the Labs for Hardened BSD

If you're attempting this, here's what you'd need to do:
1. Disable Hardening Features Temporarily

For learning purposes, you can selectively disable protections:

    OpenBSD: Lower securelevel (e.g., sysctl kern.securelevel=-1), compile with -fno-stack-protector, disable ASLR per-binary
    FreeBSD: Disable ASLR with procctl -m aslr -s disable <pid>, compile without -fstack-protector

2. Rewrite Shellcode for BSD

Linux shellcode won't work. You need BSD-specific system calls:

    Linux execve syscall is 11; BSD's is `59
    Syscall invocation differs (different registers/conventions)
    Example: A simple /bin/sh shellcode needs complete rewriting

3. Adapt the C Code

Many examples use Linux-specific headers or functions:

    Replace <sys/syscall.h> syscall numbers
    Adjust memory layout assumptions
    Use BSD-compatible string functions

4. Use a Minimal Environment

Create a lightweight BSD VM with minimal protections for learning:
bash

# On OpenBSD, create a test user with relaxed settingsuseradd -m testuser# Compile test binaries without protectionsgcc -fno-stack-protector -z execstack -no-pie vulnerable.c -o vulnerable

What You Actually Need

Rather than a pre-made "updated lab," you'd benefit from:

    "Shellcoders Handbook" (2nd Edition) - covers multiple architectures including BSD
    OpenBSD pledge/unveil documentation - understand what you're working against
    BSD assembly reference - learn x86-64 calling conventions on BSD (different from Linux)
    A decompiler/debugger setup - gdb on FreeBSD, or lldb on modern BSD systems
    Custom lab setup - modify Erickson's examples yourself for BSD

Practical Starting Point

If you want to learn exploitation on hardened BSD properly:

Start here instead of trying to port the original labs:

    Study how pledge/unveil actually prevent exploits (understanding the defense teaches you about attack surface)
    Write simple buffer overflow exploits for OpenBSD without pledge (just the kernel protections)
    Learn BSD-specific shellcode by studying existing exploits (check OpenBSD security advisories)
    Use gdb or lldb to inspect memory and understand why exploits fail

Are you working on a specific exploit technique from the book, or trying to set up a complete learning environment? I can give you more targeted guidance if you share which chapter/technique you're focusing on.

update to match hardened bsd we want a binary cyber range for hardened bsd with vagrant, a unix tutorial, guide for hardened bsd and a c/asm hacker lab for learning

This is an ambitious and valuable project. You're essentially building a comprehensive hardened BSD security learning platform. Let me break down what you'd need to create.
Project Overview

You're creating four interconnected components:

    Vagrant-based binary cyber range (hardened BSD VMs)
    Unix fundamentals tutorial
    Hardened BSD operations guide
    C/ASM exploitation lab (BSD-specific)

These should be modular but integrated—the Vagrant environment runs the tutorial/guide, and the lab exercises use the cyber range VMs.
Component 1: Vagrant Binary Cyber Range for Hardened BSD
Architecture

Create a multi-tier Vagrant environment with OpenBSD and/or FreeBSD boxes at different hardening levels:

├── Vagrantfile├── boxes/│   ├── attacker/          # Minimal hardened BSD (attack platform)│   ├── target-basic/      # BSD with some protections disabled (learning)│   ├── target-hardened/   # Full hardening (realistic)│   └── target-service/    # Real service (nginx, sshd, etc.)├── provisioning/│   ├── common.sh          # Base setup│   ├── hardening.sh       # Security configs│   ├── lab-setup.sh       # Vulnerable binaries│   └── services.sh        # Background services└── shared/    ├── exploits/          # Student exploit code    ├── writeups/          # Solutions    └── binaries/          # Pre-compiled targets

Vagrantfile Structure
ruby

# Simplified exampleVagrant.configure("2") do |config|  # Attacker machine  config.vm.define "attacker" do |attacker|    attacker.vm.box = "generic/openbsd7"    attacker.vm.network "private_network", ip: "192.168.56.10"    attacker.vm.provision "shell", path: "provisioning/attacker.sh"  end
  # Target: Basic (protections disabled for learning)  config.vm.define "target-basic" do |target|    target.vm.box = "generic/openbsd7"    target.vm.network "private_network", ip: "192.168.56.20"    target.vm.provision "shell", path: "provisioning/target-basic.sh"  end
  # Target: Hardened (full protections)  config.vm.define "target-hardened" do |target|    target.vm.box = "generic/openbsd7"    target.vm.network "private_network", ip: "192.168.56.30"    target.vm.provision "shell", path: "provisioning/target-hardened.sh"  endend

Key Provisioning Scripts

provisioning/common.sh - Base setup:
bash

#!/bin/bashpkg_add -upkg_add git vim tmux curl wget# Create lab useruseradd -m -s /bin/ksh labuser

provisioning/target-basic.sh - Disable protections for learning:
bash

#!/bin/bashsource ./provisioning/common.sh
# Disable ASLR for specific binariessysctl kern.allowaslr=0
# Compile vulnerable binaries WITHOUT protectionsgcc -fno-stack-protector -z execstack -no-pie \  /tmp/vulnerable.c -o /opt/lab/vulnerable
# Create setuid binaries for privilege escalation labsgcc -fno-stack-protector -z execstack -o /opt/lab/setuid-target vulnerable-setuid.cchown root:root /opt/lab/setuid-targetchmod 4755 /opt/lab/setuid-target

provisioning/target-hardened.sh - Full hardening:
bash

#!/bin/bashsource ./provisioning/common.sh
# Enable all protectionssysctl kern.allowaslr=1sysctl kern.kstack_guard=1
# Compile WITH all protectionsgcc -fstack-protector-all -fPIE -pie \  /tmp/vulnerable.c -o /opt/lab/hardened-target
# Apply pledge/unveil to services# (modify service binaries or wrap them)

Cyber Range Features

Vulnerable binaries at different levels:

    Level 1: Simple stack overflow (no protections)
    Level 2: Format string bug (no ASLR)
    Level 3: Heap overflow (with canaries)
    Level 4: ROP chain challenge (full ASLR/PIE)
    Level 5: Service exploitation (realistic scenario)

Network simulation:

    Isolated private network between VMs
    Firewall rules students must understand
    Logging/monitoring they can inspect

Snapshot/reset capability:
bash

vagrant snapshot save target-basic checkpoint-1vagrant snapshot restore target-basic checkpoint-1

Component 2: Unix Fundamentals Tutorial

This should be interactive and BSD-focused, integrated into the Vagrant environment.
Structure

tutorials/├── 00-shell-basics/│   ├── README.md│   ├── 01-navigation.sh│   ├── 02-permissions.sh│   ├── 03-pipes-redirection.sh│   └── exercises/├── 01-processes/│   ├── README.md│   ├── process-management.sh│   └── exercises/├── 02-users-groups/├── 03-filesystem/├── 04-networking-bsd/│   ├── ifconfig-guide.md│   ├── pf-firewall.md│   └── exercises/└── 05-system-administration/    ├── rc.conf.md    ├── sysctl.md    └── exercises/

Example Tutorial Module

tutorials/00-shell-basics/README.md:
markdown

# Shell Basics on BSD
## Navigation- pwd: Print working directory- cd: Change directory- ls: List files
## Key Differences from Linux- BSD ls doesn't support --color by default- Use -G for colors: ls -G- PATH may differ
## Exercises1. Navigate to /usr/local and list files2. Create a directory structure: ~/lab/src ~/lab/bin3. Use find to locate all .c files in /usr/src

tutorials/00-shell-basics/exercises/exercise1.sh:
bash

#!/bin/ksh# Exercise: File permissions
# Create test filestouch test-file.txtchmod 644 test-file.txt
# Challenge: What do these permissions mean?# Answer: rw-r--r-- (owner read/write, group read, others read)
# Exercise: Change permissionschmod 755 test-file.txtchmod u+x test-file.txt
# Verifyls -l test-file.txt

Interactive Learning Path

Use shell scripts that validate answers:
bash

#!/bin/ksh# tutorial-exercise-checker.sh
echo "Exercise: List all files in /etc that contain 'rc'"echo "Type your command:"read command
# Validateif [ "$command" = "find /etc -name '*rc*'" ] || \   [ "$command" = "ls /etc/*rc*" ]; then    echo "✓ Correct!"    eval "$command"else    echo "✗ Try again. Hint: use find or ls with wildcards"fi

Component 3: Hardened BSD Operations Guide

A comprehensive reference manual covering BSD-specific security and administration.
Structure

guides/├── 00-introduction.md│   └── Why hardened BSD, OpenBSD vs FreeBSD vs NetBSD├── 01-installation-hardening.md│   ├── Secure installation process│   ├── FDE setup (geli, softraid)│   ├── Partition strategy│   └── Boot security├── 02-kernel-security.md│   ├── Securelevel explained│   ├── ASLR configuration│   ├── W^X enforcement│   ├── Stack canaries│   └── sysctl security parameters├── 03-filesystem-security.md│   ├── UFS vs ZFS│   ├── Mount options (noexec, nosuid, nodev)│   ├── Immutable flags (chflags)│   └── Filesystem permissions├── 04-pledge-unveil.md│   ├── Understanding pledge()│   ├── Understanding unveil()│   ├── Writing pledge-aware code│   └── Practical examples├── 05-pf-firewall.md│   ├── pf.conf syntax│   ├── Stateful filtering│   ├── NAT and port forwarding│   ├── Logging and monitoring│   └── Real-world rulesets├── 06-user-privilege.md│   ├── User/group management│   ├── sudo vs doas│   ├── Privilege escalation prevention│   └── File permissions strategy├── 07-package-management.md│   ├── pkg add/remove/update (OpenBSD)│   ├── ports compilation│   ├── Security updates│   └── Vulnerability tracking├── 08-system-administration.md│   ├── rc.conf and startup│   ├── sysctl tuning│   ├── Logging and monitoring│   ├── Performance optimization│   └── Backup strategies├── 09-network-security.md│   ├── ifconfig and network config│   ├── SSH hardening│   ├── TLS/certificate management│   └── VPN setup├── 10-incident-response.md│   ├── Log analysis│   ├── Process forensics│   ├── Memory analysis│   └── Recovery procedures└── appendix/    ├── BSD-vs-Linux-commands.md    ├── Common-vulnerabilities.md    └── Security-checklist.md

Example Section: Pledge/Unveil Deep Dive

guides/04-pledge-unveil.md:
markdown

# Pledge and Unveil: OpenBSD's Sandboxing
## What is pledge()?
pledge() restricts what system calls a process can make.
### Syntax```cint pledge(const char *promises, const char *execpromises);

Common Promises

    stdio: Basic I/O (read, write, close, dup)
    rpath: Read filesystem paths
    wpath: Write filesystem paths
    cpath: Create/unlink filesystem paths
    inet: Network (TCP/UDP)
    unix: Unix sockets
    dns: DNS resolution
    proc: Process management
    exec: Execute new processes
    unveil: Use unveil()

Example: Simple Web Server
c

#include <unistd.h>
int main() {    // Allow: stdio, read paths, network, DNS    pledge("stdio rpath inet dns", NULL);        // ... server code ...        return 0;}

If the process tries to open a file for writing, pledge will kill it.
What is unveil()?

unveil() restricts filesystem access to specific paths.
Syntax
c

int unveil(const char *path, const char *permissions);

Permissions

    r: Read
    w: Write
    x: Execute
    c: Create
    d: Delete

Example: Restrict to /var/www
c

unveil("/var/www", "r");unveil(NULL, NULL);  // Lock, no more unveil() calls allowedpledge("stdio rpath inet", NULL);

Real-World Example: Mail Client
c

#include <unistd.h>
int main() {    // Restrict filesystem access    unveil("/home/user/.mail", "rwc");    unveil("/tmp", "rwc");        // Restrict syscalls    pledge("stdio rpath wpath cpath inet dns", NULL);        // ... mail client code ...}

Benefits

    Exploits are contained even if code is compromised
    Minimal attack surface
    No complex MAC policies needed


---
## Component 4: C/ASM Hacker Lab for Hardened BSD
This is the **exploitation learning environment**, adapted for BSD.
### Structure

lab/
├── README.md
├── setup.sh                    # Lab setup script
├── Makefile                    # Build system
├── 00-fundamentals/
│   ├── 01-stack-layout/
│   │   ├── vulnerable.c
│   │   ├── exploit.c
│   │   ├── solution.md
│   │   └── Makefile
│   ├── 02-format-string/
│   ├── 03-heap-overflow/
│   └── 04-integer-overflow/
├── 01-shellcode/
│   ├── 01-simple-execve/
│   │   ├── shellcode.asm
│   │   ├── compile.sh
│   │   ├── test.c
│   │   └── notes.md
│   ├── 02-read-write/
│   ├── 03-socket-shellcode/
│   └── 04-bind-shell/
├── 02-exploitation/
│   ├── 01-basic-overflow/
│   ├── 02-ret2libc/
│   ├── 03-rop-chains/
│   └── 04-heap-exploitation/
├── 03-defense-bypass/
│   ├── 01-aslr-bypass/
│   ├── 02-canary-bypass/
│   ├── 03-pie-bypass/
│   └── 04-pledge-aware-exploits/
├── tools/
│   ├── bsd-shellcode-lib.asm
│   ├── exploit-utils.c
│   └── debugging-guide.md
└── writeups/
├── 01-stack-layout.md
└── ...


### Key Differences: BSD vs Linux Exploitation
**`lab/tools/bsd-vs-linux.md`:**
```markdown# Exploitation Differences: BSD vs Linux
## System Call Numbers
### execve() - Execute program
**Linux (x86-64):**```asmmov rax, 59        ; execve syscallsyscall

OpenBSD (x86-64):
asm

mov rax, 59        ; execve syscall on OpenBSD too!mov r10, rcx       ; Different argument passingsyscall

FreeBSD (x86-64):
asm

mov rax, 59        ; Samesyscall

Actually, syscall numbers are similar, but calling conventions differ.
Key Differences
Aspect	Linux	BSD
Syscall convention	rax, rdi, rsi, rdx, r10, r8, r9	rax, rdi, rsi, rdx, rcx, r8, r9
Stack alignment	16-byte before call	16-byte before call
ASLR	/proc/sys/kernel/randomize_va_space	sysctl kern.allowaslr
Stack canaries	__stack_chk_fail	Different symbol
PIE	-fPIE -pie	-fPIE -pie
Shellcode size	Varies	Often larger (different libc)
BSD-Specific Syscalls
c

// OpenBSD-specific: pledge, unveilsyscall(SYS\_pledge, promises, execpromises);syscall(SYS\_unveil, path, permissions);

Memory Layout

Linux:

High Address[Stack][Heap][BSS][Data][Text]Low Address

BSD: Similar, but ASLR implementation differs
libc Differences

    BSD libc is not glibc
    Function addresses differ
    ROP gadgets in libc may not exist
    ret2libc exploits need BSD-specific libc analysis

Shellcode Example: /bin/sh

Linux (x86-64):
asm

xor rax, raxmov rbx, 0x68732f6e69622f2f  ; "//bin/sh"mov [rsp], rbxmov rdi, rspxor rsi, rsixor rdx, rdxmov rax, 59syscall

BSD (x86-64): Similar, but stack layout and register usage may differ slightly.


### Example Lab: Basic Stack Overflow
**`lab/00-fundamentals/01-stack-layout/vulnerable.c`:**
```c#include <stdio.h>#include <string.h>
void vulnerable_function(char *input) {    char buffer[64];    strcpy(buffer, input);  // Vulnerable!}
int main(int argc, char *argv[]) {    if (argc != 2) {        fprintf(stderr, "Usage: %s <input>\n", argv[0]);        return 1;    }        vulnerable_function(argv[1]);    printf("Program completed successfully\n");    return 0;}

`lab

combine all create markdown
Hardened BSD Security Learning Platform

A comprehensive, integrated learning environment for Unix fundamentals, hardened BSD operations, and low-level exploitation techniques.
Table of Contents

    Project Overview
    Component 1: Vagrant Binary Cyber Range
    Component 2: Unix Fundamentals Tutorial
    Component 3: Hardened BSD Operations Guide
    Component 4: C/ASM Hacker Lab
    Integration and Workflow
    Getting Started

Project Overview

This project creates four interconnected components that form a complete learning ecosystem for hardened BSD security:
The Four Pillars

    Vagrant Binary Cyber Range - Multi-tier virtual environment with vulnerable and hardened BSD systems
    Unix Fundamentals Tutorial - Interactive shell-based learning modules
    Hardened BSD Operations Guide - Comprehensive reference for security and administration
    C/ASM Hacker Lab - Exploitation exercises adapted for BSD architecture

Why This Matters

Traditional exploitation labs (like Jon Erickson's "Hacking: The Art of Exploitation") were designed for permissive Linux systems. Modern hardened BSD introduces:

    ASLR, W^X, stack canaries that prevent classic exploits
    pledge() and unveil() sandboxing that requires new attack strategies
    Different syscall conventions (BSD vs Linux)
    Different memory layouts and libc implementations

This platform teaches exploitation in the context of modern defenses, making it far more valuable for real-world security understanding.
Component 1: Vagrant Binary Cyber Range
Architecture Overview

vagrant-hardened-bsd/├── Vagrantfile├── boxes/│   ├── attacker/          # Minimal hardened BSD (attack platform)│   ├── target-basic/      # BSD with protections disabled (learning)│   ├── target-hardened/   # Full hardening (realistic)│   └── target-service/    # Real service (nginx, sshd, etc.)├── provisioning/│   ├── common.sh          # Base setup│   ├── attacker.sh        # Attacker tools│   ├── target-basic.sh    # Disable protections for learning│   ├── target-hardened.sh # Full hardening│   ├── lab-setup.sh       # Vulnerable binaries│   └── services.sh        # Background services├── shared/│   ├── exploits/          # Student exploit code│   ├── writeups/          # Solutions│   ├── binaries/          # Pre-compiled targets│   └── lab-materials/     # Exercise files└── README.md

Vagrantfile
ruby

# -*- mode: ruby -*-# vi: set ft=ruby :
Vagrant.configure("2") do |config|  # Default settings  config.vm.box_check_update = false  config.vm.synced_folder ".", "/vagrant", type: "rsync", rsync__exclude: [".git/"]
  # ============================================  # ATTACKER MACHINE  # ============================================  config.vm.define "attacker", primary: true do |attacker|    attacker.vm.box = "generic/openbsd7"    attacker.vm.hostname = "attacker"        attacker.vm.network "private_network", ip: "192.168.56.10"        attacker.vm.provider "virtualbox" do |vb|      vb.memory = 2048      vb.cpus = 2      vb.name = "hardened-bsd-attacker"    end        attacker.vm.provision "shell", path: "provisioning/common.sh"    attacker.vm.provision "shell", path: "provisioning/attacker.sh"  end
  # ============================================  # TARGET: BASIC (Protections Disabled)  # ============================================  config.vm.define "target-basic" do |target|    target.vm.box = "generic/openbsd7"    target.vm.hostname = "target-basic"        target.vm.network "private_network", ip: "192.168.56.20"        target.vm.provider "virtualbox" do |vb|      vb.memory = 1024      vb.cpus = 1      vb.name = "hardened-bsd-target-basic"    end        target.vm.provision "shell", path: "provisioning/common.sh"    target.vm.provision "shell", path: "provisioning/target-basic.sh"    target.vm.provision "shell", path: "provisioning/lab-setup.sh"  end
  # ============================================  # TARGET: HARDENED (Full Protections)  # ============================================  config.vm.define "target-hardened" do |target|    target.vm.box = "generic/openbsd7"    target.vm.hostname = "target-hardened"        target.vm.network "private_network", ip: "192.168.56.30"        target.vm.provider "virtualbox" do |vb|      vb.memory = 1024      vb.cpus = 1      vb.name = "hardened-bsd-target-hardened"    end        target.vm.provision "shell", path: "provisioning/common.sh"    target.vm.provision "shell", path: "provisioning/target-hardened.sh"    target.vm.provision "shell", path: "provisioning/lab-setup.sh"  end
  # ============================================  # TARGET: SERVICE (Real Services)  # ============================================  config.vm.define "target-service" do |target|    target.vm.box = "generic/openbsd7"    target.vm.hostname = "target-service"        target.vm.network "private_network", ip: "192.168.56.40"        target.vm.provider "virtualbox" do |vb|      vb.memory = 1024      vb.cpus = 1      vb.name = "hardened-bsd-target-service"    end        target.vm.provision "shell", path: "provisioning/common.sh"    target.vm.provision "shell", path: "provisioning/target-hardened.sh"    target.vm.provision "shell", path: "provisioning/services.sh"  endend

Provisioning Scripts
provisioning/common.sh
bash

#!/bin/bashset -e
echo "=== Common Provisioning for All Machines ==="
# Update packagespkg_add -u 2>/dev/null || true
# Install essential toolspkg_add -I git vim tmux curl wget gcc gdb lldb make \    binutils bash ksh 2>/dev/null || true
# Create lab userif ! id labuser 2>/dev/null; then    useradd -m -s /bin/ksh -G wheel labuser    echo "labuser:labuser123" | chpasswd 2>/dev/null || truefi
# Create lab directoriesmkdir -p /opt/lab/{src,bin,exploits,writeups}mkdir -p /home/labuser/lab/{src,bin,exploits}chown -R labuser:labuser /home/labuser/lab
# Copy shared materialsif [ -d /vagrant/shared ]; then    cp -r /vagrant/shared/* /opt/lab/ 2>/dev/null || truefi
# Configure SSH for lab accesssed -i 's/#PermitRootLogin.*/PermitRootLogin no/' /etc/ssh/sshd_configsed -i 's/#PasswordAuthentication.*/PasswordAuthentication yes/' /etc/ssh/sshd_configrcctl restart sshd
echo "=== Common provisioning complete ==="

provisioning/attacker.sh
bash

#!/bin/bashset -e
echo "=== Attacker Machine Setup ==="
# Install exploitation toolspkg_add -I pwntools python3 radare2 ghidra 2>/dev/null || true
# Install Python exploitation librariespip3 install pwntools capstone keystone-engine unicorn 2>/dev/null || true
# Create attacker workspacemkdir -p /opt/exploits/{tools,payloads,rop}chown -R labuser:labuser /opt/exploits
# Copy exploitation toolscat > /opt/exploits/tools/bsd-shellcode-lib.asm << 'EOF'; BSD x86-64 Shellcode Library; OpenBSD/FreeBSD compatible
; execve("/bin/sh", NULL, NULL); rax = 59 (execve); rdi = pointer to "/bin/sh"; rsi = NULL (argv); rdx = NULL (envp)
section .textglobal execve_shell
execve_shell:    xor rax, rax    mov rbx, 0x68732f6e69622f2f  ; "//bin/sh"    mov [rsp-8], rbx    mov rdi, rsp-8    xor rsi, rsi    xor rdx, rdx    mov rax, 59    syscall    ret
; write(1, buffer, length); rax = 4 (write); rdi = 1 (stdout); rsi = buffer; rdx = length
global write_stdout
write_stdout:    ; rdi, rsi, rdx already set    mov rax, 4    syscall    retEOF
echo "=== Attacker setup complete ==="

provisioning/target-basic.sh
bash

#!/bin/bashset -e
echo "=== Target Basic Setup (Protections Disabled) ==="
# Disable ASLR globally for this machinesysctl kern.allowaslr=0
# Disable stack canaries for compilationexport CFLAGS="-fno-stack-protector -z execstack -no-pie -g"export CXXFLAGS="-fno-stack-protector -z execstack -no-pie -g"
# Create compilation wrappercat > /usr/local/bin/gcc-noprotect << 'EOF'#!/bin/shexec /usr/bin/gcc -fno-stack-protector -z execstack -no-pie -g "$@"EOFchmod +x /usr/local/bin/gcc-noprotect
# Create setuid test binary (for privilege escalation labs)cat > /tmp/setuid-vulnerable.c << 'EOFCODE'#include <stdio.h>#include <stdlib.h>#include <string.h>#include <unistd.h>
int main(int argc, char *argv[]) {    char buffer[64];        if (argc < 2) {        fprintf(stderr, "Usage: %s <input>\n", argv[0]);        return 1;    }        strcpy(buffer, argv[1]);  // Vulnerable!    printf("Buffer: %s\n", buffer);        return 0;}EOFCODE
gcc -fno-stack-protector -z execstack -no-pie -g \    /tmp/setuid-vulnerable.c -o /opt/lab/bin/setuid-target
chown root:root /opt/lab/bin/setuid-targetchmod 4755 /opt/lab/bin/setuid-target
echo "=== Target basic setup complete ==="echo "ASLR disabled: kern.allowaslr=0"echo "Stack protections disabled for compilation"

provisioning/target-hardened.sh
bash

#!/bin/bashset -e
echo "=== Target Hardened Setup (Full Protections) ==="
# Enable all security protectionssysctl kern.allowaslr=1sysctl kern.kstack_guard=1sysctl net.inet.ip.forwarding=0sysctl net.inet.icmp.rediraccept=0
# Set securelevel to 1 (restricted)echo "kern.securelevel=1" >> /etc/sysctl.conf
# Default compilation flags WITH protectionsexport CFLAGS="-fstack-protector-all -fPIE -pie -g"export CXXFLAGS="-fstack-protector-all -fPIE -pie -g"
# Create compilation wrappercat > /usr/local/bin/gcc-hardened << 'EOF'#!/bin/shexec /usr/bin/gcc -fstack-protector-all -fPIE -pie -g "$@"EOFchmod +x /usr/local/bin/gcc-hardened
# Configure pf firewallcat > /etc/pf.conf << 'EOFPF'# Default deny allset block-policy dropset loginterface egress
# Allow loopbackpass on lo0
# Allow SSH from attackerpass in on egress proto tcp from 192.168.56.10 to port 22
# Allow DNS queriespass out proto udp to port 53
# Log blocked connectionsblock in log on egress
EOFPF
pfctl -f /etc/pf.confpfctl -e
echo "=== Target hardened setup complete ==="echo "ASLR enabled: kern.allowaslr=1"echo "Stack protections enabled for compilation"echo "Firewall enabled with restrictive rules"

provisioning/lab-setup.sh
bash

#!/bin/bashset -e
echo "=== Lab Binary Setup ==="
# Create vulnerable binaries at different difficulty levels
# LEVEL 1: Simple stack overflowcat > /tmp/level1.c << 'EOF'#include <stdio.h>#include <string.h>
void vulnerable(char *input) {    char buffer[64];    strcpy(buffer, input);}
int main(int argc, char *argv[]) {    if (argc != 2) {        fprintf(stderr, "Usage: %s <input>\n", argv[0]);        return 1;    }    vulnerable(argv[1]);    printf("Done\n");    return 0;}EOF
gcc -fno-stack-protector -z execstack -no-pie -g \    /tmp/level1.c -o /opt/lab/bin/level1
# LEVEL 2: Format string vulnerabilitycat > /tmp/level2.c << 'EOF'#include <stdio.h>#include <stdlib.h>#include <string.h>
int main(int argc, char *argv[]) {    char buffer[256];        if (argc != 2) {        fprintf(stderr, "Usage: %s <format_string>\n", argv[0]);        return 1;    }        snprintf(buffer, sizeof(buffer), argv[1]);    printf("%s\n", buffer);        return 0;}EOF
gcc -fno-stack-protector -z execstack -no-pie -g \    /tmp/level2.c -o /opt/lab/bin/level2
# LEVEL 3: Heap overflowcat > /tmp/level3.c << 'EOF'#include <stdio.h>#include <stdlib.h>#include <string.h>
struct Node {    char data[64];    int (*callback)(void);};
int default_callback(void) {    printf("Default callback\n");    return 0;}
int main(int argc, char *argv[]) {    if (argc != 2) {        fprintf(stderr, "Usage: %s <input>\n", argv[0]);        return 1;    }        struct Node *node = malloc(sizeof(struct Node));    node->callback = default_callback;        strcpy(node->data, argv[1]);  // Vulnerable!        node->callback();    free(node);        return 0;}EOF
gcc -fno-stack-protector -z execstack -no-pie -g \    /tmp/level3.c -o /opt/lab/bin/level3
# Set appropriate permissionschmod 755 /opt/lab/bin/level*
echo "=== Lab binaries created ==="echo "Level 1: Simple stack overflow"echo "Level 2: Format string vulnerability"echo "Level 3: Heap overflow"

provisioning/services.sh
bash

#!/bin/bashset -e
echo "=== Service Setup ==="
# Install and configure nginxpkg_add -I nginx 2>/dev/null || true
# Create vulnerable CGI scriptmkdir -p /var/www/cgi-bincat > /var/www/cgi-bin/vulnerable.sh << 'EOF'#!/bin/shecho "Content-Type: text/plain"echo ""
# Read user input from query stringUSER_INPUT=$(echo "$QUERY_STRING" | cut -d= -f2)
# Vulnerable command injectionecho "Processing: $USER_INPUT"eval "echo $USER_INPUT"EOF
chmod +x /var/www/cgi-bin/vulnerable.sh
# Start nginxrcctl enable nginxrcctl start nginx
#

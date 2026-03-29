# HardenedBSD Overview

## What Is HardenedBSD?
HardenedBSD is a security-focused fork of FreeBSD that integrates exploit mitigations directly into the base system. Where FreeBSD offers some mitigations as options, HardenedBSD enables them by default and adds additional protections.

## Key Differences from FreeBSD
| Feature | FreeBSD | HardenedBSD |
|---------|---------|-------------|
| ASLR | Optional | Enabled by default |
| PIE | Optional | Default for all binaries |
| RELRO | Partial | Full RELRO |
| CFI | Not available | Integrated |
| SEGVGUARD | Not available | Enabled |
| Retpoline | Available | Default |
| SafeStack | Not available | Available |

## Security Philosophy
1. **Defense in depth** — multiple overlapping protections
2. **Secure by default** — mitigations are on, not opt-in
3. **Minimal attack surface** — reduce what's exposed
4. **Fail closed** — when in doubt, deny

## Getting HardenedBSD
- ISOs: `https://installers.hardenedbsd.org/pub/15-stable/`
- Source: `https://github.com/HardenedBSD/hardenedbsd-src`
- This cyber range builds a Vagrant box from the ISO via Packer

## Package Management
HardenedBSD uses FreeBSD's `pkg` system:
```sh
pkg update                  # update repo metadata
pkg install vim             # install a package
pkg search radare           # search packages
pkg info                    # list installed packages
pkg audit -F                # check for vulnerabilities
```

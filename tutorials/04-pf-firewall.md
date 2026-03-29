# pf Firewall

## Overview
`pf` (Packet Filter) is BSD's stateful firewall. Configuration lives in `/etc/pf.conf`.

## Basic Commands
```sh
pfctl -e                    # enable pf
pfctl -d                    # disable pf
pfctl -f /etc/pf.conf       # reload rules
pfctl -sr                   # show active rules
pfctl -ss                   # show state table
pfctl -si                   # show statistics
```

## Rule Syntax
```
action [direction] [log] [quick] on interface [proto protocol] \
    from source [port port] to dest [port port] [flags] [state]
```

## Example: Minimal Secure Ruleset
```
# /etc/pf.conf

# Macros
ext_if = "em0"
trusted = "192.168.56.10"      # attacker VM

# Default: block everything
set block-policy drop

# Skip loopback
set skip on lo0

# Block all by default
block all

# Allow loopback
pass on lo0

# Allow SSH from trusted host
pass in on $ext_if proto tcp from $trusted to port 22

# Allow outbound with state tracking
pass out on $ext_if keep state

# Allow ping from trusted
pass in on $ext_if proto icmp from $trusted
```

## Logging
```sh
# Enable logging on a rule:
block in log on $ext_if

# View logs:
tcpdump -n -e -ttt -i pflog0
```

## NAT (if needed)
```
# NAT outbound traffic
nat on $ext_if from 192.168.56.0/24 to any -> ($ext_if)
```

## Tables (dynamic IP lists)
```
table <bruteforce> persist
block in from <bruteforce>

# Add IP to table:
pfctl -t bruteforce -T add 10.0.0.5
```

## Exercise
1. SSH to `target-hardened` and examine `/etc/pf.conf`
2. Try to connect from `attacker` to ports other than 22
3. Modify the rules to allow HTTP (port 80) and verify

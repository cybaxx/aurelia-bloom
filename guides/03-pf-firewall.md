# pf Firewall Guide

## Architecture
`pf` processes packets through rules top-to-bottom. The **last matching rule wins** unless `quick` is used (which stops processing immediately).

## Configuration: `/etc/pf.conf`

### Macros & Tables
```
ext_if = "em0"
int_net = "192.168.56.0/24"
table <allowed> { 192.168.56.10, 192.168.56.20 }
```

### Rule Structure
```
action direction [log] [quick] on interface [af] proto protocol \
    from source port source_port to dest port dest_port [state]
```

### The Cyber Range Firewall (target-hardened)
```
set block-policy drop
set loginterface egress

# Loopback is fine
pass on lo0

# Allow SSH only from attacker
pass in proto tcp from 192.168.56.10 to port 22

# Allow established outbound
pass out keep state

# Block and log everything else
block in log on egress
```

## Stateful Filtering
`keep state` tracks connections so return traffic is automatically allowed:
```
pass out on $ext_if proto tcp keep state
```

## Antispoofing
```
antispoof for $ext_if
```

## Rate Limiting
```
pass in on $ext_if proto tcp to port 22 \
    keep state (max-src-conn 5, max-src-conn-rate 3/10)
```

## Logging & Monitoring
```sh
# Real-time log viewing
tcpdump -n -e -ttt -i pflog0

# Check stats
pfctl -si

# Show states
pfctl -ss

# Show rules with counters
pfctl -vsr
```

## Enabling at Boot
```sh
sysrc pf_enable="YES"
sysrc pflog_enable="YES"
```

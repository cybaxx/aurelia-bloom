# Networking on BSD

## Interface Configuration
```sh
ifconfig                    # show all interfaces
ifconfig em0                # show specific interface
ifconfig em0 inet 192.168.56.10 netmask 255.255.255.0  # set static IP
```

## Routing
```sh
netstat -rn                 # routing table
route add default 10.0.2.2  # add default gateway
```

## Connections
```sh
sockstat -4l                # listening TCP/UDP sockets (BSD netstat -tlnp)
sockstat -c                 # connected sockets
netstat -an                 # all connections
```

## DNS
```sh
cat /etc/resolv.conf        # DNS servers
host example.com            # DNS lookup
dig example.com             # detailed DNS query
```

## SSH
```sh
ssh labuser@target-basic            # connect
ssh -L 8080:localhost:80 target     # local port forward
ssh -D 1080 target                  # SOCKS proxy
scp file labuser@target:/tmp/       # copy file
```

## Useful Network Tools
```sh
nc -lvp 4444                # listen on port (netcat)
nc target-basic 4444        # connect to port
tcpdump -i em0 port 80      # packet capture
curl http://target-service  # HTTP request
```

## Cyber Range Network
```
192.168.56.10   attacker
192.168.56.20   target-basic
192.168.56.30   target-hardened
192.168.56.40   target-service
```
All VMs are on a private VirtualBox host-only network.

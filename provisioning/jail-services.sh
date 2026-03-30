#!/bin/sh
set -e

echo "=== Service Jail Provisioning ==="

SERVICE="/jails/service"

# Install nginx into service jail
# For BusyBox jails we copy the static binary from host
pkg install -y nginx 2>/dev/null || true

# Copy nginx binary and its dependencies into jail
mkdir -p "${SERVICE}/usr/local/sbin"
mkdir -p "${SERVICE}/usr/local/etc/nginx"
mkdir -p "${SERVICE}/usr/local/www/cgi-bin"
mkdir -p "${SERVICE}/var/log/nginx"
mkdir -p "${SERVICE}/var/run"

cp /usr/local/sbin/nginx "${SERVICE}/usr/local/sbin/nginx" 2>/dev/null || true

# Copy nginx's shared library dependencies
for lib in /usr/local/lib/libpcre2*.so* /usr/local/lib/libssl.so* \
           /usr/local/lib/libcrypto.so* /usr/local/lib/libz.so*; do
  if [ -f "${lib}" ]; then
    mkdir -p "${SERVICE}/usr/local/lib"
    cp "${lib}" "${SERVICE}/usr/local/lib/" 2>/dev/null || true
  fi
done

# Vulnerable CGI script (command injection)
cat > "${SERVICE}/usr/local/www/cgi-bin/ping.sh" <<'EOF'
#!/bin/sh
echo "Content-Type: text/plain"
echo ""
QUERY=$(echo "$QUERY_STRING" | sed 's/host=//')
echo "Pinging: $QUERY"
ping -c 1 $QUERY
EOF
chmod +x "${SERVICE}/usr/local/www/cgi-bin/ping.sh"

# Vulnerable lookup script (format string via user-controlled log)
cat > "${SERVICE}/usr/local/www/cgi-bin/lookup.sh" <<'EOF'
#!/bin/sh
echo "Content-Type: text/plain"
echo ""
NAME=$(echo "$QUERY_STRING" | sed 's/name=//')
printf "Looking up: $NAME\n"
id
EOF
chmod +x "${SERVICE}/usr/local/www/cgi-bin/lookup.sh"

# nginx config
cat > "${SERVICE}/usr/local/etc/nginx/nginx.conf" <<'NGINXCONF'
worker_processes 1;
events { worker_connections 64; }
http {
    server {
        listen 80;
        server_name target-service;
        root /usr/local/www;

        location /cgi-bin/ {
            gzip off;
            alias /usr/local/www/cgi-bin/;
            autoindex on;
        }
    }
}
NGINXCONF

# Update service jail rc to start nginx + dropbear
cat > "${SERVICE}/etc/rc" <<'RCEOF'
#!/bin/sh
/usr/local/sbin/dropbear -R -p 22 2>/dev/null &
/usr/local/sbin/nginx 2>/dev/null &
echo "Service jail started: $(hostname)"
RCEOF
chmod +x "${SERVICE}/etc/rc"

cat > "${SERVICE}/etc/rc.shutdown" <<'RCEOF'
#!/bin/sh
kill $(cat /var/run/dropbear.pid 2>/dev/null) 2>/dev/null || true
/usr/local/sbin/nginx -s stop 2>/dev/null || true
RCEOF
chmod +x "${SERVICE}/etc/rc.shutdown"

chown -R 1001:1001 "${SERVICE}/opt/lab"
chown -R 1001:1001 "${SERVICE}/home/labuser"

echo "=== Service jail provisioning complete ==="
echo "  service (192.168.56.40): nginx + vulnerable CGI"
echo "  http://192.168.56.40/cgi-bin/ping.sh?host=127.0.0.1"
echo "  http://192.168.56.40/cgi-bin/lookup.sh?name=test"

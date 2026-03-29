#!/bin/sh
set -e

echo "=== Vulnerable Service Setup ==="

pkg install -y nginx 2>/dev/null || true

# Vulnerable CGI script (command injection)
mkdir -p /usr/local/www/cgi-bin

cat > /usr/local/www/cgi-bin/ping.sh <<'EOF'
#!/bin/sh
echo "Content-Type: text/plain"
echo ""
QUERY=$(echo "$QUERY_STRING" | sed 's/host=//')
echo "Pinging: $QUERY"
ping -c 1 $QUERY
EOF
chmod +x /usr/local/www/cgi-bin/ping.sh

# Vulnerable lookup script (format string via user-controlled log)
cat > /usr/local/www/cgi-bin/lookup.sh <<'EOF'
#!/bin/sh
echo "Content-Type: text/plain"
echo ""
NAME=$(echo "$QUERY_STRING" | sed 's/name=//')
printf "Looking up: $NAME\n"
id
EOF
chmod +x /usr/local/www/cgi-bin/lookup.sh

# nginx config with CGI via fastcgi
cat > /usr/local/etc/nginx/nginx.conf <<'NGINXCONF'
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

sysrc nginx_enable="YES"
service nginx restart 2>/dev/null || service nginx start 2>/dev/null || true

echo "=== Services deployed ==="
echo "  http://target-service/cgi-bin/ping.sh?host=127.0.0.1"
echo "  http://target-service/cgi-bin/lookup.sh?name=test"

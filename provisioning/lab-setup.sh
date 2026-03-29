#!/bin/sh
set -e

echo "=== Lab Binary Setup ==="

if [ -d /vagrant/lab ] && [ -f /vagrant/lab/Makefile ]; then
  cd /vagrant/lab

  # Detect which target this is based on hostname
  case "$(hostname)" in
    target-basic)
      gmake basic DESTDIR=/opt/lab/bin
      ;;
    target-hardened)
      gmake hardened DESTDIR=/opt/lab/bin
      ;;
    *)
      gmake basic DESTDIR=/opt/lab/bin
      ;;
  esac

  chown -R labuser:labuser /opt/lab/bin
fi

echo "=== Lab binaries installed to /opt/lab/bin ==="

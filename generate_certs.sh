#!/bin/bash
set -e

# Generate a self-signed certificate for development
# Key: server.key
# Cert: server.crt
# Validity: 365 days

openssl req -new -newkey rsa:2048 -days 365 -nodes -x509 \
    -subj "/C=US/ST=Dev/L=Dev/O=Dev/CN=localhost" \
    -keyout server.key -out server.crt

echo "✅ Generated server.key and server.crt"

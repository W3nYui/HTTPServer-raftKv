#!/usr/bin/env bash

set -euo pipefail

readonly ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
readonly CERT_DIR="${CERT_DIR:-$ROOT_DIR/runtime/certs}"
readonly CA_CERT="$CERT_DIR/local-ca.crt"
readonly CA_KEY="$CERT_DIR/local-ca.key"
readonly SERVER_CERT="$CERT_DIR/server.crt"
readonly SERVER_KEY="$CERT_DIR/server.key"
readonly SERVER_CSR="$CERT_DIR/server.csr"

if [[ -s "$CA_CERT" && -s "$CA_KEY" && -s "$SERVER_CERT" && -s "$SERVER_KEY" ]]; then
  printf 'Local TLS certificate already exists: %s\n' "$SERVER_CERT"
  exit 0
fi

if [[ -e "$CERT_DIR" ]] && find "$CERT_DIR" -mindepth 1 -maxdepth 1 -print -quit | grep -q .; then
  echo "Certificate directory is incomplete: $CERT_DIR. Remove it before generating a new certificate." >&2
  exit 1
fi

umask 077
mkdir -p "$CERT_DIR"

openssl genrsa -out "$CA_KEY" 4096
openssl req -x509 -new -sha256 -days 365 -key "$CA_KEY" -out "$CA_CERT" \
  -subj '/CN=Gomoku Local Development CA' \
  -addext 'basicConstraints=critical,CA:TRUE' \
  -addext 'keyUsage=critical,keyCertSign,cRLSign'

openssl genrsa -out "$SERVER_KEY" 2048
openssl req -new -key "$SERVER_KEY" -out "$SERVER_CSR" -subj '/CN=localhost'
openssl x509 -req -sha256 -days 365 -in "$SERVER_CSR" -CA "$CA_CERT" -CAkey "$CA_KEY" \
  -CAcreateserial -out "$SERVER_CERT" -extfile "$ROOT_DIR/scripts/local-tls-extensions.cnf" \
  -extensions server_certificate
openssl verify -CAfile "$CA_CERT" "$SERVER_CERT"

printf 'Generated local CA: %s\nGenerated server certificate: %s\n' "$CA_CERT" "$SERVER_CERT"

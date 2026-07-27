#!/usr/bin/env bash

set -euo pipefail

readonly ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
readonly RUNTIME_DIR="${RUNTIME_DIR:-$ROOT_DIR/runtime}"
readonly DATABASE_ENV_FILE="${DATABASE_ENV_FILE:-$RUNTIME_DIR/gomoku-db.env}"
readonly DATABASE_NAME="Gomoku"
readonly DATABASE_USER="gomoku"

if ! command -v mariadb >/dev/null 2>&1; then
  echo "MariaDB client is not installed. Install the supported MariaDB package first." >&2
  exit 1
fi

mkdir -p "$RUNTIME_DIR"
umask 077

if [[ -r "$DATABASE_ENV_FILE" ]]; then
  # The file is generated locally by this script and is intentionally not tracked.
  # shellcheck disable=SC1090
  source "$DATABASE_ENV_FILE"
else
  GOMOKU_DB_HOST="tcp://127.0.0.1:3306"
  GOMOKU_DB_USER="$DATABASE_USER"
  GOMOKU_DB_PASSWORD="$(openssl rand -base64 32)"
  GOMOKU_DB_NAME="$DATABASE_NAME"

  temp_env_file="$(mktemp "$RUNTIME_DIR/gomoku-db.env.XXXXXX")"
  {
    printf 'GOMOKU_DB_HOST=%q\n' "$GOMOKU_DB_HOST"
    printf 'GOMOKU_DB_USER=%q\n' "$GOMOKU_DB_USER"
    printf 'GOMOKU_DB_PASSWORD=%q\n' "$GOMOKU_DB_PASSWORD"
    printf 'GOMOKU_DB_NAME=%q\n' "$GOMOKU_DB_NAME"
  } >"$temp_env_file"
  chmod 600 "$temp_env_file"
  mv "$temp_env_file" "$DATABASE_ENV_FILE"
fi

: "${GOMOKU_DB_HOST:?Missing GOMOKU_DB_HOST in $DATABASE_ENV_FILE}"
: "${GOMOKU_DB_USER:?Missing GOMOKU_DB_USER in $DATABASE_ENV_FILE}"
: "${GOMOKU_DB_PASSWORD:?Missing GOMOKU_DB_PASSWORD in $DATABASE_ENV_FILE}"
: "${GOMOKU_DB_NAME:?Missing GOMOKU_DB_NAME in $DATABASE_ENV_FILE}"

sql_password=${GOMOKU_DB_PASSWORD//\'/\'\'}
sudo mariadb --protocol=socket <<SQL
CREATE DATABASE IF NOT EXISTS \`$GOMOKU_DB_NAME\`;
CREATE USER IF NOT EXISTS '$GOMOKU_DB_USER'@'localhost' IDENTIFIED BY '$sql_password';
ALTER USER '$GOMOKU_DB_USER'@'localhost' IDENTIFIED BY '$sql_password';
CREATE USER IF NOT EXISTS '$GOMOKU_DB_USER'@'127.0.0.1' IDENTIFIED BY '$sql_password';
ALTER USER '$GOMOKU_DB_USER'@'127.0.0.1' IDENTIFIED BY '$sql_password';
GRANT ALL PRIVILEGES ON \`$GOMOKU_DB_NAME\`.* TO '$GOMOKU_DB_USER'@'localhost';
GRANT ALL PRIVILEGES ON \`$GOMOKU_DB_NAME\`.* TO '$GOMOKU_DB_USER'@'127.0.0.1';
FLUSH PRIVILEGES;
SQL

sudo mariadb --protocol=socket <"$ROOT_DIR/scripts/init-db.sql"
echo "Initialized $GOMOKU_DB_NAME with local-only user $GOMOKU_DB_USER."
echo "Credentials are stored in $DATABASE_ENV_FILE."

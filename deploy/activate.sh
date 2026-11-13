#!/usr/bin/env bash
# Uploaded with the locally built archive. This script performs no installation
# and no build. Usage: bash activate.sh RELEASE_ID EXPECTED_SHA256 ARCHIVE
set -euo pipefail
release="${1:?release required}"
export RELEASE_ID="$release"
digest="${2:?sha256 required}"
archive="${3:?archive required}"
[[ "$release" =~ ^[0-9]{8}T[0-9]{6}Z$ ]] || { echo 'Invalid release id'; exit 1; }
printf '%s  %s\n' "$digest" "$archive" | sha256sum -c -
base=/var/www/cardgame
target="$base/releases/$release"
mkdir -p "$target"
tar -xzf "$archive" -C "$target" --no-same-owner
chmod 755 "$target/server/bin/cardgame_server"
"$target/server/bin/cardgame_server" --help
node --check "$target/server/main.mjs"
for part in client server; do
  if [[ -e "$base/$part" && ! -L "$base/$part" ]]; then
    echo "$base/$part is a real directory; refusing to overwrite"; exit 1
  fi
done
previous_client=$(readlink "$base/client" || true)
previous_server=$(readlink "$base/server" || true)
config=/etc/nginx/sites-available/cardgame
backup="$target/nginx.previous"
if [[ -f "$config" ]]; then cp "$config" "$backup"; fi
rollback() {
  if [[ -n "$previous_client" ]]; then ln -sfn "$previous_client" "$base/client"; else unlink "$base/client" 2>/dev/null || true; fi
  if [[ -n "$previous_server" ]]; then ln -sfn "$previous_server" "$base/server"; else unlink "$base/server" 2>/dev/null || true; fi
  if [[ -f "$backup" ]]; then cp "$backup" "$config"; else unlink /etc/nginx/sites-enabled/cardgame 2>/dev/null || true; fi
  if [[ -n "$previous_server" ]]; then
    local previous_release
    previous_release=$(basename "$(dirname "$previous_server")")
    RELEASE_ID="$previous_release" pm2 startOrRestart "$base/ecosystem.config.cjs" --update-env
  else pm2 delete cardgame-web 2>/dev/null || true; fi
  nginx -t && systemctl reload nginx
}
trap rollback ERR
ln -sfn "$target/client" "$base/client"
ln -sfn "$target/server" "$base/server"
cp "$target/ecosystem.config.cjs" "$base/ecosystem.config.cjs"
cp "$target/nginx.conf" "$config"
ln -sfn "$config" /etc/nginx/sites-enabled/cardgame
nginx -t
pm2 startOrRestart "$base/ecosystem.config.cjs" --update-env
response=''
for i in {1..30}; do
  if response=$(curl -fsS http://127.0.0.1:3000/healthz 2>/dev/null) && [[ "$response" == *"\"version\":\"$release\""* ]]; then break; fi
  sleep 0.3
done
[[ "$response" == *"\"version\":\"$release\""* ]]
echo "$response"
systemctl reload nginx
# A successful reload signal precedes new workers accepting requests. Poll the
# proxied health route instead of failing on a response from an old worker.
response=''
for i in {1..30}; do
  if response=$(curl -fsS -H 'Host: 47.76.64.154' http://127.0.0.1/healthz 2>/dev/null) && [[ "$response" == *"\"version\":\"$release\""* ]]; then break; fi
  sleep 0.3
done
[[ "$response" == *"\"version\":\"$release\""* ]]
echo "$response"
pm2 save
pm2 startup systemd -u root --hp /root
trap - ERR
echo "Activated $release"

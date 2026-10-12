#!/usr/bin/env bash
set -euo pipefail
ROOT="/home/parm/Documents/KDE Index"
BASE="$ROOT/local/clean-arch-v1/root.x86_64"
OWN="$BASE/public-clean-v2"
EV="$ROOT/evidence/public-clean-build-v2"
cp "$ROOT/local/public-source-v2/kde-index-source.tar.gz" "$OWN/source.tar.gz"
cp "$ROOT/local/public-source-v2/receipt.json" "$OWN/export-receipt.json"
sha256sum "$OWN/source.tar.gz" > "$EV/archive-hash.txt"
/usr/bin/bwrap --die-with-parent --unshare-all --share-net \
  --ro-bind "$BASE" / --bind "$OWN" /public-clean-v2 \
  --proc /proc --dev /dev --tmpfs /tmp \
  --ro-bind /etc/resolv.conf /etc/resolv.conf \
  --chdir /public-clean-v2 \
  /usr/bin/bash -lc '
set -euo pipefail
mkdir source
/usr/bin/python - <<"PY"
import tarfile
with tarfile.open("source.tar.gz", "r:gz") as tf:
    tf.extractall("source", filter="data")
PY
cd source/kde-index-source
/usr/bin/sha256sum -c <(/usr/bin/python - <<"PY"
import json
r=json.load(open("/public-clean-v2/export-receipt.json"))
for p,h in r["retained_files_sha256"].items(): print(h+"  "+p)
PY
)
/usr/bin/bash tools/runtime_setup.sh
/usr/bin/cmake -S . -B ../build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr
/usr/bin/cmake --build ../build --parallel 4
DESTDIR=/public-clean-v2/stage /usr/bin/cmake --install ../build --prefix /usr
'

#!/bin/sh
# test-config.sh [instance-dir]: validate an instance (default: examples/instance).
# Checks screens.conf (fields, positions, unique sizes and ports) and cams/*.env.
set -eu
here=$(cd "$(dirname "$0")/.." && pwd)
inst="${1:-$here/examples/instance}"
fail=0
err() { echo "config error: $*" >&2; fail=1; }

[ -f "$inst/sk-vdisplay.conf" ] || err "missing $inst/sk-vdisplay.conf"
[ -f "$inst/screens.conf" ] || { err "missing $inst/screens.conf"; exit 1; }

awk '!/^[[:space:]]*#/ && NF' "$inst/screens.conf" > "${TMPDIR:-/tmp}/skv-screens.$$"
while read -r name mode pos port extra; do
	[ -z "${extra:-}" ] || err "screen $name: too many fields"
	echo "$mode" | grep -Eq '^[0-9]+x[0-9]+@[0-9]+$' || err "screen $name: mode '$mode' is not WxH@HZ"
	case "$pos" in left-of|right-of|above|below) ;; *) err "screen $name: bad position '$pos'" ;; esac
	echo "$port" | grep -Eq '^[0-9]+$' || err "screen $name: port '$port' is not a number"
done < "${TMPDIR:-/tmp}/skv-screens.$$"
dups=$(awk '{sub(/@.*/, "", $2); print $2}' "${TMPDIR:-/tmp}/skv-screens.$$" | sort | uniq -d)
[ -z "$dups" ] || err "screen sizes must be unique (outputs are matched by size): $dups"
dupp=$(awk '{print $4}' "${TMPDIR:-/tmp}/skv-screens.$$" | sort | uniq -d)
[ -z "$dupp" ] || err "Sunshine ports must be unique: $dupp"
ports=$(awk '{print $4}' "${TMPDIR:-/tmp}/skv-screens.$$" | sort -n)
prev=""
for p in $ports; do
	[ -z "$prev" ] || [ $((p - prev)) -ge 100 ] || err "Sunshine ports $prev and $p are closer than 100 (each uses base-5..base+21)"
	prev=$p
done
rm -f "${TMPDIR:-/tmp}/skv-screens.$$"

for f in "$inst"/cams/*.env; do
	[ -f "$f" ] || continue
	n=$(basename "$f" .env)
	( SRC=""; DEV=""; . "$f"
	  [ -n "$SRC" ] || { echo "config error: cam $n: SRC not set" >&2; exit 1; }
	  echo "$DEV" | grep -Eq '^/dev/video[0-9]+$' || { echo "config error: cam $n: DEV must be /dev/videoN" >&2; exit 1; }
	) || fail=1
done
devs=$(for f in "$inst"/cams/*.env; do [ -f "$f" ] && (. "$f"; echo "$DEV"); done | sort | uniq -d)
[ -z "$devs" ] || err "two cams share $devs"

[ "$fail" = 0 ] && echo "config ok: $inst"
exit "$fail"

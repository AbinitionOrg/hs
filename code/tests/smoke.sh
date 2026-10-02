#!/bin/sh
# make check smoke test: the freshly built hs starts, reports the release
# version from hypdef.h, evaluates integer, float and string expressions,
# and shuts down cleanly.  Needs no router, database or network.

HS=${HS:-./hs}
srcdir=${srcdir:-.}

tmp=`mktemp -d 2>/dev/null || echo /tmp/hs-smoke.$$`
mkdir -p "$tmp/fifo" "$tmp/run" "$tmp/bin" "$tmp/log" "$tmp/spool"
trap 'rm -rf "$tmp"' 0

AUTOROUTER=router;     export AUTOROUTER
AUTOHOST=localhost;    export AUTOHOST
AUTOFIFO=$tmp/fifo;    export AUTOFIFO
AUTORUN=$tmp/run;      export AUTORUN
AUTOBIN=$tmp/bin;      export AUTOBIN
AUTOLOG=$tmp/log;      export AUTOLOG
AUTOSPOOL=$tmp/spool;  export AUTOSPOOL

want=`sed -n 's/.*VERSION_HYPERSCRIPT_BUILD[^"]*"\([^"]*\)".*/\1/p' "$srcdir/hypdef.h"`

cat > "$tmp/smoke.hyp" <<'HYP'
i = 2 + 3 ;
s = "int=" ; s = s + i ;
put s ;
f = 7.5 / 2 ;
s = "float=" ; s = s + f ;
put s ;
exit ;
HYP

out=`"$HS" -f "$tmp/smoke.hyp" 2>&1`
rc=$?

fail () { echo "FAIL: $1"; echo "$out"; exit 1; }

[ $rc -eq 0 ]                              || fail "hs exited with status $rc"
echo "$out" | grep "Version HS-$want" >/dev/null || fail "expected version HS-$want"
echo "$out" | grep '"int=5"' >/dev/null    || fail "integer arithmetic"
echo "$out" | grep '"float=3.75"' >/dev/null || fail "float arithmetic"
echo "$out" | grep 'Terminating HyperScript' >/dev/null || fail "clean shutdown"

echo "PASS: hs $want"
exit 0

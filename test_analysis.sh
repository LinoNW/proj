#!/bin/bash
cd /Users/linoww/Documents/UNI/FSO/Projeto/proj/proj/proj

echo "=== Test 1: Empty string handling ==="
git checkout small.dsk 2>&1 | head -1
./fso-sh small.dsk <<INNER_EOF
create ""
mkdir ""
ls /
exit
INNER_EOF

echo ""
echo "=== Test 2: Root directory special case ==="
git checkout small.dsk 2>&1 | head -1
./fso-sh small.dsk <<INNER_EOF
ls /
ls ""
exit
INNER_EOF

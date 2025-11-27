#!/bin/bash
cd /Users/linoww/Documents/UNI/FSO/Projeto/proj/proj/proj
make clean && make
echo "=== Test: unlink with trailing slash ==="
git checkout small.dsk 2>&1 | head -1
./fso-sh small.dsk <<EOF
create /test
rm /test/
ls /
exit
EOF

#!/bin/bash
cd /Users/linoww/Documents/UNI/FSO/Projeto/proj/proj/proj
git checkout small.dsk 2>&1 | head -1

echo "=== Test 1: Basic operations ==="
./fso-sh small.dsk <<EOF
create /test1
mkdir /testdir
create /testdir/file1
ln /test1 /link1
ls /
ls /testdir
exit
EOF

echo ""
echo "=== Test 2: Error cases ==="
git checkout small.dsk 2>&1 | head -1
./fso-sh small.dsk <<EOF
create /test
create /test
mkdir /test
create /nonexist/file
ls /
exit
EOF

echo ""
echo "=== Test 3: Delete and reuse ==="
git checkout small.dsk 2>&1 | head -1
./fso-sh small.dsk <<EOF
create /a
create /b
create /c
rm /b
create /d
ls /
debug
exit
EOF

#!/bin/bash
cd /Users/linoww/Documents/UNI/FSO/Projeto/proj/proj/proj

echo "=== Test: Empty and special paths ==="
git checkout small.dsk 2>&1 | head -1
./fso-sh small.dsk <<EOF
create ""
create /
create //
create ///
mkdir ""
mkdir /
ls /
exit
EOF

echo ""
echo "=== Test: Paths without leading slash ==="
git checkout small.dsk 2>&1 | head -1
./fso-sh small.dsk <<EOF
create test
mkdir mydir
create mydir/file
ln test mydir/link
ls /
ls mydir
exit
EOF

echo ""
echo "=== Test: Nested directories ==="
git checkout small.dsk 2>&1 | head -1
./fso-sh small.dsk <<EOF
mkdir /a
mkdir /a/b
mkdir /a/b/c
create /a/b/c/file
ls /
ls /a
ls /a/b
ls /a/b/c
exit
EOF

echo ""
echo "=== Test: Multiple links ==="
git checkout small.dsk 2>&1 | head -1
./fso-sh small.dsk <<EOF
create /original
ln /original /link1
ln /original /link2
ln /original /link3
ls /
rm /original
ls /
rm /link1
ls /
rm /link2
ls /
exit
EOF

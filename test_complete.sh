#!/bin/bash
cd /Users/linoww/Documents/UNI/FSO/Projeto/proj/proj/proj

echo "=== Test 1: NULL checks and empty strings ==="
git checkout small.dsk 2>&1 | head -1
./fso-sh small.dsk <<EOF
create /test
rm /test/
create /dir/
mkdir /dir/
ln /test /link/
ls /
exit
EOF

echo ""
echo "=== Test 2: Verify all operations return correct values ==="
git checkout small.dsk 2>&1 | head -1
./fso-sh small.dsk <<EOF
create /file1
mkdir /dir1
ln /file1 /link1
create /file1
mkdir /dir1
ln /dir1 /badlink
rm /dir1
rm /link1
ls /
exit
EOF

echo ""
echo "=== Test 3: Complex path scenarios ==="
git checkout small.dsk 2>&1 | head -1
./fso-sh small.dsk <<EOF
mkdir /a
mkdir /a/b
create /a/b/file
ln /a/b/file /a/link
rm /a/b/file
ls /a/b
ls /a
exit
EOF

echo ""
echo "=== Test 4: Maximum filename length ==="
git checkout small.dsk 2>&1 | head -1
./fso-sh small.dsk <<EOF
create /a123456789012345678901234567890123456789012345678901234567890
ln /a123456789012345678901234567890123456789012345678901234567890 /b123456789012345678901234567890123456789012345678901234567890
ls /
exit
EOF

echo ""
echo "=== Test 5: Paths without leading slash ==="
git checkout small.dsk 2>&1 | head -1
./fso-sh small.dsk <<EOF
create test
mkdir mydir
create mydir/file
ln test mydir/link
rm test
ls /
ls mydir
exit
EOF

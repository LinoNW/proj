#!/bin/bash
cd /Users/linoww/Documents/UNI/FSO/Projeto/proj/proj/proj

echo "=== Test 1: link/unlink only work on files ==="
git checkout small.dsk 2>&1 | head -1
./fso-sh small.dsk <<EOF
mkdir /mydir
ln /mydir /link_to_dir
create /myfile
ln /myfile /link_to_file
rm /mydir
rm /myfile
ls /
exit
EOF

echo ""
echo "=== Test 2: fs_ls returns 0 on success, -1 on error ==="
git checkout small.dsk 2>&1 | head -1
./fso-sh small.dsk <<EOF
ls /
ls /nonexistent
create /file
ls /file
exit
EOF

echo ""
echo "=== Test 3: Directory size doesn't decrease when deleting ==="
git checkout small.dsk 2>&1 | head -1
./fso-sh small.dsk <<EOF
create /a
create /b
create /c
debug
rm /b
debug
create /d
debug
exit
EOF

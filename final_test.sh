#!/bin/bash
cd /Users/linoww/Documents/UNI/FSO/Projeto/proj/proj/proj

echo "=== Final Test: All edge cases ==="
git checkout HEAD -- small.dsk
./fso-sh small.dsk <<INNER_EOF
create /test1
create /test1
mkdir /dir1
mkdir /dir1
ln /test1 /link1
ln /dir1 /badlink
rm /test1/
rm /
create /test2
mkdir /dir2
ln /test2 /link2
rm /test2
rm /link2
ls /
exit
INNER_EOF

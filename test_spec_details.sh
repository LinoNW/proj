#!/bin/bash
cd /Users/linoww/Documents/UNI/FSO/Projeto/proj/proj/proj

echo "=== Test: ls return values (0 on success, -1 on error) ==="
git checkout HEAD -- small.dsk > /dev/null 2>&1
./fso-sh small.dsk <<INNER
ls /
ls /nonexist
ls /f0
exit
INNER

echo ""
echo "=== Test: link/unlink return inode number ==="
git checkout HEAD -- small.dsk > /dev/null 2>&1
./fso-sh small.dsk <<INNER
create /myfile
ln /myfile /mylink
rm /mylink
exit
INNER

echo ""
echo "=== Test: Directory entries marked FREE but size unchanged ==="
git checkout HEAD -- small.dsk > /dev/null 2>&1
./fso-sh small.dsk <<INNER
create /x
create /y
create /z
debug
rm /y
debug
exit
INNER

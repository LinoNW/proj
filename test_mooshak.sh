#!/bin/bash
cd /Users/linoww/Documents/UNI/FSO/Projeto/proj/proj/proj

echo "=== Test 1: Path without leading slash ==="
git checkout HEAD -- small.dsk > /dev/null 2>&1
./fso-sh small.dsk <<INNER
create file1
mkdir dir1
ln file1 link1
ls /
exit
INNER

echo ""
echo "=== Test 2: Multiple slashes in path ==="
git checkout HEAD -- small.dsk > /dev/null 2>&1
./fso-sh small.dsk <<INNER
create //test
mkdir ///dir
ls /
exit
INNER

echo ""
echo "=== Test 3: Link operations ==="
git checkout HEAD -- small.dsk > /dev/null 2>&1
./fso-sh small.dsk <<INNER
create /file
ln /file /link1
ln /file /link2
ln /file /link3
rm /file
rm /link1
rm /link2
rm /link3
ls /
exit
INNER

echo ""
echo "=== Test 4: Return values ==="
git checkout HEAD -- small.dsk > /dev/null 2>&1
./fso-sh small.dsk <<INNER
create /test
create /test
exit
INNER

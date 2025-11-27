#!/bin/bash
cd /Users/linoww/Documents/UNI/FSO/Projeto/proj/proj/proj
git checkout small.dsk 2>&1 | head -1
./fso-sh small.dsk <<EOF
create /a
create /b
create /c
mkdir /d
ln /a /link
rm /b
ls /
exit
EOF

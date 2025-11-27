#!/bin/bash
cd /Users/linoww/Documents/UNI/FSO/Projeto/proj/proj/proj
git checkout small.dsk 2>&1 | head -1
./fso-sh small.dsk <<EOF
create /a
create /a
mkdir /b  
mkdir /b
create /b
mkdir /a
create /nonexist/file
mkdir /c
create /c/file
ln /a /c/linka
ls /
ls /c
exit
EOF

#!/bin/bash
cd /Users/linoww/Documents/UNI/FSO/Projeto/proj/proj/proj

echo "=== Test: Long filename (exactly 61 chars) ==="
git checkout small.dsk 2>&1 | head -1
./fso-sh small.dsk <<EOF
create /a123456789012345678901234567890123456789012345678901234567890
ls /
exit
EOF

echo ""
echo "=== Test: Very long filename (70 chars - should truncate) ==="
git checkout small.dsk 2>&1 | head -1
./fso-sh small.dsk <<EOF
create /b1234567890123456789012345678901234567890123456789012345678901234567890
ls /
exit
EOF

echo ""
echo "=== Test: Filename exactly MAXFILENAME (62 chars) ==="
git checkout small.dsk 2>&1 | head -1
./fso-sh small.dsk <<EOF
create /c12345678901234567890123456789012345678901234567890123456789012
ls /
exit
EOF

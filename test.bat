echo off
echo test with ZLIB
gcc main.c -I zlib/ -I libd-eflate/ -I isa-l/ -Lzlib/ -Llibdeflate/ -Lisa-l/ -lz -D ZLIB
a.exe butterfly.png output.txt
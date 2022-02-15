del *.o /s
del *.elf
del *.dol
del *.map
make -f Makefile.glN64_wii --jobs=5
del *.o /s
make -f Makefile.Rice_wii --jobs=5

del release\*.dol /s
mv cube*.dol release\gamecube\
mv wii64-glN64.dol release\apps\wii64\boot.dol
mv wii64-Rice.dol "release\apps\wii64 Rice\boot.dol"
pause
@echo OFF
echo CREATING RELEASE BUILD
windres bzccscanner.rc -O coff -o bzccscanner.res
gcc bzccscanner.c bzcchtable.c bzccnet.c bzccsettings.c bzccscanner.res -lComctl32 -lCrypt32 -lWs2_32 -o bzccscanner.exe -s -mwindows -static-libgcc -static-libstdc++

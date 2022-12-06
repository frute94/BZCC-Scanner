@echo OFF
echo CREATING RELEASE BUILD
windres bzccscanner.rc -O coff -o bzccscanner.res
gcc bzccscanner.c bzccb64.c bzcchtable.c bzccnet.c bzccsettings.c bzccscanner.res -lComctl32 -lWs2_32 -o bzccscanner.exe -s -mwindows

@echo OFF
echo CREATING DEBUG BUILD
gcc bzccscanner.c bzccb64.c bzcchtable.c bzccnet.c bzccsettings.c bzccscanner.res -lComctl32 -lWs2_32 -ggdb -o bzccscanner.exe

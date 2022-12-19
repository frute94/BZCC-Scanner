@echo OFF
echo CREATING DEBUG BUILD
gcc bzccscanner.c bzcchtable.c bzccnet.c bzccsettings.c bzccscanner.res -lComctl32 -lWs2_32 -lCrypt32 -ggdb -o bzccscanner_testing.exe

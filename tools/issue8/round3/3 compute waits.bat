@echo off
rem Issue #8 test (round 3): 3 compute waits
cd /d "%~dp0"
set LARECOMP_VK_CS_SYNC=1
set LARECOMP_VK_CS_LOG=1
start "" "%~dp0lasrecomp.exe"

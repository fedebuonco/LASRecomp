@echo off
rem Issue #8 test (round 3): 4 heap without addresses
cd /d "%~dp0"
set LARECOMP_VK_GMEM_LAZY=0
set LARECOMP_VK_CS_LOG=1
start "" "%~dp0lasrecomp.exe"

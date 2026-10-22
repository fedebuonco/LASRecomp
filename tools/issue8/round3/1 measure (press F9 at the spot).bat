@echo off
rem Issue #8 test (round 3): 1 measure (press F9 at the spot)
cd /d "%~dp0"
set LARECOMP_VK_CS_LOG=1
set LARECOMP_VK_PASS_MEANS=1
set LARECOMP_LOG_FSHASH=1
set LARECOMP_VK_DIAG_LOG=1
start "" "%~dp0lasrecomp.exe"

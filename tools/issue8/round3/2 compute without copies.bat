@echo off
rem Issue #8 test (round 3): 2 compute without copies
cd /d "%~dp0"
set LARECOMP_VK_GMEM_FAST=0
set LARECOMP_VK_CS_LOG=1
start "" "%~dp0lasrecomp.exe"

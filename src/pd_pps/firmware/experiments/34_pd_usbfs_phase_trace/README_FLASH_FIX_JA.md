# M33 Flash overflow correction

The M33 Makefile previously appended to CFLAGS before including ch32fun.mk. That caused the ch32fun.mk `CFLAGS ?=` size-optimized defaults to be skipped.

This revision explicitly adds -Os -flto -ffunction-sections -fdata-sections -fno-tree-loop-distribute-patterns. No USB/PD/diagnostic C logic has been changed.

Run Test-M33-Host.ps1 and Build-M33.ps1 using powershell.exe -NoProfile -ExecutionPolicy Bypass -File ... .
If FLASH still exceeds 62 KB, report the exact new overflow amount; do not flash.

@echo off
call "D:\VS2022.1\VC\Auxiliary\Build\vcvars64.bat" > NUL 2>&1
cd /d "D:\code.2024\test.2\test.2"
cl /nologo /utf-8 /W3 /Fe:water_analysis.exe main.c common.c dataset.c query.c modify.c backup.c
echo EXIT_CODE=%ERRORLEVEL%

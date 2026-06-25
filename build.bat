@echo off
cd /d "%~dp0"
gcc -std=c11 -O2 -Wall -o water_analysis.exe main.c common.c dataset.c query.c modify.c backup.c statistics.c predict.c auth.c -lm
echo EXIT_CODE=%ERRORLEVEL%

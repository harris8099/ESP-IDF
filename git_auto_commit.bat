@echo off
REM Get current date and time
set datetime=%date%_%time%
set datetime=%datetime: =_%
set datetime=%datetime::=%
set datetime=%datetime:.=%

echo Committing with message: %datetime%

git add .
git commit -m "%datetime%"
git push

pause

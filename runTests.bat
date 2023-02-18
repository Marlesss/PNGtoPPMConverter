Setlocal EnableDelayedExpansion
echo off
for /r %%i in (/test/*.png) do (
   set boba=%%i
   echo !in!
   set in=!boba:\c_2-Marlesss\=\c_2-Marlesss\test\!
   set out=!in:.png=_out.ppm!
   cmake-build-debug\main.exe !in! !out!
   echo it was !in!
   echo %ERRORLEVEL%
)
PAUSE
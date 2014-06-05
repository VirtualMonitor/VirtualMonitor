echo off
cscript configure.vbs --with-libjpeg=.\libjpeg-turbo
call env.bat
kmk

echo off

cscript configure.vbs --with-libjpeg=.\src\libs\libjpeg-turbo
call env.bat

kmk

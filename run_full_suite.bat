@echo off
set PATH=C:\Qt\6.8.0\msvc2022_64\bin;%PATH%
cd /d "C:\Users\andre\Documents\Code\apop_patched\apophysis-cpp\build-release"
ctest --output-on-failure 2>&1

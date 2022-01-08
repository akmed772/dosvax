echo off
echo This batch requires MODE.COM that comes with IBM DOS K3.x, J4.0 and J5.0.
mode 0,1,19
echo mode 0,1,19 : Mode 8 (Monochrome), Normal 1 (Blue), Highlight 19 (Sky)
attr
pause
mode 0,19,1
echo mode 0,19,1 : Mode 8 (Monochrome), Normal 19 (Sky), Highlight 1 (Blue)
attr
pause
mode 3,0
echo mode 3,0 : Mode E (16 colors), Normal colors
attr
pause
mode 3,1
echo mode 3,1 : Mode E (16 colors), Light colors
attr
pause
mode 3,0,11
echo mode 3,0,11 : Mode E (16 colors), Line color 11 (Light Cyan)
attr
pause
mode 0,18,63
echo on
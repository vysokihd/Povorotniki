@echo off
E:
cd E:\AVR_Programs\-=SOFT=-\avrdude-6.3\
avrdude.exe -p t13 -B 10 -c usbasp -P usb -U flash:w:"E:\AVR_Programs\-=MY_PROGECTS=-\Povorotniki\Debug\Povorotniki.hex":i -U lfuse:w:0x6a:m -U hfuse:w:0xff:m
pause
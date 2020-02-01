# Arduino-Switch-Box

Usually upload via Arduino should work, but if you need to use avrdude, then try this (Pro Micro):

```
sudo avrdude -C/etc/avrdude.conf -patmega32u4 -cavr109 -v -v -v -v -P/dev/ttyACM0  -D -F -Uflash:w:Arduino-Switch-Box.ino.hex:i
```


#!/bin/sh
# acer-console：把 EC 相关节点放给普通用户（单用户笔电）
[ -e /dev/tuxedo_io ] && chmod 0666 /dev/tuxedo_io
for f in /sys/class/leds/rgb:kbd_backlight*/brightness \
         /sys/class/leds/rgb:kbd_backlight*/multi_intensity; do
    [ -e "$f" ] && chmod 0666 "$f"
done
exit 0

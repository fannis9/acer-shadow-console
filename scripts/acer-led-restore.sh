#!/bin/sh
# acer-console：开机把上次的键盘灯颜色写回 EC（重启后 EC 会回落到固件默认）
LED=/sys/class/leds/rgb:kbd_backlight
[ -d "$LED" ] || exit 0

apply() {
    conf="$1/.config/acer-console/config.json"
    [ -f "$conf" ] || return 1
    vals=$(python3 - "$conf" <<'PY' 2>/dev/null
import json, sys
c = json.load(open(sys.argv[1]))
col = c.get("color") or [255, 255, 255]
bri = int(c.get("brightness", 153))
print(col[0], col[1], col[2], max(1, min(255, bri)))
PY
)
    [ -n "$vals" ] || return 1
    set -- $vals
    echo "$1 $2 $3" > "$LED/multi_intensity" || return 1
    echo $(($4 - 1)) > "$LED/brightness"
    echo "$4" > "$LED/brightness"
    return 0
}

for home in /home/*; do
    [ -d "$home" ] || continue
    apply "$home" && logger -t acer-led "restored keyboard LED for $home" && exit 0
done
exit 0

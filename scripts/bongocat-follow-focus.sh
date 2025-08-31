#!/usr/bin/env bash
set -u

CONF="${HOME}/.config/bongocat/bongocat.conf"
BONGO_BIN="${HOME}/.config/bongocat/bongocat"
CAT_H=${CAT_H:-64}

get_win_rect() {
    swaymsg -t get_tree | jq -c '
      .. | (.nodes? + .floating_nodes? // [])[]? 
      | select(.focused==true) | .rect' 2>/dev/null
}

get_mon_rect() {
    swaymsg -t get_outputs | jq -c '.[] | select(.active and .focused==true) | .rect' 2>/dev/null
}

write_offsets() {
    local ox="$1" oy="$2"

    cat > "$CONF" <<EOF
cat_height=$CAT_H
cat_x_offset=$ox
cat_y_offset=$oy
cat_align=center

overlay_opacity=0
overlay_position=top

fps=60
keypress_duration=100
test_animation_interval=3

keyboard_device=/dev/input/event0   # Sleep Button
keyboard_device=/dev/input/event2   # Power Button
keyboard_device=/dev/input/event3   # AT Translated Set 2 keyboard
keyboard_device=/dev/input/event4   # ThinkPad Extra Buttons
keyboard_device=/dev/input/event5   # Video Bus
keyboard_device=/dev/input/event6   # Intel HID events
keyboard_device=/dev/input/event7   # PC Speaker
keyboard_device=/dev/input/event16   # AT Translated Set 2 keyboard
keyboard_device=/dev/input/event16   # AT Translated Set 2 keyboard

monitor=eDP-1

enable_debug=1
EOF
}

restart_bongocat() {
    pkill -x bongocat 2>/dev/null || true
    nohup "$BONGO_BIN" --config "$CONF" >/dev/null 2>&1 &
}

WIN="$(get_win_rect)" || true
MON="$(get_mon_rect)" || true

swaymsg -m -t subscribe '["window","workspace","output"]' | while read -r _; do
    WIN="$(get_win_rect)"
    MON="$(get_mon_rect)"
    [ -z "$WIN" ] && continue
    [ -z "$MON" ] && continue

    WX=$(jq -r '.x'      <<<"$WIN")
    WY=$(jq -r '.y'      <<<"$WIN")
    WW=$(jq -r '.width'  <<<"$WIN")
    WH=$(jq -r '.height' <<<"$WIN")

    MX=$(jq -r '.x'      <<<"$MON")
    MY=$(jq -r '.y'      <<<"$MON")
    MW=$(jq -r '.width'  <<<"$MON")
    MH=$(jq -r '.height' <<<"$MON")

    SCX=$(( MX + MW / 2 ))
    SCY=$(( MY + MH / 2 ))

    TX=$(( WX + WW / 2 ))
    TY=$(( WY - CAT_H * 13 / 64 ))

    if [ "$WY" -eq 0  ]; then
        TY=$(( -200 ))
    fi

    OX=$(( TX - SCX ))
    OY=$(( TY - SCY ))

    write_offsets "$OX" "$OY"
    restart_bongocat
done

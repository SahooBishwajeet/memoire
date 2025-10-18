#!/usr/bin/env bash

DATAPATH="${XDG_DATA_HOME:-$HOME/.local/share}/memoire/data.txt"
mkdir -p "$(dirname "$DATAPATH")"
touch "$DATAPATH"

options="add\nupdate\ndelete\n$(cat "$DATAPATH")"

selected=$(echo -e "$options" | rofi -dmenu -p "Memoire:" -format "s")
[ -z "$selected" ] && exit 0

# Handle Add
if [ "$selected" = "add" ]; then
    new_entry=$(rofi -dmenu -p "Add (key:value):")
    [ -z "$new_entry" ] && exit 0

    if ! echo "$new_entry" | grep -qE '^[^:]+:.*$'; then
        if command -v notify-send >/dev/null 2>&1; then
            notify-send "Memoire" "Invalid format. Use key:value"
        else
            echo "Invalid format. Use key:value"
        fi
        exit 1
    fi

    echo "$new_entry" >> "$DATAPATH"
    if command -v notify-send >/dev/null 2>&1; then
        notify-send "Memoire" "Added: $new_entry"
    else
        echo "Added: $new_entry"
    fi
    exit 0
fi

# Handle Update
if [ "$selected" = "update" ]; then
    target=$(cat "$DATAPATH" | rofi -dmenu -p "Select entry to update:")
    [ -z "$target" ] && exit 0

    new_value=$(rofi -dmenu -p "Update to (key:value):" -filter "$target")
    [ -z "$new_value" ] && exit 0

    if ! echo "$new_value" | grep -qE '^[^:]+:.*$'; then
        if command -v notify-send >/dev/null 2>&1; then
            notify-send "Memoire" "Invalid format. Use key:value"
        else
            echo "Invalid format. Use key:value"
        fi
        exit 1
    fi

    sed -i "s|^$target\$|$new_value|" "$DATAPATH"

    if command -v notify-send >/dev/null 2>&1; then
        notify-send "Memoire" "Updated: $target -> $new_value"
    else
        echo "Updated: $target -> $new_value"
    fi
    exit 0
fi

# Handle Delete
if [ "$selected" = "delete" ]; then
    target=$(cat "$DATAPATH" | rofi -dmenu -p "Select entry to delete:")
    [ -z "$target" ] && exit 0

    sed -i "/^$target$/d" "$DATAPATH"

    if command -v notify-send >/dev/null 2>&1; then
        notify-send "Memoire" "Deleted: $target"
    else
        echo "Deleted: $target"
    fi
    exit 0
fi

# Extract value from Key:Value pair; Copy to clipboard; Notify user
value=$(echo "$selected" | cut -d':' -f2- | xargs)

if command -v wl-copy >/dev/null 2>&1; then
    echo -n "$value" | wl-copy
elif command -v xclip >/dev/null 2>&1; then
    echo -n "$value" | xclip -selection clipboard
else
    echo "No clipboard utility found (install wl-clipboard or xclip)" >&2
    exit 1
fi

if command -v notify-send >/dev/null 2>&1; then
    notify-send "Memoire" "Copied to clipboard: $value"
else
    echo "Copied to clipboard: $value"
fi

#!/bin/bash

# Script to help find input devices for bongocat configuration
# Usage: ./scripts/find_input_devices.sh

echo "=== Bongo Cat Input Device Finder ==="
echo ""

echo "1. Available input devices:"
echo "=========================="
ls -la /dev/input/event* 2>/dev/null | while read -r line; do
    device=$(echo "$line" | awk '{print $NF}')
    if [ -c "$device" ]; then
        echo "$device"
    fi
done

echo ""
echo "2. Input device details:"
echo "======================="
if [ -r /proc/bus/input/devices ]; then
    cat /proc/bus/input/devices | grep -E "(Name|Handlers)" | while read -r line; do
        if [[ $line == N:* ]]; then
            name=$(echo "$line" | cut -d'"' -f2)
            echo "Device: $name"
        elif [[ $line == H:* ]]; then
            handlers=$(echo "$line" | grep -o 'event[0-9]*')
            if [ -n "$handlers" ]; then
                for handler in $handlers; do
                    echo "  -> /dev/input/$handler"
                done
            fi
            echo ""
        fi
    done
else
    echo "Cannot read /proc/bus/input/devices (try running as root)"
fi

echo "3. Suggested configuration:"
echo "=========================="
echo "Add these lines to your bongocat.conf file:"
echo ""

# Find keyboard-like devices
if [ -r /proc/bus/input/devices ]; then
    # Look for devices that contain "keyboard" in their name
    grep -A 10 -B 2 -i "keyboard" /proc/bus/input/devices | grep -E "(Name|Handlers)" | while read -r line; do
        if [[ $line == N:* ]]; then
            name=$(echo "$line" | cut -d'"' -f2)
            current_name="$name"
        elif [[ $line == H:* ]]; then
            handlers=$(echo "$line" | grep -o 'event[0-9]*')
            if [ -n "$handlers" ]; then
                for handler in $handlers; do
                    echo "keyboard_device=/dev/input/$handler  # $current_name"
                done
            fi
        fi
    done
    
    # Also look for common external keyboard brands
    grep -A 10 -B 2 -i "keychron\|logitech\|corsair\|razer\|steelseries" /proc/bus/input/devices | grep -E "(Name|Handlers)" | while read -r line; do
        if [[ $line == N:* ]]; then
            name=$(echo "$line" | cut -d'"' -f2)
            current_name="$name"
        elif [[ $line == H:* ]]; then
            handlers=$(echo "$line" | grep -o 'event[0-9]*')
            if [ -n "$handlers" ]; then
                for handler in $handlers; do
                    echo "keyboard_device=/dev/input/$handler  # $current_name (external)"
                done
            fi
        fi
    done
    
    echo ""
    echo "# If no devices are listed above, try the default:"
    echo "keyboard_device=/dev/input/event4  # Default keyboard"
else
    echo "# Run this script as root to get device suggestions"
    echo "keyboard_device=/dev/input/event4  # Default - adjust as needed"
fi

echo ""
echo "4. Testing devices:"
echo "=================="
echo "To test which device generates events when you type:"
echo "sudo evtest"
echo ""
echo "Then select a device number and type on your keyboard."
echo "Use Ctrl+C to exit evtest."
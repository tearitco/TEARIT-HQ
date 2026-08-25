
#!/bin/bash
# Configuration
TRACKER_SERVICES="tracker-miner-fs-3.service tracker-extract-3.service"

show_status() {
    echo "=== Tracker Services Status ==="
    systemctl --user status $TRACKER_SERVICES | grep -E "Active:|Loaded:"
}

turn_off() {
    echo "Stopping and masking Tracker services to prevent high CPU usage..."
    systemctl --user stop $TRACKER_SERVICES
    systemctl --user mask $TRACKER_SERVICES
    echo "Tracker services have been disabled."
}

turn_on() {
    echo "Unmasking and starting Tracker services..."
    systemctl --user unmask $TRACKER_SERVICES
    systemctl --user start $TRACKER_SERVICES
    echo "Tracker services have been enabled."
}
# Interactive Menu
echo "1) Turn OFF (Stop & Mask)"
echo "2) Turn ON (Unmask & Start)"
echo "3) Check Status"
read -p "Select an option [1-3]: " choice
case $choice in
    1) turn_off ;;
    2) turn_on ;;
    3) show_status ;;
    *) echo "Invalid option." ;;esac

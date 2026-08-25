#!/bin/bash
# GPU Hang Watchdog for TPMOS
# Detects AMD/Intel GPU hangs and attempts recovery before full system freeze
# 
# Usage: 
#   sudo ./gpu_watchdog.sh &
#   Or add to crontab: * * * * * /path/to/gpu_watchdog.sh

LOG_DIR="/tmp/gpu_watchdog"
mkdir -p "$LOG_DIR"

THRESHOLD_SECONDS=60
CHECK_INTERVAL=10

log() {
    echo "[$(date '+%Y-%m-%d %H:%M:%S')] $1" | tee -a "$LOG_DIR/watchdog.log"
}

check_gpu_hang() {
    # Check dmesg for recent GPU hang messages
    local hang_count
    hang_count=$(dmesg --time-format iso 2>/dev/null | \
        grep -E "blocked for more than|ring.*timeout|GPU hang|amdgpu.*reset" | \
        tail -20 | wc -l)
    
    if [ "$hang_count" -gt 0 ]; then
        return 0  # Hang detected
    fi
    return 1  # No hang
}

attempt_gpu_reset() {
    log "Attempting GPU reset via sysfs..."
    
    # Try AMD GPU reset
    if [ -w /sys/class/drm/card0/device/reset ]; then
        echo 1 > /sys/class/drm/card0/device/reset 2>/dev/null
        if [ $? -eq 0 ]; then
            log "GPU reset successful"
            return 0
        fi
    fi
    
    # Try Intel GPU reset
    if [ -w /sys/class/drm/card0/device/i915_reset ]; then
        echo 1 > /sys/class/drm/card0/device/i915_reset 2>/dev/null
        if [ $? -eq 0 ]; then
            log "Intel GPU reset successful"
            return 0
        fi
    fi
    
    log "GPU reset via sysfs failed"
    return 1
}

kill_gpu_hogs() {
    log "Killing processes likely causing GPU hang..."
    
    # Kill known GPU-heavy processes
    local pids
    pids=$(ps aux | grep -E "gl_desktop\.\+x|chrome|chromium|firefox|browser.*rcs" | \
        grep -v grep | grep -v watchdog | awk '{print $2}')
    
    if [ -n "$pids" ]; then
        log "Killing PIDs: $pids"
        echo $pids | xargs -r kill -TERM
        sleep 2
        
        # Force kill if still running
        pids=$(ps aux | grep -E "gl_desktop\.\+x|chrome|chromium|firefox|browser.*rcs" | \
            grep -v grep | grep -v watchdog | awk '{print $2}')
        if [ -n "$pids" ]; then
            log "Force killing still-running PIDs: $pids"
            echo $pids | xargs -r kill -KILL
        fi
    fi
}

dump_gpu_state() {
    local timestamp
    timestamp=$(date '+%Y%m%d_%H%M%S')
    local dump_file="$LOG_DIR/gpu_state_$timestamp.log"
    
    log "Dumping GPU state to $dump_file"
    
    {
        echo "=== DMESG (last 100 lines) ==="
        dmesg | tail -100
        
        echo -e "\n=== GPU PROCESSES ==="
        ps aux | grep -E "gl_|gpu|drm|chrome|firefox" | grep -v grep
        
        echo -e "\n=== DRM DEVICES ==="
        ls -la /dev/dri/ 2>/dev/null
        
        echo -e "\n=== GPU MEMORY (AMD) ==="
        cat /sys/kernel/debug/dri/0/amdgpu_vram_mm 2>/dev/null || echo "Not available"
        
    } > "$dump_file" 2>&1
}

# Main loop
log "GPU Watchdog started (check interval: ${CHECK_INTERVAL}s)"

while true; do
    if check_gpu_hang; then
        log "GPU hang detected!"
        
        # Dump state for debugging
        dump_gpu_state
        
        # Try graceful recovery
        attempt_gpu_reset
        if [ $? -ne 0 ]; then
            # Reset failed, kill offenders
            kill_gpu_hogs
        fi
        
        # Wait for system to recover
        sleep 5
    fi
    
    sleep $CHECK_INTERVAL
done

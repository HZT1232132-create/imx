#!/bin/bash
# EdgeGuard-Sort WiFi Auto-Connect Script
# Usage: ./connect_wifi.sh

SSID="hzt"
PASS="12345678"
IFACE="mlan0"

echo "[WiFi] Loading NXP driver..."
modprobe moal mod_para=nxp/wifi_mod_para.conf 2>/dev/null

echo "[WiFi] Creating config..."
cat > /tmp/wifi.conf << WIFIEOF
network={
    ssid="${SSID}"
    psk="${PASS}"
}
WIFIEOF

echo "[WiFi] Connecting..."
pkill wpa_supplicant 2>/dev/null
sleep 1
/usr/sbin/wpa_supplicant -B -i ${IFACE} -c /tmp/wifi.conf

echo "[WiFi] Getting IP..."
/usr/sbin/dhclient ${IFACE} 2>/dev/null

echo "[WiFi] Testing..."
sleep 2
if ping -c 1 -W 3 8.8.8.8 > /dev/null 2>&1; then
    IP=$(ip addr show ${IFACE} | grep 'inet ' | awk '{print $2}' | cut -d/ -f1)
    echo "[WiFi] Connected! IP: ${IP}"
else
    echo "[WiFi] Failed!"
fi

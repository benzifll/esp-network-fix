#!/bin/bash

# ============================================================
# ASOA 3.1 - Raspberry Pi WiFi Bridge Setup
# This script configures the Pi to receive internet from a 
# phone hotspot and share it with the ESP device.
# ============================================================

# --- 1. CHANGE THESE TO YOUR PHONE SETTINGS ---
PHONE_SSID="realme C55"
PHONE_PASS="i2awfe4t"

# --- 2. DO NOT CHANGE THESE (Matches ESP Code) ---
ESP_SSID="RasberryPi"
ESP_PASS="rasberrypi123"

# Colors for output
GREEN='\033[0;32m'
RED='\033[0;31m'
NC='\033[0m' # No Color

echo -e "${GREEN}Starting ASOA Bridge Setup...${NC}"

# Check for root
if [ "$EUID" -ne 0 ]; then 
  echo -e "${RED}Please run as root (use sudo ./raspberry_bridge.sh)${NC}"
  exit
fi

# Install NetworkManager if not present
if ! command -v nmcli &> /dev/null; then
    echo "Installing NetworkManager..."
    apt update && apt install -y network-manager
fi

# Enable IP Forwarding (Essential for bridging)
echo "Enabling IP Forwarding..."
echo 1 > /proc/sys/net/ipv4/ip_forward
sed -i 's/#net.ipv4.ip_forward=1/net.ipv4.ip_forward=1/' /etc/sysctl.conf

# 1. Connect to Phone Hotspot
echo -e "${GREEN}Connecting to Phone: $PHONE_SSID...${NC}"
nmcli device wifi connect "$PHONE_SSID" password "$PHONE_PASS"

# 2. Create the ESP Hotspot (TeamMehano)
# Note: This creates a hotspot on wlan0 while staying connected to the phone.
# If your Pi model doesn't support this, it will error here.
echo -e "${GREEN}Creating ESP Hotspot: $ESP_SSID...${NC}"

# Remove old connection if exists
nmcli connection delete "$ESP_SSID" &> /dev/null

# Create Hotspot
nmcli device wifi hotspot ssid "$ESP_SSID" password "$ESP_PASS" ifname wlan0
nmcli connection modify "$ESP_SSID" connection.autoconnect yes
nmcli connection modify "$ESP_SSID" ipv4.method shared

# 3. Ensure Phone connection also auto-connects
nmcli connection modify "$PHONE_SSID" connection.autoconnect yes

echo -e "===================================================="
echo -e "${GREEN}SETUP COMPLETE!${NC}"
echo -e "1. Your Pi is now connected to: ${PHONE_SSID}"
echo -e "2. Your Pi is broadcasting: ${ESP_SSID}"
echo -e "3. Your ESP should now see the WiFi and connect."
echo -e "===================================================="

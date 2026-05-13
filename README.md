Raspberry Pi Bridge Instructions

This directory contains `raspberry_bridge.sh`. Follow these steps to set up your Raspberry Pi:

### 1. Copy the script to your Pi
You can use a USB drive or SFTP to move `raspberry_bridge.sh` to your Raspberry Pi's `/home/pi` folder.

### 2. Edit your Phone Credentials
On the Raspberry Pi terminal, open the file:
```bash
nano raspberry_bridge.sh
```
Change the `PHONE_SSID` and `PHONE_PASS` at the top of the file to match your mobile hotspot. 
Save and exit (`Ctrl+O`, `Enter`, `Ctrl+X`).

### 3. Run the script
Run these commands:
```bash
chmod +x raspberry_bridge.sh
sudo ./raspberry_bridge.sh
```

### 4. How it works
Once the script finishes:
1. The Pi will connect to your phone.
2. The Pi will start a network called **RasberryPi**.
3. Your ESP (with the code we just updated) will automatically connect to **RasberryPi**.
4. Data will flow from: **ESP -> Pi -> Phone -> Vercel**.

### Troubleshooting
If the ESP says "WiFi FAIL":
* Ensure the Pi has a clear line of sight to the ESP.
* Check that your Phone Hotspot is actually turned on.
* If the script says "Error: device does not support hotspot", you may need a small USB WiFi dongle for the Pi to handle both connections at once.

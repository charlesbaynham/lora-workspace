#!/usr/bin/env bash
# List attached Heltec/ESP32-S3 boards (native USB, VID 303a:1001).
# The USB serial number of these chips is their MAC address, so it identifies
# a physical board across re-plugs, unlike /dev/ttyACM* numbering.
for p in /dev/serial/by-id/usb-Espressif_USB_JTAG_serial_debug_unit_*; do
  [ -e "$p" ] || { echo "no boards found"; exit 1; }
  access=$([ -r "$(readlink -f "$p")" ] && [ -w "$(readlink -f "$p")" ] && echo ok || echo "NO ACCESS (see README: udev rule)")
  printf '%s\n  -> %s  [%s]\n' "$p" "$(readlink -f "$p")" "$access"
done

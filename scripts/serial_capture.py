"""Reset a board via RTS and print its serial output for N seconds.

Usage: uv run --with pyserial python -I scripts/serial_capture.py PORT [SECONDS]
"""
import sys, time, serial

port, secs = sys.argv[1], float(sys.argv[2]) if len(sys.argv) > 2 else 30
s = serial.Serial(port, 115200, timeout=0.2, dsrdtr=False, rtscts=False)
s.dtr = False; s.rts = True; time.sleep(0.1); s.rts = False   # EN pulse = reset
end = time.time() + secs
while time.time() < end:
    data = s.read(4096)
    if data:
        sys.stdout.write(data.decode(errors="replace")); sys.stdout.flush()

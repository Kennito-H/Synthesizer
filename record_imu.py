import argparse
import re
import sys
import time
import serial
import serial.tools.list_ports

BAUD_RATE = 115200

# Regex that matches both raw ESP_LOGI lines and plain printed lines
LINE_RE = re.compile(
    r"AX:(?P<ax>[-\d.]+)\s+AY:(?P<ay>[-\d.]+)\s+AZ:(?P<az>[-\d.]+)"
    r"\s*\|\s*"
    r"GX:(?P<gx>[-\d.]+)\s+GY:(?P<gy>[-\d.]+)\s+GZ:(?P<gz>[-\d.]+)"
    r"\s*\|\s*"
    r"T:(?P<t>[-\d.]+)"
)

def find_port() -> str:
    """Auto-detect the first USB-serial port."""
    ports = serial.tools.list_ports.comports()
    usb = [p for p in ports if "usb" in p.device.lower() or "usbserial" in p.device.lower()]
    if usb:
        return usb[0].device
    if ports:
        return ports[0].device
    print("[ERROR] No serial ports found. Plug in your ESP32 or specify --port.", file=sys.stderr)
    sys.exit(1)

def parse_line(line: str):
    """Return (ax, ay, az, gx, gy, gz, temp) floats or None."""
    m = LINE_RE.search(line)
    if m:
        return tuple(float(m.group(k)) for k in ("ax", "ay", "az", "gx", "gy", "gz", "t"))
    return None

def main():
    parser = argparse.ArgumentParser(description="Record IMU data to a file with labels for gestures.")
    parser.add_argument("--port", default=None, help="Serial port (auto-detected if omitted)")
    parser.add_argument("--baud", default=BAUD_RATE, type=int, help=f"Baud rate (default {BAUD_RATE})")
    parser.add_argument("--label", required=True, help="Label for the gesture (e.g., UP, DOWN, LEFT, RIGHT)")
    parser.add_argument("--output", required=True, help="Output file name (e.g., up.txt)")
    parser.add_argument("--duration", type=float, default=None, help="Duration to record in seconds. If not set, records until Ctrl+C.")
    args = parser.parse_args()

    port = args.port or find_port()
    baud = args.baud

    valid_samples = 0
    start_time = time.time()
    
    try:
        with serial.Serial(port, baud, timeout=1) as ser, open(args.output, "w") as f_out:
            # Write header so data is easily labeled later
            f_out.write("timestamp,label,ax,ay,az,gx,gy,gz,temp\n")
            
            while True:
                if args.duration and (time.time() - start_time) > args.duration:
                    break
                
                raw = ser.readline()
                try:
                    line = raw.decode("utf-8", errors="replace").strip()
                except Exception:
                    continue
                
                parsed = parse_line(line)
                if parsed is None:
                    continue
                
                ax, ay, az, gx, gy, gz, temp = parsed
                current_time = time.time() - start_time
                
                # Write a row with the timestamp, the specified label, and the IMU values
                f_out.write(f"{current_time:.3f},{args.label},{ax},{ay},{az},{gx},{gy},{gz},{temp}\n")
                f_out.flush()
                valid_samples += 1
                
                # Print progress occasionally
                if valid_samples % 50 == 0:
                    print(f"\r[INFO] Recorded {valid_samples} samples...", end="", flush=True)

    except KeyboardInterrupt:
        print("\n[INFO] Recording stopped by user.")
    except serial.SerialException as e:
        print(f"\n[ERROR] Serial port error: {e}", file=sys.stderr)
    except Exception as e:
        print(f"\n[ERROR] {e}", file=sys.stderr)
    
    print(f"\n[INFO] Complete. Wrote {valid_samples} samples to {args.output}")

if __name__ == "__main__":
    main()

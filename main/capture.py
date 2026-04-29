import serial
import time

ser = serial.Serial('/dev/cu.usbserial-110', 115200)

for i in range(21): 
    filename = f'./data/up_files/up_{i:02d}.txt'
    print(f"Recording {filename}... press Enter to start")
    input()  

    with open(filename, 'w') as f:
        t_start = time.time()
        while time.time() - t_start < 4.0:
            line = ser.readline().decode('utf-8').strip()
            if line and line[0].isdigit():
                parts = line.split()
                data = parts[1:]  
                elapsed = str(round(time.time() - t_start, 6))
                new_line = elapsed + ' ' + ' '.join(data)
                print(new_line)
                f.write(new_line + '\n')

    print(f"Done! Saved {filename}")

print("All files collected!")
ser.close()
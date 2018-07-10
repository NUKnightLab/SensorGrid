import serial
import time

ser = serial.Serial('/dev/cu.usbmodem1411', 115200)
#ser = serial.Serial('/dev/cu.usbmodem1411', 9600)

ts = time.time()
ts = int(ts)
ts = str(ts)
ts = 't' + ts

ser.write(ts.encode())

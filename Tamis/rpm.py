#!/usr/bin/python3
import RPi.GPIO as GPIO
import sys
import time



#simulation rpm utilisant la pin 21 avec un adaptateur 3.3v 5V
#et  le arduino gpio pin2 pour le transistor a effet de HALL.

pin = 21
# set GPIO for RPM
GPIO.setmode(GPIO.BCM)
GPIO.setup(pin,GPIO.OUT)

nombre_aimant = 6

rpm = float(sys.argv[1])
pulse_ON=0.1
pulse_OFF=  ((60.0 / rpm) / nombre_aimant) - pulse_ON


print("argument size ",len(sys.argv))
print("rpm = {:2.1f}".format(float(sys.argv[1])))
print("pulse ON={:2.1f} OFF={:2.1f}".format(pulse_ON,pulse_OFF))
try:
    while(True):

      GPIO.output(pin,False)
      print(".",end="")
      sys.stdout.flush()
      time.sleep(pulse_ON)
      GPIO.output(pin,True)
      time.sleep(pulse_OFF)

except KeyboardInterrupt:
  pass


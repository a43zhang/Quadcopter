Quadcopter
==========
Uses 2 Arduino Unos due to timer complications.
Arduino 1 runs pitchsend, reading sensor values and controlling motors 1 and 3 (pitch)
Arduino 2 runs rollreceive, receiving one value from Arduino 1 that commands it to control motors 2 and 4 (roll)

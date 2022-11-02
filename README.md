# CD_Semtech_Demo
Connected Development sample demos of Semtech LoRa devices.

1. PingPong -  This is a point-to-point example.  It runs on two Nordic nRF52840DK boards with Connected Development SX1262 LoRa shields.  One board sends out "PING" data packets and the other board responds with "PONG" packets.  Each side will display a log in the terminal window for each LoRa packet that is sent or received, including a packet counter.  Each receive log includes the packet counter, the signal strength (RSSI), and Signal-to-Noise Ratio (SNR).  LED1 will toggle each time a packet is received, and LED2 will toggle each time a packet is sent.

2. lorawan - This is a LoRaWAN Class A example derived from the sample in the Nordic nRF Connect SDK at zephyr\samples\subsys\lorawan.  It runs on a Nordic nRF52840DK boards with a Connected Development SX1262 LoRa shield.  The example application performs an OTAA Join, then sends out "helloworldxxxx" data packets once every 5 seconds.  The "xxxx" data is an incrementing counter so each packet is different and can be tracked.

#Wiring the NRF	24L01+ radio

|NRF24l01+|Rpi Header Pin|
|---|---|
|GND|25|
|VCC|17|
|CE|22|
|CSN|24|
|SCK|23|
|MOSI|19|
|MISO|21|
|IRQ|--|

#Building & Installing

##RF24 library
* Download the library from https://github.com/TMRh20/RF24
 * Either an official release(tested with 1.1.3) or clone the master branch.
* Decompress(if needed) and change to the library directory
* Run `make all` followed by `sudo make install`

##TCP Gateway

The standard configuration will build the TCP Gateway with a port 0.0.0.0:5003
The default install location will be /usr/local/sbin. If you want to change
that edit the variables in the head of the Makefile.

###Build the Gateway
* Clone this repository
* Change to the Raspberry directory
* Run `make all` followed by `sudo make install`
* (if you want to start daemon at boot) sudo make enable-gwtcp


#Uninstalling

* Change to Raspberry directory
* Run `sudo make uninstall`

Support: http://forum.mysensors.org

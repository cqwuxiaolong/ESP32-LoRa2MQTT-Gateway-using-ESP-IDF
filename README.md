# ESP32-LoRa2MQTT-Gateway-using-ESP-IDF
In this project I use the esp-idf along with a LoRa esp-idf component library that was originally ported from Sandeep Mistry's arduino LoRa Library.  The link to the component library is [here](https://github.com/Inteform/esp32-lora-library).

I also use the ssd1306 component library from (here)[https://github.com/TaraHoleInIt/tarablessd1306].

To set up, you need to copy the "lora" folder and the "ssd1306" folder to the components folder in the esp-idf directory.  then copy the LoRa2MQTT folder to "examples/protocols/mqtt/ folder.

then do the install.bat and export.bat scripts to ensure the "lora" and "ssd1306" component libraries have been incorporated in to the system.

Then open the esp-idf command prompt and navigate to the the project folder "$ESP-IDF_PATH/examples/protococols/mqtt/LoRaMQTT".

Then run idf.py menuconfig if you want to make any changes, but otherwise do idf.py -p [COM7] flash monitor.


| Supported Targets | ESP32 |
| ----------------- | ----- |

# ESP32 I2S 8 Channel Sampler

* Controlled over a web page. 
* Samples data from 8 channels.
* Data is displayed as graphs on the page.

## Project Status
Currently not working properly. The data received is corrupt.

## How to Use esp32sampler

### Hardware Required

* A development board with ESP32 SoC (e.g., ESP32-DevKitC, ESP-WROVER-KIT, etc.)
* A USB cable for power supply and programming
* A function generator to create the signal going into ADC1 pins.

The following is the hardware connection:

|AD Channel|GPIO|
|:---:|:---:|
|ADC1_CH0|GPIO36|
|ADC1_CH3|GPIO39|
|ADC1_CH4|GPIO32|
|ADC1_CH5|GPIO33|
|ADC1_CH6|GPIO34|
|ADC1_CH7|GPIO35|

### Configure the Project

```
idf.py menuconfig
```

* Set the flash size to 4 MB under Serial Flasher Options.
* Select "Custom partition table CSV" and rename "Custom partition CSV file" to "partitions_esp32sampler.csv".

(Note that you can use `sdkconfig.defaults`)

### Build and Flash

Build the project and flash it to the board, then run monitor tool to view serial output:

```
idf.py -p PORT flash monitor
```

(To exit the serial monitor, type ``Ctrl-]``.)

## Example Output
1. Open webapp.html in browser
2. Press "Set IP" and put in the IP like this: "<ipaddress of development board>:80/ws"
3. Press button "Sample Frequency (Hz)".
4. Press button "Sample Time (us)".
5. Press button "Request Data"

You should see some data from all 8 channels on the page.


## Troubleshooting

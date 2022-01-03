| Supported Targets | ESP32 |
| ----------------- | ----- |

# ESP32 I2S 8 Channel Sampler

* Controlled over a web page. 
* Samples data from 8 channels.
* Data is displayed as graphs on the page.

## How to Use Example

### Hardware Required

* A development board with ESP32 SoC (e.g., ESP32-DevKitC, ESP-WROVER-KIT, etc.)
* A USB cable for power supply and programming
* A function generator to create the signal going into ADC1 pins.

The following is the hardware connection:

|AD Channel|GPIO|
|:---:|:---:|:---:|
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

## Troubleshooting

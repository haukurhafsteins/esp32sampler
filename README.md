| Supported Targets | ESP32 |
| ----------------- | ----- |

# ESP32 I2S 8 Channel Sampler

* Controlled over a web page. 
* Samples data from 8 channels.
* Data is displayed as graphs on the page.

![image](https://user-images.githubusercontent.com/8937068/149617393-996aee96-2b04-4ed6-9995-2302ae7776a3.png)

## Project Status
### Register settings
The register settings are not set right for all sampling frequencies to work. This leads to drop in samples, mostly on channel 6 and 7 (well visible on the 
image above) and the sample frequency only working for a certain range, starts around 21500Hz up to 35000Hz. Using the following register settings (for 1000000Hz) work well
(these settings are taken from this thread: https://github.com/espressif/esp-idf/pull/1991).

    // ***IMPORTANT*** enable continuous adc sampling
    SYSCON.saradc_ctrl2.meas_num_limit = 0;

    // ADC setting
    SYSCON.saradc_sar1_patt_tab[0] = ((ADC1_CHANNEL_0 << 4) | (ADC_WIDTH_BIT_12 << 2) | ADC_ATTEN_DB_11) << 24;
    SYSCON.saradc_ctrl.sar1_patt_len = 0;

    // reduce sample time for 2Msps
    SYSCON.saradc_ctrl.sar_clk_div = 2;
    SYSCON.saradc_fsm.sample_cycle = 2;

    // sampling rate 2Msps setting
    I2S0.clkm_conf.clkm_div_num = 20;
    I2S0.clkm_conf.clkm_div_b = 0;
    I2S0.clkm_conf.clkm_div_a = 1;
    I2S0.sample_rate_conf.rx_bck_div_num = 2;

### Max nr samples
You can sample around 2200 samples max. Could probably go higher with some memory adjustments.

### Number of channels
Currently 8 channels are sampled but only 6 are shown. Channel 1 and 2 are of no use as they pins are not available. This also needs to be changed for 
the SYSCON_SARADC_SAR1_PATT_TAB1_REG and SYSCON_SARADC_SAR1_PATT_TAB2_REG registers.

## How to Use esp32sampler
See Example Output below.

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

## Example Output
1. Open webapp.html in browser
2. Press "Set IP" and put in the IP like this: "xxx.xxx.xxx.xxx:80/ws"
3. Adjust sample frequency and press button "Sample Frequency (Hz)".
4. Adjust sample time and press button "Sample Time (us)".
5. Press button "Request Data"

You should see some data from all 8 channels on the page.


## Troubleshooting

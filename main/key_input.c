#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_adc/adc_oneshot.h"

#define FSR1_CHANNEL ADC_CHANNEL_0 
#define FSR2_CHANNEL ADC_CHANNEL_1  
#define FSR3_CHANNEL ADC_CHANNEL_2  
#define FSR4_CHANNEL ADC_CHANNEL_3  
#define FSR5_CHANNEL ADC_CHANNEL_4  

void app_main(void)
{
    // Configure ADC
    adc_oneshot_unit_handle_t adc_handle;
    adc_oneshot_unit_init_cfg_t init_config = {
        .unit_id = ADC_UNIT_1,
    };
    adc_oneshot_new_unit(&init_config, &adc_handle);

    adc_oneshot_chan_cfg_t chan_config = {
        .atten = ADC_ATTEN_DB_12,   // 0-3.3V range
        .bitwidth = ADC_BITWIDTH_12, // 0-4095 range
    };
    
    adc_oneshot_config_channel(adc_handle, FSR1_CHANNEL, &chan_config);
    adc_oneshot_config_channel(adc_handle, FSR2_CHANNEL, &chan_config);
    adc_oneshot_config_channel(adc_handle, FSR3_CHANNEL, &chan_config);
    adc_oneshot_config_channel(adc_handle, FSR4_CHANNEL, &chan_config);
    adc_oneshot_config_channel(adc_handle, FSR5_CHANNEL, &chan_config);

    int fsr_values[5] = {0};


    while (1) {
        adc_oneshot_read(adc_handle, FSR1_CHANNEL, &fsr_values[0]);
        adc_oneshot_read(adc_handle, FSR2_CHANNEL, &fsr_values[1]);
        adc_oneshot_read(adc_handle, FSR3_CHANNEL, &fsr_values[2]);
        adc_oneshot_read(adc_handle, FSR4_CHANNEL, &fsr_values[3]);
        adc_oneshot_read(adc_handle, FSR5_CHANNEL, &fsr_values[4]);

        printf("FSR: 0: %d 1: %d 2: %d 3: %d 4: %d\n",
            fsr_values[0], fsr_values[1], fsr_values[2],
            fsr_values[3], fsr_values[4]);

        vTaskDelay(pdMS_TO_TICKS(200));
    }
}

#include<stdio.h>
#include"driver/rmt_tx.h"

#define RTM_RESOLUTION_HZ 10000000 //10MHz
#define N_LEDS 4
#define BITS_PER_LED 24
#define SYMBOLS_PER_LED BITS_PER_LED
#define TOTAL_SYMBOLS (N_LEDS*SYMBOLS_PER_LED)

rmt_tx_channel_config_t tx_chan_config = {
    .clk_src = RMT_CLK_SRC_DEFAULT,
    .gpio_num = GPIO_NUM_7,
    .mem_block_symbols = 64,
    .resolution_hz = RTM_RESOLUTION_HZ,
    .trans_queue_depth = 4,
};
rmt_channel_handle_t tx_channel = NULL;
rmt_encoder_handle_t encoder = NULL;

static uint8_t pixel_buffer[N_LEDS*3];
static rmt_symbol_word_t symbol_buffer[TOTAL_SYMBOLS];

rmt_symbol_word_t led_strip_bit(bool bit){
    rmt_symbol_word_t symbol;
    if(bit){
        symbol.level0 = 1; symbol.duration0 = 7;
        symbol.level1 = 0; symbol.duration1 =6;
    }
    else{
       symbol.level0 = 1; symbol.duration0 = 3;
        symbol.level1 = 0; symbol.duration1 =9; 
    }
    return symbol;
}
void led_encode_pixel(uint8_t r,uint8_t g,uint8_t b, rmt_symbol_word_t * target_buffer ){
    uint8_t color_data[3] = {g,r,b};
    int symbol_index= 0;
    for(int i = 0; i<3; i++){
        uint8_t byte = color_data[i];
        for(int bit = 7; bit>=0; bit--){
            bool bit_value = (byte>>bit)&0x01;
            target_buffer[symbol_index++] = led_strip_bit(bit_value);
        }
    }
}

void led_strip_encode_all_pixels(){
    for (int i = 0; i < N_LEDS; i++)
    {
        uint8_t g = pixel_buffer[i*3];
        uint8_t r = pixel_buffer[i*3+1];
        uint8_t b = pixel_buffer[i*3+2];
        rmt_symbol_word_t *target_buffer = &symbol_buffer[i*SYMBOLS_PER_LED];
        led_encode_pixel(r,g,b,target_buffer);
    }
    
}

void led_strip_init(){
    if((rmt_new_tx_channel(&tx_chan_config,&tx_channel))!= ESP_OK){
        return;
    }
    rmt_copy_encoder_config_t encoder_config;
    if(rmt_new_copy_encoder(&encoder_config, &encoder)!=ESP_OK){
        return;
    }
    if(rmt_enable(tx_channel)!=ESP_OK){
        return;
    }

}

void led_strip_set_pixel(uint32_t index, uint8_t r, uint8_t g, uint8_t b){
    if(index >= N_LEDS) return;
    pixel_buffer[index*3] = g;
    pixel_buffer[index*3+1] = r;
    pixel_buffer[index*3+2] = b;
}


void led_strip_refresh(){
    led_strip_encode_all_pixels();
    rmt_transmit_config_t tx_cfg = {
        .loop_count =0
    };
    if(rmt_transmit(tx_channel, encoder, symbol_buffer, sizeof(symbol_buffer),&tx_cfg) !=ESP_OK){
        return;
    }
    //rmt_tx_wait_all_done(tx_channel,10);
}

void led_strip_set_all_colors(uint8_t r, uint8_t g, uint8_t b){
    for(int i = 0; i< N_LEDS; i++){
        led_strip_set_pixel(i,r,g,b);
    }
    led_strip_refresh();
}
void led_strip_batt_init(){
    for(int i = 0; i< N_LEDS; i++){
        led_strip_set_pixel(i,10,100,10);
    }
    led_strip_refresh();
}
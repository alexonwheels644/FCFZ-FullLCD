#include "u8g2_glue.h"
#include <applications/services/notification/notification_app.h>
#include <toolbox/saved_struct.h>
#include <notification/notification_settings_filename.h>

#include <furi.h>
#include <storage/storage.h>
#include <furi_hal.h>
#include <stm32wbxx_ll_gpio.h>

#define CONTRAST_ERC 32
#define CONTRAST_MGG 28

/* Адрес дисплея ST7567 */
#define ST7567_I2C_ADDRESS 0x3F

volatile bool display_needs_reinit = false;
volatile bool display_is_sleeping = false;

/* Фейковый ШИМ-счетчик для плавной регулировки яркости */
static uint8_t pwm_counter = 0;

uint8_t u8g2_gpio_and_delay_stm32(u8x8_t* u8x8, uint8_t msg, uint8_t arg_int, void* arg_ptr) {
    UNUSED(u8x8);
    UNUSED(arg_ptr);
    switch(msg) {
    case U8X8_MSG_GPIO_AND_DELAY_INIT:
        break;
    case U8X8_MSG_DELAY_MILLI:
        furi_delay_ms(arg_int);
        break;
    case U8X8_MSG_DELAY_10MICRO:
        furi_delay_us(10);
        break;
    case U8X8_MSG_DELAY_100NANO:
        __NOP();
        break;
    case U8X8_MSG_GPIO_RESET:
        break;
    case U8X8_MSG_GPIO_I2C_CLOCK:
        if(arg_int) {
            furi_hal_gpio_init_simple(&gpio_i2c_1_scl, GpioModeInput);
        } else {
            furi_hal_gpio_init_simple(&gpio_i2c_1_scl, GpioModeOutputOpenDrain);
            furi_hal_gpio_write(&gpio_i2c_1_scl, false);
        }
        break;
    case U8X8_MSG_GPIO_I2C_DATA:
        if(arg_int) {
            furi_hal_gpio_init_simple(&gpio_i2c_1_sda, GpioModeInput);
        } else {
            furi_hal_gpio_init_simple(&gpio_i2c_1_sda, GpioModeOutputOpenDrain);
            furi_hal_gpio_write(&gpio_i2c_1_sda, false);
        }
        break;
    default:
        return 0;
    }
    return 1;
}

uint8_t u8x8_byte_hw_i2c_stm32(u8x8_t* u8x8, uint8_t msg, uint8_t arg_int, void* arg_ptr) {
    /* ИСПРАВЛЕНО: Сделали нормальный массив на 128 байт, компилятор пропустит */
    static uint8_t buffer[128];  
    static uint16_t buf_idx = 0;
    
    switch(msg) {
    case U8X8_MSG_BYTE_SEND: {
        uint8_t* data = (uint8_t*)arg_ptr;
        for(uint8_t i = 0; i < arg_int; i++) {
            if(buf_idx < sizeof(buffer)) {
                buffer[buf_idx++] = data[i];
            } else {
                display_needs_reinit = true;
                break;
            }
        }
        break;
    }
    case U8X8_MSG_BYTE_INIT:
        break;
    case U8X8_MSG_BYTE_SET_DC:
        break;
    case U8X8_MSG_BYTE_START_TRANSFER:
        buf_idx = 0;
        furi_hal_i2c_acquire(&furi_hal_i2c_handle_power);
        break;
    case U8X8_MSG_BYTE_END_TRANSFER:
        if(buf_idx > 0) {
            bool success = false;
            for(int retry = 0; retry < 3; retry++) {
                success = furi_hal_i2c_tx(
                    &furi_hal_i2c_handle_power,
                    u8x8_GetI2CAddress(u8x8),
                    buffer,
                    buf_idx,
                    100);
                if(success) {
                    break;
                }
                furi_delay_ms(1);
            }
            if(!success) {
                furi_hal_i2c_release(&furi_hal_i2c_handle_power);
                return 0;
            }
        }
        furi_hal_i2c_release(&furi_hal_i2c_handle_power);
        buf_idx = 0;
        break;
    default:
        return 0;
    }
    return 1;
}

static const uint8_t u8x8_d_st7567_powersave0_seq[] = {
    U8X8_START_TRANSFER(), 
    U8X8_C(0xA4), 
    U8X8_C(0xAF), 
    U8X8_END_TRANSFER(), 
    U8X8_END() 
};

static const uint8_t u8x8_d_st7567_powersave1_seq[] = {
    U8X8_START_TRANSFER(), 
    U8X8_C(0xAE), 
    U8X8_END_TRANSFER(), 
    U8X8_END() 
};

static const uint8_t u8x8_d_st7567_flip0_seq[] = {
    U8X8_START_TRANSFER(), 
    U8X8_C(0xA0), 
    U8X8_C(0xC0), 
    U8X8_END_TRANSFER(), 
    U8X8_END() 
};

static const uint8_t u8x8_d_st7567_flip1_seq[] = {
    U8X8_START_TRANSFER(), 
    U8X8_C(0xA1), 
    U8X8_C(0xC8), 
    U8X8_END_TRANSFER(), 
    U8X8_END() 
};

static const u8x8_display_info_t u8x8_st7567_128x64_display_info = {
    .chip_enable_level = 0,
    .chip_disable_level = 1,
    .post_chip_enable_wait_ns = 150, 
    .pre_chip_disable_wait_ns = 50, 
    .reset_pulse_width_ms = 5,
    .post_reset_wait_ms = 5,
    .sda_setup_time_ns = 100, 
    .sck_pulse_width_ns = 120, 
    .sck_clock_hz = 400000UL, 
    .spi_mode = 0, 
    .i2c_bus_clock_100kHz = 4,
    .data_setup_time_ns = 40, 
    .write_pulse_width_ns = 80, 
    .tile_width = 16, 
    .tile_height = 8,
    .default_x_offset = 4,
    .flipmode_x_offset = 4,
    .pixel_width = 128,
    .pixel_height = 64
};

uint8_t u8x8_d_ssd1306_common(u8x8_t* u8x8, uint8_t msg, uint8_t arg_int, void* arg_ptr) {
    uint8_t x, c;
    uint8_t* ptr;

    switch(msg) {
    case U8X8_MSG_DISPLAY_DRAW_TILE:
        u8x8_cad_StartTransfer(u8x8);

        x = ((u8x8_tile_t*)arg_ptr)->x_pos;
        x *= 8;
        x += u8x8->x_offset;
        
        u8x8_cad_SendCmd(u8x8, 0x010 | (x >> 4));
        u8x8_cad_SendCmd(u8x8, 0x000 | (x & 15));
        u8x8_cad_SendCmd(u8x8, 0x0b0 | (((u8x8_tile_t*)arg_ptr)->y_pos));

        c = ((u8x8_tile_t*)arg_ptr)->cnt;
        c *= 8;
        ptr = ((u8x8_tile_t*)arg_ptr)->tile_ptr;

        if(c + x > 132u) {
            c = 132u;
            c -= x;
        }

        do {
            u8x8_cad_SendData(u8x8, c, ptr); 
            arg_int--;
        } while(arg_int > 0);

        u8x8_cad_EndTransfer(u8x8);

        /* Генерируем программный ШИМ на пине PB0 прямо во время отправки графики */
        LL_GPIO_SetPinMode(GPIOB, LL_GPIO_PIN_0, LL_GPIO_MODE_OUTPUT);
        if(display_is_sleeping) {
            LL_GPIO_ResetOutputPin(GPIOB, LL_GPIO_PIN_0); /* Ноль вольт во время сна */
        } else {
            /* Считываем текущее положение ползунка бэклайта из системных настроек Флиппера */
            if(furi_record_exists(RECORD_NOTIFICATION)) {
                NotificationApp* notif = furi_record_open(RECORD_NOTIFICATION);
                uint8_t target_brightness = (uint8_t)(notif->settings.display_brightness * 4.0f); 
                furi_record_close(RECORD_NOTIFICATION);

                pwm_counter = (pwm_counter + 1) % 4;
                if(pwm_counter < target_brightness) {
                    LL_GPIO_SetOutputPin(GPIOB, LL_GPIO_PIN_0); /* 3.3В */
                } else {
                    LL_GPIO_ResetOutputPin(GPIOB, LL_GPIO_PIN_0); /* 0В */
                }
            } else {
                LL_GPIO_SetOutputPin(GPIOB, LL_GPIO_PIN_0); /* Дефолт 100% если сервис занят */
            }
        }
        break;

    case U8X8_MSG_DISPLAY_SET_POWER_SAVE:
        LL_GPIO_SetPinMode(GPIOB, LL_GPIO_PIN_0, LL_GPIO_MODE_OUTPUT);
        if(arg_int == 0) {
            display_is_sleeping = false;
            LL_GPIO_SetOutputPin(GPIOB, LL_GPIO_PIN_0); 
            u8x8_cad_SendSequence(u8x8, u8x8_d_st7567_powersave0_seq);
        } else {
            display_is_sleeping = true;
            LL_GPIO_ResetOutputPin(GPIOB, LL_GPIO_PIN_0); 
            u8x8_cad_SendSequence(u8x8, u8x8_d_st7567_powersave1_seq);
        }
        break;
#ifdef U8X8_WITH_SET_CONTRAST
    case U8X8_MSG_DISPLAY_SET_CONTRAST:
        u8x8_cad_StartTransfer(u8x8);
        u8x8_cad_SendCmd(u8x8, 0x081);
        u8x8_cad_SendArg(u8x8, arg_int >> 2); 
        u8x8_cad_EndTransfer(u8x8);
        break;
#endif
    default:
        return 0;
    }
    return 1;
}

void u8x8_d_ssd1306_init(u8x8_t* u8x8, uint8_t contrast, uint8_t regulation_ratio, bool bias) {
    UNUSED(contrast);
    UNUSED(regulation_ratio);
    UNUSED(bias);

    u8x8_cad_StartTransfer(u8x8);
    
    u8x8_cad_SendCmd(u8x8, 0xE2); 
    u8x8_cad_SendCmd(u8x8, 0xAE); 
    
    u8x8_cad_SendCmd(u8x8, 0xA0); 
    u8x8_cad_SendCmd(u8x8, 0xC0); 
    
    u8x8_cad_SendCmd(u8x8, 0xA6); 
    u8x8_cad_SendCmd(u8x8, 0xA4); 
    u8x8_cad_SendCmd(u8x8, 0xA3); 
    
    u8x8_cad_SendCmd(u8x8, 0x2F); 
    u8x8_cad_SendCmd(u8x8, 0x20); 
    
    u8x8_cad_SendCmd(u8x8, 0x081); 
    u8x8_cad_SendCmd(u8x8, 0x1A);  
    
    u8x8_cad_SendCmd(u8x8, 0x040); 
    u8x8_cad_SendCmd(u8x8, 0xAF);  

    u8x8_cad_EndTransfer(u8x8);
}

void u8x8_d_ssd1306_set_contrast(u8x8_t* u8x8, int32_t contrast_val) {
    u8x8_cad_StartTransfer(u8x8);
    u8x8_cad_SendCmd(u8x8, 0x081);
    u8x8_cad_SendArg(u8x8, (uint8_t)(contrast_val >> 2));
    u8x8_cad_EndTransfer(u8x8);
}

void u8x8_d_ssd1306_set_invert(u8x8_t* u8x8, bool invert) {
    u8x8_cad_StartTransfer(u8x8);
    u8x8_cad_SendCmd(u8x8, invert ? 0xA7 : 0xA6);
    u8x8_cad_EndTransfer(u8x8);
}

uint8_t u8x8_d_ssd1306_flipper(u8x8_t* u8x8, uint8_t msg, uint8_t arg_int, void* arg_ptr) {
    if(u8x8_d_ssd1306_common(u8x8, msg, arg_int, arg_ptr) == 0) {
        switch(msg) {
        case U8X8_MSG_DISPLAY_SETUP_MEMORY:
            u8x8_d_helper_display_setup_memory(u8x8, &u8x8_st7567_128x64_display_info);
            break;
        case U8X8_MSG_DISPLAY_INIT:
            u8x8_d_helper_display_init(u8x8);
            u8x8_d_ssd1306_init(u8x8, 0, 0, false);
            break;
        case U8X8_MSG_DISPLAY_SET_FLIP_MODE:
            if(arg_int == 0) {
                u8x8_cad_SendSequence(u8x8, u8x8_d_st7567_flip0_seq);
                u8x8->x_offset = u8x8->display_info->default_x_offset;
            } else {
                u8x8_cad_SendSequence(u8x8, u8x8_d_st7567_flip1_seq);
                u8x8->x_offset = u8x8->display_info->flipmode_x_offset;
            }
            break;
        default:
            return 0;
        }
    }
    return 1;
}

void u8g2_Setup_ssd1306_flipper(
    u8g2_t* u8g2,
    const u8g2_cb_t* rotation,
    u8x8_msg_cb byte_cb,
    u8x8_msg_cb gpio_and_delay_cb) {
    uint8_t tile_buf_height;
    uint8_t* buf;
    UNUSED(byte_cb);

    u8g2_SetupDisplay(
        u8g2,
        u8x8_d_ssd1306_128x64_noname,
        u8x8_cad_ssd13xx_fast_i2c,
        u8x8_byte_hw_i2c_stm32,
        gpio_and_delay_cb);
        
    u8x8_SetI2CAddress(&u8g2->u8x8, ST7567_I2C_ADDRESS << 1);

    buf = u8g2_m_16_8_f(&tile_buf_height);
    u8g2_SetupBuffer(u8g2, buf, tile_buf_height, u8g2_ll_hvline_vertical_top_lsb, rotation);
}
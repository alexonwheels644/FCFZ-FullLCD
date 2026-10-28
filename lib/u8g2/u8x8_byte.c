#include "u8x8.h"

uint8_t u8x8_byte_SetDC(u8x8_t* u8x8, uint8_t dc) {
    return u8x8->byte_cb(u8x8, U8X8_MSG_BYTE_SET_DC, dc, NULL);
}

uint8_t u8x8_byte_SendBytes(u8x8_t* u8x8, uint8_t cnt, uint8_t* data) {
    return u8x8->byte_cb(u8x8, U8X8_MSG_BYTE_SEND, cnt, (void*)data);
}

uint8_t u8x8_byte_SendByte(u8x8_t* u8x8, uint8_t byte) {
    return u8x8_byte_SendBytes(u8x8, 1, &byte);
}

uint8_t u8x8_byte_StartTransfer(u8x8_t* u8x8) {
    return u8x8->byte_cb(u8x8, U8X8_MSG_BYTE_START_TRANSFER, 0, NULL);
}

uint8_t u8x8_byte_EndTransfer(u8x8_t* u8x8) {
    return u8x8->byte_cb(u8x8, U8X8_MSG_BYTE_END_TRANSFER, 0, NULL);
}

uint8_t u8x8_byte_empty(
    U8X8_UNUSED u8x8_t* u8x8,
    uint8_t msg,
    U8X8_UNUSED uint8_t arg_int,
    U8X8_UNUSED void* arg_ptr) {
    switch(msg) {
    case U8X8_MSG_BYTE_SEND:
    case U8X8_MSG_BYTE_INIT:
    case U8X8_MSG_BYTE_SET_DC:
    case U8X8_MSG_BYTE_START_TRANSFER:
    case U8X8_MSG_BYTE_END_TRANSFER:
        break; 
    }
    return 1; 
}

uint8_t u8x8_byte_4wire_sw_spi(u8x8_t* u8x8, uint8_t msg, uint8_t arg_int, void* arg_ptr) {
    uint8_t i, b;
    uint8_t* data;
    uint8_t takeover_edge = u8x8_GetSPIClockPhase(u8x8);
    uint8_t not_takeover_edge = 1 - takeover_edge;

    switch(msg) {
    case U8X8_MSG_BYTE_SEND:
        data = (uint8_t*)arg_ptr;
        while(arg_int > 0) {
            b = *data;
            data++;
            arg_int--;
            for(i = 0; i < 8; i++) {
                if(b & 128)
                    u8x8_gpio_SetSPIData(u8x8, 1);
                else
                    u8x8_gpio_SetSPIData(u8x8, 0);
                b <<= 1;

                u8x8_gpio_SetSPIClock(u8x8, not_takeover_edge);
                u8x8_gpio_Delay(u8x8, U8X8_MSG_DELAY_NANO, u8x8->display_info->sda_setup_time_ns);
                u8x8_gpio_SetSPIClock(u8x8, takeover_edge);
                u8x8_gpio_Delay(u8x8, U8X8_MSG_DELAY_NANO, u8x8->display_info->sck_pulse_width_ns);
            }
        }
        break;

    case U8X8_MSG_BYTE_INIT:
        u8x8_gpio_SetCS(u8x8, u8x8->display_info->chip_disable_level);
        u8x8_gpio_SetSPIClock(u8x8, u8x8_GetSPIClockPhase(u8x8));
        break;
    case U8X8_MSG_BYTE_SET_DC:
        u8x8_gpio_SetDC(u8x8, arg_int);
        break;
    case U8X8_MSG_BYTE_START_TRANSFER:
        u8x8_gpio_SetCS(u8x8, u8x8->display_info->chip_enable_level);
        u8x8->gpio_and_delay_cb(
            u8x8, U8X8_MSG_DELAY_NANO, u8x8->display_info->post_chip_enable_wait_ns, NULL);
        break;
    case U8X8_MSG_BYTE_END_TRANSFER:
        u8x8->gpio_and_delay_cb(
            u8x8, U8X8_MSG_DELAY_NANO, u8x8->display_info->pre_chip_disable_wait_ns, NULL);
        u8x8_gpio_SetCS(u8x8, u8x8->display_info->chip_disable_level);
        break;
    default:
        return 0;
    }
    return 1;
}

uint8_t u8x8_byte_8bit_6800mode(u8x8_t* u8x8, uint8_t msg, uint8_t arg_int, void* arg_ptr) {
    uint8_t i, b;
    uint8_t* data;

    switch(msg) {
    case U8X8_MSG_BYTE_SEND:
        data = (uint8_t*)arg_ptr;
        while(arg_int > 0) {
            b = *data;
            data++;
            arg_int--;
            for(i = U8X8_MSG_GPIO_D0; i <= U8X8_MSG_GPIO_D7; i++) {
                u8x8_gpio_call(u8x8, i, b & 1);
                b >>= 1;
            }

            u8x8_gpio_Delay(u8x8, U8X8_MSG_DELAY_NANO, u8x8->display_info->data_setup_time_ns);
            u8x8_gpio_call(u8x8, U8X8_MSG_GPIO_E, 1);
            u8x8_gpio_Delay(u8x8, U8X8_MSG_DELAY_NANO, u8x8->display_info->write_pulse_width_ns);
            u8x8_gpio_call(u8x8, U8X8_MSG_GPIO_E, 0);
        }
        break;

    case U8X8_MSG_BYTE_INIT:
        u8x8_gpio_SetCS(u8x8, u8x8->display_info->chip_disable_level);
        u8x8_gpio_call(u8x8, U8X8_MSG_GPIO_E, 0);
        break;
    case U8X8_MSG_BYTE_SET_DC:
        u8x8_gpio_SetDC(u8x8, arg_int);
        break;
    case U8X8_MSG_BYTE_START_TRANSFER:
        u8x8_gpio_SetCS(u8x8, u8x8->display_info->chip_enable_level);
        u8x8->gpio_and_delay_cb(
            u8x8, U8X8_MSG_DELAY_NANO, u8x8->display_info->post_chip_enable_wait_ns, NULL);
        break;
    case U8X8_MSG_BYTE_END_TRANSFER:
        u8x8->gpio_and_delay_cb(
            u8x8, U8X8_MSG_DELAY_NANO, u8x8->display_info->pre_chip_disable_wait_ns, NULL);
        u8x8_gpio_SetCS(u8x8, u8x8->display_info->chip_disable_level);
        break;
    default:
        return 0;
    }
    return 1;
}

uint8_t u8x8_byte_8bit_8080mode(u8x8_t* u8x8, uint8_t msg, uint8_t arg_int, void* arg_ptr) {
    uint8_t i, b;
    uint8_t* data;

    switch(msg) {
    case U8X8_MSG_BYTE_SEND:
        data = (uint8_t*)arg_ptr;
        while(arg_int > 0) {
            b = *data;
            data++;
            arg_int--;
            for(i = U8X8_MSG_GPIO_D0; i <= U8X8_MSG_GPIO_D7; i++) {
                u8x8_gpio_call(u8x8, i, b & 1);
                b >>= 1;
            }

            u8x8_gpio_Delay(u8x8, U8X8_MSG_DELAY_NANO, u8x8->display_info->data_setup_time_ns);
            u8x8_gpio_call(u8x8, U8X8_MSG_GPIO_E, 0);
            u8x8_gpio_Delay(u8x8, U8X8_MSG_DELAY_NANO, u8x8->display_info->write_pulse_width_ns);
            u8x8_gpio_call(u8x8, U8X8_MSG_GPIO_E, 1);
        }
        break;

    case U8X8_MSG_BYTE_INIT:
        u8x8_gpio_SetCS(u8x8, u8x8->display_info->chip_disable_level);
        u8x8_gpio_call(u8x8, U8X8_MSG_GPIO_E, 1);
        break;
    case U8X8_MSG_BYTE_SET_DC:
        u8x8_gpio_SetDC(u8x8, arg_int);
        break;
    case U8X8_MSG_BYTE_START_TRANSFER:
        u8x8_gpio_SetCS(u8x8, u8x8->display_info->chip_enable_level);
        u8x8->gpio_and_delay_cb(
            u8x8, U8X8_MSG_DELAY_NANO, u8x8->display_info->post_chip_enable_wait_ns, NULL);
        break;
    case U8X8_MSG_BYTE_END_TRANSFER:
        u8x8->gpio_and_delay_cb(
            u8x8, U8X8_MSG_DELAY_NANO, u8x8->display_info->pre_chip_disable_wait_ns, NULL);
        u8x8_gpio_SetCS(u8x8, u8x8->display_info->chip_disable_level);
        break;
    default:
        return 0;
    }
    return 1;
}

uint8_t u8x8_byte_3wire_sw_spi(u8x8_t* u8x8, uint8_t msg, uint8_t arg_int, void* arg_ptr) {
    uint8_t i;
    uint8_t* data;
    uint8_t takeover_edge = u8x8_GetSPIClockPhase(u8x8);
    uint8_t not_takeover_edge = 1 - takeover_edge;
    uint16_t b;
    static uint8_t last_dc;

    switch(msg) {
    case U8X8_MSG_BYTE_SEND:
        data = (uint8_t*)arg_ptr;
        while(arg_int > 0) {
            b = *data;
            if(last_dc != 0) b |= 256;
            data++;
            arg_int--;
            for(i = 0; i < 9; i++) {
                if(b & 256)
                    u8x8_gpio_SetSPIData(u8x8, 1);
                else
                    u8x8_gpio_SetSPIData(u8x8, 0);
                b <<= 1;

                u8x8_gpio_SetSPIClock(u8x8, not_takeover_edge);
                u8x8_gpio_Delay(u8x8, U8X8_MSG_DELAY_NANO, u8x8->display_info->sda_setup_time_ns);
                u8x8_gpio_SetSPIClock(u8x8, takeover_edge);
                u8x8_gpio_Delay(u8x8, U8X8_MSG_DELAY_NANO, u8x8->display_info->sck_pulse_width_ns);
            }
        }
        break;

    case U8X8_MSG_BYTE_INIT:
        u8x8_gpio_SetCS(u8x8, u8x8->display_info->chip_disable_level);
        u8x8_gpio_SetSPIClock(u8x8, u8x8_GetSPIClockPhase(u8x8));
        break;
    case U8X8_MSG_BYTE_SET_DC:
        last_dc = arg_int;
        break;
    case U8X8_MSG_BYTE_START_TRANSFER:
        u8x8_gpio_SetCS(u8x8, u8x8->display_info->chip_enable_level);
        u8x8->gpio_and_delay_cb(
            u8x8, U8X8_MSG_DELAY_NANO, u8x8->display_info->post_chip_enable_wait_ns, NULL);
        break;
    case U8X8_MSG_BYTE_END_TRANSFER:
        u8x8->gpio_and_delay_cb(
            u8x8, U8X8_MSG_DELAY_NANO, u8x8->display_info->pre_chip_disable_wait_ns, NULL);
        u8x8_gpio_SetCS(u8x8, u8x8->display_info->chip_disable_level);
        break;
    default:
        return 0;
    }
    return 1;
}

void u8x8_byte_set_ks0108_cs(u8x8_t* u8x8, uint8_t arg) {
    u8x8_gpio_SetCS(u8x8, arg & 1);
    arg = arg >> 1;
    u8x8_gpio_call(u8x8, U8X8_MSG_GPIO_CS1, arg & 1);
    arg = arg >> 1;
    u8x8_gpio_call(u8x8, U8X8_MSG_GPIO_CS2, arg & 1);
}

uint8_t u8x8_byte_ks0108(u8x8_t* u8x8, uint8_t msg, uint8_t arg_int, void* arg_ptr) {
    uint8_t i, b;
    uint8_t* data;

    switch(msg) {
    case U8X8_MSG_BYTE_SEND:
        data = (uint8_t*)arg_ptr;
        while(arg_int > 0) {
            b = *data;
            data++;
            arg_int--;
            for(i = U8X8_MSG_GPIO_D0; i <= U8X8_MSG_GPIO_D7; i++) {
                u8x8_gpio_call(u8x8, i, b & 1);
                b >>= 1;
            }
            u8x8_gpio_Delay(u8x8, U8X8_MSG_DELAY_NANO, u8x8->display_info->data_setup_time_ns);
            u8x8_gpio_call(u8x8, U8X8_MSG_GPIO_E, 1);
            u8x8_gpio_Delay(u8x8, U8X8_MSG_DELAY_NANO, u8x8->display_info->write_pulse_width_ns);
            u8x8_gpio_call(u8x8, U8X8_MSG_GPIO_E, 0);
        }
        break;
    case U8X8_MSG_BYTE_INIT:
        u8x8_gpio_SetCS(u8x8, u8x8->display_info->chip_disable_level);
        u8x8_gpio_call(u8x8, U8X8_MSG_GPIO_E, 0);
        break;
    case U8X8_MSG_BYTE_SET_DC:
        u8x8_gpio_SetDC(u8x8, arg_int);
        break;
    case U8X8_MSG_BYTE_START_TRANSFER:
        u8x8_byte_set_ks0108_cs(u8x8, arg_int);
        u8x8->gpio_and_delay_cb(
            u8x8, U8X8_MSG_DELAY_NANO, u8x8->display_info->post_chip_enable_wait_ns, NULL);
        break;
    case U8X8_MSG_BYTE_END_TRANSFER:
        u8x8->gpio_and_delay_cb(
            u8x8, U8X8_MSG_DELAY_NANO, u8x8->display_info->pre_chip_disable_wait_ns, NULL);
        u8x8_byte_set_ks0108_cs(u8x8, arg_int);
        break;
    default:
        return 0;
    }
    return 1;
}

uint8_t u8x8_byte_sed1520(u8x8_t* u8x8, uint8_t msg, uint8_t arg_int, void* arg_ptr) {
    uint8_t i, b;
    uint8_t* data;
    static uint8_t enable_pin;
    switch(msg) {
    case U8X8_MSG_BYTE_SEND:
        data = (uint8_t*)arg_ptr;
        while(arg_int > 0) {
            b = *data;
            data++;
arg_int--;
for(i = U8X8_MSG_GPIO_D0; i <= U8X8_MSG_GPIO_D7; i++) {
u8x8_gpio_call(u8x8, i, b & 1);
b >>= 1;
}
u8x8_gpio_Delay(u8x8, U8X8_MSG_DELAY_NANO, u8x8->display_info->data_setup_time_ns);
u8x8_gpio_call(u8x8, enable_pin, 1);
u8x8_gpio_Delay(u8x8, U8X8_MSG_DELAY_NANO, 200);
u8x8_gpio_Delay(u8x8, U8X8_MSG_DELAY_NANO, u8x8->display_info->write_pulse_width_ns);
u8x8_gpio_call(u8x8, enable_pin, 0);
}
break;
case U8X8_MSG_BYTE_INIT:
u8x8_gpio_SetCS(u8x8, u8x8->display_info->chip_disable_level);
u8x8_gpio_call(u8x8, U8X8_MSG_GPIO_E, 0);
u8x8_gpio_call(u8x8, U8X8_MSG_GPIO_CS, 0);
enable_pin = U8X8_MSG_GPIO_E;
break;
case U8X8_MSG_BYTE_SET_DC:
u8x8_gpio_SetDC(u8x8, arg_int);
break;
case U8X8_MSG_BYTE_START_TRANSFER:
enable_pin = U8X8_MSG_GPIO_E;
if(arg_int != 0) enable_pin = U8X8_MSG_GPIO_CS;
break;
case U8X8_MSG_BYTE_END_TRANSFER:
break;
default:
return 0;
}
return 1;
}
static void i2c_delay(u8x8_t* u8x8) U8X8_NOINLINE;
static void i2c_delay(u8x8_t* u8x8) {
u8x8_gpio_Delay(u8x8, U8X8_MSG_DELAY_I2C, u8x8->display_info->i2c_bus_clock_100kHz);
}
static void i2c_init(u8x8_t* u8x8) {
u8x8_gpio_SetI2CClock(u8x8, 1);
u8x8_gpio_SetI2CData(u8x8, 1);
i2c_delay(u8x8);
}
static void i2c_read_scl_and_delay(u8x8_t* u8x8) {
u8x8_gpio_SetI2CClock(u8x8, 1);
i2c_delay(u8x8);
}
static void i2c_clear_scl(u8x8_t* u8x8) {
u8x8_gpio_SetI2CClock(u8x8, 0);
}
static void i2c_read_sda(u8x8_t* u8x8) {
u8x8_gpio_SetI2CData(u8x8, 1);
}
static void i2c_clear_sda(u8x8_t* u8x8) {
u8x8_gpio_SetI2CData(u8x8, 0);
}
static void i2c_start(u8x8_t* u8x8) {
if(u8x8->i2c_started != 0) {
i2c_read_sda(u8x8);
i2c_delay(u8x8);
i2c_read_scl_and_delay(u8x8);
}
i2c_read_sda(u8x8);
i2c_clear_sda(u8x8);
i2c_delay(u8x8);
i2c_clear_scl(u8x8);
u8x8->i2c_started = 1;
}
static void i2c_stop(u8x8_t* u8x8) {
i2c_clear_sda(u8x8);
i2c_delay(u8x8);
i2c_read_scl_and_delay(u8x8);
i2c_read_sda(u8x8);
i2c_delay(u8x8);
u8x8->i2c_started = 0;
}
static void i2c_write_bit(u8x8_t* u8x8, uint8_t val) {
if(val)
i2c_read_sda(u8x8);
else
i2c_clear_sda(u8x8);
i2c_delay(u8x8);
i2c_read_scl_and_delay(u8x8);
i2c_clear_scl(u8x8);
}
static void i2c_read_bit(u8x8_t* u8x8) {
i2c_read_sda(u8x8);
i2c_delay(u8x8);
i2c_read_scl_and_delay(u8x8);
i2c_read_sda(u8x8);
i2c_delay(u8x8);
i2c_clear_scl(u8x8);
}
static void i2c_write_byte(u8x8_t* u8x8, uint8_t b) {
i2c_write_bit(u8x8, b & 128);
i2c_write_bit(u8x8, b & 64);
i2c_write_bit(u8x8, b & 32);
i2c_write_bit(u8x8, b & 16);
i2c_write_bit(u8x8, b & 8);
i2c_write_bit(u8x8, b & 4);
i2c_write_bit(u8x8, b & 2);
i2c_write_bit(u8x8, b & 1);
i2c_read_bit(u8x8);
}
void i2c_transfer(u8x8_t* u8x8, uint8_t adr, uint8_t cnt, uint8_t* data) {
uint8_t i;
i2c_start(u8x8);
i2c_write_byte(u8x8, adr);
for(i = 0; i < cnt; i++)
i2c_write_byte(u8x8, data[i]);
i2c_stop(u8x8);
}
uint8_t u8x8_byte_sw_i2c(u8x8_t* u8x8, uint8_t msg, uint8_t arg_int, void* arg_ptr) {
    /* УВЕЛИЧИЛИ БУФЕР: Дисплеи типа ST7567 за раз могут выбрасывать длинные цепочки инициализации */
    static uint8_t buffer[128]; 
    static uint8_t buf_idx = 0;

    switch(msg) {
    case U8X8_MSG_BYTE_SEND: {
        uint8_t* data = (uint8_t*)arg_ptr;
        while(arg_int > 0 && buf_idx < 128) {
            buffer[buf_idx++] = *data;
            data++;
            arg_int--;
        }
        break;
    }
    case U8X8_MSG_BYTE_INIT:
        i2c_init(u8x8); 
        break;
    case U8X8_MSG_BYTE_SET_DC:
        break;
    case U8X8_MSG_BYTE_START_TRANSFER:
        buf_idx = 0;
        break;
    case U8X8_MSG_BYTE_END_TRANSFER:
        if(buf_idx > 0) {
            /* ДВОЙНОЙ УДАР: Шлём инициализацию на оба возможных физических адреса в шине, 
               чтобы один из них гарантированно зацепил контроллер дисплея */
            i2c_transfer(u8x8, 0x3F, buf_idx, buffer);
            i2c_transfer(u8x8, 0x3F << 1, buf_idx, buffer);
        }
        break;
    default:
        return 0;
    }
    return 1;
}


static uint16_t static_utf8_char = 0;
void u8x8_utf8_init(u8x8_t *u8x8) {
u8x8->utf8_state = 0;
}
uint16_t u8x8_utf8_next(u8x8_t *u8x8, uint8_t b) {
if ( b == 0 )  {
u8x8->utf8_state = 0;
return 0xffff;
}
if ( u8x8->utf8_state == 0 ) {
if ( b < 0x80 ) return b;
if ( (b & 0xe0) == 0xc0 ) {
u8x8->utf8_state = 1;
static_utf8_char = b & 0x1f;
} else if ( (b & 0xf0) == 0xe0 ) {
u8x8->utf8_state = 2;
static_utf8_char = b & 0x0f;
} else if ( (b & 0xf8) == 0xf0 ) {
u8x8->utf8_state = 3;
static_utf8_char = b & 0x07;
}
return 0xfffe;
}
u8x8->utf8_state--;
static_utf8_char <<= 6;
static_utf8_char |= (b & 0x3f);
if ( u8x8->utf8_state == 0 ) return static_utf8_char;
return 0xfffe;
}
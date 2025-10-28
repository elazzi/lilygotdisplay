#include <TFT_eSPI.h>
#include <Wire.h>
#include "USB.h"
#include "USBHIDKeyboard.h"
#include <Preferences.h>
#include <mbedtls/aes.h>
#include <mbedtls/sha256.h>
#include <esp_system.h>

#include "stdint.h"
/***********************pin config*************************/

#define SPI_FREQUENCY         32000000 // corruption occured at 40000000 so 32000000 is probably a safe upper limit
#define TFT_SPI_MODE          SPI_MODE0
#define TFT_SPI_HOST          SPI2_HOST

#define WIFI_SSID             "xxxx"
#define WIFI_PASSWORD         "xxxx"

#define WIFI_CONNECT_WAIT_MAX (30 * 1000)

#define NTP_SERVER1           "pool.ntp.org"
#define NTP_SERVER2           "time.nist.gov"
#define GMT_OFFSET_SEC        0
#define DAY_LIGHT_OFFSET_SEC  0
#define GET_TIMEZONE_API      "https://ipapi.co/timezone/"


#define SEND_BUF_SIZE         (28800/2) //
#define TFT_QSPI_CS           12
#define TFT_QSPI_SCK          17
#define TFT_QSPI_D0           13
#define TFT_QSPI_D1           18
#define TFT_QSPI_D2           21
#define TFT_QSPI_D3           14
#define TFT_QSPI_RST          16
#define TFT_BL                1
#define PIN_BAT_VOLT          8
#define PIN_BUTTON_1          0
#define PIN_BUTTON_2          21


#define TOUCH_IICSCL 10
#define TOUCH_IICSDA 15
#define TOUCH_INT 11
#define TOUCH_RES 16

#define AXS_TOUCH_ONE_POINT_LEN             6
#define AXS_TOUCH_BUF_HEAD_LEN              2

#define AXS_TOUCH_GESTURE_POS               0
#define AXS_TOUCH_POINT_NUM                 1
#define AXS_TOUCH_EVENT_POS                 2
#define AXS_TOUCH_X_H_POS                   2
#define AXS_TOUCH_X_L_POS                   3
#define AXS_TOUCH_ID_POS                    4
#define AXS_TOUCH_Y_H_POS                   4
#define AXS_TOUCH_Y_L_POS                   5
#define AXS_TOUCH_WEIGHT_POS                6
#define AXS_TOUCH_AREA_POS                  7

#define AXS_GET_POINT_NUM(buf) buf[AXS_TOUCH_POINT_NUM]
#define AXS_GET_GESTURE_TYPE(buf)  buf[AXS_TOUCH_GESTURE_POS]
#define AXS_GET_POINT_X(buf,point_index) (((uint16_t)(buf[AXS_TOUCH_ONE_POINT_LEN*point_index+AXS_TOUCH_X_H_POS] & 0x0F) <<8) + (uint16_t)buf[AXS_TOUCH_ONE_POINT_LEN*point_index+AXS_TOUCH_X_L_POS])
#define AXS_GET_POINT_Y(buf,point_index) (((uint16_t)(buf[AXS_TOUCH_ONE_POINT_LEN*point_index+AXS_TOUCH_Y_H_POS] & 0x0F) <<8) + (uint16_t)buf[AXS_TOUCH_ONE_POINT_LEN*point_index+AXS_TOUCH_Y_L_POS])
#define AXS_GET_POINT_EVENT(buf,point_index) (buf[AXS_TOUCH_ONE_POINT_LEN*point_index+AXS_TOUCH_EVENT_POS] >> 6)

/***********************pin config*************************/

//#define LCD_SPI_DMA 

#define TFT_MADCTL    0x36
#define TFT_MAD_MY    0x80
#define TFT_MAD_MX    0x40
#define TFT_MAD_MV    0x20
#define TFT_MAD_ML    0x10
#define TFT_MAD_BGR   0x08
#define TFT_MAD_MH    0x04
#define TFT_MAD_RGB   0x00

#define TFT_INVOFF    0x20
#define TFT_INVON     0x21

#define TFT_SCK_H     digitalWrite(TFT_SCK, 1);
#define TFT_SCK_L     digitalWrite(TFT_SCK, 0);
#define TFT_SDA_H     digitalWrite(TFT_MOSI, 1);
#define TFT_SDA_L     digitalWrite(TFT_MOSI, 0);

#define TFT_RES_H     digitalWrite(TFT_QSPI_RST, 1);
#define TFT_RES_L     digitalWrite(TFT_QSPI_RST, 0);
#define TFT_DC_H      digitalWrite(TFT_DC, 1);
#define TFT_DC_L      digitalWrite(TFT_DC, 0);
#define TFT_CS_H      digitalWrite(TFT_QSPI_CS, 1);
#define TFT_CS_L      digitalWrite(TFT_QSPI_CS, 0);

typedef struct
{
    uint8_t cmd;
    uint8_t data[36];
    uint8_t len;
} lcd_cmd_t;

void axs15231_init(void);

void lcd_address_set(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2);

void lcd_setRotation(uint8_t r);

void lcd_DrawPoint(uint16_t x, uint16_t y, uint16_t color);

void lcd_fill(uint16_t xsta, uint16_t ysta, uint16_t xend, uint16_t yend, uint16_t color);

void lcd_PushColors(uint16_t x, uint16_t y, uint16_t width, uint16_t high, uint16_t *data);

void lcd_PushColors_rotated_90(uint16_t x, uint16_t y, uint16_t width, uint16_t high, uint16_t *data);   

void lcd_PushColors(uint16_t *data, uint32_t len);// use directly after lcd_address_set()

void lcd_sleep();

bool get_lcd_spi_dma_write(void);



void hw_set_brightness(uint8_t val);
void hw_colour_fill(uint8_t r, uint8_t g, uint8_t b);
void hw_clear_screen_black();




uint16_t* qBuffer = (uint16_t*) heap_caps_malloc(230400, MALLOC_CAP_SPIRAM ); //psram buffer for matrix rotation (640 * 180 * 2)
static volatile bool lcd_spi_dma_write = false;
uint32_t transfer_num = 0;
size_t lcd_PushColors_len = 0;

const static lcd_cmd_t axs15231b_qspi_init[] = {
    {0x28, {0x00}, 0x40},
    {0x10, {0x00}, 0x20},
    {0x11, {0x00}, 0x80},
    {0x29, {0x00}, 0x00}, 
};



bool get_lcd_spi_dma_write(void)
{
    return lcd_spi_dma_write;
}



static spi_device_handle_t spi;

static void WriteComm(uint8_t data)
{
    TFT_CS_L;
    SPI.beginTransaction(SPISettings(SPI_FREQUENCY, MSBFIRST, TFT_SPI_MODE));
    SPI.write(0x00);
    SPI.write(data);
    SPI.write(0x00);
    SPI.endTransaction();
    TFT_CS_H;
}




static void WriteData(uint8_t data)
{
    TFT_CS_L;
    SPI.beginTransaction(SPISettings(SPI_FREQUENCY, MSBFIRST, TFT_SPI_MODE));
    SPI.write(data);
    SPI.endTransaction();
    TFT_CS_H;
}





static void lcd_send_cmd(uint32_t cmd, uint8_t *dat, uint32_t len)
{
    TFT_CS_L;
    spi_transaction_t t;
    memset(&t, 0, sizeof(t));
    t.flags = (SPI_TRANS_MULTILINE_CMD | SPI_TRANS_MULTILINE_ADDR);
    #ifdef LCD_SPI_DMA
        if(cmd == 0xff && len == 0x1f)
        {
            t.cmd = 0x02;
            t.addr = 0xffff;
            len = 0;
        }
        else if(cmd == 0x00)
        {
            t.cmd = 0X00;
            t.addr = 0X0000;
            len = 4;
        }
        else 
        {
            t.cmd = 0x02;
            t.addr = cmd << 8;
        }
    #else
        t.cmd = 0x02;
        t.addr = cmd << 8;
    #endif
    if (len != 0) {
        t.tx_buffer = dat; 
        t.length = 8 * len;
    } else {
        t.tx_buffer = NULL;
        t.length = 0;
    }
    spi_device_polling_transmit(spi, &t);
    TFT_CS_H;
    if(0)
    {
        WriteComm(cmd);
        if (len != 0) {
            for (int i = 0; i < len; i++)
                WriteData(dat[i]);
        }
    }
}





void axs15231_init(void)
{

    pinMode(TFT_QSPI_CS, OUTPUT);
    pinMode(TFT_QSPI_RST, OUTPUT);

    TFT_RES_H;
    delay(130);
    TFT_RES_L;
    delay(130);
    TFT_RES_H;
    delay(300);

    esp_err_t ret;

    spi_bus_config_t buscfg = {
        .data0_io_num = TFT_QSPI_D0,
        .data1_io_num = TFT_QSPI_D1,
        .sclk_io_num = TFT_QSPI_SCK,
        .data2_io_num = TFT_QSPI_D2,
        .data3_io_num = TFT_QSPI_D3,
        .max_transfer_sz = (SEND_BUF_SIZE * 16) + 8,
        .flags = SPICOMMON_BUSFLAG_MASTER | SPICOMMON_BUSFLAG_GPIO_PINS /* |
                 SPICOMMON_BUSFLAG_QUAD */
        ,
    };
    spi_device_interface_config_t devcfg = {
        .command_bits = 8,
        .address_bits = 24,
        .mode = TFT_SPI_MODE,
        .clock_speed_hz = SPI_FREQUENCY,
        .spics_io_num = -1,
        // .spics_io_num = TFT_QSPI_CS,
        .flags = SPI_DEVICE_HALFDUPLEX,
        .queue_size = 17,
//        .post_cb = spi_dma_cd,
    };
    ret = spi_bus_initialize(TFT_SPI_HOST, &buscfg, SPI_DMA_CH_AUTO);
    ESP_ERROR_CHECK(ret);
    ret = spi_bus_add_device(TFT_SPI_HOST, &devcfg, &spi);
    ESP_ERROR_CHECK(ret);


    // Initialize the screen multiple times to prevent initialization failure
    int i = 1;
    while (i--) {

        const lcd_cmd_t *lcd_init = axs15231b_qspi_init;
        for (int i = 0; i < sizeof(axs15231b_qspi_init) / sizeof(lcd_cmd_t); i++)

        {
            lcd_send_cmd(lcd_init[i].cmd,
                         (uint8_t *)lcd_init[i].data,
                         lcd_init[i].len & 0x3f);

            if (lcd_init[i].len & 0x80)
                delay(200);
            if (lcd_init[i].len & 0x40)
                delay(20);
        }
    }
}





void lcd_setRotation(uint8_t r)
{
    uint8_t gbr = TFT_MAD_RGB;

    switch (r) {
    case 0: // Portrait
        // WriteData(gbr);
        break;
    case 1: // Landscape (Portrait + 90)
        gbr = TFT_MAD_MX | TFT_MAD_MV | gbr;
        break;
    case 2: // Inverter portrait
        gbr = TFT_MAD_MX | TFT_MAD_MY | gbr;
        break;
    case 3: // Inverted landscape
        gbr = TFT_MAD_MV | TFT_MAD_MY | gbr;
        break;
    }
    lcd_send_cmd(TFT_MADCTL, &gbr, 1);
}







void lcd_address_set(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2)
{
    lcd_cmd_t t[3] = {
        {0x2a, {(uint8_t)(x1 >> 8), (uint8_t)x1, uint8_t(x2 >> 8), (uint8_t)(x2)}, 0x04},
        {0x2b, {(uint8_t)(y1 >> 8), (uint8_t)(y1), (uint8_t)(y2 >> 8), (uint8_t)(y2)}, 0x04},
    };

    for (uint32_t i = 0; i < 2; i++) {
        lcd_send_cmd(t[i].cmd, t[i].data, t[i].len);
    }
}






void lcd_fill(uint16_t xsta,
              uint16_t ysta,
              uint16_t xend,
              uint16_t yend,
              uint16_t color)
{

    uint16_t w = xend - xsta;
    uint16_t h = yend - ysta;
    uint16_t *color_p = (uint16_t *)heap_caps_malloc(w * h * 2, MALLOC_CAP_INTERNAL);
    int i = 0;
    for(i = 0; i < w * h ; i+=1)
    {
        color_p[i] = color;
    }

    lcd_PushColors(xsta, ysta, w, h, color_p);
    free(color_p);
}







void lcd_DrawPoint(uint16_t x, uint16_t y, uint16_t color)
{
    lcd_address_set(x, y, x + 1, y + 1);
    lcd_PushColors(&color, 1);
}






void spi_device_queue_trans_fun(spi_device_handle_t handle, spi_transaction_t *trans_desc, TickType_t ticks_to_wait)
{
    ESP_ERROR_CHECK(spi_device_queue_trans(spi, (spi_transaction_t *)trans_desc, portMAX_DELAY));
}






#ifdef LCD_SPI_DMA 
spi_transaction_ext_t t = {0};
void lcd_PushColors(uint16_t x,
                        uint16_t y,
                        uint16_t width,
                        uint16_t high,
                        uint16_t *data)
    {
        static bool first_send = 1;
        static uint16_t *p = (uint16_t *)data;
        static uint32_t transfer_num_old = 0;

        if(data != NULL && (width != 0) && (high != 0))
        {
            lcd_PushColors_len = width * high;
            p = (uint16_t *)data;
            first_send = 1;

            transfer_num = 0;
            lcd_address_set(x, y, x + width - 1, y + high - 1);
            TFT_CS_L;
        }

        for (int x = 0; x < (transfer_num_old - (transfer_num_old-(transfer_num_old-transfer_num))); x++) {
            spi_transaction_t *rtrans;
            esp_err_t ret = spi_device_get_trans_result(spi, &rtrans, portMAX_DELAY);
            if (ret != ESP_OK) {
            // ESP_LOGW(TAG, "1. transfer_num = %d", transfer_num_old);
            }
            assert(ret == ESP_OK);
        }
        transfer_num_old -= (transfer_num_old - (transfer_num_old-(transfer_num_old-transfer_num)));

        do {
            if(transfer_num >= 3 || ESP.getFreeHeap() <= 70000)
            {
                break;
            }
            size_t chunk_size = lcd_PushColors_len;

            memset(&t, 0, sizeof(t));
            if (first_send) {
                t.base.flags =
                    SPI_TRANS_MODE_QIO ;// | SPI_TRANS_MODE_DIOQIO_ADDR 
                t.base.cmd = 0x32 ;// 0x12 
                t.base.addr = 0x002C00;
                first_send = 0;
            } else {
                t.base.flags = SPI_TRANS_MODE_QIO | SPI_TRANS_VARIABLE_CMD |
                            SPI_TRANS_VARIABLE_ADDR | SPI_TRANS_VARIABLE_DUMMY;
                t.command_bits = 0;
                t.address_bits = 0;
                t.dummy_bits = 0;
            }
            if (chunk_size > SEND_BUF_SIZE) {
                chunk_size = SEND_BUF_SIZE;
            }
            t.base.tx_buffer = p;
            t.base.length = chunk_size * 16;

            lcd_spi_dma_write = true;

            transfer_num++;
            transfer_num_old++;
            lcd_PushColors_len -= chunk_size;
            esp_err_t ret;

            ESP_ERROR_CHECK(spi_device_queue_trans(spi, (spi_transaction_t *)&t, portMAX_DELAY));
            assert(ret == ESP_OK);

            p += chunk_size;
        } while (lcd_PushColors_len > 0);
    }
 
#else
    void lcd_PushColors(uint16_t x,
                        uint16_t y,
                        uint16_t width,
                        uint16_t high,
                        uint16_t *data)
    {
        bool first_send = 1;
        size_t len = width * high;
        uint16_t *p = (uint16_t *)data;

        lcd_address_set(x, y, x + width - 1, y + high - 1);
        
        do {

            TFT_CS_L;
            size_t chunk_size = len;
            spi_transaction_ext_t t = {0};
            memset(&t, 0, sizeof(t));
            if (1) {
                t.base.flags =
                    SPI_TRANS_MODE_QIO /* | SPI_TRANS_MODE_DIOQIO_ADDR */;
                t.base.cmd = 0x32 /* 0x12 */;
                if(first_send)
                {
                    t.base.addr = 0x002C00;
                }
                else 
                    t.base.addr = 0x003C00;
                first_send = 0;
            } else {
                t.base.flags = SPI_TRANS_MODE_QIO | SPI_TRANS_VARIABLE_CMD |
                            SPI_TRANS_VARIABLE_ADDR | SPI_TRANS_VARIABLE_DUMMY;
                t.command_bits = 0;
                t.address_bits = 0;
                t.dummy_bits = 0;
            }
            if (chunk_size > SEND_BUF_SIZE) {
                chunk_size = SEND_BUF_SIZE;
            }
            t.base.tx_buffer = p;
            t.base.length = chunk_size * 16;
            int aaa = 0;
            aaa = aaa>>1;
            aaa = aaa>>1;
            aaa = aaa>>1;
            if(!first_send)
                TFT_CS_H;
            aaa = aaa>>1;
            aaa = aaa>>1;
            aaa = aaa>>1;
            aaa = aaa>>1;
            aaa = aaa>>1;
            TFT_CS_L;
            aaa = aaa>>1;
            aaa = aaa>>1;
            aaa = aaa>>1;
            spi_device_polling_transmit(spi, (spi_transaction_t *)&t);
            len -= chunk_size;
            p += chunk_size;
        } while (len > 0);
        TFT_CS_H;
    }
#endif






void lcd_PushColors(uint16_t *data, uint32_t len)
{
    bool first_send = 1;
    uint16_t *p = (uint16_t *)data;
    TFT_CS_L;
    do {
        size_t chunk_size = len;
        spi_transaction_ext_t t = {0};
        memset(&t, 0, sizeof(t));
        if (first_send) {
            t.base.flags =
                SPI_TRANS_MODE_QIO /* | SPI_TRANS_MODE_DIOQIO_ADDR */;
            t.base.cmd = 0x32 /* 0x12 */;
            t.base.addr = 0x002C00;
            first_send = 0;
        } else {
            t.base.flags = SPI_TRANS_MODE_QIO | SPI_TRANS_VARIABLE_CMD |
                           SPI_TRANS_VARIABLE_ADDR | SPI_TRANS_VARIABLE_DUMMY;
            t.command_bits = 0;
            t.address_bits = 0;
            t.dummy_bits = 0;
        }
        if (chunk_size > SEND_BUF_SIZE) {
            chunk_size = SEND_BUF_SIZE;
        }
        t.base.tx_buffer = p;
        t.base.length = chunk_size * 16;

        spi_device_polling_transmit(spi, (spi_transaction_t *)&t);
        len -= chunk_size;
        p += chunk_size;
    } while (len > 0);
    TFT_CS_H;
}






void lcd_PushColors_rotated_90(
                    uint16_t x,
                    uint16_t y,
                    uint16_t width,
                    uint16_t high,
                    uint16_t *data)
{
    uint16_t  _x = 180 - (y + high);
    uint16_t  _y = x;
    uint16_t  _h = width;
    uint16_t  _w = high;

    lcd_address_set(_x, _y, _x + _w - 1, _y + _h - 1);

    bool first_send = 1;
    size_t len = width * high;
    uint16_t *p = (uint16_t *)data;
    uint16_t *q = (uint16_t *)qBuffer;
    uint32_t index = 0; //qBuffer index
   
    for (uint16_t j = 0; j < width; j++)
    {
        for (uint16_t i = 0; i < high; i++)
        {
            qBuffer[index++] = ((uint16_t)p[width * (high - i - 1) + j]);             
        }
    }


    TFT_CS_L;
    do
    {
        size_t chunk_size = len;  
        spi_transaction_ext_t t = {0};
        memset(&t, 0, sizeof(t));
        if (first_send)
        {
            t.base.flags =
                SPI_TRANS_MODE_QIO /* | SPI_TRANS_MODE_DIOQIO_ADDR */;
            t.base.cmd = 0x32 /* 0x12 */;
            t.base.addr = 0x002C00;
            first_send = 0;
        }
        else
        {
            t.base.flags = SPI_TRANS_MODE_QIO | SPI_TRANS_VARIABLE_CMD |
                           SPI_TRANS_VARIABLE_ADDR | SPI_TRANS_VARIABLE_DUMMY;
            t.command_bits = 0;
            t.address_bits = 0;
            t.dummy_bits = 0;
        }
        if (chunk_size > SEND_BUF_SIZE)
        {
            chunk_size = SEND_BUF_SIZE;
        }
        t.base.tx_buffer = q; 
        t.base.length = chunk_size * 16;
        spi_device_polling_transmit(spi, (spi_transaction_t *)&t);
        len -= chunk_size;
        q += chunk_size;
    } while (len > 0);
    TFT_CS_H;   
}




void lcd_sleep()
{
    lcd_send_cmd(0x10, NULL, 0);
}




void hw_set_brightness(uint8_t val)
{
    lcd_send_cmd(0x51, &val, 1);
}

void hw_colour_fill(uint8_t r, uint8_t g, uint8_t b)
{
    uint8_t rgb[] = {r,g,b};
    lcd_send_cmd(0x2f, rgb, 3);
}


void hw_clear_screen_black()
{
    lcd_send_cmd(0x22, NULL, 0);
}



// ==================== APPLICATION CODE ====================
            
// Hardware configuration
#define TFT_BL          1
#define TFT_BACKLIGHT_ON HIGH
#define TOUCH_SDA       15
#define TOUCH_SCL       10
#define TOUCH_RST       16
#define TOUCH_INT       11

// Display dimensions
#define TFT_WIDTH       640
#define TFT_HEIGHT      180

// Security constants
#define MAX_PIN_LENGTH      10
#define MAX_PASSWORDS       4
#define MAX_PASSWORD_LENGTH 1024
#define PBKDF2_ITERATIONS   5000  // Reduced for ESP32 performance
#define AES_KEY_SIZE        32
#define SALT_SIZE           16
#define HASH_SIZE           32
#define TOUCH_DEBOUNCE_MS   250
#define WIPE_HOLD_TIME_MS   3000
#define MAX_PIN_ATTEMPTS    5
#define LOCKOUT_TIME_MS     30000

// Colors
#define COLOR_BG        TFT_BLACK     // Black
#define COLOR_BTN1      0x39C7      // Primary button
#define COLOR_BTN2      0x2945      // Secondary button  
#define COLOR_ACCENT    TFT_ORANGE  // Accent color
#define COLOR_TEXT      TFT_SILVER  // Text color
#define COLOR_ERROR     TFT_RED     // Error color
#define COLOR_SUCCESS   TFT_GREEN   // Success color

TFT_eSPI tft = TFT_eSPI();
TFT_eSprite sprite = TFT_eSprite(&tft);
USBHIDKeyboard Keyboard;
Preferences preferences;

// Numpad layout
const int xpos[] = {4, 92, 180};
const int ypos[] = {4, 43, 82, 121, 160};
const char* btns[4][3] = {{"1","2","3"},{"4","5","6"},{"7","8","9"},{"CLR","0","OK"}};

// Application state
enum AppState {
    STATE_SETUP_PIN_NEW,
    STATE_SETUP_PIN_CONFIRM,
    STATE_SETUP_PASSWORD,
    STATE_PIN_ENTRY,
    STATE_PASSWORD_SELECTION,
    STATE_WIPE_CONFIRM,
    STATE_LOCKED_OUT,
    STATE_ERROR
};

AppState currentState;
String pin_buffer = "";
String new_pin_buffer = "";
String password_buffer = "";
String error_message = "";
bool touch_detected = false;
unsigned long last_touch_time = 0;
unsigned long wipe_start_time = 0;
unsigned long lockout_start_time = 0;
bool wiping = false;
int pin_attempts = 0;

int setup_password_index = 1;
int qwerty_page = 0; // 0 for alpha, 1 for symbols
// Security buffers (will be securely wiped)
uint8_t aes_key[AES_KEY_SIZE] = {0};
uint8_t salt[SALT_SIZE] = {0};
uint8_t pin_hash[HASH_SIZE] = {0};


// Touch handling
#define TOUCH_I2C_ADDR  0x3B
uint8_t read_touchpad_cmd[] = {0xb5, 0xab, 0xa5, 0x5a, 0x0, 0x0, 0x0, 0x8};

#define AXS_GET_GESTURE_TYPE(buf) ((buf[0] >> 6) & 0x03)
#define AXS_GET_POINT_X(buf, idx) ((buf[2 + (idx << 2)] & 0x0F) << 8 | buf[3 + (idx << 2)])
#define AXS_GET_POINT_Y(buf, idx) ((buf[4 + (idx << 2)] & 0x0F) << 8 | buf[5 + (idx << 2)])

// Security function declarations
bool hashPIN(const String& pin, uint8_t* output_hash);
bool deriveKey(const String& pin, const uint8_t* salt, uint8_t* output_key);
bool encryptPassword(const String& password, const uint8_t* key, String& encrypted_output);
bool decryptPassword(const String& encrypted, const uint8_t* key, String& decrypted_output);
void generateSalt(uint8_t* salt);
void secureWipeBuffer(void* buffer, size_t size);
bool loadStoredData();
bool saveStoredData();
void secureWipe();
void hmac_sha256(const uint8_t* key, size_t keylen, const uint8_t* data, size_t datalen, uint8_t* output);

// Forward declarations for functions that need to be called before they're defined
void handleTouchInterrupt();
void handleOKButton();
void handlePasswordOK();
void typePassword(int index);
void finishSetup();

// ==================== INTERRUPT HANDLER ====================

void IRAM_ATTR handleTouchInterrupt() {
    touch_detected = true;
}

// ==================== SECURITY FUNCTIONS ====================

void secureWipeBuffer(void* buffer, size_t size) {
    if (buffer != NULL) {
        volatile uint8_t* ptr = (volatile uint8_t*)buffer;
        for (size_t i = 0; i < size; i++) {
            ptr[i] = 0;
        }
    }
}

// HMAC-SHA256 implementation
void hmac_sha256(const uint8_t* key, size_t keylen, const uint8_t* data, size_t datalen, uint8_t* output) {
    uint8_t k_ipad[64] = {0};
    uint8_t k_opad[64] = {0};
    uint8_t tk[HASH_SIZE] = {0};
    uint8_t buffer_out[HASH_SIZE] = {0};
    
    // If key is longer than 64 bytes, hash it
    if (keylen > 64) {
        mbedtls_sha256_context ctx;
        mbedtls_sha256_init(&ctx);
        mbedtls_sha256_starts_ret(&ctx, 0);
        mbedtls_sha256_update_ret(&ctx, key, keylen);
        mbedtls_sha256_finish_ret(&ctx, tk);
        mbedtls_sha256_free(&ctx);
        key = tk;
        keylen = HASH_SIZE;
    }
    
    // Prepare inner and outer padding
    memset(k_ipad, 0x36, 64);
    memset(k_opad, 0x5C, 64);
    
    for (size_t i = 0; i < keylen; i++) {
        k_ipad[i] ^= key[i];
        k_opad[i] ^= key[i];
    }
    
    // Inner hash
    mbedtls_sha256_context ctx;
    mbedtls_sha256_init(&ctx);
    mbedtls_sha256_starts_ret(&ctx, 0);
    mbedtls_sha256_update_ret(&ctx, k_ipad, 64);
    mbedtls_sha256_update_ret(&ctx, data, datalen);
    mbedtls_sha256_finish_ret(&ctx, buffer_out);
    mbedtls_sha256_free(&ctx);
    
    // Outer hash
    mbedtls_sha256_init(&ctx);
    mbedtls_sha256_starts_ret(&ctx, 0);
    mbedtls_sha256_update_ret(&ctx, k_opad, 64);
    mbedtls_sha256_update_ret(&ctx, buffer_out, HASH_SIZE);
    mbedtls_sha256_finish_ret(&ctx, output);
    mbedtls_sha256_free(&ctx);
    
    // Cleanup
    secureWipeBuffer(k_ipad, sizeof(k_ipad));
    secureWipeBuffer(k_opad, sizeof(k_opad));
    secureWipeBuffer(tk, sizeof(tk));
    secureWipeBuffer(buffer_out, sizeof(buffer_out));
}

// PBKDF2 implementation
bool deriveKey(const String& pin, const uint8_t* salt, uint8_t* output_key) {
    if (pin.length() == 0 || salt == NULL || output_key == NULL) return false;
    
    size_t pin_len = pin.length();
    const uint8_t* pin_data = (const uint8_t*)pin.c_str();
    
    uint8_t U[HASH_SIZE] = {0};
    uint8_t T[HASH_SIZE] = {0};
    uint8_t salt_buffer[SALT_SIZE + 4] = {0}; // salt + block index
    
    // Copy salt to buffer
    memcpy(salt_buffer, salt, SALT_SIZE);
    
    // For AES-256, we need 32 bytes (one block)
    size_t key_length = AES_KEY_SIZE;
    size_t block_count = 1; // 32 bytes = 1 SHA256 block
    
    for (size_t i = 1; i <= block_count; i++) {
        // Add block index to salt
        salt_buffer[SALT_SIZE + 0] = (i >> 24) & 0xFF;
        salt_buffer[SALT_SIZE + 1] = (i >> 16) & 0xFF;
        salt_buffer[SALT_SIZE + 2] = (i >> 8) & 0xFF;
        salt_buffer[SALT_SIZE + 3] = i & 0xFF;
        
        // First iteration
        hmac_sha256(pin_data, pin_len, salt_buffer, SALT_SIZE + 4, U);
        memcpy(T, U, HASH_SIZE);
        
        // Subsequent iterations
        for (unsigned long j = 1; j < PBKDF2_ITERATIONS; j++) {
            hmac_sha256(pin_data, pin_len, U, HASH_SIZE, U);
            for (size_t k = 0; k < HASH_SIZE; k++) {
                T[k] ^= U[k];
            }
        }
        
        // Copy to output key
        memcpy(output_key, T, key_length);
    }
    
    // Cleanup
    secureWipeBuffer(U, sizeof(U));
    secureWipeBuffer(T, sizeof(T));
    secureWipeBuffer(salt_buffer, sizeof(salt_buffer));
    
    return true;
}

bool hashPIN(const String& pin, uint8_t* output_hash) {
    if (pin.length() == 0 || output_hash == NULL) return false;
    
    mbedtls_sha256_context ctx;
    mbedtls_sha256_init(&ctx);
    
    // Use the _ret versions which return error codes
    if (mbedtls_sha256_starts_ret(&ctx, 0) != 0) {
        mbedtls_sha256_free(&ctx);
        return false;
    }
    
    if (mbedtls_sha256_update_ret(&ctx, (const unsigned char*)pin.c_str(), pin.length()) != 0) {
        mbedtls_sha256_free(&ctx);
        return false;
    }
    
    if (mbedtls_sha256_finish_ret(&ctx, output_hash) != 0) {
        mbedtls_sha256_free(&ctx);
        return false;
    }
    
    mbedtls_sha256_free(&ctx);
    return true;
}

void generateSalt(uint8_t* salt) {
    if (salt == NULL) return;
    
    // Use ESP32 hardware random number generator
    for (int i = 0; i < SALT_SIZE; i++) {
        salt[i] = esp_random() & 0xFF;
    }
}

bool encryptPassword(const String& password, const uint8_t* key, String& encrypted_output) {
    if (password.length() == 0 || key == NULL) return false;
    
    mbedtls_aes_context aes;
    mbedtls_aes_init(&aes);
    
    // Set encryption key
    if (mbedtls_aes_setkey_enc(&aes, key, 256) != 0) {
        mbedtls_aes_free(&aes);
        return false;
    }
    
    // Pad password to 16-byte boundary
    size_t padded_len = ((password.length() + 15) / 16) * 16;
    uint8_t* input = (uint8_t*)malloc(padded_len);
    uint8_t* output = (uint8_t*)malloc(padded_len);
    
    if (input == NULL || output == NULL) {
        if (input) free(input);
        if (output) free(output);
        mbedtls_aes_free(&aes);
        return false;
    }
    
    // Copy and pad with PKCS7
    memcpy(input, password.c_str(), password.length());
    uint8_t pad_value = padded_len - password.length();
    memset(input + password.length(), pad_value, pad_value);
    
    // Encrypt each block
    bool success = true;
    for (size_t i = 0; i < padded_len; i += 16) {
        if (mbedtls_aes_crypt_ecb(&aes, MBEDTLS_AES_ENCRYPT, input + i, output + i) != 0) {
            success = false;
            break;
        }
    }
    
    if (success) {
        // Convert to hex string for storage
        encrypted_output = "";
        for (size_t i = 0; i < padded_len; i++) {
            char hex[3];
            snprintf(hex, sizeof(hex), "%02x", output[i]);
            encrypted_output += hex;
        }
    }
    
    secureWipeBuffer(input, padded_len);
    secureWipeBuffer(output, padded_len);
    free(input);
    free(output);
    mbedtls_aes_free(&aes);
    
    return success;
}

bool decryptPassword(const String& encrypted, const uint8_t* key, String& decrypted_output) {
    if (encrypted.length() == 0 || key == NULL || encrypted.length() % 2 != 0) return false;
    
    mbedtls_aes_context aes;
    mbedtls_aes_init(&aes);
    
    // Set decryption key
    if (mbedtls_aes_setkey_dec(&aes, key, 256) != 0) {
        mbedtls_aes_free(&aes);
        return false;
    }
    
    // Convert from hex string
    size_t data_len = encrypted.length() / 2;
    uint8_t* input = (uint8_t*)malloc(data_len);
    uint8_t* output = (uint8_t*)malloc(data_len);
    
    if (input == NULL || output == NULL) {
        if (input) free(input);
        if (output) free(output);
        mbedtls_aes_free(&aes);
        return false;
    }
    
    // Parse hex string
    for (size_t i = 0; i < data_len; i++) {
        sscanf(encrypted.c_str() + i * 2, "%2hhx", &input[i]);
    }
    
    // Decrypt each block
    bool success = true;
    for (size_t i = 0; i < data_len; i += 16) {
        if (mbedtls_aes_crypt_ecb(&aes, MBEDTLS_AES_DECRYPT, input + i, output + i) != 0) {
            success = false;
            break;
        }
    }
    
    if (success && data_len > 0) {
        // Remove PKCS7 padding
        uint8_t pad_value = output[data_len - 1];
        if (pad_value > 0 && pad_value <= 16) {
            // Verify padding
            bool padding_valid = true;
            for (size_t i = data_len - pad_value; i < data_len; i++) {
                if (output[i] != pad_value) {
                    padding_valid = false;
                    break;
                }
            }
            
            if (padding_valid) {
                size_t actual_len = data_len - pad_value;
                decrypted_output = String((const char*)output, actual_len);
            } else {
                success = false;
            }
        } else {
            success = false;
        }
    } else {
        success = false;
    }
    
    secureWipeBuffer(input, data_len);
    secureWipeBuffer(output, data_len);
    free(input);
    free(output);
    mbedtls_aes_free(&aes);
    
    return success;
}

bool loadStoredData() {
    // Load salt
    if (preferences.getBytesLength("salt") != SALT_SIZE) return false;
    if (preferences.getBytes("salt", salt, SALT_SIZE) != SALT_SIZE) return false;
    
    // Load PIN hash
    if (preferences.getBytesLength("pin_hash") != HASH_SIZE) return false;
    if (preferences.getBytes("pin_hash", pin_hash, HASH_SIZE) != HASH_SIZE) return false;
    
    return true;
}

bool saveStoredData() {
    if (!preferences.putBytes("salt", salt, SALT_SIZE)) return false;
    if (!preferences.putBytes("pin_hash", pin_hash, HASH_SIZE)) return false;
    return true;
}

void secureWipe() {
    secureWipeBuffer(aes_key, sizeof(aes_key));
    secureWipeBuffer(salt, sizeof(salt));
    secureWipeBuffer(pin_hash, sizeof(pin_hash));
    
    preferences.remove("salt");
    preferences.remove("pin_hash");
    for (int i = 1; i <= MAX_PASSWORDS; i++) {
        preferences.remove(("enc_pass_" + String(i)).c_str());
    }
    preferences.remove("is_configured");
    preferences.putBool("needs_setup", true);
}

// ==================== DISPLAY FUNCTIONS ====================

void initializeDisplay() {
    //tft.init();
    //tft.setRotation(1);
    //tft.fillScreen(TFT_BLACK);
    //sprite.createSprite(TFT_WIDTH, TFT_HEIGHT);
    
    if (!sprite.createSprite(TFT_WIDTH, TFT_HEIGHT)) {
        Serial.println("Sprite creation failed!");
        return;
    }
    sprite.setSwapBytes(true);
}

void initializeTouch() {
    pinMode(TOUCH_RST, OUTPUT);
    digitalWrite(TOUCH_RST, HIGH);
    delay(2);
    digitalWrite(TOUCH_RST, LOW);
    delay(10);
    digitalWrite(TOUCH_RST, HIGH);
    delay(2);

    Wire.begin(TOUCH_SDA, TOUCH_SCL);
    //pinMode(TOUCH_INT, INPUT_PULLUP);
    //attachInterrupt(digitalPinToInterrupt(TOUCH_INT), handleTouchInterrupt, FALLING);
}


void drawNumpadScreen(const String& title, bool show_error = false) {
    sprite.fillSprite(TFT_BLACK);
    sprite.setTextFont(1);
    sprite.setTextSize(2);
    sprite.setTextColor(TFT_SILVER, TFT_BLACK);
    sprite.drawString(title, 350, 10);

    

    // Draw PIN buffer
    sprite.drawRect(350, 40, 260, 40, TFT_WHITE);
    sprite.setTextFont(1);
    sprite.setTextSize(3);
    sprite.drawString(pin_buffer, 360, 50);

    // Draw keypad
    sprite.setTextFont(1);
    sprite.setTextSize(2);
    for (int y = 0; y < 4; y++) {
        for (int x = 0; x < 3; x++) {
            int x_pos = xpos[x];
            int y_pos = ypos[y];
            unsigned short bg_color = (y == 3 && (x == 0 || x == 2)) ? COLOR_BTN2 : COLOR_BTN1;
            unsigned short text_color = (y == 3 && (x == 0 || x == 2)) ? COLOR_ACCENT : COLOR_TEXT;
            sprite.fillRoundRect(x_pos, y_pos, 80, 35, 8, bg_color);
            sprite.setTextColor(text_color, bg_color);
            sprite.drawCentreString(btns[y][x], x_pos + 40, y_pos , 2);
        }
    }
    sprite.setTextSize(2);

    // Draw the "CLEAR ALL" button only on the main PIN entry screen
    if (currentState == STATE_PIN_ENTRY) {
        sprite.fillRoundRect(350, 111, 120, 35, 8, COLOR_BTN2);
        sprite.setTextColor(TFT_ORANGE, COLOR_BTN2);
        sprite.setTextSize(1);
        sprite.drawCentreString("CLEAR ALL", 350 + 60, 111 + 17, 2);
    }
    lcd_PushColors_rotated_90(0, 0, 640, 180, (uint16_t*)sprite.getPointer());

    
}

void drawSerialPasswordEntryScreen() {
    sprite.fillSprite(COLOR_BG);
    sprite.setTextColor(TFT_SILVER, COLOR_BG);
    sprite.drawCentreString("Set Passwords", TFT_WIDTH / 2, 20, 4);
    sprite.drawCentreString("Enter Password " + String(setup_password_index) + " in Serial Monitor", TFT_WIDTH / 2, 70, 2);
    sprite.drawCentreString("Press Enter to submit.", TFT_WIDTH / 2, 100, 2);
    sprite.drawCentreString("Submit empty password to finish.", TFT_WIDTH / 2, 130, 2);
    lcd_PushColors_rotated_90(0, 0, TFT_WIDTH, TFT_HEIGHT, (uint16_t*)sprite.getPointer());
}

void drawPasswordSelectionScreen() {
    sprite.fillSprite(COLOR_BG);
    sprite.setTextColor(TFT_SILVER, COLOR_BG);
    sprite.drawCentreString("Select Password", TFT_WIDTH / 2, 15, 4);

    int btn_width = (TFT_WIDTH - 50) / 2;
    int btn_height = 50;
    int x_spacing = 10;
    int y_spacing = 10;
    int start_x = 20;
    int start_y = 60;

    for (int i = 0; i < MAX_PASSWORDS; i++) {
        int row = i / 2;
        int col = i % 2;
        sprite.fillRoundRect(start_x + col * (btn_width + x_spacing), start_y + row * (btn_height + y_spacing), btn_width, btn_height, 8, COLOR_BTN1);
        sprite.drawCentreString("Password " + String(i + 1), start_x + col * (btn_width + x_spacing) + btn_width / 2, start_y + row * (btn_height + y_spacing) + btn_height / 2 - 8, 2);
    }
    lcd_PushColors_rotated_90(0, 0, TFT_WIDTH, TFT_HEIGHT, (uint16_t*)sprite.getPointer());
}

void drawWipeConfirmation() {
    Serial.println("drawWipeConfirmation called");
    sprite.fillSprite(COLOR_BG);
    sprite.setTextColor(TFT_WHITE, COLOR_BG);
    sprite.drawCentreString("WIPE ALL DATA?", TFT_WIDTH / 2, 30, 4);
    sprite.drawCentreString("Long touch to confirm", TFT_WIDTH / 2, 70, 2);
    
    unsigned long hold_time = millis() - wipe_start_time;
    int progress = (hold_time * 100) / WIPE_HOLD_TIME_MS;
    progress = constrain(progress, 0, 100);
    

    lcd_PushColors_rotated_90(0, 0, TFT_WIDTH, TFT_HEIGHT, (uint16_t*)sprite.getPointer());
}

void drawLockedScreen() {
    Serial.println("drawLockedScreen called");
    sprite.fillSprite(COLOR_BG);
    sprite.setTextColor(COLOR_ERROR, COLOR_BG);
    sprite.drawCentreString("DEVICE LOCKED", TFT_WIDTH / 2, 30, 4);
    
    sprite.setTextColor(COLOR_TEXT, COLOR_BG);
    sprite.drawCentreString("Too many incorrect PIN attempts", TFT_WIDTH / 2, 80, 2);
    
    unsigned long remaining = (LOCKOUT_TIME_MS - (millis() - lockout_start_time)) / 1000;
    if (remaining < 0) remaining = 0;
    sprite.drawCentreString("Try again in " + String(remaining) + " seconds", TFT_WIDTH / 2, 110, 2);
    
    sprite.setTextColor(COLOR_ACCENT, COLOR_BG);
    sprite.drawCentreString("Or press 'CLEAR ALL' to wipe device", TFT_WIDTH / 2, 150, 2);
    
    lcd_PushColors_rotated_90(0, 0, TFT_WIDTH, TFT_HEIGHT, (uint16_t*)sprite.getPointer());
}

void drawErrorScreen() {
    Serial.println("drawErrorScreen called");
    sprite.fillSprite(COLOR_BG);
    sprite.setTextColor(COLOR_ERROR, COLOR_BG);
    sprite.drawCentreString("FATAL ERROR", TFT_WIDTH / 2, 30, 4);
    
    sprite.setTextColor(COLOR_TEXT, COLOR_BG);
    sprite.drawCentreString(error_message, TFT_WIDTH / 2, 80, 2);
    
    sprite.setTextColor(COLOR_ACCENT, COLOR_BG);
    sprite.drawCentreString("Device will restart", TFT_WIDTH / 2, 120, 2);
    
    lcd_PushColors_rotated_90(0, 0, TFT_WIDTH, TFT_HEIGHT, (uint16_t*)sprite.getPointer());
}

void drawScreen() {
    Serial.println("drawScreen called. Current state: " + String(currentState));
    switch (currentState) {
        case STATE_SETUP_PIN_NEW:
            Serial.println("Drawing: STATE_SETUP_PIN_NEW");
            drawNumpadScreen("Set New PIN");
            break;
        case STATE_SETUP_PIN_CONFIRM:
            Serial.println("Drawing: STATE_SETUP_PIN_CONFIRM");
            drawNumpadScreen("Confirm PIN", pin_buffer != new_pin_buffer);
            break;
        case STATE_SETUP_PASSWORD:
            Serial.println("Drawing: STATE_SETUP_PASSWORD");
            drawSerialPasswordEntryScreen();
            break;
        case STATE_PIN_ENTRY:
            Serial.println("Drawing: STATE_PIN_ENTRY");
            drawNumpadScreen("Enter PIN to Unlock");
            break;
        case STATE_PASSWORD_SELECTION:
            Serial.println("Drawing: STATE_PASSWORD_SELECTION");
            drawPasswordSelectionScreen();
            break;
        case STATE_WIPE_CONFIRM:
            Serial.println("Drawing: STATE_WIPE_CONFIRM");
            drawWipeConfirmation();
            break;
        case STATE_LOCKED_OUT:
            Serial.println("Drawing: STATE_LOCKED_OUT");
            drawLockedScreen();
            break;
        case STATE_ERROR:
            Serial.println("Drawing: STATE_ERROR");
            drawErrorScreen();
            delay(3000);
            ESP.restart();
            break;
        default:
            Serial.println("drawScreen: Unknown state!");
            break;
    }
}

// ==================== INPUT HANDLING ====================

void handleNumpadInput() {
    uint8_t buff[8] = {0};
    Wire.beginTransmission(TOUCH_I2C_ADDR);
    if (Wire.write(read_touchpad_cmd, 8) != 8) {
        return;
    }
    if (Wire.endTransmission() != 0) {
        return;
    }
    
    uint8_t received = Wire.requestFrom(TOUCH_I2C_ADDR, 8);
    if (received == 8) {
        Wire.readBytes(buff, 8);
    } else {
        return;
    }

    int pointX = AXS_GET_POINT_X(buff, 0);
    int pointY = AXS_GET_POINT_Y(buff, 0);

    if (pointX > 0 && pointY > 0) {
        int tx = map(pointX, 627, 10, 0, TFT_WIDTH);
        int ty = map(pointY, 180, 0, 0, TFT_HEIGHT);

        // Check keypad buttons
        for (int y = 0; y < 4; y++) {
            for (int x = 0; x < 3; x++) {
                if (tx >= xpos[x] && tx <= xpos[x] + 70 &&
                    ty >= ypos[y] && ty <= ypos[y] + 35) {
                    
                    const char* key = btns[y][x];
                    if (strcmp(key, "CLR") == 0) {
                        pin_buffer = "";
                    } else if (strcmp(key, "OK") == 0) {
                        Serial.println("handleOKButton called");
                        handleOKButton();
                    } else {
                        if (pin_buffer.length() < MAX_PIN_LENGTH) {
                            pin_buffer += key;
                        }
                    }
                    drawScreen();
                    return;
                }
            }
        }
        
        // Wipe data button (only in PIN entry mode)
        if (currentState == STATE_PIN_ENTRY &&
            tx >= 350 && tx <= (350 + 120) && ty >= 111 && ty <= (111 + 35)) {
            currentState = STATE_WIPE_CONFIRM;
            wipe_start_time = millis();
            drawScreen();
        }
    }
}

void handlePasswordSelectionInput() {
    uint8_t buff[8] = {0};
    Wire.beginTransmission(TOUCH_I2C_ADDR);
    if (Wire.write(read_touchpad_cmd, 8) != 8) return;
    if (Wire.endTransmission() != 0) return;
    
    uint8_t received = Wire.requestFrom(TOUCH_I2C_ADDR, 8);
    if (received == 8) Wire.readBytes(buff, 8);
    else return;

    int pointX = AXS_GET_POINT_X(buff, 0);
    int pointY = AXS_GET_POINT_Y(buff, 0);

    if (pointX > 0 && pointY > 0) {
        int tx = map(pointX, 627, 10, 0, TFT_WIDTH);
        int ty = map(pointY, 180, 0, 0, TFT_HEIGHT);

        int btn_width = (TFT_WIDTH - 50) / 2;
        int btn_height = 50;
        int x_spacing = 10;
        int y_spacing = 10;
        int start_x = 20;
        int start_y = 60;

        for (int i = 0; i < MAX_PASSWORDS; i++) {
            int row = i / 2;
            int col = i % 2;
            int btn_x = start_x + col * (btn_width + x_spacing);
            int btn_y = start_y + row * (btn_height + y_spacing);

            if (tx >= btn_x && tx <= btn_x + btn_width && ty >= btn_y && ty <= btn_y + btn_height) {
                typePassword(i + 1);
                return;
            }
        }
    }
}
void handleOKButton() {
    Serial.println("handleOKButton called");
    switch (currentState) {
        case STATE_PIN_ENTRY: {
            Serial.println("State: STATE_PIN_ENTRY");
            if (pin_buffer.length() == 0) {
                error_message = "PIN cannot be empty";
                drawScreen();
                Serial.println("PIN is empty. Returning.");
                return;
            }
            
            uint8_t input_hash[HASH_SIZE] = {0};
            if (!hashPIN(pin_buffer, input_hash)) {
                error_message = "Security error";
                currentState = STATE_ERROR;
                Serial.println("PIN hashing failed.");
                drawScreen();
                return;
            }
            
            // Compare hashes
            Serial.println("Comparing PIN hashes...");
            bool match = true;
            for (int i = 0; i < HASH_SIZE; i++) {
                if (input_hash[i] != pin_hash[i]) {
                    match = false;
                    break;
                }
            }
            
            secureWipeBuffer(input_hash, sizeof(input_hash));
            
            if (match) {
                // Successful unlock
                Serial.println("PIN match. Unlocking.");
                currentState = STATE_PASSWORD_SELECTION;
                pin_attempts = 0; // pin_buffer is intentionally kept for decryption
                error_message = "";
                Serial.println("Moving to password selection screen.");
                drawScreen();
                
            } else {
                // Incorrect PIN
                pin_attempts++;
                Serial.println("Incorrect PIN. Attempts: " + String(pin_attempts));
                pin_buffer = "";
                
                if (pin_attempts >= MAX_PIN_ATTEMPTS) {
                    currentState = STATE_LOCKED_OUT;
                    lockout_start_time = millis();
                    error_message = "";
                } else {
                    Serial.println("Locking device out.");
                    error_message = "Incorrect PIN (" + String(MAX_PIN_ATTEMPTS - pin_attempts) + " attempts left)";
                }
                drawScreen();
            }
            break;
        }
        case STATE_SETUP_PIN_NEW:
            Serial.println("State: STATE_SETUP_PIN_NEW");
            if (pin_buffer.length() >= 4) {
                new_pin_buffer = pin_buffer;
                pin_buffer = "";
                error_message = "";
                currentState = STATE_SETUP_PIN_CONFIRM;
                Serial.println("New PIN entered. Moving to confirmation.");
            } else {
                error_message = "PIN must be at least 4 digits";
                Serial.println("PIN too short.");
            }
            drawScreen();
            break;
            
        case STATE_SETUP_PIN_CONFIRM:
            Serial.println("State: STATE_SETUP_PIN_CONFIRM");
            if (pin_buffer == new_pin_buffer) {
                // PIN confirmed, save hash and move to password setup
                Serial.println("PINs match. Hashing and saving.");
                if (!hashPIN(pin_buffer, pin_hash)) {
                    error_message = "Hashing failed";
                    currentState = STATE_ERROR;
                    drawScreen();
                    return;
                }
                
                generateSalt(salt);
                Serial.println("Salt generated. Moving to password setup.");
                
                error_message = "";
                currentState = STATE_SETUP_PASSWORD;
            } else {
                Serial.println("PINs do not match. Returning to new PIN setup.");
                // PIN mismatch
                error_message = "PINs do not match";
                pin_buffer = "";
                currentState = STATE_SETUP_PIN_NEW;
            }
            drawScreen();
            break;
    }
}

void typePassword(int index) {
    String key_name = "enc_pass_" + String(index);
    String encrypted_password = preferences.getString(key_name.c_str(), "");

    if (encrypted_password.length() == 0) {
        Serial.println("No password in slot " + String(index));
        // Optionally provide feedback on screen
        return;
    }

    if (!deriveKey(pin_buffer, salt, aes_key)) {
        error_message = "Key derivation failed";
        currentState = STATE_ERROR;
        Serial.println("Error: Key derivation failed.");
        drawScreen();
        return;
    }

    String decrypted_password;
    if (!decryptPassword(encrypted_password, aes_key, decrypted_password)) {
        error_message = "Decryption failed";
        currentState = STATE_ERROR;
        Serial.println("Error: Password decryption failed.");
        drawScreen();
        return;
    }

    // Type the password
    Serial.println("Typing password " + String(index) + " via USB HID.");
    Keyboard.print(decrypted_password);

    // Securely wipe sensitive data from RAM
    secureWipeBuffer(aes_key, sizeof(aes_key));
    decrypted_password = "";

    // Return to PIN entry screen after typing for security
    Serial.println("Password typed. Returning to PIN entry screen.");
    pin_buffer = ""; // Securely clear the PIN from memory
    currentState = STATE_PIN_ENTRY;
    drawScreen();
}

void handlePasswordOK() {
    Serial.println("handlePasswordOK: Function started.");
    if (password_buffer.length() == 0) {
        Serial.println("handlePasswordOK: Error - Password buffer is empty.");
        error_message = "Password cannot be empty";
        drawScreen();
        return;
    }

    // Derive key and encrypt password
    Serial.println("handlePasswordOK: Deriving key from new PIN...");
    if (!deriveKey(new_pin_buffer, salt, aes_key)) {
        Serial.println("handlePasswordOK: FATAL - Key derivation failed.");
        error_message = "Key derivation failed";
        currentState = STATE_ERROR;
        drawScreen();
        return;
    }
    Serial.println("handlePasswordOK: Key derivation successful.");
    
    String encrypted_password;
    Serial.println("handlePasswordOK: Encrypting password...");
    if (!encryptPassword(password_buffer, aes_key, encrypted_password)) {
        Serial.println("handlePasswordOK: FATAL - Encryption failed.");
        error_message = "Encryption failed";
        currentState = STATE_ERROR;
        drawScreen();
        return;
    }
    Serial.println("handlePasswordOK: Encryption successful.");
    
    // Save this password
    Serial.println("handlePasswordOK: Saving encrypted password to preferences...");
    String key_name = "enc_pass_" + String(setup_password_index);
    if (!preferences.putString(key_name.c_str(), encrypted_password)) {
        Serial.println("handlePasswordOK: FATAL - Failed to save encrypted password.");
        error_message = "Storage failed";
        currentState = STATE_ERROR;
        drawScreen();
        return;
    }
    
    // If this is the first password, we also need to save the PIN hash and salt.
    if (setup_password_index == 1) {
        Serial.println("handlePasswordOK: Saving PIN hash and salt for the first time...");
        if (!saveStoredData()) {
            Serial.println("handlePasswordOK: FATAL - Failed to save PIN hash and salt.");
            error_message = "Storage failed";
            currentState = STATE_ERROR;
            drawScreen();
            return;
        }
    }

    // Move to the next password or finish
    setup_password_index++;
    if (setup_password_index > MAX_PASSWORDS) {
        finishSetup();
    } else {
        // Ready for next password
        password_buffer = "";
        drawScreen();
    }
}

void finishSetup() {
    Serial.println("finishSetup: Finalizing setup...");

    Serial.println("handlePasswordOK: Setting configuration flags...");
    preferences.putBool("is_configured", true);
    preferences.putBool("needs_setup", false);
    
    // Securely wipe sensitive data
    Serial.println("finishSetup: Wiping sensitive data from memory...");
    secureWipeBuffer(aes_key, sizeof(aes_key));
    password_buffer = "";
    new_pin_buffer = "";
    error_message = "";
    
    // Restart device to enter normal operation mode
    Serial.println("handlePasswordOK: Setup complete. Restarting device.");
    ESP.restart();
}

void handleWipeConfirmation() {
    if (wiping) {
        unsigned long current_time = millis();
        if (current_time - wipe_start_time >= WIPE_HOLD_TIME_MS) {
            // Wipe confirmed
            secureWipe();
            ESP.restart();
        }
    }
}

void processTouch() {
    if (touch_detected) {
        unsigned long current_time = millis();
        if (current_time - last_touch_time > TOUCH_DEBOUNCE_MS) {
            last_touch_time = current_time;
            
            if (currentState == STATE_WIPE_CONFIRM) {
                wiping = true;
                Serial.println("handleWipeConfirmation called");
                handleWipeConfirmation();
            } else if (currentState == STATE_PASSWORD_SELECTION) {
                Serial.println("handlePasswordSelectionInput called");
                handlePasswordSelectionInput();
            } else if (currentState == STATE_PIN_ENTRY || currentState == STATE_SETUP_PIN_NEW || currentState == STATE_SETUP_PIN_CONFIRM) {
                Serial.println("handleNumpadInput called");
                handleNumpadInput();
            } else if (currentState == STATE_LOCKED_OUT) {
                // Check if lockout period has expired
                if (current_time - lockout_start_time >= LOCKOUT_TIME_MS) {
                    currentState = STATE_PIN_ENTRY;
                    pin_attempts = 0;
                    error_message = "";
                    Serial.println("STATE_LOCKED_OUT drawScreen called");
                    drawScreen();
                }
            }
        }
        touch_detected = false;
        wiping = false;
    }
}

// ==================== INITIALIZATION ====================

void initializeAppState() {
    preferences.begin("secure-pass", false);
    
    if (preferences.getBool("needs_setup", true) || 
        !preferences.isKey("is_configured")) {
        
        currentState = STATE_SETUP_PIN_NEW;
        preferences.putBool("needs_setup", false);
        
    } else {
        if (!loadStoredData()) {
            error_message = "Corrupted data - needs setup";
            currentState = STATE_SETUP_PIN_NEW;
            preferences.putBool("needs_setup", true);
        } else {
            currentState = STATE_PIN_ENTRY;
            Keyboard.begin();
            USB.begin();
        }
    }
}

void setup() {
    Serial.begin(115200);
    Serial.println("Starting Secure Password Device...");
    pinMode(TOUCH_INT, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(TOUCH_INT), handleTouchInterrupt, FALLING);
    // Initialize hardware
    initializeDisplay();

    
    // Backlight
    pinMode(TFT_BL, OUTPUT);
    digitalWrite(TFT_BL, HIGH);
    initializeTouch();
    // Initialize application state
    initializeAppState();
    axs15231_init();
    

    drawScreen();
    //drawNumpadScreen("Set New PIN");
    Serial.println("Device started successfully");
}

void loop() {
    processTouch();

    if (currentState == STATE_SETUP_PASSWORD) {
        if (Serial.available() > 0) {
            password_buffer = Serial.readStringUntil('\n');
            password_buffer.trim(); // Remove any leading/trailing whitespace
            Serial.println("Password received via Serial: " + password_buffer);
            if (password_buffer.length() == 0) {
                // Empty password means user is done with setup
                finishSetup();
            } else {
                // Process the entered password
                handlePasswordOK();
            }
        }
    }
    
    // Handle wipe confirmation progress
    if (currentState == STATE_WIPE_CONFIRM && wiping) {
        handleWipeConfirmation();
        drawNumpadScreen("Set New PIN");
        drawScreen();
    }
    
    // Update locked screen timer
    if (currentState == STATE_LOCKED_OUT) {
        static unsigned long last_update = 0;
        if (millis() - last_update > 1000) {
            drawScreen();
            last_update = millis();
        }
    }
    
    delay(10);
}
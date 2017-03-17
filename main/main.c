#include "driver/adc.h"
#include "driver/i2c.h"
#include "sdkconfig.h"
#include "driver/gpio.h"
#include "esp_event.h"
#include "esp_event_loop.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "nvs_flash.h"
#include <gde.h>
#include <gdeh029a1.h>
#include <pictures.h>
#include <pins.h>

#ifdef CONFIG_SHA_BADGE_V1
#define PIN_NUM_LED 22
#define PIN_NUM_BUTTON_A 0
#define PIN_NUM_BUTTON_B 27
#define PIN_NUM_BUTTON_MID 25
#define PIN_NUM_BUTTON_UP 26
#define PIN_NUM_BUTTON_DOWN 32
#define PIN_NUM_BUTTON_LEFT 33
#define PIN_NUM_BUTTON_RIGHT 35
#else
#define PIN_NUM_BUTTON_MID    25
#endif

esp_err_t event_handler(void *ctx, system_event_t *event) { return ESP_OK; }

#define I2C_MASTER_SCL_IO     27    /*!<gpio number for i2c slave clock  */
#define I2C_MASTER_SDA_IO     26    /*!<gpio number for i2c slave data */
#define I2C_MASTER_NUM I2C_NUM_1   /*!< I2C port number for master dev */
#define I2C_MASTER_TX_BUF_DISABLE   0   /*!< I2C master do not need buffer */
#define I2C_MASTER_RX_BUF_DISABLE   0   /*!< I2C master do not need buffer */
#define I2C_MASTER_FREQ_HZ    100000     /*!< I2C master clock frequency */
#define ESP_SLAVE_ADDR 0x70         /*!< ESP32 slave address, you can set any 7bit value */
#define WRITE_BIT  I2C_MASTER_WRITE /*!< I2C master write */
#define READ_BIT   I2C_MASTER_READ  /*!< I2C master read */
#define ACK_CHECK_EN   0x1     /*!< I2C master will check ack from slave*/
#define ACK_CHECK_DIS  0x0     /*!< I2C master will not check ack from slave */
#define ACK_VAL    0x0         /*!< I2C ack value */
#define NACK_VAL   0x1         /*!< I2C nack value */

#define I2C_EXPANDER_ADDR 0b1000100 // FXL6408 port expander address (ADDR=1)
uint8_t expander_output_state = 0x0;

uint32_t
get_buttons(void)
{
	uint32_t bits = 0;
#ifdef CONFIG_SHA_BADGE_V1
	bits |= gpio_get_level(PIN_NUM_BUTTON_A)     << 0; // A
	bits |= gpio_get_level(PIN_NUM_BUTTON_B)     << 1; // B
#endif // CONFIG_SHA_BADGE_V1
	bits |= gpio_get_level(PIN_NUM_BUTTON_MID)   << 2; // MID
#ifdef CONFIG_SHA_BADGE_V1
	bits |= gpio_get_level(PIN_NUM_BUTTON_UP)    << 3; // UP
	bits |= gpio_get_level(PIN_NUM_BUTTON_DOWN)  << 4; // DOWN
	bits |= gpio_get_level(PIN_NUM_BUTTON_LEFT)  << 5; // LEFT
	bits |= gpio_get_level(PIN_NUM_BUTTON_RIGHT) << 6; // RIGHT
#endif // CONFIG_SHA_BADGE_V1
	bits |= gpio_get_level(PIN_NUM_BUSY)         << 7; // GDE BUSY
	return bits;
}

#include "event_queue.h"

uint32_t buttons_state = 0;

void gpio_intr_test(void *arg) {
  // read status to get interrupt status for GPIO 0-31
  uint32_t gpio_intr_status_lo = READ_PERI_REG(GPIO_STATUS_REG);
  // read status to get interrupt status for GPIO 32-39
  uint32_t gpio_intr_status_hi = READ_PERI_REG(GPIO_STATUS1_REG);
  // clear intr for GPIO 0-31
  SET_PERI_REG_MASK(GPIO_STATUS_W1TC_REG, gpio_intr_status_lo);
  // clear intr for GPIO 32-39
  SET_PERI_REG_MASK(GPIO_STATUS1_W1TC_REG, gpio_intr_status_hi);

  uint32_t buttons_new = get_buttons();
  uint32_t buttons_down = (~buttons_new) & buttons_state;
  uint32_t buttons_up = buttons_new & (~buttons_state);
  buttons_state = buttons_new;

  if (buttons_down != 0)
    xQueueSendFromISR(evt_queue, &buttons_down, NULL);

  if (buttons_down & (1 << 0))
    ets_printf("Button A\n");
  if (buttons_down & (1 << 1))
    ets_printf("Button B\n");
  if (buttons_down & (1 << 2))
    ets_printf("Button MID\n");
  if (buttons_down & (1 << 3))
    ets_printf("Button UP\n");
  if (buttons_down & (1 << 4))
    ets_printf("Button DOWN\n");
  if (buttons_down & (1 << 5))
    ets_printf("Button LEFT\n");
  if (buttons_down & (1 << 6))
    ets_printf("Button RIGHT\n");
  if (buttons_down & (1 << 7))
    ets_printf("GDE-Busy down\n");
  if (buttons_up & (1 << 7))
    ets_printf("GDE-Busy up\n");

#ifdef PIN_NUM_LED
  // pass on BUSY signal to LED.
  gpio_set_level(PIN_NUM_LED, 1 - gpio_get_level(PIN_NUM_BUSY));
#endif // PIN_NUM_LED
}

struct menu_item {
  const char *title;
  void (*handler)(void);
};

#include "demo_text1.h"
#include "demo_text2.h"
#include "demo_greyscale1.h"
#include "demo_greyscale2.h"
#include "demo_greyscale_img1.h"
#include "demo_greyscale_img2.h"
#include "demo_greyscale_img3.h"
#include "demo_partial_update.h"
#include "demo_dot1.h"
#include "demo_test_adc.h"

const struct menu_item demoMenu[] = {
    {"text demo 1", &demoText1},
    {"text demo 2", &demoText2},
    {"greyscale 1", &demoGreyscale1},
    {"greyscale 2", &demoGreyscale2},
    {"greyscale image 1", &demoGreyscaleImg1},
    {"greyscale image 2", &demoGreyscaleImg2},
    {"greyscale image 3", &demoGreyscaleImg3},
    {"partial update test", &demoPartialUpdate},
    {"dot 1", &demoDot1},
    {"ADC test", &demoTestAdc},
    {"tetris?", NULL},
    {"something else", NULL},
    {"test, test, test", NULL},
    {"another item..", NULL},
    {"dot 2", NULL},
    {"dot 3", NULL},
    {NULL, NULL},
};

#define MENU_UPDATE_CYCLES 8
void displayMenu(const char *menu_title, const struct menu_item *itemlist) {
  int num_items = 0;
  while (itemlist[num_items].title != NULL)
    num_items++;

  int scroll_pos = 0;
  int item_pos = 0;
  int num_draw = 0;
  while (1) {
    TickType_t xTicksToWait = portMAX_DELAY;

    /* draw menu */
    if (num_draw < 2) {
      // init buffer
      drawText(14, 0, DISP_SIZE_Y, menu_title,
               FONT_16PX | FONT_INVERT | FONT_FULL_WIDTH | FONT_UNDERLINE_2);
      int i;
      for (i = 0; i < 7; i++) {
        int pos = scroll_pos + i;
        drawText(12 - 2 * i, 0, DISP_SIZE_Y,
                 (pos < num_items) ? itemlist[pos].title : "",
                 FONT_16PX | FONT_FULL_WIDTH |
                     ((pos == item_pos) ? 0 : FONT_INVERT));
      }
    }
    if (num_draw == 0) {
      // init LUT
      static const uint8_t lut[30] = {
          0x99, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
          0,    0, 0, 0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0,
      };
      gdeWriteCommandStream(0x32, lut, 30);
      // init timings
      gdeWriteCommand_p1(0x3a, 0x02); // 2 dummy lines per gate
      gdeWriteCommand_p1(0x3b, 0x00); // 30us per line
      //			gdeWriteCommand_p1(0x3a, 0x1a); // 26 dummy
      //lines per gate
      //			gdeWriteCommand_p1(0x3b, 0x08); // 62us per line
    }
    if (num_draw < MENU_UPDATE_CYCLES) {
      updateDisplay();
      gdeBusyWait();
      num_draw++;
      if (num_draw < MENU_UPDATE_CYCLES)
        xTicksToWait = 0;
    }

    /* handle input */
    uint32_t buttons_down;
    if (xQueueReceive(evt_queue, &buttons_down, xTicksToWait)) {
      if (buttons_down & (1 << 1)) {
        ets_printf("Button B handling\n");
        return;
      }
      if (buttons_down & (1 << 2)) {
        ets_printf("Selected '%s'\n", itemlist[item_pos].title);
        if (itemlist[item_pos].handler != NULL)
          itemlist[item_pos].handler();
        num_draw = 0;
        ets_printf("Button MID handled\n");
        continue;
      }
      if (buttons_down & (1 << 3)) {
        if (item_pos > 0) {
          item_pos--;
          if (scroll_pos > item_pos)
            scroll_pos = item_pos;
          num_draw = 0;
        }
        ets_printf("Button UP handled\n");
      }
      if (buttons_down & (1 << 4)) {
        if (item_pos + 1 < num_items) {
          item_pos++;
          if (scroll_pos + 6 < item_pos)
            scroll_pos = item_pos - 6;
          num_draw = 0;
        }
        ets_printf("Button DOWN handled\n");
      }
    }
  }
}

// interesting stuff here

/**
 * @brief test code to read esp-i2c-slave
 *        We need to fill the buffer of esp slave device, then master can read them out.
 *
 * _______________________________________________________________________________________
 * | start | slave_addr + rd_bit +ack | read n-1 bytes + ack | read 1 byte + nack | stop |
 * --------|--------------------------|----------------------|--------------------|------|
 *
 */
esp_err_t i2c_master_read_slave(i2c_port_t i2c_num, uint8_t* data_rd, size_t size)
{
    if (size == 0) {
        return ESP_OK;
    }
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, ( ESP_SLAVE_ADDR << 1 ) | READ_BIT, ACK_CHECK_EN);
    if (size > 1) {
        i2c_master_read(cmd, data_rd, size - 1, ACK_VAL);
    }
    i2c_master_read_byte(cmd, data_rd + size - 1, NACK_VAL);
    i2c_master_stop(cmd);
    esp_err_t ret = i2c_master_cmd_begin(i2c_num, cmd, 1000 / portTICK_RATE_MS);
    i2c_cmd_link_delete(cmd);
    return ret;
}

/**
 * @brief Test code to write esp-i2c-slave
 *        Master device write data to slave(both esp32),
 *        the data will be stored in slave buffer.
 *        We can read them out from slave buffer.
 *
 * ___________________________________________________________________
 * | start | slave_addr + wr_bit + ack | write n bytes + ack  | stop |
 * --------|---------------------------|----------------------|------|
 *
 */
esp_err_t i2c_master_write_slave(i2c_port_t i2c_num, uint8_t* data_wr, size_t size)
{
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, ( ESP_SLAVE_ADDR << 1 ) | WRITE_BIT, ACK_CHECK_EN);
    i2c_master_write(cmd, data_wr, size, ACK_CHECK_EN);
    i2c_master_stop(cmd);
    esp_err_t ret = i2c_master_cmd_begin(i2c_num, cmd, 1000 / portTICK_RATE_MS);
    i2c_cmd_link_delete(cmd);
    return ret;
}

/**
 * @brief i2c master initialization
 */
void i2c_master_init()
{
    int i2c_master_port = I2C_MASTER_NUM;
    i2c_config_t conf;
    conf.mode = I2C_MODE_MASTER;
    conf.sda_io_num = I2C_MASTER_SDA_IO;
    conf.sda_pullup_en = GPIO_PULLUP_ENABLE;
    conf.scl_io_num = I2C_MASTER_SCL_IO;
//    conf.scl_pullup_en = GPIO_PULLUP_ENABLE;
    conf.master.clk_speed = I2C_MASTER_FREQ_HZ;
    i2c_param_config(i2c_master_port, &conf);
    i2c_driver_install(i2c_master_port, conf.mode, I2C_MASTER_RX_BUF_DISABLE, I2C_MASTER_TX_BUF_DISABLE, 0);
}

/**
 * Write simple byte to I2C slave (hopefully)
 * ___________________________________________________________________________________________________
 * | start | slave_addr + wr_bit + ack | write RA (1 byte) + ack  | write data (1 byte) + ack | stop |
 * |-------|---------------------------|--------------------------|---------------------------|------|
 */
void i2c_write(uint8_t addr, uint8_t RA, uint8_t data) {

    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);

    i2c_master_write_byte(cmd, addr << 1 | WRITE_BIT, ACK_CHECK_EN);
    i2c_master_write_byte(cmd, RA, ACK_CHECK_EN);
    i2c_master_write_byte(cmd, data, ACK_CHECK_EN);
    i2c_master_stop(cmd);

    // Timeout 1 second
    int ret = i2c_master_cmd_begin(2, cmd, 1000 / portTICK_RATE_MS);
    i2c_cmd_link_delete(cmd);

    if (ret == ESP_FAIL) {
        // FAIL, handle?
    }
}


void i2c_expander_init() {
    // IO direction (0 = input, 1 = output)
    i2c_write(I2C_EXPANDER_ADDR, 0x03, 0b00000110);

    expander_output_state = 0x0;
    i2c_write(I2C_EXPANDER_ADDR, 0x05, expander_output_state);

}

/**
 * Pager motor stuff
 */
void buzz_pager(int duration) {
    expander_output_state |= 1 << 1;
    i2c_write(I2C_EXPANDER_ADDR, 0x05, expander_output_state);

    vTaskDelay(duration / portTICK_RATE_MS);

    expander_output_state &= ~(1 << 1);
    i2c_write(I2C_EXPANDER_ADDR, 0x05, expander_output_state);
}

void
app_main(void) {
	nvs_flash_init();

	/* create event queue */
	evt_queue = xQueueCreate(10, sizeof(uint32_t));

	/** configure input **/
	gpio_isr_register(gpio_intr_test, NULL, 0, NULL);

#ifdef CONFIG_SHA_BADGE_V2
    i2c_master_init();
    i2c_expander_init();
    buzz_pager(500);
#endif

	gpio_config_t io_conf;
	io_conf.intr_type = GPIO_INTR_ANYEDGE;
	io_conf.mode = GPIO_MODE_INPUT;
	io_conf.pin_bit_mask =
		(1LL << PIN_NUM_BUSY) |
#ifdef CONFIG_SHA_BADGE_V1
		(1LL << PIN_NUM_BUTTON_A) |
		(1LL << PIN_NUM_BUTTON_B) |
#endif // CONFIG_SHA_BADGE_V1
		(1LL << PIN_NUM_BUTTON_MID) |
#ifdef CONFIG_SHA_BADGE_V1
		(1LL << PIN_NUM_BUTTON_UP) |
		(1LL << PIN_NUM_BUTTON_DOWN) |
		(1LL << PIN_NUM_BUTTON_LEFT) |
		(1LL << PIN_NUM_BUTTON_RIGHT) |
#endif // CONFIG_SHA_BADGE_V1
		0LL;
	io_conf.pull_down_en = 0;
	io_conf.pull_up_en = 1;
	gpio_config(&io_conf);


  //i2c_master_init();

	/** configure output **/
#ifdef PIN_NUM_LED
  gpio_pad_select_gpio(PIN_NUM_LED);
  gpio_set_direction(PIN_NUM_LED, GPIO_MODE_OUTPUT);
#endif // PIN_NUM_LED

  tcpip_adapter_init();
  ESP_ERROR_CHECK(esp_event_loop_init(event_handler, NULL));

#ifdef CONFIG_WIFI_USE
  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  ESP_ERROR_CHECK(esp_wifi_init(&cfg));
  ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
  ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
  wifi_config_t sta_config = {
      .sta = {.ssid = CONFIG_WIFI_SSID, .password = CONFIG_WIFI_PASSWORD, .bssid_set = false}};
  ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &sta_config));
  ESP_ERROR_CHECK(esp_wifi_start());
  ESP_ERROR_CHECK(esp_wifi_connect());
#endif // CONFIG_WIFI_USE

  gdeInit();
  initDisplay(LUT_DEFAULT); // configure slow LUT

  int picture_id = 0;
  drawImage(pictures[picture_id]);
  updateDisplay();
  gdeBusyWait();

  int selected_lut = LUT_PART;
  writeLUT(selected_lut); // configure fast LUT

  while (1) {
    uint32_t buttons_down;
    if (xQueueReceive(evt_queue, &buttons_down, portMAX_DELAY)) {
      if (buttons_down & (1 << 1)) {
        ets_printf("Button B handling\n");
        /* redraw with default LUT */
        writeLUT(LUT_DEFAULT);
        drawImage(pictures[picture_id]);
        updateDisplay();
        gdeBusyWait();
        writeLUT(selected_lut);
      }
      if (buttons_down & (1 << 2)) {
        ets_printf("Button MID handling\n");
        /* open menu */
        displayMenu("Demo menu", demoMenu);

        writeLUT(selected_lut);
        drawImage(pictures[picture_id]);
        updateDisplay();
        gdeBusyWait();
      }
      if (buttons_down & (1 << 3)) {
        ets_printf("Button UP handling\n");
        /* switch LUT */
        selected_lut = (selected_lut + 1) % (LUT_MAX + 1);
        writeLUT(selected_lut);
        drawImage(pictures[picture_id]);
        updateDisplay();
        gdeBusyWait();
      }
      if (buttons_down & (1 << 4)) {
        ets_printf("Button DOWN handling\n");
        /* switch LUT */
        selected_lut = (selected_lut + LUT_MAX) % (LUT_MAX + 1);
        writeLUT(selected_lut);
        drawImage(pictures[picture_id]);
        updateDisplay();
        gdeBusyWait();
      }
      if (buttons_down & (1 << 5)) {
        ets_printf("Button LEFT handling\n");
        /* previous picture */
        if (picture_id > 0) {
          picture_id--;
          drawImage(pictures[picture_id]);
          updateDisplay();
          gdeBusyWait();
        }
      }
      if (buttons_down & (1 << 6)) {
        ets_printf("Button RIGHT handling\n");
        /* next picture */
        if (picture_id + 1 < NUM_PICTURES) {
          picture_id++;
          drawImage(pictures[picture_id]);
          updateDisplay();
          gdeBusyWait();
        }
      }
    }
  }
}

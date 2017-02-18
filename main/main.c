#include "sdkconfig.h"
#include "driver/adc.h"
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

#include "img_hacking.h"

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
  gpio_set_level(PIN_NUM_LED, 1 - gpio_get_level(21));
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

void demoGreyscaleImg3(void) {
  // curved
  const uint8_t lvl_buf[128] = {
      0,   4,   9,   13,  17,  22,  26,  30,  34,  38,  42,  45,  49,  53,  57,
      60,  64,  67,  71,  74,  77,  81,  84,  87,  90,  93,  97,  100, 103, 105,
      108, 111, 114, 117, 119, 122, 125, 127, 130, 132, 135, 137, 140, 142, 145,
      147, 149, 151, 154, 156, 158, 160, 162, 164, 166, 168, 170, 172, 174, 176,
      178, 179, 181, 183, 185, 186, 188, 190, 191, 193, 195, 196, 198, 199, 201,
      202, 204, 205, 206, 208, 209, 211, 212, 213, 214, 216, 217, 218, 219, 221,
      222, 223, 224, 225, 226, 227, 228, 230, 231, 232, 233, 234, 235, 236, 237,
      237, 238, 239, 240, 241, 242, 243, 244, 245, 245, 246, 247, 248, 249, 249,
      250, 251, 252, 252, 253, 254, 254, 255 // e=0.15, n=127
  };

  /* update LUT */
  writeLUT(LUT_DEFAULT);
  gdeWriteCommand_p1(0x3a, 0x1a); // 26 dummy lines per gate
  gdeWriteCommand_p1(0x3b, 0x08); // 62us per line

  // trying to get rid of all ghosting and end with a black screen.
  int i;
  for (i = 0; i < 3; i++) {
    /* draw initial pattern */
    setRamArea(0, DISP_SIZE_X_B - 1, 0, DISP_SIZE_Y - 1);
    setRamPointer(0, 0);
    gdeWriteCommandInit(0x24);
    int c;
    for (c = 0; c < DISP_SIZE_X_B * DISP_SIZE_Y; c++)
      gdeWriteByte((i & 1) ? 0xff : 0x00);
    gdeWriteCommandEnd();

    /* update display */
    updateDisplay();
    gdeBusyWait();
  }

  gdeWriteCommand_p1(0x3a, 0x00); // no dummy lines per gate
  gdeWriteCommand_p1(0x3b, 0x00); // 30us per line

  for (i = 64; i > 0; i >>= 1) {
    int ii = i;
    int p = 8;

    while ((ii & 1) == 0 && (p > 1)) {
      ii >>= 1;
      p >>= 1;
    }

    int j;
    for (j = 0; j < p; j++) {
      int y_start = 0 + j * (DISP_SIZE_Y / p);
      int y_end = y_start + (DISP_SIZE_Y / p) - 1;

      setRamArea(0, DISP_SIZE_X_B - 1, 0, DISP_SIZE_Y - 1);
      setRamPointer(0, 0);
      gdeWriteCommandInit(0x24);
      int x, y;
      const uint8_t *ptr = img_hacking;
      for (y = 0; y < DISP_SIZE_Y; y++) {
        uint8_t res = 0;
        for (x = 0; x < DISP_SIZE_X; x++) {
          res <<= 1;
          uint8_t pixel = *ptr++;
          int j;
          for (j = 0; pixel > lvl_buf[j]; j++)
            ;
          if (y >= y_start && y <= y_end && j & i)
            res++;
          if ((x & 7) == 7)
            gdeWriteByte(res);
        }
      }
      gdeWriteCommandEnd();

      // LUT:
      //   Ignore old state;
      //   Do nothing when bit is not set;
      //   Make pixel whiter when bit is set;
      //   Duration is <ii> cycles.
      uint8_t lut[30] = {
          0x88, 0, 0, 0, 0, 0,  0, 0, 0, 0, 0, 0, 0, 0, 0,
          0,    0, 0, 0, 0, ii, 0, 0, 0, 0, 0, 0, 0, 0, 0,
      };
      gdeWriteCommandStream(0x32, lut, 30);

      /* update display */
      updateDisplayPartial(y_start, y_end + 1);
      gdeBusyWait();
    }
  }

  gdeWriteCommand_p1(0x3a, 0x1a); // 26 dummy lines per gate
  gdeWriteCommand_p1(0x3b, 0x08); // 62us per line

  // wait for random keypress
  uint32_t buttons_down = 0;
  while ((buttons_down & 0x7f) == 0)
    xQueueReceive(evt_queue, &buttons_down, portMAX_DELAY);
}

void demoPartialUpdate(void) {
  /* update LUT */
  writeLUT(LUT_DEFAULT);

  // tweak timing a bit.. (FIXME)
  gdeWriteCommand_p1(0x3a, 0x3f); // 63 dummy lines per gate
  gdeWriteCommand_p1(0x3b, 0x0f); // 208us per line

  int i;
  for (i = 0; i < 8; i++) {
    int j = ((i << 1) | (i >> 2)) & 7;
    drawImage(gImage_sha);
    updateDisplayPartial(37 * j, 37 * j + 36);
    gdeBusyWait();
    vTaskDelay(1000 / portTICK_PERIOD_MS);
  }

  for (i = 0; i < 8; i++) {
    int j = ((i << 1) | (i >> 2)) & 7;
    drawImage(gImage_nick);
    updateDisplayPartial(37 * j, 37 * j + 36);
    gdeBusyWait();
    vTaskDelay(1000 / portTICK_PERIOD_MS);
  }

  gdeWriteCommand_p1(0x3a, 0x1a); // 26 dummy lines per gate
  gdeWriteCommand_p1(0x3b, 0x08); // 62us per line

  // wait for random keypress
  uint32_t buttons_down = 0;
  while ((buttons_down & 0x7f) == 0)
    xQueueReceive(evt_queue, &buttons_down, portMAX_DELAY);
}

void demoDot1(void) {
  /* update LUT */
  writeLUT(LUT_DEFAULT);

  /* clear screen */
  setRamArea(0, DISP_SIZE_X_B - 1, 0, DISP_SIZE_Y - 1);
  setRamPointer(0, 0);
  gdeWriteCommandInit(0x24);
  {
    int x, y;
    for (y = 0; y < DISP_SIZE_Y; y++) {
      for (x = 0; x < 16; x++)
        gdeWriteByte(0xff);
    }
  }
  gdeWriteCommandEnd();

  /* update display */
  updateDisplay();
  gdeBusyWait();

  // init LUT
  static const uint8_t lut[30] = {
      //		0x18,0,0,0,0,0,0,0,0,0,
      0x99, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
      0,    0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  };
  gdeWriteCommandStream(0x32, lut, 30);

  // tweak timing a bit..
  gdeWriteCommand_p1(0x3a, 0x02); // 2 dummy lines per gate
  gdeWriteCommand_p1(0x3b, 0x00); // 30us per line

  int px = 100;
  int py = 64;
  int xd = 1;
  int yd = 1;

  uint16_t dots[16] = {
      0,
  };
  int dot_pos = 0;

  while (1) {
    uint32_t buttons_down = 0;
    if (xQueueReceive(evt_queue, &buttons_down, portMAX_DELAY))
      if ((buttons_down & 0x7f) != 0)
        return;

    /* update dot */
    if (px + xd < 0)
      xd = -xd;
    if (px + xd >= DISP_SIZE_X)
      xd = -xd;
    px += xd;

    if (py + yd < 0)
      yd = -yd;
    if (py + yd >= DISP_SIZE_Y)
      yd = -yd;
    py += yd;

    dots[dot_pos] = (py << 7) | px;
    dot_pos++;
    if (dot_pos == 16)
      dot_pos = 0;

    /* clear screen */
    setRamArea(0, DISP_SIZE_X_B - 1, 0, DISP_SIZE_Y - 1);
    setRamPointer(0, 0);
    gdeWriteCommandInit(0x24);
    {
      int x, y;
      for (y = 0; y < DISP_SIZE_Y; y++) {
        for (x = 0; x < 16; x++)
          gdeWriteByte(0xff);
      }
    }
    gdeWriteCommandEnd();

    int i;
    for (i = 0; i < 16; i++) {
      int px = dots[i] & 127;
      int py = dots[i] >> 7;
      setRamPointer(px >> 3, py);
      gdeWriteCommand_p1(0x24, 0xff ^ (128 >> (px & 7)));
    }

    /* update display */
    updateDisplay();
    gdeBusyWait();
  }
}

void testAdc(void) {
  int channel;
  int adc_mask = 0xFF;

  adc1_config_width(ADC_WIDTH_12Bit);
  for (channel = 0; channel < ADC1_CHANNEL_MAX; channel++)
    if (adc_mask & (1 << channel))
      adc1_config_channel_atten(channel, ADC_ATTEN_0db);

  while (1) {
    uint32_t buttons_down = 0;

    /* update LUT */
    writeLUT(LUT_DEFAULT);

    /* clear screen */
    setRamArea(0, DISP_SIZE_X_B - 1, 0, DISP_SIZE_Y - 1);
    setRamPointer(0, 0);
    gdeWriteCommandInit(0x24);
    {
      int x, y;
      for (y = 0; y < DISP_SIZE_Y; y++) {
        for (x = 0; x < 16; x++)
          gdeWriteByte(0xff);
      }
    }
    gdeWriteCommandEnd();

    for (channel = 0; channel < ADC1_CHANNEL_MAX; channel++) {
#define TEXTLEN 32
      char text[TEXTLEN];
      int val;
      if (adc_mask & (1 << channel))
        val = adc1_get_voltage(channel);
      else
        val = -1;
      snprintf(text, TEXTLEN, "ADC channel %d: %d", channel, val);
      drawText(14 - channel, 16, -16, text,
               FONT_FULL_WIDTH | FONT_MONOSPACE | FONT_INVERT);
      ets_printf("%s\n", text);
    }

    /* update display */
    updateDisplay();
    gdeBusyWait();

    if (xQueueReceive(evt_queue, &buttons_down, portMAX_DELAY))
      if ((buttons_down & 0x7f) != 0)
        return;
  }
}

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
    {"ADC test", &testAdc},
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

void
app_main(void) {
	nvs_flash_init();

	/* create event queue */
	evt_queue = xQueueCreate(10, sizeof(uint32_t));

	/** configure input **/
	gpio_isr_register(gpio_intr_test, NULL, 0, NULL);

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

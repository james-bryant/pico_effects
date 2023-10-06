/*

MIT No Attribution

Copyright (c) 2021-2023 Mika Tuupola

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

-cut-

SPDX-License-Identifier: MIT-0

*/

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <malloc.h>
#include <wchar.h>
#include <pico/stdlib.h>
#include <hardware/clocks.h>

#include <hardware/clocks.h>

#include <hagl_hal.h>
#include <hagl.h>
#include <mipi_dcs.h>
#include <font6x9.h>
#include <fps.h>
#include <aps.h>

#include "metaballs.h"
#include "plasma.h"
#include "rotozoom.h"
#include "deform.h"

static uint8_t effect = 1;
volatile bool fps_flag = false;
volatile bool switch_flag = true;
static fps_instance_t fps;
static aps_instance_t bps;

static hagl_backend_t *display1;
static hagl_backend_t *display2;
static hagl_backend_t *display3;

wchar_t message[32];

static const uint64_t US_PER_FRAME_60_FPS = 1000000 / 60;
static const uint64_t US_PER_FRAME_30_FPS = 1000000 / 30;
static const uint64_t US_PER_FRAME_25_FPS = 1000000 / 25;

static char demo[4][32] = {
    "METABALLS",
    "PLASMA",
    "ROTOZOOM",
    "DEFORM",
};

static void
init_cs_pin(int16_t pin)
{
    gpio_set_function(pin, GPIO_FUNC_SIO);
    gpio_set_dir(pin, GPIO_OUT);
 
    /* Set CS high to ignore any traffic on SPI bus. */
    gpio_put(pin, 1);
}

size_t
total_heap()
{
    extern char __StackLimit, __bss_end__;

    return &__StackLimit  - &__bss_end__;
}

size_t
free_heap(void)
{
    struct mallinfo m = mallinfo();

    return total_heap() - m.uordblks;
}

bool
switch_timer_callback(struct repeating_timer *t)
{
    switch_flag = true;
    return true;
}

bool
show_timer_callback(struct repeating_timer *t)
{
    fps_flag = true;
    return true;
}

void static inline
switch_demo()
{
    switch_flag = false;

    switch(effect) {
        case 0:
        case 1:
            plasma_close();
            break;
        case 2:
            plasma_close();
        case 3:
            deform_close();
            break;
    }

    effect = (effect + 1) % 4;

    switch(effect) {
        case 0:
            metaballs_init(display1);
            plasma_init(display2);
            rotozoom_init(display3);
            printf("[main] Initialized metaballs(1), plasma(2), and rotozoom(3) %d free heap \r\n", free_heap());
            break;
        case 1:
            plasma_init(display1);
            metaballs_init(display2);
            rotozoom_init(display3);
            printf("[main] Initialized plasma(1), metaballs(2), and rotozoom(3) %d free heap \r\n", free_heap());
            break;
        case 2:
            rotozoom_init(display1);
            deform_init(display2);
            plasma_init(display3);
            printf("[main] Initialized rotozoom(1), deform(2), and plasma(3), %d free heap \r\n", free_heap());
            break;
        case 3:
            deform_init(display1);
            rotozoom_init(display2);
            metaballs_init(display3);
            printf("[main] Initialized deform(1), rotozoom(2), and metaballs(3), %d free heap \r\n", free_heap());
            break;
    }

    fps_init(&fps);
    aps_init(&bps);
}

void static inline
show_fps()
{
    hagl_color_t green = hagl_color(display1, 0, 255, 0);

    fps_flag = 0;

    /* Set clip window to full screen so we can display the messages. */
    hagl_set_clip(display1, 0, 0, display1->width - 1, display1->height - 1);

    /* Print the message on top left corner. */
    swprintf(message, sizeof(message), L"%s    ", demo[effect]);
    hagl_put_text(display1, message, 4, 4, green, font6x9);

    /* Print the message on lower left corner. */
    swprintf(message, sizeof(message), L"%.*f FPS  ", 0, fps.current);
    hagl_put_text(display1, message, 4, display1->height - 14, green, font6x9);

    /* Print the message on lower right corner. */
    swprintf(message, sizeof(message), L"%.*f KBPS  ", 0, bps.current / 1024);
    hagl_put_text(display1, message, display1->width - 60, display1->height - 14, green, font6x9);

    /* Set clip window back to smaller so effects do not mess the messages. */
    hagl_set_clip(display1, 0, 20, display1->width - 1, display1->height - 21);
}

int
main()
{
    size_t bytes = 0;
    struct repeating_timer switch_timer;
    struct repeating_timer show_timer;

    // set_sys_clock_khz(133000, true);
    // clock_configure(
    //     clk_peri,
    //     0,
    //     CLOCKS_CLK_PERI_CTRL_AUXSRC_VALUE_CLKSRC_PLL_SYS,
    //     133000 * 1000,
    //     133000 * 1000
    // );

    stdio_init_all();

    /* Sleep so that we have time to open the serial console. */
    sleep_ms(5000);

    printf("[main] %d total heap \r\n", total_heap());
    printf("[main] %d free heap \r\n", free_heap());

    fps_init(&fps);

    // memset(&backend, 0, sizeof(hagl_backend_t));
    // backend.buffer = malloc(MIPI_display->width * MIPI_display->height * (DISPLAY_DEPTH / 8));
    // backend.buffer2 = malloc(MIPI_display->width * MIPI_display->height * (DISPLAY_DEPTH / 8));
    // hagl_hal_init(&backend);
    // display = &backend;

    // memset(&backend, 0, sizeof(hagl_backend_t));
    // backend.buffer = malloc(MIPI_display->width * MIPI_display->height * (DISPLAY_DEPTH / 8));
    // hagl_hal_init(&backend);
    // display = &backend;

    mipi_display_config_t *display_config = malloc(sizeof(mipi_display_config_t));
    
    display_config->pin_cs = 9;
    init_cs_pin(9);
    display_config->pin_dc = 8;
    display_config->pin_rst = 12;
    display_config->pin_bl = 13;
    display_config->pin_clk = 10;
    display_config->pin_mosi = 11;
    display_config->pin_miso = -1;
    display_config->pin_power = -1;
    display_config->pin_te = -1;
    display_config->spi = spi1;
    display_config->spi_freq = 62500000;
    display_config->address_mode = MIPI_DCS_ADDRESS_MODE_BGR;
    display_config->pixel_format = MIPI_DCS_PIXEL_FORMAT_16BIT;
    display_config->width = 80;
    display_config->height = 160;
    display_config->depth = 16;
    display_config->offset_x = 26;
    display_config->offset_y = 1;
    display_config->invert = 1;
    display_config->init_spi = 1;

    mipi_display_config_t *display_config2 = malloc(sizeof(mipi_display_config_t));

    /* Copy first config */
    memcpy(display_config2, display_config, sizeof(mipi_display_config_t));
    
    display_config2->pin_cs = 5;
    init_cs_pin(5);
    display_config2->pin_rst = 4;
    display_config2->init_spi = -1;

    mipi_display_config_t *display_config3 = malloc(sizeof(mipi_display_config_t));

    /* Copy first config */
    memcpy(display_config3, display_config, sizeof(mipi_display_config_t));
        
    display_config3->pin_cs = 3;
    init_cs_pin(3);
    display_config3->pin_rst = 2;
    display_config3->init_spi = -1;

    display1 = hagl_init((uint8_t *)display_config);
    display2 = hagl_init((uint8_t *)display_config2);
    display3 = hagl_init((uint8_t *)display_config3);


    hagl_clear(display1);
    hagl_set_clip(display1, 0, 20, display1->width - 1, display1->height - 21);

    hagl_clear(display2);
    hagl_set_clip(display2, 0, 0, display2->width - 1, display2->height - 1);

    hagl_clear(display3);
    hagl_set_clip(display3, 0, 0, display2->width - 1, display2->height - 1);

    /* Change demo every 10 seconds. */
    add_repeating_timer_ms(10000, switch_timer_callback, NULL, &switch_timer);

    /* Update displayed FPS counter every 250 ms. */
    add_repeating_timer_ms(250, show_timer_callback, NULL, &show_timer);

    while (1) {

        absolute_time_t start = get_absolute_time();

        switch(effect) {
            case 0:
                metaballs_animate(display1);
                metaballs_render(display1);
                plasma_animate(display2);
                plasma_render(display2);
                rotozoom_animate();
                rotozoom_render(display3);
                break;
            case 1:
                plasma_animate(display1);
                plasma_render(display1);
                metaballs_animate(display2);
                metaballs_render(display2);
                rotozoom_animate();
                rotozoom_render(display3);
                  break;
            case 2:
                rotozoom_animate();
                rotozoom_render(display1);
                deform_animate();
                deform_render(display2);
                plasma_animate(display3);
                plasma_render(display3);
                break;
            case 3:
                deform_animate();
                deform_render(display1);
                rotozoom_animate();
                rotozoom_render(display2);
                metaballs_animate(display3);
                metaballs_render(display3);
                break;
        }

        /* Update the displayed fps if requested. */
        if (fps_flag) {
            show_fps();
        }

        /* Flush back buffer contents to display. NOP if single buffering. */
        bytes = hagl_flush(display1);
        bytes += hagl_flush(display2);
        bytes += hagl_flush(display3);

        aps_update(&bps, bytes);
        fps_update(&fps);

        /* Print the message in console and switch to next demo. */
        if (switch_flag) {
            printf("[main] %s at %d fps / %d kBps\r\n", demo[effect], (uint32_t)fps.current, (uint32_t)(bps.current / 1024));
            switch_demo();
        }

        /* Cap the demos to 60 fps. This is mostly to accommodate to smaller */
        /* screens where plasma will run too fast. */
        /* busy_wait_until(start + US_PER_FRAME_60_FPS); */
    };

    return 0;
}

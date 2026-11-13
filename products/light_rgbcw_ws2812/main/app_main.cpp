// Copyright 2024 Espressif Systems (Shanghai) PTE LTD
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <stdio.h>

#include <system.h>
#include <low_code.h>
#include <math.h>

#include "app_priv.h"

static const char *TAG = "app_main";

static void setup()
{
    /* Register callbacks */
    low_code_register_callbacks(feature_update_from_system, event_from_system);

    /* Initialize driver */
    app_driver_init();
}

static void loop()
{
    /* The corresponding callbacks are called if data is received from system */
    low_code_get_feature_update_from_system();
    low_code_get_event_from_system();
}

struct RGBColor {
    uint8_t r;
    uint8_t g;
    uint8_t b;
};
struct HSBColor {
    uint8_t h;
    uint8_t s;
    uint8_t b;
};

//following function is adapted from Matt DesLauriers' https://github.com/mattdesl/kelvin-to-rgb/blob/master/index.js
RGBColor kelvinToRGB(uint16_t temp) {
  temp = temp / 100;

  float red,green,blue;

  if (temp <= 66) {
    red = 255;
  } else {
    red = temp - 60;
    red = 329.698727466 * pow(red, -0.1332047592);
    if (red < 0) {
      red = 0;
    }
    if (red > 255) {
      red = 255;
    }
  }

  if (temp <= 66) {
    green = temp;
    green = 99.4708025861 * log(green) - 161.1195681661;
    if (green < 0) {
      green = 0;
    }
    if (green > 255) {
      green = 255;
    }
  } else {
    green = temp - 60;
    green = 288.1221695283 * pow(green, -0.0755148492);
    if (green < 0) {
      green = 0;
    }
    if (green > 255) {
      green = 255;
    }
  }

  if (temp >= 66) {
    blue = 255;
  } else {
    if (temp <= 19) {
      blue = 0;
    } else {
      blue = temp - 10;
      blue = 138.5177312231 * log(blue) - 305.0447927307;
      if (blue < 0) {
        blue = 0;
      }
      if (blue > 255) {
        blue = 255;
      }
    }
  }

  return RGBColor{(uint8_t)red,(uint8_t)green,(uint8_t)blue};
}

//adapted from https://github.com/jakebesworth/Simple-Color-Conversions/blob/2d806bb45a167f4e253f542dfda928ab1fb2cd69/color.c by  Jake Besworth
#define MIN(a,b) (((a)<(b))?(a):(b))
#define MAX(a,b) (((a)>(b))?(a):(b))
#define MAX3(a,b,c) MAX(MAX(a,b),c)
#define MIN3(a,b,c) MIN(MIN(a,b),c)
HSBColor RgbToHsb(const RGBColor& rgb)
{
    const unsigned char max = MAX3(rgb.r, rgb.g, rgb.b);
    const float max2 = max / 255.0;
    const unsigned char min = MIN3(rgb.r, rgb.g, rgb.b);
    const float min2 = min / 255.0;

    float h,s,v;

    s = (max2 < 0.0001) ? 0 : (max2 - min2) / max2;
    v = max2;

    /* Saturation is 0 */
    if((s * 100.0) < 0.1)
    {
        /* Hue is undefined, monochrome */
        h = 0;
    }
    else if(rgb.r == max)
    {
        if(rgb.g == min)
        {
            /* H = 5 + B' */
            h = 5 + ((max2 - (rgb.b / 255.0)) / (max2 - min2));
        }
        else
        {
            /* H = 1 - G' */
            h = 1 - ((max2 - (rgb.g / 255.0)) / (max2 - min2));
        }
    }
    else if(rgb.g == max)
    {
        if(rgb.b == min)
        {
            /* H = R' + 1 */
            h = ((max2 - (rgb.r / 255.0)) / (max2 - min2)) + 1;
        }
        else
        {
            /* H = 3 - B' */
            h = 3 - ((max2 - (rgb.b / 255.0)) / (max2 - min2));
        }
    }
    /* This is actually a problem with the original paper, I've fixed it here */
    else if(rgb.b == max && rgb.r == min)
    {
        /* H = 3 + G' */
        h = 3 + ((max2 - (rgb.g / 255.0)) / (max2 - min2));
    }
    else
    {
        /* H = 5 - R' */
        h = 5 - ((max2 - (rgb.r / 255.0)) / (max2 - min2));
    }

    h = h*42;
    s=s*255;
    v=v*255;

    return HSBColor{(uint8_t)h,(uint8_t)s,(uint8_t)v};
}

int feature_update_from_system(low_code_feature_data_t *data)
{
    /* Get the device feature updates */
    uint16_t endpoint_id = data->details.endpoint_id;
    uint32_t feature_id = data->details.feature_id;

    if (endpoint_id == 1) {
        if (feature_id == LOW_CODE_FEATURE_ID_POWER) {  // Power
            bool power_value = *(bool *)data->value.value;
            printf("%s: Feature update: power: %d\n", TAG, power_value);
            app_driver_set_light_state(power_value);
        } else if (feature_id == LOW_CODE_FEATURE_ID_BRIGHTNESS) {  // Brightness
            uint8_t brightness = *(uint8_t *)data->value.value;
            printf("%s: Feature update: brightness: %d\n", TAG, brightness);
            app_driver_set_light_brightness(brightness);
        } else if (feature_id == LOW_CODE_FEATURE_ID_COLOR_TEMPERATURE) {  // Color temperature
            uint16_t color_temp = *(uint16_t *)data->value.value;
            uint16_t kelvin = 1000000/color_temp;
            RGBColor rgb = kelvinToRGB(kelvin);
            HSBColor hsb = RgbToHsb(rgb);
            printf(
                "%s: Feature update: color temperature: %dmired -> %dkelvin -> R%d G%d B%d -> H%d S%d B%d\n",
                TAG, color_temp, kelvin, rgb.r, rgb.g, rgb.b, hsb.h, hsb.s, hsb.b);
            app_driver_set_light_saturation(hsb.s);
            app_driver_set_light_hue(hsb.h);
        } else if (feature_id == LOW_CODE_FEATURE_ID_HUE) {  // Hue
            uint8_t hue = *(uint8_t *)data->value.value;
            printf("%s: Feature update: hue: %d\n", TAG, hue);
            app_driver_set_light_hue(hue);
        } else if (feature_id == LOW_CODE_FEATURE_ID_SATURATION) {  // Saturation
            uint8_t saturation = *(uint8_t *)data->value.value;
            printf("%s: Feature update: saturation: %d\n", TAG, saturation);
            app_driver_set_light_saturation(saturation);
        }
    }

    return 0;
}

int event_from_system(low_code_event_t *event)
{
    /* Handle the events from low_code_event_type_t */
    return app_driver_event_handler(event);
}

extern "C" int main()
{
    printf("%s: Starting low code\n", TAG);

    /* Pre-Initializations: This should be called first and should always be present */
    system_setup();
    setup();

    /* Loop */
    while (1) {
        system_loop();
        loop();
    }
    return 0;
}

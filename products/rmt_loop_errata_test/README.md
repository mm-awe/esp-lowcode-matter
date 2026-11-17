# RMT loop errata test

Standalone ESP-AMP LP-core test project. It does not use or include the low-code
components. It configures RMT channel 0 on GPIO 8 with the same 40 MHz clock,
12/36-tick WS2812 symbols, 24-bit payload, 12 hardware repeats, 48-symbol TX
limit, and zero-duration end marker as the current driver.

Each second it sends a randomly generated RGB color. The blue-channel LSB is
forced to alternate between 0 and 1, which makes the last RMT symbol alternate
between the bit-0 and bit-1 encodings. The log prints the wire color, final
symbol, all 25 RMT symbols, and the terminal event/status registers.

Build from the workspace root after exporting ESP-IDF and ESP-AMP:

```sh
source /workspace/esp-idf/export.sh
export ESP_AMP_PATH=/workspace/esp-amp
idf.py -C products/rmt_loop_errata_test set-target esp32c6
idf.py -C products/rmt_loop_errata_test build
```

The resulting binary is an LP subcore image; it must be loaded by an
ESP-AMP-compatible HP-core loader. GPIO 8 is the RMT output.

Subcore `printf` is routed through the ESP-AMP system supplicant, matching the
template product. The HP-core firmware must also be built with
`CONFIG_ESP_AMP_SYSTEM_ENABLE_SUPPLICANT=y` and
`CONFIG_ESP_AMP_ROUTE_SUBCORE_PRINT=y` for logs to appear on its console.

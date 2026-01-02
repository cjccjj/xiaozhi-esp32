epaper37_display:
1. class: display-lvgl-lcddisplay-epaper37_display
2. epd driver: ported from zhongjignyuan stm32 driver
3. UI: setupUI() constructor reconfigured layout
4. font: c24xcn.ttf convert to 24px bitmap single bit - best for single bit epaper
5. emoji: use mono png for best display on epaper
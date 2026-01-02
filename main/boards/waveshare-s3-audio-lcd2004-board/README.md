char lcd 2004_display:
1. class: display->char_display  skip lvgl and all. 
2. char display driver: some code from github and rewrite some, just i2c cmds
3. UI: overriding all display functions, direct print on display
4. chinese: used a simple pinyin convertor
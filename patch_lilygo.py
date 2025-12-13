Import("env")
import os, io, glob

def patch(paths):
    for p in paths:
        try:
            s = io.open(p, "r", encoding="utf-8").read()
        except FileNotFoundError:
            continue
        s2 = s.replace("static uint32_t lv_tick_get_cb(", "static uint32_t my_lv_tick_get_cb(")
        s2 = s2.replace("lv_tick_set_cb(lv_tick_get_cb);", "lv_tick_set_cb(my_lv_tick_get_cb);")
        if s2 != s:
            io.open(p, "w", encoding="utf-8").write(s2)
            print(f"[patch] {p}")

patch(glob.glob(".pio/libdeps/**/LilyGo-AMOLED-Series/src/LV_Helper_v9.cpp", recursive=True))
patch(glob.glob(".pio/build/**/LilyGo-AMOLED-Series/src/LV_Helper_v9.cpp", recursive=True))

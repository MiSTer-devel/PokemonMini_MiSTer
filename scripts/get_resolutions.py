import numpy as np
import math

if __name__ == '__main__':
    base_clock = 32000000
    lcd_width  = 96
    lcd_height = 64

    for d in range(4,10):
        F = base_clock // d
        min_duration_us = (60.0 / F) * 1000000 
        hs_width = int(math.ceil(125 / min_duration_us))
        print(2*hs_width)
        for y in range(257, 263):
            x = int((F / 60.0) / y)
            if x <= lcd_width + 2 * hs_width:
                continue
            print("ce_pix: %.2fMHz, %.2fHz, w %d, h %d, ar %.2f" % (F / 1000000.0, F / (x * y), x, y, x / y))

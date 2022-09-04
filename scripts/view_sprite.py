import struct
import sys
import numpy as np
import matplotlib.pyplot as plt

if __name__ == '__main__':
    full = (len(sys.argv) == 4) and (sys.argv[3] == 'full')

    rom_fp = sys.argv[1]
    sprite_address = sys.argv[2]
    sprite_address = int(sprite_address, base=16)

    if full:
        mask_sprite = np.empty((16, 16), dtype=np.uint8)
        draw_sprite = np.empty((16, 16), dtype=np.uint8)

        with open(rom_fp, 'rb') as fp:
            fp.seek(sprite_address)
            for si in range(8):
                print("0x%x" % fp.tell())
                sprite = np.empty((8, 8), dtype=np.uint8)
                for bi in range(8):
                    column = fp.read(1)[0]
                    print("0x%x" % column)
                    for i in range(8):
                        sprite[i,bi] = (column & 1) == 0
                        column = column >> 1

                m = si // 4
                l = si % 2
                if si in [0, 1, 4, 5]:
                    mask_sprite[8*l:8*(l+1),8*m:8*(m+1)] = sprite
                else: # [2, 3, 6, 7]
                    draw_sprite[8*l:8*(l+1),8*m:8*(m+1)] = sprite

        plt.imshow(draw_sprite +1-mask_sprite, cmap='gray')
        plt.show()

    else:
        sprite = np.empty((8, 8), dtype=np.uint8)
        with open(rom_fp, 'rb') as fp:
            fp.seek(sprite_address)
            for bi in range(8):
                column = fp.read(1)[0]
                for i in range(8):
                    sprite[i,bi] = (column & 1) == 0
                    column = column >> 1

        plt.imshow(sprite, cmap='gray')
        plt.show()

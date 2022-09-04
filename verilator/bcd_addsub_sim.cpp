#include "Vbcd_addsub.h"
#include "Vbcd_addsub___024root.h"
#include "verilated.h"
#include "verilated_vcd_c.h"
#include <cstdio>
#include <cstring>
#include <cstdint>

static inline uint8_t sub8_minimon(uint8_t A, uint8_t B, int C, uint8_t* Cout) {
	int o = A - B - C;
    int h = (A & 0xF) - (B & 0xF) - C;

    if (h < 0) o -= 0x06;
    if (o < 0) o -= 0x60;

    *Cout = (o < 0);

    return o & 0xFF;
}

static inline uint8_t sub8_pokemini(uint8_t A, uint8_t B, uint8_t C)
{
    uint8_t o = A - B - C;
    uint8_t h = (A & 0xF) - (B & 0xF) - C;

    if(h >= 0x0A) o -= 0x06;
    if(o >= 0xA0) o -= 0x60;

    return o & 0xFF;
}

int main(int argc, char** argv, char** env)
{

    Verilated::commandArgs(argc, argv);

    Vbcd_addsub* bcd_addsub = new Vbcd_addsub;

    bcd_addsub->add_sub = 0;
    for(int c = 0; c < 2; ++c)
    {
        bcd_addsub->carry_in = c;
        for(int i = 0; i <= 99; ++i)
        {
            bcd_addsub->a = ((i / 10) << 4) | (i % 10);
            for(int j = 0; j <= 99; ++j)
            {
                bcd_addsub->b = ((j / 10) << 4) | (j % 10);
                bcd_addsub->eval();
                int result = (bcd_addsub->r >> 4) * 10 + (bcd_addsub->r & 0xF);
                int true_result = (i + j + c) % 100;
                int true_carry = (i + j) + c > 99;
                if((true_result != result) || (true_carry != ((bcd_addsub->flags >> 1) & 1)))
                    printf("%d+%d+%d=%d, c=%d\n", i, j, c, result, (bcd_addsub->flags >> 1) & 1);
            }
        }
    }

    bcd_addsub->add_sub = 1;
    for(int c = 0; c < 2; ++c)
    {
        bcd_addsub->carry_in = c;
        for(int i = 0; i <= 99; ++i)
        {
            bcd_addsub->a = ((i / 10) << 4) | (i % 10);
            for(int j = 0; j <= 99; ++j)
            {
                bcd_addsub->b = ((j / 10) << 4) | (j % 10);
                bcd_addsub->eval();
                int result = (bcd_addsub->r >> 4) * 10 + (bcd_addsub->r & 0xF);
                int true_result = (100 + i - j - c) % 100;
                int true_carry = i - j - c < 0;
                uint8_t temp_carry;
                uint8_t temp = sub8_minimon(bcd_addsub->a, bcd_addsub->b, c, &temp_carry);
                temp = ((temp & 0xF0) >> 4) * 10 + (temp & 0xF);

                if((true_result != result) || (true_carry != ((bcd_addsub->flags >> 1) & 1)) || temp != true_result || temp_carry != true_carry)
                {
                    printf("%d%d-%d%d-%d=%d, c=%d, %d,%d\n",
                        (bcd_addsub->a & 0xF0)>>4, bcd_addsub->a & 0xF,
                        (bcd_addsub->b & 0xF0)>>4, bcd_addsub->b & 0xF,
                        c, result,
                        (bcd_addsub->flags >> 1) & 1,
                        true_result, temp
                    );
                }
            }
        }
    }
    printf("Done.\n");

    return 0;
}

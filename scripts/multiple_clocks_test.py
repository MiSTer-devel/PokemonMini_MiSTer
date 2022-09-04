osc1_clocks = 4000000.0 / 32768.0
osc1_clock_count = 0
osc1_rest = 0.0

if __name__ == '__main__':
    for i in range(32768):
        print(i)
        if (osc1_clock_count + 1) > osc1_clocks + osc1_rest:
            osc1_rest = osc1_clocks + osc1_rest - osc1_clock_count;
            print(i + osc1_rest)
            osc1_clock_count = 0;

        osc1_clock_count += 1

    next_clock = osc1_clocks
    for i in range(32768):
        print(i)
        if (i + 1) > next_clock:
            print(next_clock)
            next_clock += osc1_clocks

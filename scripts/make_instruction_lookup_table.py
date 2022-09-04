def print_instructions(instructions, key_id=0, prefix='8\'h'):
    instructions = sorted(instructions)

    i = 0
    while i < len(instructions):
        char = instructions[i][key_id]
        while i < len(instructions):
            if instructions[i][key_id] == char:
                print(prefix, instructions[i], end=', ', sep='')
            else:
                print()
                break

            i += 1

    print()

if __name__ == '__main__':

    fp = open('docs/instructions.csv')
    lines = fp.readlines()
    instructions = [x.split(',')[0].strip('"') for x in lines]
    cycles       = [x.strip().split(',')[-2:] for x in lines]
    cycles       = [(int(x[0]) // 4, int(x[1]) // 4) for x in cycles]
    fp.close()

    #is_extended = [int(x[:2] == 'CE' or x[:2] == 'CF') for x in instructions]
    #for i in range(len(cycles)):
    #    cycles[i] -= is_extended[i]

    cycles_all = [(0,0)] * 0x300
    for i in range(len(instructions)):
        instruction_index = 0
        parts = instructions[i].split(' ')
        if parts[0] == 'CE':
            instruction_index = 0x100 + int(parts[1], 16)
        elif parts[0] == 'CF':
            instruction_index = 0x200 + int(parts[1], 16)
        else:
            instruction_index = int(parts[0], 16)

        cycles_all[instruction_index] = cycles[i]

    with open('instruction_cycles.h', 'w') as fp:
        fp.write('uint8_t instruction_cycles[%d] = {\n    ' % (0x300*2))
        for i, c in enumerate(cycles_all):
            fp.write('%d,%d' % c)
            if i < len(cycles_all) - 1:
                fp.write(',')
            if (i + 1) % 10 == 0:
                fp.write('\n    ')

        fp.write('\n};\n')


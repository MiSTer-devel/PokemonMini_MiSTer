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
    fp.close()

    extended_instructions = [x for x in instructions if x[:2] == 'CE' or x[:2] == 'CF']
    instructions = [x for x in instructions if x[:2] != 'CE' and x[:2] != 'CF']

    one_byte_instructions = [x[:2] for x in instructions if len(x) == 2]
    two_byte_instructions = [x[:2] for x in instructions if len(x.split(' ')) == 2]
    three_byte_instructions = [x[:2] for x in instructions if len(x.split(' ')) == 3]

    print_instructions(one_byte_instructions)
    print()
    print_instructions(two_byte_instructions)
    print()
    print_instructions(three_byte_instructions)
    print()

    extended_instructions = [x.split(' ') for x in extended_instructions]
    one_byte_extended_instructions   = [''.join(x[:2]) for x in extended_instructions if len(x) == 2]
    two_byte_extended_instructions   = [''.join(x[:2]) for x in extended_instructions if len(x) == 3]
    three_byte_extended_instructions = [''.join(x[:2]) for x in extended_instructions if len(x) == 4]

    print_instructions(one_byte_extended_instructions, 2, '16\'h')
    print()
    print_instructions(two_byte_extended_instructions, 2, '16\'h')
    print()
    print_instructions(three_byte_extended_instructions, 2, '16\'h')
    print()

import os

# @warning: This is not a full-fledged verilog parser and might fail in certain
# cases. Use with caution.
def read_localparams(filepath):
    text = open(filepath, 'r').read()
    localparam_dict= {}
    while True:
        text = text.lstrip()

        if len(text) == 0:
            break

        while text.startswith('//'):
            newline_pos = text.find('\n')
            if newline_pos == -1:
                text = ""
                break
            text = text[newline_pos+1:]
            text = text.lstrip()

        if len(text) == 0:
            break

        if text.startswith('/*'):
            endcomment_pos = text.find('*/')
            if endcomment_pos == -1:
                text = ""
                break
            text = text[endcomment_pos+1:]
            text = text.lstrip()

        endcommand_pos = text.find(';')
        if endcommand_pos == -1:
            text = ""
            break

        command = text[0:endcommand_pos]
        # @todo: The current parsing does not allow for comments!
        # @todo: Perhaps it's nice to be able to annotate the localparams to
        # modify the number of bits/value, e.g. it would be nice to change
        # MICRO_ALU_OP_NONE to be 7'd0 so that we do not have to define the alu
        # size and flag updating explicitly.
        if command.startswith('localparam'):
            command = command.strip().replace('\n', '').replace(' ', '')
            if command.find('[') != -1:
                command = command[15:]
            else:
                command = command[10:]

            if command.startswith('MICRO_'):
                parts = command.split(',')
                for part in parts:
                    name, value = part.split('=')
                    if "'" in value:
                        bits, value = value.split("'")
                        if value[0] == 'b':
                            value = value[1:]
                        elif value[0] == 'h':
                            value = bin(int(value[1:], base=16))[2:].zfill(int(bits))
                        elif value[0] == 'd':
                            value = bin(int(value[1:]))[2:].zfill(int(bits))
                        else:
                            print("Cannot read value in %s" % part)
                    else:
                        print('Not implemented %s' % command)

                    localparam_dict[name[6:]] = value

        text = text[endcommand_pos+1:]

    return localparam_dict

def num_string_to_binary_string(x):

    if '\'' in x:
        num_bits, value = x.split('\'')
        num_bits = int(num_bits)
        format, value = value[0], value[1:]
        if format == 'd':
            value = int(value)
        elif format == 'h':
            value = int(value, base=16)
        elif format == 'b':
            value = int(value, base=2)
        else:
            print("Couldn't decode x:", x)
            return x

        value = bin(value)[2:].zfill(num_bits)

        return value

    x = bin(int(x))[2:]
    return x

if __name__ == '__main__':
    microinstruction_width = 36
    rom_bits = 11

    root = os.path.abspath(os.path.join(os.path.dirname(__file__), '../'))

    localparam_dict = read_localparams(os.path.join(root, 'rtl/s1c88.sv'))

    lines = open(os.path.join(root, 'rom/microinstructions.txt'), 'r').readlines()

    # @todo: Also check if MOV_DATA is called enough times given the
    # instruction number of arguments.
    done_exceptions = [
        0xE0, 0xE1, 0xE2, 0xE3, 0x1F0, 0x1F1, 0x1F2, 0x1F3, 0x1F4, 0x1F5, 0x1F6, 0x1F7,
        0x1F8, 0x1F9, 0x1FA, 0x1FB, 0x1FC, 0x1FD, 0x1FE, 0x1FF, 0xE8, 0xE9, 0xEA, 0xEB
    ]

    addresses = [-1] * 768
    rom = []
    microinstruction_address = 0
    num_opcodes_implemented = 0
    default_address = 0
    done_flags_in_instruction = 0
    microinstruction_addresses = []
    for line in lines:
        line = line.strip()

        comment_start = line.find('//')
        if comment_start > -1:
            line = line[:comment_start].strip()

        if len(line) == 0:
            continue

        if line[0] == '#':
            if done_flags_in_instruction > 1:
                if opcode not in done_exceptions:
                    print('Warning: opcode 0x%x has multiple DONE!' % opcode)
            done_flags_in_instruction = 0
            if line[1:] == 'default':
                default_address = microinstruction_address
                for i in range(len(addresses)):
                    if addresses[i] == -1:
                        addresses[i] = default_address
            else:
                opcode = int(line[1:], base=16)
                if opcode >= 0xCF00:
                    opcode = 0x200 | opcode & 0xFF
                elif opcode >= 0xCE00:
                    opcode = 0x100 | opcode & 0xFF

                if addresses[opcode] != default_address and addresses[opcode] != -1:
                    print('Warning: opcode 0x%x already implemented.' % opcode)
                else:
                    num_opcodes_implemented += 1
                    addresses[opcode] = microinstruction_address
                    microinstruction_addresses.append(microinstruction_address)
        else:
            microcommands = line.split(' ')
            microcommands = [x for x in microcommands if len(x) > 0]
            if 'DONE' in microcommands:
                done_flags_in_instruction += 1
            microcommands = [
                    localparam_dict[x] if x in localparam_dict else
                    num_string_to_binary_string(x)
                    for x in microcommands]

            command = ''.join(microcommands)
            assert(len(command) == microinstruction_width)
            rom.append(command)
            microinstruction_address += 1

    from collections import Counter
    microprograms = [int(''.join(rom[x[0]:x[1]]),2) for x in zip(microinstruction_addresses[:-1], microinstruction_addresses[1:])]
    duplicates = [i for i,(k,v) in enumerate(Counter(microprograms).items()) if v > 1]
    if len(duplicates) > 0:
        print("Warning: duplicates found! Check the following rom addresses:")
        print([microinstruction_addresses[x] for x in duplicates])

    print('%d microinstructions in rom.' % len(rom))
    print('%d/608 opcodes implemented.' % num_opcodes_implemented)

    rom = '\n'.join([hex(int(x, base=2))[2:] for x in rom])
    addresses = '\n'.join([hex(int(bin(x)[2:].zfill(rom_bits)[:rom_bits], base=2))[2:] for x in addresses])

    with open('translation_rom.mem', 'w') as fp:
        fp.write(addresses)

    with open('rom.mem', 'w') as fp:
        fp.write(rom)


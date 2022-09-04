def bits(x):
    return [int(u) for u in bin(x)[2:].zfill(8)]

def unsigned_int(x):
    return int(''.join([str(u) for u in x]), base=2)

def signed_int(x):
    s = x[0]
    if s == 0:
        x_temp = x
    else:
        x_temp, _, _ = add([1-u for u in x], [0]*7 + [1])

    return (1-2*s) * int(''.join([str(u) for u in x_temp]), base=2)

def add(a, b, cin = 0):
    r = [0] * 8
    c = cin
    v = 0
    for i in range(1,9):
        temp = a[-i] + b[-i] + c
        c = 0
        if temp > 1:
            c = 1
            r[-i] = temp - 2
        else:
            r[-i] = temp

    v = (a[0] & b[0] & (1-r[0])) | ((1-a[0]) & (1-b[0]) & r[0])

    return r, c, v

def sub(a, b, cin = 0):
    r = [0] * 8
    c = cin
    v = 0
    for i in range(1,9):
        temp = a[-i] - b[-i] - c
        c = 0
        if temp < 0:
            c = 1
            r[-i] = temp + 2
        else:
            r[-i] = temp

    v = (a[0] & (1-b[0]) & (1-r[0])) | ((1-a[0]) & b[0] & r[0])

    return r, c, v

if __name__ == '__main__':

    print('Testing alu add output...')

    for cin in range(2):
        for xa in range(-128, 128):
            for xb in range(-128, 128):
                v_true = int((xa + xb + cin > 127) or (xa + xb + cin < -128))
                r_true = (xa + xb + cin) & 0xFF

                a = xa & 0xFF
                b = xb & 0xFF

                c_true = int(a + b + cin > 255)

                r, c, v = add(bits(a), bits(b), cin)
                r = unsigned_int(r)

                assert(r_true == r)
                assert(c_true == c)
                assert(v_true == v)

    print('Testing alu sub output...')

    for cin in range(2):
        for xa in range(-128, 128):
            for xb in range(-128, 128):
                v_true = int((xa - xb - cin > 127) or (xa - xb - cin < -128))
                r_true = (xa - xb - cin) & 0xFF

                a = xa & 0xFF
                b = xb & 0xFF

                c_true = int(a - b - cin < 0)

                r, c, v = sub(bits(a), bits(b), cin)

                assert(r_true == unsigned_int(r))
                assert(c_true == c)
                assert(v_true == v)

    print('Success!')

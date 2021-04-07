def checksum(msg):
    s = 0
    for i in range(0, len(msg), 2):
        w = (ord(msg[i]) << 8) + (ord(msg[i + 1]))
        s = s + w

    s = (s >> 16) + (s & 0xffff)
    s = ~s & 0xffff

    return s

def die(s):
    print(s)
    exit()
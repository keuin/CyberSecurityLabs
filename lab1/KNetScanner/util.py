import time
from typing import Iterator


def get_time_string() -> str:
    return time.strftime('%Y-%m-%d %H:%M:%S')


def inet4_subnet_generator(ns: str) -> Iterator[str]:
    ele = ns.replace('/', '.').split('.')
    # ele: 0, 1, 2, 3 -> inet4 addr; 4 -> mask
    # now only supports 0, 8, 16, 24
    a1, b1, c1, d1 = [int(x) for x in ele[:-1]]
    net_sz = int(ele[4])
    if net_sz not in [8, 16, 24]:
        raise ValueError('Only supports 8,16,24 sub networks.')
    if net_sz == 8:
        for b in range(256):
            for c in range(256):
                for d in range(1, 256):
                    yield f'{a1}.{b}.{c}.{d}'
    elif net_sz == 16:
        for c in range(256):
            for d in range(1, 256):
                yield f'{a1}.{b1}.{c}.{d}'
    elif net_sz == 24:
        for d in range(1, 256):
            yield f'{a1}.{b1}.{c1}.{d}'


if __name__ == '__main__':

    for i, addr in enumerate(inet4_subnet_generator('192.168.1.0/24')):
        expected = f'192.168.1.{i + 1}'
        assert addr == expected, f'case {expected} failed.'

    for i, addr in enumerate(inet4_subnet_generator('192.168.0.0/16')):
        a3 = i // 255
        a4 = i % 255 + 1
        expected = f'192.168.{a3}.{a4}'
        assert addr == expected, f'case {addr} failed. expected: {expected}. i={i}'

    for i, addr in enumerate(inet4_subnet_generator('10.0.0.0/8')):
        a2 = i // 255 // 256
        a3 = (i // 255) % 256
        a4 = i % 255 + 1
        expected = f'10.{a2}.{a3}.{a4}'
        assert addr == expected, f'case {addr} failed. expected: {expected}. i={i}'

    print('All tests passed.')
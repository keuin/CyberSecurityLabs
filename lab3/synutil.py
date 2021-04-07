import os
import socket
import struct


def make_ip_segment(src: str, dst: str, layer4_data: bytes) -> bytes:
    ihl = 5
    version = 4
    tos = 0
    tot_len = 20 + len(layer4_data)
    id_ = int.from_bytes(os.urandom(2), byteorder='little', signed=False)  # Id of this packet
    frag_off = 0
    ttl = 255
    protocol = socket.IPPROTO_TCP
    check = 0  # let the socket calculate
    saddr = socket.inet_aton(src)
    daddr = socket.inet_aton(dst)

    ihl_version = (version << 4) + ihl

    # the ! in the pack format string means network order
    return struct.pack('!BBHHHBBH4s4s', ihl_version, tos, tot_len, id_,
                       frag_off, ttl, protocol, check, saddr, daddr) + layer4_data


def tcp_syn(dst_port: int) -> bytes:
    # tcp header fields
    sport = 1234  # source port
    dport = dst_port  # destination port
    seq = 0
    ack_seq = 0
    doff = 5
    # tcp flags
    fin = 0
    syn = 1
    rst = 0
    psh = 0
    ack = 0
    urg = 0
    window = socket.htons(5840)  # maximum allowed window size
    check = 0
    urg_ptr = 0

    offset_res = (doff << 4) + 0
    tcp_flags = fin + (syn << 1) + (rst << 2) + (psh << 3) + (ack << 4) + (urg << 5)

    # the ! in the pack format string means network order
    return struct.pack('!HHLLBBHHH', sport, dport, seq, ack_seq, offset_res, tcp_flags, window, check, urg_ptr)


def send_syn(s: socket.socket, src: str, dst: str, dst_port: int):
    data = make_ip_segment(src, dst, tcp_syn(dst_port))
    remaining = len(data)
    while remaining:
        remaining -= s.sendto(data[-remaining:], (dst, 0))



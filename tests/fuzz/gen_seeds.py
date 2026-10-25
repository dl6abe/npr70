#!/usr/bin/env python3
"""Writes the seed corpus of a fuzz target from the inputs of the host tests.
usage: python3 -I gen_seeds.py snmp|dhcp|radio|signaling|eth|w5500|arp OUTDIR
  snmp  the requests of snmp_host/test_snmp.py plus the client table, once as master and once as client
  radio segments of IPv4 packets, signaling and allocation frames, in the record format of radio_fuzz.cpp
  signaling  WHOIS, connect request, ACK, NACK and disconnect fields, in the record format of signaling_fuzz.cpp
  eth  Ethernet frames with IPv4 headers (inside the subnet, to the clients, outside), good and with wrong length fields
  w5500  receive buffers of the fake chip: UDP packets and MAC frames, good and with wrong length fields
  arp  ARP requests and replies, address lookups and table timeouts, in the record format of arp_fuzz.cpp
  dhcp  DHCP discover, request, release and odd messages as in dhcp_host, in the record format of dhcp_fuzz.cpp"""
import ast, os, sys

here = os.path.dirname(os.path.abspath(__file__))


def snmp_seeds():
    src = open(os.path.join(here, '..', 'snmp_host', 'test_snmp.py')).read()
    ns = {}
    # only the request builders: the rest of test_snmp.py starts the agent and runs the checks
    for node in ast.parse(src).body:
        if isinstance(node, ast.FunctionDef) and node.name in ('enc_len', 'tlv', 'enc_oid', 'request', 'request_multi'):
            exec(compile(ast.Module([node], []), 'test_snmp.py', 'exec'), ns)
    B = '1.3.6.1.4.1.54539.100'
    oids = ['1.3.6.1.2.1.1.1.0', '1.3.6.1.2.1.1.5.0', '1.3.6.1', B + '.1.0', B + '.3.0', B + '.12.0', '1.3.6.1.2.1.1.3.0']
    seeds = []
    for kind in ('get', 'next'):
        for o in oids:
            seeds.append(ns['request'](kind, o))
    seeds.append(ns['request']('set', '1.3.6.1.2.1.1.1.0', setval=b'HACKED'))
    seeds.append(ns['request']('get', oids[0], community=b'wrong'))
    seeds.append(ns['request']('get', oids[0], version=1))
    seeds.append(ns['request_multi']('get', oids))
    seeds.append(ns['request_multi']('next', oids))
    # client table (columns 1 to 5, client IDs 0 to 6), FEC counters, and requests with many OIDs (long lengths, tooBig)
    table = [B + '.10.1.%d.%d' % (c, i) for c in range(1, 6) for i in range(7)]
    for o in table[::3]:
        seeds.append(ns['request']('get', o))
        seeds.append(ns['request']('next', o))
    seeds.append(ns['request_multi']('get', table))
    seeds.append(ns['request_multi']('next', table))
    seeds.append(ns['request_multi']('get', table * 3))
    for k in (1, 2, 3, 4):
        seeds.append(ns['request']('get', B + '.12.%d.0' % k))
    return [bytes([mode]) + s for s in seeds for mode in (0, 1)]    # byte 0 bit 0: master or client


def dhcp_packet(mac, msg_type, req_ip=None, server_id=False, extra=b''):
    """A DHCP request as the W5500 hands it over: 8 bytes UDP header, then the DHCP message."""
    p = bytearray(248 + 16)
    p[8] = 1
    p[12:16] = b'\x11\x22\x33\x44'
    p[36:42] = mac
    o = bytearray([53, 1, msg_type])
    if req_ip:
        o += bytes([50, 4]) + bytes(req_ip)
    if server_id:
        o += bytes([54, 4, 1, 1, 1, 1])
    o += extra + bytes([255])
    p[248:248 + len(o)] = o
    n = (248 + len(o) + 3) // 4 * 4
    return bytes(p[:n])


def dhcp_seeds():
    macs = [bytes([0x10 * c + i for i in range(6)]) for c in range(3)]
    ip = [192, 168, 10, 4]
    recs = []
    for m in macs:
        recs += [dhcp_packet(m, 1), dhcp_packet(m, 3, ip), dhcp_packet(m, 3, ip, True), dhcp_packet(m, 7)]
    recs += [dhcp_packet(macs[0], 8), dhcp_packet(macs[0], 2), dhcp_packet(macs[0], 1, extra=bytes([12, 3]) + b'abc')]
    rec = lambda r: bytes([len(r) // 4]) + r
    out = [bytes([cfg]) + b''.join(rec(r) for r in recs[:8]) for cfg in range(16)]
    out += [bytes([0]) + rec(r) for r in recs]
    return out


def radio_seeds():
    def rec(tdma, payload, delta=0, rssi=100):
        r = bytes([tdma, delta & 0xFF, rssi]) + payload
        return bytes([len(r)]) + r
    out = []
    for hdr, tdma, addr in ((0b00110, 0x00, 6), (0b00110, 0x00, 0x7E), (0b00100, 0x40, 0x7F), (0b01000, 0x40, 2)):
        # packet 5 in 3 segments of 100 bytes (counter 0, 1, last 2), a gap, a lone first segment
        segs = [bytes([addr, 0x02, 0x50 | (0x08 if k == 2 else 0) | k]) + bytes(range(100)) for k in range(3)]
        out.append(bytes([hdr, 0xFF]) + b''.join(rec(tdma, x) for x in segs))
        out.append(bytes([hdr, 0xFF]) + rec(tdma, segs[0]) + rec(tdma, segs[2]))
        out.append(bytes([hdr, 0xFF]) + b''.join(rec(tdma, x) for x in (segs[0], segs[1], segs[1], segs[2])))
        out.append(bytes([hdr, 0xFF]) + rec(tdma, bytes([addr, 0x1E, 1, 2, 3, 4, 5, 6, 7, 8])))
        out.append(bytes([hdr, 0xFF]) + rec(tdma, bytes([addr, 0x1F]) + bytes(range(40))))
        out.append(bytes([hdr, 0xFF]) + rec(tdma, bytes([addr, 0x02, 0x58]) + bytes(200)))
        out.append(bytes([hdr, 0xFF]) + rec(tdma, bytes([addr, 0x02]), delta=-20))
        out.append(bytes([hdr, 0xFF]) + rec(tdma, b'', delta=5))
    # the largest packet of the own sender: 6 full segments in order (mem_host case 2b)
    # and 7 and 8 segments, which the firmware must drop (mem_host case 2; the fuzzer does not find this by itself:
    # the 7th segment adds no new coverage, so nothing guides it there)
    for n in (6, 7, 8):
        full = [bytes([6, 0x02, 0x70 | (0x08 if k == n - 1 else 0) | k]) + bytes(249) for k in range(n)]
        out.append(bytes([0b00110, 0xFF]) + b''.join(rec(0x00, x) for x in full))
    out.append(bytes([0b111, 0xFF, 40]) + bytes(range(40)))      # raw mode: a frame whose FEC data is garbage
    return out


def signaling_seeds():
    me = b'DL6ABE-72'.ljust(16, b'\0')
    other = b'DL6ABE-71'.ljust(16, b'\0')
    ip = lambda a, b, c, d: bytes([a, b, c, d])
    fields = {
        'whois': bytes([0x01, 32, 3]) + other + ip(192, 168, 10, 3) + ip(0, 0, 0, 1) + bytes([100, 5, 0, 50, 0]) + bytes(2),
        'whois_master': bytes([0x01, 32, 0x7F]) + me + ip(192, 168, 10, 3) + ip(0, 0, 0, 7) + bytes([100, 5, 0, 50, 0]) + bytes(2),
        'req': bytes([0x05, 21]) + other + ip(0, 0, 0, 1) + bytes([0, 0]),
        'req_big': bytes([0x05, 21]) + other + ip(0, 0, 0, 20) + bytes([1, 0]),
        'ack': bytes([0x06, 59, 2]) + me + ip(192, 168, 10, 3) + ip(0, 0, 0, 1) + other[:16] + ip(192, 168, 10, 2)
               + ip(255, 255, 255, 240) + bytes([1]) + ip(192, 168, 10, 1) + bytes([1]) + ip(9, 9, 9, 9),
        'nack': bytes([0x07, 33]) + me + bytes([2]) + other,
        'disc': bytes([0x0B, 17, 2]) + other,
        'disc_ack': bytes([0x0C, 17, 2]) + me,
    }
    out = []
    for hdr in (0b0001, 0b0011, 0b1111, 0b0000, 0b10000, 0b110000):
        for f in fields.values():
            fr = bytes([0x7E, 0x1E]) + f + bytes([0xFF, 0])
            out.append(bytes([hdr, 20, len(fr)]) + fr)
        fr = bytes([0x7E, 0x1E]) + b''.join(fields[k] for k in ('whois', 'req', 'disc')) + bytes([0xFF, 0])
        out.append(bytes([hdr, 20, len(fr)]) + fr)
    return out


def eth_seeds():
    def frame(dst_ip, total=None, size=None, mac=bytes([1, 2, 3, 4, 5, 6])):
        n = size if size is not None else 14 + 20 + 6
        f = bytearray(n)
        f[0:6] = mac
        if n > 13:
            f[12:14] = b'\x08\x00'
        t = total if total is not None else n - 14
        if n > 17:
            f[16], f[17] = t >> 8, t & 0xFF
        if n > 33:
            f[30:34] = bytes(dst_ip)
        return bytes(f)
    out = []
    for hdr in (0b0000, 0b0110, 0b0100, 0b1100, 0b100110, 0b110110, 0b1, 0b111, 0b1101, 0b110111):
        for ip in ((192, 168, 10, 3), (192, 168, 10, 4), (192, 168, 10, 11), (8, 8, 8, 8), (192, 168, 10, 2)):
            out.append(bytes([hdr]) + frame(ip))
            out.append(bytes([hdr]) + frame(ip, size=100))
        out.append(bytes([hdr]) + frame((192, 168, 10, 3), total=1500, size=100))   # length field larger than the frame
        out.append(bytes([hdr]) + frame((192, 168, 10, 11), total=1486, size=1500))
        out.append(bytes([hdr]) + frame((192, 168, 10, 11), size=20))
        out.append(bytes([hdr]) + frame((192, 168, 10, 11), size=34))
    return out


def w5500_seeds():
    out = []
    for mac in (0, 1):
        for bufsize in (592, 1600, 8, 2, 60):
            for plen in (0, 1, 8, 100, 548, 584, 600, 1500, 3000):
                if mac:
                    body = bytes([(plen + 2) >> 8, (plen + 2) & 0xFF]) + bytes(range(256)) * 7
                else:
                    body = bytes([192, 168, 10, 5, 0x1A, 0x3C, plen >> 8, plen & 0xFF]) + bytes([1]) + bytes(300)
                out.append(bytes([mac, bufsize & 0xFF, bufsize >> 8, 0x00, 0x01]) + body)
    return out


def arp_seeds():
    def arp(op, sender_ip, target_ip, sender_mac=bytes([0x10, 0x11, 0x12, 0x13, 0x14, 0x15]), size=42):
        f = bytearray(max(size, 14))
        f[0:6] = b'\xff' * 6
        f[6:12] = sender_mac
        f[12:14] = b'\x08\x06'
        if size > 21:
            f[14:22] = b'\x00\x01\x08\x00\x06\x04\x00' + bytes([op])
            f[22:28] = sender_mac
            f[28:32] = bytes(sender_ip)
            f[38:42] = bytes(target_ip)
        return bytes(f[:size])
    rec = lambda t, b: bytes([t, len(b)]) + b
    me = (192, 168, 10, 2)
    out = []
    for hdr in (0b001, 0b000, 0b011, 0b010, 0b101, 0b111):
        frames = [arp(1, (192, 168, 10, 3), (192, 168, 10, 5)), arp(1, (192, 168, 10, 3), (192, 168, 10, 3)),
                  arp(2, (192, 168, 10, 7), me), arp(2, (192, 168, 10, 8), me), arp(1, (192, 168, 10, 9), me),
                  arp(1, (10, 0, 0, 1), (192, 168, 10, 4)), arp(1, (1, 2, 3, 4), (8, 8, 8, 8), size=60),
                  arp(1, (1, 2, 3, 4), (8, 8, 8, 8), size=20), arp(2, (192, 168, 10, 7), me, size=41)]
        body = b''.join(rec(0, f) for f in frames)
        body += rec(1, bytes([192, 168, 10, 7])) + rec(3, bytes([200, 0, 0, 0])) + rec(1, bytes([192, 168, 10, 7]))
        body += rec(3, bytes([255, 255, 255, 0])) + rec(2, b'') + rec(1, bytes([192, 168, 10, 8]))
        out.append(bytes([hdr]) + body)
        out.append(bytes([hdr]) + rec(0, frames[2]) + rec(1, bytes([192, 168, 10, 7])))
    return out


if __name__ == '__main__':
    which, outdir = sys.argv[1], sys.argv[2]
    seeds = {'snmp': snmp_seeds, 'dhcp': dhcp_seeds, 'radio': radio_seeds, 'signaling': signaling_seeds, 'eth': eth_seeds, 'w5500': w5500_seeds, 'arp': arp_seeds}[which]()
    os.makedirs(outdir, exist_ok=True)
    for n, s in enumerate(seeds):
        open(os.path.join(outdir, 'seed%03d' % n), 'wb').write(s)
    print('%d seeds in %s' % (len(seeds), outdir))

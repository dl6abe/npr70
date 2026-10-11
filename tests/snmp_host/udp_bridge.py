"""Serve the host-built firmware SNMP agent on a UDP port, so net-snmp tools can talk to it.
usage: SNMP_HOST_BIN=... udp_bridge.py [master|client] [port]"""
import os, socket, subprocess, sys

mode = sys.argv[1] if len(sys.argv) > 1 else "master"
port = int(sys.argv[2]) if len(sys.argv) > 2 else 16161
p = subprocess.Popen([os.environ["SNMP_HOST_BIN"], mode, "4"], stdin=subprocess.PIPE, stdout=subprocess.PIPE, text=True)
sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
sock.bind(("127.0.0.1", port))
while True:
    data, addr = sock.recvfrom(2048)
    p.stdin.write(data.hex() + "\n")
    p.stdin.flush()
    while True:
        line = p.stdout.readline().strip()
        if line.startswith("RSP:"):
            break
    if not line.startswith("RSP:ERR"):
        sock.sendto(bytes.fromhex(line[4:]), addr)

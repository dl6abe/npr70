"""Host test for the NPR-70 SNMP agent. Builds BER requests, runs them through the firmware parser."""
import os, subprocess, sys

BIN = os.environ["SNMP_HOST_BIN"]


def enc_len(n):
    if n < 128:
        return bytes([n])
    if n < 256:
        return bytes([0x81, n])
    return bytes([0x82, n >> 8, n & 0xFF])


def tlv(tag, body):
    return bytes([tag]) + enc_len(len(body)) + body


def enc_oid(dotted):
    a = [int(x) for x in dotted.split(".")]
    out = bytearray([a[0] * 40 + a[1]])
    for v in a[2:]:
        chunk = [v & 0x7F]
        v >>= 7
        while v:
            chunk.append(0x80 | (v & 0x7F))
            v >>= 7
        out += bytes(reversed(chunk))
    return bytes(out)


def dec_oid(b):
    out = [str(b[0] // 40), str(b[0] % 40)]
    v = 0
    for x in b[1:]:
        v = (v << 7) | (x & 0x7F)
        if not x & 0x80:
            out.append(str(v))
            v = 0
    return ".".join(out)


def request(kind, oid, community=b"public", version=0, setval=None):
    pdu = {"get": 0xA0, "next": 0xA1, "set": 0xA3}[kind]
    vb = tlv(0x30, tlv(0x06, enc_oid(oid)) + (tlv(0x04, setval) if setval is not None else tlv(0x05, b"")))
    body = tlv(0x02, b"\x01") + tlv(0x02, b"\x00") + tlv(0x02, b"\x00") + tlv(0x30, vb)
    return tlv(0x30, tlv(0x02, bytes([version])) + tlv(0x04, community) + tlv(pdu, body))


def request_multi(kind, oids, community=b"public"):
    pdu = {"get": 0xA0, "next": 0xA1}[kind]
    vbs = b"".join(tlv(0x30, tlv(0x06, enc_oid(o)) + tlv(0x05, b"")) for o in oids)
    body = tlv(0x02, b"\x07") + tlv(0x02, b"\x00") + tlv(0x02, b"\x00") + tlv(0x30, vbs)
    return tlv(0x30, tlv(0x02, b"\x00") + tlv(0x04, community) + tlv(pdu, body))


def parse_multi(raw):
    """Returns (error status, error index, [(oid, tag, value)]) and checks every length."""
    tag, body, end = read_tlv(raw, 0)
    assert tag == 0x30 and end == len(raw), "outer length"
    _, _, i = read_tlv(body, 0)
    _, _, i = read_tlv(body, i)
    ptag, pbody, pend = read_tlv(body, i)
    assert ptag == 0xA2 and pend == len(body), "pdu length"
    j = 0
    _, _, j = read_tlv(pbody, j)
    _, est, j = read_tlv(pbody, j)
    _, eidx, j = read_tlv(pbody, j)
    ltag, lbody, lend = read_tlv(pbody, j)
    assert ltag == 0x30 and lend == len(pbody), "list length"
    out, k = [], 0
    while k < len(lbody):
        _, vb, k = read_tlv(lbody, k)
        _, oid, m = read_tlv(vb, 0)
        vtag, val, mend = read_tlv(vb, m)
        assert mend == len(vb), "varbind length"
        out.append((dec_oid(oid), vtag, val))
    return int.from_bytes(est, "big"), int.from_bytes(eidx, "big"), out


def read_tlv(b, i):
    tag = b[i]
    n = b[i + 1]
    j = i + 2
    if n & 0x80:
        k = n & 0x7F
        n = int.from_bytes(b[j:j + k], "big")
        j += k
    return tag, b[j:j + n], j + n


def parse_response(raw):
    tag, body, end = read_tlv(raw, 0)
    assert tag == 0x30, "outer tag %x" % tag
    assert end == len(raw), "outer length %d, bytes %d" % (end, len(raw))
    _, _, i = read_tlv(body, 0)
    _, _, i = read_tlv(body, i)
    pdu, pbody, _ = read_tlv(body, i)
    assert pdu == 0xA2, "pdu %x" % pdu
    j = 0
    _, _, j = read_tlv(pbody, j)
    _, est, j = read_tlv(pbody, j)
    _, eidx, j = read_tlv(pbody, j)
    _, vbl, _ = read_tlv(pbody, j)
    _, vb, vend = read_tlv(vbl, 0)
    assert vend == len(vbl), "varbind list length"
    _, oid, k = read_tlv(vb, 0)
    vtag, val, kend = read_tlv(vb, k)
    assert kend == len(vb), "varbind length"
    return int.from_bytes(est, "big"), dec_oid(oid), vtag, val


class Agent:
    def __init__(self, mode, reason=0):
        self.p = subprocess.Popen([BIN, mode, str(reason)], stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                                  stderr=subprocess.PIPE, text=True)

    def ask(self, kind, oid, **kw):
        self.p.stdin.write(request(kind, oid, **kw).hex() + "\n")
        self.p.stdin.flush()
        while True:
            line = self.p.stdout.readline().strip()
            if line.startswith("RSP:"):
                break
            if not line:
                raise RuntimeError("driver exited: " + self.p.stderr.read()[:600])
        line = line[4:]
        if line.startswith("ERR"):
            return line
        return parse_response(bytes.fromhex(line))

    def ask_raw(self, packet):
        self.p.stdin.write(packet.hex() + "\n")
        self.p.stdin.flush()
        while True:
            line = self.p.stdout.readline().strip()
            if line.startswith("RSP:"):
                break
            if not line:
                raise RuntimeError("driver exited: " + self.p.stderr.read()[:600])
        line = line[4:]
        return None if line.startswith("ERR") else bytes.fromhex(line)

    def close(self):
        self.p.stdin.close()
        err = self.p.stderr.read()
        rc = self.p.wait()
        return rc, err


def value(vtag, val):
    if vtag == 0x04:
        return val.decode()
    return int.from_bytes(val, "big")


def walk(agent, start="1.3.6.1"):
    oid, out = start, []
    for _ in range(200):
        r = agent.ask("next", oid)
        if isinstance(r, str):
            return out, r
        est, noid, vtag, val = r
        if est:
            return out, "error status %d at %s" % (est, oid)
        out.append((noid, value(vtag, val)))
        oid = noid
    return out, "no end after 200 steps"


fails = 0


def check(name, ok, info=""):
    global fails
    print(("PASS " if ok else "FAIL ") + name + ("  " + str(info) if info and not ok else ""))
    if not ok:
        fails += 1


B = "1.3.6.1.4.1.54539.100"

# --- master -----------------------------------------------------------------
a = Agent("master", reason=4)
r = a.ask("get", "1.3.6.1.2.1.1.1.0")
check("get sysDescr", not isinstance(r, str) and value(r[2], r[3]) == "NPR-70", r)
r = a.ask("get", "1.3.6.1.2.1.1.5.0")
check("get sysName = own callsign", not isinstance(r, str) and value(r[2], r[3]) == "DL6ABE-70", r)
r = a.ask("get", B + ".1.0")
check("get temperature", not isinstance(r, str) and value(r[2], r[3]) == 32, r)
r = a.ask("get", "1.3.6.1.2.1.1.1.0", community=b"wrong")
check("wrong community dropped", isinstance(r, str), r)
r = a.ask("get", "1.3.6.1.2.1.1.1.0", version=1)
check("v2c dropped (agent is v1 only)", isinstance(r, str), r)

r = a.ask("set", "1.3.6.1.2.1.1.1.0", setval=b"HACKED")
check("SET is refused (noSuchName)", not isinstance(r, str) and r[0] == 2, r)
r = a.ask("get", "1.3.6.1.2.1.1.1.0")
check("sysDescr unchanged after SET", not isinstance(r, str) and value(r[2], r[3]) == "NPR-70", r)
for pkt in ("", "30", "3000", "0102030405060708"):
    a.p.stdin.write(pkt + "\n")
    a.p.stdin.flush()
    while not a.p.stdout.readline().startswith("RSP:"):
        pass
check("short and garbage packets survived", True)
r = a.ask("get", "1.3.6.1.2.1.1.1.0")
check("agent still answers after garbage", not isinstance(r, str) and value(r[2], r[3]) == "NPR-70", r)

# several variables in one request, the response is longer than 127 bytes and longer than the request
cols = [B + ".10.1.%d.1" % c for c in (1, 2, 3, 4, 5)]
raw = a.ask_raw(request_multi("get", cols))
check("multi get: answered", raw is not None)
if raw:
    est, eidx, vbs = parse_multi(raw)
    check("multi get: response is longer than 127 bytes", len(raw) > 127, len(raw))
    check("multi get: no error", est == 0 and eidx == 0, (est, eidx))
    check("multi get: values", [(o, value(t, v)) for o, t, v in vbs] ==
          [(cols[0], 1), (cols[1], "DL6ABE-71"), (cols[2], 10), (cols[3], 80), (cols[4], 250)], vbs)
raw = a.ask_raw(request_multi("get", [cols[0], cols[1], "1.3.6.1.2.1.1.9.9", cols[2]]))
est, eidx, vbs = parse_multi(raw) if raw else (None, None, [])
check("multi get: missing name gives noSuchName at its index", (est, eidx) == (2, 3), (est, eidx))
check("multi get: the other variables are answered", len(vbs) == 4 and value(vbs[3][1], vbs[3][2]) == 10, vbs)
raw = a.ask_raw(request_multi("next", [B + ".10.1.1", B + ".10.1.2", B + ".10.1.3"]))
est, eidx, vbs = parse_multi(raw) if raw else (None, None, [])
check("multi get-next (what snmptable sends)", est == 0 and [o for o, _, _ in vbs] ==
      [B + ".10.1.1.0", B + ".10.1.2.0", B + ".10.1.3.0"], (est, vbs))
raw = a.ask_raw(request_multi("get", [B + ".10.1.2.1"] * 18))
est, eidx, vbs = parse_multi(raw) if raw else (None, None, [])
check("response too big for the buffer: tooBig, empty list", (est, eidx, vbs) == (1, 0, []), (est, eidx, len(vbs)))

# GET-NEXT from OIDs that are no entry: the next greater entry, not the first one
for start, want in (
        ("1.3.6.1.2.1.1", "1.3.6.1.2.1.1.1.0"),
        ("1.3.6.1.2.1.1.1.0.5", "1.3.6.1.2.1.1.3.0"),
        (B + ".10", B + ".10.1.1.0"),
        (B + ".10.1", B + ".10.1.1.0"),
        (B + ".10.1.1", B + ".10.1.1.0"),
        (B + ".10.1.1.6", B + ".10.1.2.0"),
        (B + ".11", B + ".11.0"),
        ("1.3.6.1.4.1.54539.99", B + ".1.0"),
        ("1.3.6.1.4.1.54540", None),
        (B + ".11.0", None)):
    r = a.ask("next", start)
    got = None if (isinstance(r, str) or r[0]) else r[1]
    check("get-next from %s" % start, got == want, got)

entries, why = walk(a)
oids = [o for o, _ in entries]
print("walk steps:", len(entries), "stop:", why)
check("walk reaches the vendor subtree", any(o.startswith(B) for o in oids), why)
check("walk is strictly increasing", all(tuple(map(int, x.split("."))) < tuple(map(int, y.split(".")))
                                         for x, y in zip(oids, oids[1:])))
check("walk ends with noSuchName", why.startswith("error status 2"), why)
d = dict(entries)
check("table: client 1 connected", d.get(B + ".10.1.1.1") == 1, d.get(B + ".10.1.1.1"))
check("table: client 2 not connected", d.get(B + ".10.1.1.2") == 0, d.get(B + ".10.1.1.2"))
check("table: callsign client 1", d.get(B + ".10.1.2.1") == "DL6ABE-71", d.get(B + ".10.1.2.1"))
check("table: callsign client 3", d.get(B + ".10.1.2.3") == "DL6ABE-72", d.get(B + ".10.1.2.3"))
check("table: callsign free slot empty", d.get(B + ".10.1.2.0") == "", d.get(B + ".10.1.2.0"))
check("table: TA client 3 = 25", d.get(B + ".10.1.3.3") == 25, d.get(B + ".10.1.3.3"))
check("table: RSSI client 1 = 80", d.get(B + ".10.1.4.1") == 0x50, d.get(B + ".10.1.4.1"))
check("table: BER client 3 = 1000", d.get(B + ".10.1.5.3") == 1000, d.get(B + ".10.1.5.3"))
check("firmware version", d.get(B + ".11.0") == "261011G", d.get(B + ".11.0"))
check("reset reason code", d.get(B + ".8.0") == 4, d.get(B + ".8.0"))
check("reset reason text", d.get(B + ".9.0") == "iwdg", d.get(B + ".9.0"))
check("master scalar RSSI is 0, not garbage", d.get(B + ".4.0") == 0, d.get(B + ".4.0"))
rc, err = a.close()
check("no sanitizer report (master)", rc == 0 and not err, err[:400])

# --- client -----------------------------------------------------------------
c = Agent("client", reason=1)
entries, why = walk(c)
d = dict(entries)
check("client walk ends cleanly", why.startswith("error status 2"), why)
check("client scalar link status", d.get(B + ".2.0") == 2, d.get(B + ".2.0"))
check("client reset reason text", d.get(B + ".9.0") == "power-on", d.get(B + ".9.0"))
rc, err = c.close()
check("no sanitizer report (client)", rc == 0 and not err, err[:400])

print("\n%d failure(s)" % fails)
sys.exit(1 if fails else 0)

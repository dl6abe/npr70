// This file is part of "NPR70 modem firmware" software
// (A GMSK data modem for ham radio 430-440MHz, at several hundreds of kbps)
// Copyright (c) 2017-2020 Guillaume F. F4HDK (amateur radio callsign)
//
// "NPR70 modem firmware" is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// "NPR70 modem firmware" is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with "NPR70 modem firmware".  If not, see <http://www.gnu.org/licenses/>

#include "SNMP.h"
#include "Eth_IPv4.h"

// We are reusing similar data structures as what's done in the WizNet ioLib
// SNMP message structures
// 

// This will reserve about 1k of RAM
struct messageStruct request_msg;
struct messageStruct response_msg;

void snmpGetVar(void *ptr, uint8_t *len, const uint32_t var);

// Per client table, filled on the master (the master keeps all client data):
//   1.3.6.1.4.1.54539.100.10.1.<column>.<client ID 0..6>   (table 10, entry 1, see NPR-70.mib)
// Column 1 connected (1/0), 2 callsign, 3 distance (x 0.15 km), 4 RSSI uplink (value/2-136 dBm),
// 5 error rate uplink (value/500 percent). The units match the scalar entries .3, .4 and .6.
// The columns are written one after the other so the list stays in ascending OID order,
// which GET-NEXT (snmpwalk) needs.
static_assert(radio_addr_table_size == 7, "the client table below lists 7 clients");

template <uint8_t ID> void snmpGetClientConnected(void *ptr, uint8_t *len)
{
    snmpGetVar(ptr, len, CONF_radio_addr_table_status[ID] ? 1 : 0);
}

template <uint8_t ID> void snmpGetClientCallsign(void *ptr, uint8_t *len)
{
    char *out = (char *)ptr; // MAX_STRING bytes, the caller measures it with strlen
    out[0] = 0;
    if (CONF_radio_addr_table_status[ID]) {
        // the first two bytes of the stored callsign are not part of the name
        strncpy(out, CONF_radio_addr_table_callsign[ID] + 2, MAX_STRING - 1);
        out[MAX_STRING - 1] = 0;
    }
    *len = strlen(out);
}

template <uint8_t ID> void snmpGetClientDistance(void *ptr, uint8_t *len)
{
    // The master keeps the distance ten times finer than what it sends in the who message
    long ta = TDMA_table_TA[ID] / 10;
    snmpGetVar(ptr, len, (CONF_radio_addr_table_status[ID] && ta > 0) ? ta : 0);
}

template <uint8_t ID> void snmpGetClientRSSI(void *ptr, uint8_t *len)
{
    // The master keeps the filtered RSSI in the upper byte
    snmpGetVar(ptr, len, CONF_radio_addr_table_status[ID] ? (G_radio_addr_table_RSSI[ID] >> 8) : 0);
}

template <uint8_t ID> void snmpGetClientErr(void *ptr, uint8_t *len)
{
    snmpGetVar(ptr, len, CONF_radio_addr_table_status[ID] ? G_radio_addr_table_BER[ID] : 0);
}

// 1.3.6.1.4.1.54539.100.10.1.<col>.<id>
#define SNMP_CLIENT_ENTRY(col, type, getter, id) \
    {13, {0x2b, 6, 1, 4, 1, 0x83, 0xaa, 0x0b, 100, 10, 1, col, id}, type, 0, {}, getter<id>, NULL }
#define SNMP_CLIENT_COLUMN(col, type, getter) \
    SNMP_CLIENT_ENTRY(col, type, getter, 0), SNMP_CLIENT_ENTRY(col, type, getter, 1), \
    SNMP_CLIENT_ENTRY(col, type, getter, 2), SNMP_CLIENT_ENTRY(col, type, getter, 3), \
    SNMP_CLIENT_ENTRY(col, type, getter, 4), SNMP_CLIENT_ENTRY(col, type, getter, 5), \
    SNMP_CLIENT_ENTRY(col, type, getter, 6)

static const dataEntryType snmpData[] =
    {
        // System MIB
        // SysDescr Entry
        {8, {0x2b, 6, 1, 2, 1, 1, 1, 0}, ASN_OCTETSTRING, 6, {"NPR-70"}, NULL, NULL},

        // SysUptime Entry
        {8, {0x2b, 6, 1, 2, 1, 1, 3, 0}, ASN_TIMETICKS, 0, {}, snmpGetUptime, NULL},

        // SysName Entry: the callsign of this modem
        {8, {0x2b, 6, 1, 2, 1, 1, 5, 0}, ASN_OCTETSTRING, 0, {}, snmpGetSysName, NULL},

        // 1.3.6.1.4.1.54539 (Wizkers)
        //               .100 (NPR-70)
        //               .1               // Temperature
        {11, {0x2b, 6, 1, 4, 1, 0x83, 0xaa, 0x0b, 100, 1, 0}, ASN_GAUGE,0, {}, snmpGetTemp, NULL },
        //               .2               // Link status
        {11, {0x2b, 6, 1, 4, 1, 0x83, 0xaa, 0x0b, 100, 2, 0}, ASN_INTEGER,0, {}, snmpGetLinkStatus, NULL },
        //               .3               // Link distance
        {11, {0x2b, 6, 1, 4, 1, 0x83, 0xaa, 0x0b, 100, 3, 0}, ASN_INTEGER,0, {}, snmpGetLinkDistance, NULL },
        //               .4.0             // RSSI Uplink
        {11, {0x2b, 6, 1, 4, 1, 0x83, 0xaa, 0x0b, 100, 4, 0}, ASN_GAUGE,0, {}, snmpGetRSSIUp, NULL },
        //               .5.0             // RSSI Downlink
        {11, {0x2b, 6, 1, 4, 1, 0x83, 0xaa, 0x0b, 100, 5, 0}, ASN_GAUGE,0, {}, snmpGetRSSIDown, NULL },
        //               .6.0             // Error rate Uplink
        {11, {0x2b, 6, 1, 4, 1, 0x83, 0xaa, 0x0b, 100, 6, 0}, ASN_GAUGE,0, {}, snmpGetErrUp, NULL },
        //               .7.0             // Error rate Downlink
        {11, {0x2b, 6, 1, 4, 1, 0x83, 0xaa, 0x0b, 100, 7, 0}, ASN_GAUGE,0, {}, snmpGetErrDown, NULL },
        //               .8.0             // Reset reason of the last start (code)
        {11, {0x2b, 6, 1, 4, 1, 0x83, 0xaa, 0x0b, 100, 8, 0}, ASN_INTEGER,0, {}, snmpGetResetReason, NULL },
        //               .9.0             // Reset reason of the last start (text)
        {11, {0x2b, 6, 1, 4, 1, 0x83, 0xaa, 0x0b, 100, 9, 0}, ASN_OCTETSTRING,0, {}, snmpGetResetReasonText, NULL },
        //               .10.1.<column>.<client ID>   // client table, see above
        SNMP_CLIENT_COLUMN(1, ASN_INTEGER, snmpGetClientConnected),
        SNMP_CLIENT_COLUMN(2, ASN_OCTETSTRING, snmpGetClientCallsign),
        SNMP_CLIENT_COLUMN(3, ASN_GAUGE, snmpGetClientDistance),
        SNMP_CLIENT_COLUMN(4, ASN_GAUGE, snmpGetClientRSSI),
        SNMP_CLIENT_COLUMN(5, ASN_GAUGE, snmpGetClientErr),
        //               .11.0            // Firmware version (text)
        {11, {0x2b, 6, 1, 4, 1, 0x83, 0xaa, 0x0b, 100, 11, 0}, ASN_OCTETSTRING,0, {}, snmpGetFirmwareVersion, NULL },
        //               .12.1.0 .. .12.4.0   // Received radio frames since start: FEC ok, repaired, lost; IP packets lost to a missing segment
        {12, {0x2b, 6, 1, 4, 1, 0x83, 0xaa, 0x0b, 100, 12, 1, 0}, ASN_COUNTER,0, {}, snmpGetFecOk, NULL },
        {12, {0x2b, 6, 1, 4, 1, 0x83, 0xaa, 0x0b, 100, 12, 2, 0}, ASN_COUNTER,0, {}, snmpGetFecFixed, NULL },
        {12, {0x2b, 6, 1, 4, 1, 0x83, 0xaa, 0x0b, 100, 12, 3, 0}, ASN_COUNTER,0, {}, snmpGetFecLost, NULL },
        {12, {0x2b, 6, 1, 4, 1, 0x83, 0xaa, 0x0b, 100, 12, 4, 0}, ASN_COUNTER,0, {}, snmpGetSegGap, NULL },

};

const uint8_t maxData = (sizeof(snmpData) / sizeof(dataEntryType));

// Names for G_reset_reason, set at start from the reset flags of the MCU
static const char* const reset_reason_names[] = {
    "unknown", "power-on", "pin", "software", "iwdg", "wwdg", "low-power", "option-bytes"
};

const char* snmp_reset_reason_text(unsigned char code)
{
    if (code >= sizeof(reset_reason_names) / sizeof(reset_reason_names[0]))
        code = 0;
    return reset_reason_names[code];
}

void snmpGetSysName(void *ptr, uint8_t *len)
{
    char *out = (char *)ptr; // MAX_STRING bytes
    // the first two bytes of the stored callsign are not part of the name
    strncpy(out, CONF_radio_my_callsign + 2, MAX_STRING - 1);
    out[MAX_STRING - 1] = 0;
    *len = strlen(out);
}

void snmpGetFecOk(void *ptr, uint8_t *len)
{
    snmpGetVar(ptr, len, G_rx_fec_ok);
}

void snmpGetFecFixed(void *ptr, uint8_t *len)
{
    snmpGetVar(ptr, len, G_rx_fec_fixed);
}

void snmpGetFecLost(void *ptr, uint8_t *len)
{
    snmpGetVar(ptr, len, G_rx_fec_lost);
}

void snmpGetSegGap(void *ptr, uint8_t *len)
{
    snmpGetVar(ptr, len, G_rx_seg_gap);
}

void snmpGetFirmwareVersion(void *ptr, uint8_t *len)
{
    char *out = (char *)ptr; // MAX_STRING bytes
    strncpy(out, FW_VERSION, MAX_STRING - 1);
    out[MAX_STRING - 1] = 0;
    *len = strlen(out);
}

void snmpGetResetReason(void *ptr, uint8_t *len)
{
    snmpGetVar(ptr, len, G_reset_reason);
}

void snmpGetResetReasonText(void *ptr, uint8_t *len)
{
    char *out = (char *)ptr; // MAX_STRING bytes
    strncpy(out, snmp_reset_reason_text(G_reset_reason), MAX_STRING - 1);
    out[MAX_STRING - 1] = 0;
    *len = strlen(out);
}

// Seconds since snmp_init() set the clock to 0 at the start, same source for SNMP and the console
uint32_t snmp_uptime_seconds(void)
{
    return (uint32_t)time(NULL);
}

void snmpGetUptime(void *ptr, uint8_t *len) {
    *(uint32_t *)ptr = snmp_uptime_seconds() * 100; // Time in 100th of a second
    *len = 4; 
}

void snmpGetVar(void *ptr, uint8_t *len,  const uint32_t var) {
	*(uint32_t *)ptr = (uint32_t)(var);
	*len = 4;
}

void snmpGetTemp(void *ptr, uint8_t *len)
{
    snmpGetVar(ptr, len, G_temperature_SI4463);
}

void snmpGetLinkStatus(void *ptr, uint8_t *len)
{
    // State can be:
    // 1: "waiting connection"
    // 2: "connected"
    // 3: "connection rejected"
    snmpGetVar(ptr, len, my_client_radio_connexion_state);
}

// The master and a client that is not connected have no valid own client ID (0x7E, 0xFE).
// Indexing the tables with it reads outside of the arrays.
static bool snmpOwnIdValid()
{
    return (!is_TDMA_master) && (my_radio_client_ID < radio_addr_table_size);
}

void snmpGetLinkDistance(void *ptr, uint8_t *len)
{
    // Formula is : 0.15 * value
    snmpGetVar(ptr, len, snmpOwnIdValid() ? TDMA_table_TA[my_radio_client_ID] : 0);
}

void snmpGetRSSIUp(void *ptr, uint8_t *len)
{
    // Formula for dBm is: value/2-136
    snmpGetVar(ptr, len, snmpOwnIdValid() ? G_radio_addr_table_RSSI[my_radio_client_ID] : 0);
}

void snmpGetRSSIDown(void *ptr, uint8_t *len)
{
    // Formula for dBm is: value/2-136
    snmpGetVar(ptr, len, (G_downlink_RSSI >> 8));
}

void snmpGetErrUp(void *ptr, uint8_t *len)
{
    // Formula for percent is: value / 500
    snmpGetVar(ptr, len, snmpOwnIdValid() ? G_radio_addr_table_BER[my_radio_client_ID] : 0);
}

void snmpGetErrDown(void *ptr, uint8_t *len)
{
    // Formula for percent is: value /500
    snmpGetVar(ptr, len, G_downlink_BER);
}

int8_t findEntry(uint8_t *oid, uint8_t len)
{
    uint8_t i;

    for (i = 0; i < maxData; i++)
    {
        if (len == snmpData[i].oidlen)
        {
            if (!memcmp(snmpData[i].oid, oid, len))
                return (i);
        }
    }

    return OID_NOT_FOUND;
}

// GET-NEXT: the first entry whose OID is greater than the requested one, even if the requested OID
// is not an entry (a table, a subtree, an OID that ends in the middle). snmpData is in ascending
// OID order. The encoded bytes of an OID compare in the same order as its numbers.
int8_t findNextEntry(uint8_t *oid, uint8_t len)
{
    uint8_t i;

    for (i = 0; i < maxData; i++)
    {
        uint8_t common = (len < snmpData[i].oidlen) ? len : snmpData[i].oidlen;
        int cmp = memcmp(snmpData[i].oid, oid, common);

        if (cmp == 0)
            cmp = (int)snmpData[i].oidlen - (int)len;

        if (cmp > 0)
            return (i);
    }

    return OID_NOT_FOUND;
}

int8_t getOID(int8_t id, uint8_t *oid, uint8_t *len)
{
    uint8_t j;

    if (!((id >= 0) && (id < maxData)))
        return INVALID_ENTRY_ID;

    *len = snmpData[id].oidlen;

    for (j = 0; j < *len; j++)
    {
        oid[j] = snmpData[id].oid[j];
    }

    return SNMP_SUCCESS;
}

// snmpData is read-only and lives in flash. The value of an entry that has a getter is built in
// these buffers for the request that is answered right now.
static int32_t getEntry(int8_t id, uint8_t *dataType, void *ptr, uint8_t *len)
{
    if (!((id >= 0) && (id < maxData)))
        return INVALID_ENTRY_ID;

    const dataEntryType *entry = &snmpData[id];
    uint8_t datalen = entry->dataLen;

    *dataType = entry->dataType;

    switch (*dataType)
    {
    case ASN_OCTETSTRING:
    case ASN_OBJECTID:
    {
        uint8_t *string = (uint8_t*) ptr;
        char text[MAX_STRING + 1];

        memcpy(text, entry->u.octetstring, MAX_STRING);
        text[MAX_STRING] = 0;

        if (entry->getfunction != NULL)
        {
            entry->getfunction((void *)text, &datalen);
            text[MAX_STRING] = 0;
        }

        if ((*dataType) == ASN_OCTETSTRING)
        {
            datalen = (uint8_t)strlen(text);
        }

        *len = datalen;
        for (uint8_t j = 0; j < *len; j++)
        {
            string[j] = (uint8_t)text[j];
        }
    }
    break;

    case ASN_INTEGER:
    case ASN_TIMETICKS:
    case ASN_COUNTER:
    case ASN_GAUGE:
    {
        uint32_t value = entry->u.intval;

        if (entry->getfunction != NULL)
        {
            entry->getfunction((void *)&value, &datalen);
        }

        if (datalen)
            *len = datalen;
        else
            *len = sizeof(uint32_t);

        uint8_t *ptr_8 = (uint8_t*) ptr;
        for (uint8_t j = 0; j < *len; j++)
        {
            ptr_8[j] = (uint8_t)((value >> ((*len - j - 1) * 8)));
        }
    }
    break;

    default:
        return INVALID_DATA_TYPE;
    }

    return SNMP_SUCCESS;
}

// ---------------------------------------------------------------------------------------------
// Request parsing and response building (SNMP v1: GET and GET-NEXT, SET is refused)
//
// The request is read with bounds checks. The response is written from scratch: each container
// (SEQUENCE, PDU) reserves a 3 byte length field and is closed with the real length in the
// shortest form. The response can be longer than the request, so its length form must not be
// taken over from the request.
// ---------------------------------------------------------------------------------------------

struct tlvRef {
    uint8_t tag;
    const uint8_t *start; // first byte of the TLV
    const uint8_t *val;   // first byte of the value
    uint16_t len;         // length of the value
    uint16_t total;       // length of the whole TLV
};

// Reads the TLV at buf[*pos], it must end before buf[end]. Moves *pos behind it.
static bool readTLV(const uint8_t *buf, uint16_t end, uint16_t *pos, tlvRef *t)
{
    uint16_t p = *pos;
    uint16_t l;

    if (p + 2 > end)
        return false;

    t->tag = buf[p];
    t->start = buf + p;
    l = buf[p + 1];
    p += 2;

    if (l & 0x80)
    {
        uint8_t n = l & 0x7f;
        if ((n == 0) || (n > 2) || (p + n > end))
            return false;
        l = 0;
        while (n--)
            l = (uint16_t)((l << 8) | buf[p++]);
    }

    if (p + l > end)
        return false;

    t->val = buf + p;
    t->len = l;
    t->total = (uint16_t)(p + l - *pos);
    *pos = p + l;
    return true;
}

struct respWriter {
    uint8_t *buf;
    uint16_t pos;
    bool ok; // false once something did not fit
};

static void wBytes(respWriter *w, const uint8_t *data, uint16_t n)
{
    if (w->pos + n > MAX_SNMPMSG_LEN)
    {
        w->ok = false;
        return;
    }
    memcpy(w->buf + w->pos, data, n);
    w->pos += n;
}

// Opens a container: tag and room for a 3 byte length. Returns where it starts.
static uint16_t wBegin(respWriter *w, uint8_t tag)
{
    uint16_t start = w->pos;
    uint8_t head[4] = {tag, 0x82, 0, 0};
    wBytes(w, head, 4);
    return start;
}

// Closes the container that starts at 'start': writes the length in the shortest form
static void wEnd(respWriter *w, uint16_t start)
{
    if (!w->ok)
        return;

    uint16_t len = w->pos - (start + 4);
    uint8_t lenBytes[3];
    uint8_t n;

    if (len < 128)
    {
        lenBytes[0] = (uint8_t)len;
        n = 1;
    }
    else if (len < 256)
    {
        lenBytes[0] = 0x81;
        lenBytes[1] = (uint8_t)len;
        n = 2;
    }
    else
    {
        lenBytes[0] = 0x82;
        lenBytes[1] = (uint8_t)(len >> 8);
        lenBytes[2] = (uint8_t)len;
        n = 3;
    }

    memmove(w->buf + start + 1 + n, w->buf + start + 4, len);
    memcpy(w->buf + start + 1, lenBytes, n);
    w->pos = start + 1 + n + len;
}

// tag, length (below 128) and the bytes
static void wValue(respWriter *w, uint8_t tag, const uint8_t *data, uint8_t len)
{
    wBytes(w, &tag, 1);
    wBytes(w, &len, 1);
    wBytes(w, data, len);
}

// Which entry answers this variable. SET is never allowed, the agent is read-only.
static int8_t lookupEntry(uint8_t reqType, const tlvRef *name)
{
    if (reqType == GET_REQUEST)
        return findEntry((uint8_t *)name->val, name->len);
    if (reqType == GET_NEXT_REQUEST)
        return findNextEntry((uint8_t *)name->val, name->len);
    return OID_NOT_FOUND;
}

// Reads the variable at buf[*p], the variable list ends before buf[end]
static bool readVarBind(const uint8_t *buf, uint16_t end, uint16_t *p, tlvRef *name, tlvRef *value)
{
    tlvRef vb;
    uint16_t q;

    if (!readTLV(buf, end, p, &vb) || (vb.tag != SNMP_SEQUENCE))
        return false;
    q = vb.val - buf;
    if (!readTLV(buf, q + vb.len, &q, name) || (name->tag != ASN_OBJECTID))
        return false;
    if (!readTLV(buf, vb.val - buf + vb.len, &q, value))
        return false;
    return true;
}

// Writes the response for the request in request_msg. Returns false if it does not fit.
static bool buildResponse(const tlvRef *ver, const tlvRef *comm, const tlvRef *reqid, uint8_t reqType,
                          uint16_t listPos, uint16_t listEnd, uint8_t errStatus, uint8_t errIndex)
{
    const uint8_t *req = request_msg.buffer;
    respWriter w = {response_msg.buffer, 0, true};
    uint16_t outer, pdu, list;
    uint16_t p = listPos;

    outer = wBegin(&w, SNMP_SEQUENCE);
    wBytes(&w, ver->start, ver->total);
    wBytes(&w, comm->start, comm->total);
    pdu = wBegin(&w, GET_RESPONSE);
    wBytes(&w, reqid->start, reqid->total);
    wValue(&w, ASN_INTEGER, &errStatus, 1);
    wValue(&w, ASN_INTEGER, &errIndex, 1);
    list = wBegin(&w, SNMP_SEQUENCE_OF);

    while ((errStatus != TOO_BIG) && (p < listEnd) && w.ok)
    {
        tlvRef name, value;
        uint16_t seq;
        int8_t id;
        uint8_t dataType, len;
        uint8_t data[MAX_STRING + 8];

        if (!readVarBind(req, listEnd, &p, &name, &value))
            return false;

        id = lookupEntry(reqType, &name);
        seq = wBegin(&w, SNMP_SEQUENCE);
        if ((id != OID_NOT_FOUND) && (getEntry(id, &dataType, data, &len) == SNMP_SUCCESS))
        {
            if (reqType == GET_NEXT_REQUEST)
            {
                uint8_t oid[MAX_OID];
                uint8_t oidLen;
                getOID(id, oid, &oidLen);
                wValue(&w, ASN_OBJECTID, oid, oidLen);
            }
            else
            {
                wBytes(&w, name.start, name.total);
            }
            wValue(&w, dataType, data, len);
        }
        else
        {
            // no such name: the variable goes back unchanged
            wBytes(&w, name.start, name.total);
            wBytes(&w, value.start, value.total);
        }
        wEnd(&w, seq);
    }

    wEnd(&w, list);
    wEnd(&w, pdu);
    wEnd(&w, outer);

    if (w.ok)
        response_msg.index = w.pos;
    return w.ok;
}

// Parses the request in request_msg and writes the response to response_msg.
// Returns 0 if there is a response to send, -1 if the request is dropped.
int8_t parse_snmp()
{
    const uint8_t *req = request_msg.buffer;
    uint16_t end = request_msg.len;
    uint16_t pos = 0;
    tlvRef msg, ver, comm, pdu, reqid, errst, erri, list;
    uint16_t p, q, listPos, listEnd;
    uint8_t errStatus = 0, errIndex = 0, index = 0;

    if (!readTLV(req, end, &pos, &msg) || (msg.tag != SNMP_SEQUENCE))
        return -1;
    p = msg.val - req;
    end = p + msg.len;

    if (!readTLV(req, end, &p, &ver) || (ver.tag != ASN_INTEGER) || (ver.len != 1) || (ver.val[0] != SNMP_V1))
        return -1;
    if (!readTLV(req, end, &p, &comm) || (comm.tag != ASN_OCTETSTRING) || (comm.len != COMMUNITY_SIZE) ||
        memcmp(comm.val, COMMUNITY, COMMUNITY_SIZE))
        return -1;
    if (!readTLV(req, end, &p, &pdu) || !VALID_REQUEST(pdu.tag))
        return -1;

    q = pdu.val - req;
    end = q + pdu.len;
    if (!readTLV(req, end, &q, &reqid) || (reqid.tag != ASN_INTEGER))
        return -1;
    if (!readTLV(req, end, &q, &errst) || !readTLV(req, end, &q, &erri))
        return -1;
    if (!readTLV(req, end, &q, &list) || (list.tag != SNMP_SEQUENCE_OF))
        return -1;
    listPos = list.val - req;
    listEnd = listPos + list.len;

    // first pass: the first variable that has no answer sets the error status
    for (p = listPos; p < listEnd;)
    {
        tlvRef name, value;
        if (!readVarBind(req, listEnd, &p, &name, &value))
            return -1;
        index++;
        if ((errStatus == 0) && (lookupEntry(pdu.tag, &name) == OID_NOT_FOUND))
        {
            errStatus = NO_SUCH_NAME;
            errIndex = index;
        }
    }

    if (buildResponse(&ver, &comm, &reqid, pdu.tag, listPos, listEnd, errStatus, errIndex))
        return 0;

    // the answer does not fit: tooBig, with an empty variable list
    if (buildResponse(&ver, &comm, &reqid, pdu.tag, listPos, listEnd, TOO_BIG, 0))
        return 0;

    return -1;
}

/**
 * Initialize the SNMP stack
 */
void snmp_init() {
    set_time(0);
}

/**
 * Called at regular intervals and acts if the SNMP socket contains data
 */
void snmp_loop(W5500_chip *W5500)
{
    int8_t ret;

    if (W5500_read_received_size(W5500, SNMP_SOCKET) > 0)
    {
        uint8_t buf[6]; // caller IP (4 bytes) and source port (2 bytes)
        uint16_t idx = 0;

        request_msg.len = W5500_read_UDP_pckt(W5500, SNMP_SOCKET, request_msg.buffer, MAX_SNMPMSG_LEN);
        if (request_msg.len <= 8) {
            // Shorter than the W5500 UDP header: nothing to parse (and the length below would wrap)
            return;
        }

        // Extract the caller's IP and source port for our reply:
        for (int i=0;i<6;i++) {
            buf[i] = request_msg.buffer[i];
        }
	    W5500_write_long(W5500, W5500_Sn_DIPR0, SNMP_SOCKET, buf, 4); //IP destination
        W5500_write_long(W5500, W5500_Sn_DPORT0, SNMP_SOCKET, buf+4, 2); //port tx

        // Initialize the response
        // We have 8 extra bytes at the beginning we don't want, so we realign the buffer:
        request_msg.len -= 8;
        while (idx < request_msg.len) {
            request_msg.buffer[idx] = request_msg.buffer[idx+8];
            idx++;
        }
        request_msg.index = 0; 
        response_msg.index = 0;
        memset(response_msg.buffer, 0x00, MAX_SNMPMSG_LEN);

        ret = parse_snmp();
        if (ret > -1)
        {
            W5500_write_TX_buffer(W5500, SNMP_SOCKET, response_msg.buffer, response_msg.index, 0);
        } else {
            printf("Failed to parse the SNMP message: %i\r\n", ret);
        }
    }
}

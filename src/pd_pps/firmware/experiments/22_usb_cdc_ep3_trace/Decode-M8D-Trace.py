#!/usr/bin/env python3
"""Decode experiment-22 EP3 event log from a Capture-M8D-UsbTrace.ps1 ZIP.

Requires Python 3.8+. Read-only; no MCU, serial port, or git access.
"""
import argparse
import re
import struct
import sys
import zipfile

MAGIC = 0x4D385452
RAM_BASE = 0x20000000
RAM_SIZE = 20480
EVENT_FIELDS = 'sequence event main_loops arg usb_flags ep3_regs ctx_flags tx_accepted ep3_in_completed'.split()
EVENT_KINDS = {
    1: 'TX_BEGIN', 2: 'TX_ACCEPTED', 3: 'TX_REJECTED',
    4: 'WAIT_SAMPLE', 5: 'WAIT_END',
    16: 'IRQ_EP3_IN', 17: 'IRQ_EP2_OUT', 18: 'IRQ_CTRL_SETUP'
}


def locate(map_text):
    # GNU ld .map symbol line normally: 0x20000abc                m8d_usb_trace
    hits = re.findall(r'^\s*(0x[0-9a-fA-F]+)\s+m8d_usb_trace\s*$', map_text, flags=re.M)
    if len(hits) != 1:
        raise ValueError('Expected exactly one m8d_usb_trace symbol in MAP, got %d' % len(hits))
    address = int(hits[0], 16)
    if not (RAM_BASE <= address < RAM_BASE + RAM_SIZE):
        raise ValueError('Symbol address is outside CH32X035 RAM: 0x%08X' % address)
    return address


def decode(sram, map_text, show=20):
    if len(sram) != RAM_SIZE:
        raise ValueError('Expected 20480 SRAM bytes, got %d' % len(sram))
    address = locate(map_text)
    pos = address - RAM_BASE
    (magic, version, size, event_size, capacity, fg_count, irq_count,
     loops, waiting, max_wait, last_usb, last_ep3, last_ctx, last_tx, last_done) = struct.unpack_from('<15I', sram, pos)
    if magic != MAGIC or version != 1 or event_size != 36 or capacity != 32 or size != 2364:
        raise ValueError('Trace ABI mismatch: magic=0x%08X version=%d struct=%d event=%d cap=%d' %
                         (magic, version, size, event_size, capacity))
    if pos + size > len(sram):
        raise ValueError('Trace struct runs past SRAM')
    print('M8D USB EP3 trace (experiment 22, ABI version 1)')
    print('Symbol: 0x%08X  struct: %d bytes  record: %d bytes' % (address, size, event_size))
    print('Foreground loops: %d; outstanding iterations: %d; max: %d' % (loops, waiting, max_wait))
    print('Foreground events: %d; IRQ events: %d' % (fg_count, irq_count))
    print('Last polled registers: INT_FG=0x%02X INT_ST=0x%02X BASE=0x%02X UDEV=0x%02X' %
          (last_usb & 0xff, (last_usb >> 8) & 0xff, (last_usb >> 16) & 0xff, (last_usb >> 24) & 0xff))
    print('Last polled EP3: TX_LEN=%d TX_CTRL=0x%02X INT_EN=0x%02X UEP2_3_MOD=0x%02X' %
          (last_ep3 & 0xff, (last_ep3 >> 8) & 0xff, (last_ep3 >> 16) & 0xff, (last_ep3 >> 24) & 0xff))
    print('Last polled context: EP3_busy=%d mode=%d setup_len=%d errata_window=%d' %
          (last_ctx & 0xff, (last_ctx >> 8) & 0xff, (last_ctx >> 16) & 0x7fff, last_ctx >> 31))
    print('Last polled TX accepted/completed: %d / %d' % (last_tx, last_done))
    out = {}
    for label, count, off in [('Foreground', fg_count, pos+60), ('IRQ', irq_count, pos+60+32*36)]:
        events = []
        beginning = max(1, count - min(show, capacity) + 1)
        for seq in range(beginning, count+1):
            index = (seq-1) % capacity
            values = struct.unpack_from('<9I', sram, off+index*36)
            event = dict(zip(EVENT_FIELDS, values))
            if event['sequence'] != seq:
                print('WARNING: %s sequence %d overwritten/missing (stored %d)' % (label, seq, event['sequence']))
                continue
            events.append(event)
        out[label] = events
        print('\n%s events, latest %d (ring holds 32):' % (label, len(events)))
        for e in events:
            flags, hw, ctx = e['usb_flags'], e['ep3_regs'], e['ctx_flags']
            print('  #%06d %-14s loops=%-11d arg=%-10d accept/complete=%d/%d busy=%d len=%d ctrl=0x%02X FG=0x%02X ST=0x%02X' %
                  (e['sequence'], EVENT_KINDS.get(e['event'], '?'+str(e['event'])),
                   e['main_loops'], e['arg'], e['tx_accepted'], e['ep3_in_completed'],
                   ctx & 0xff, hw & 0xff, (hw >> 8) & 0xff, flags & 0xff, (flags >> 8) & 0xff))
    print('\nNote: IRQ callback snapshots occur before fsusb clears EP3 busy; this is expected.')
    print('Note: foreground-loop counts are NOT elapsed time; debugger capture may change target state.')
    return out


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('archive', help='M8D-EP3-trace-Failed-<timestamp>.zip')
    ap.add_argument('--show', type=int, default=20, help='latest records per ring, 1..32')
    args = ap.parse_args()
    if not 1 <= args.show <= 32:
        ap.error('--show must be 1..32')
    with zipfile.ZipFile(args.archive) as z:
        names = z.namelist()
        def get(name):
            matches = [n for n in names if n.split('/')[-1] == name]
            if len(matches) != 1:
                raise ValueError('Expected exactly one %s in ZIP' % name)
            return z.read(matches[0])
        sram = get('sram-20kb.bin')
        map_text = get('m8d_usb_cdc_ep3_trace.map').decode('utf-8', errors='replace')
    decode(sram, map_text, args.show)

if __name__ == '__main__':
    try:
        main()
    except (OSError, zipfile.BadZipFile, ValueError, struct.error) as exc:
        print('ERROR: '+str(exc), file=sys.stderr)
        sys.exit(1)

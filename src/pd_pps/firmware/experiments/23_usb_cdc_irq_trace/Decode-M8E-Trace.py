#!/usr/bin/env python3
"""Read experiment-23 capture ZIP: M8E raw USBFS IRQ entry + M8D callback trace.

Read-only, standard library only. Event loops are not real time.
"""
import argparse
import re
import struct
import sys
import zipfile
RAM_BASE = 0x20000000
RAM_SIZE = 20480


def find_symbol(map_text, name):
    matches = re.findall(r'^\s*(0x[0-9a-fA-F]+)\s+' + re.escape(name) + r'\s*$', map_text, re.M)
    if len(matches) != 1:
        raise ValueError('Expected exactly one %s MAP symbol, got %d' % (name, len(matches)))
    address = int(matches[0], 16)
    if not (RAM_BASE <= address < RAM_BASE + RAM_SIZE):
        raise ValueError('Address outside SRAM: 0x%08X' % address)
    return address


def decode(data, map_text, show=30):
    if len(data) != RAM_SIZE: raise ValueError('SRAM must contain exactly 20480 bytes')
    addr = find_symbol(map_text, 'm8e_irq_trace')
    off = addr-RAM_BASE
    if off + 2120 > RAM_SIZE: raise ValueError('M8E trace exceeds SRAM')
    x = struct.unpack_from('<18I', data, off)
    (magic,ver,size,event_size,cap,total,transfer,in3,good,bad,out2,setup0,
     resets,suspend,other,lastflags,lastep3,lastctx) = x
    if (magic,ver,size,event_size,cap) != (0x4d384952,1,2120,32,64):
        raise ValueError('Wrong M8E ABI: %s' % (x[:5],))
    print('Experiment 23 USBFS IRQ entry trace, address=0x%08X' % addr)
    print('IRQ entries=%d transfer=%d EP3_IN=%d (TOG_OK=%d TOG_BAD=%d)' %
          (total,transfer,in3,good,bad))
    print('EP2_OUT=%d EP0_SETUP=%d BUS_RESET=%d SUSPEND=%d OTHER=%d' %
          (out2,setup0,resets,suspend,other))
    print('Last combined FG/ST=0x%04X EP3_CTRL=0x%02X LEN=%d BUSY=%d' %
          (lastflags, (lastep3>>8)&0xff, lastep3&0xff, lastctx&0xff))
    print('Last %d IRQ entry events:' % min(show,total,cap))
    for seq in range(max(1,total-min(show,cap)+1),total+1):
        idx=(seq-1) & 63
        e=struct.unpack_from('<8I',data,off+72+idx*32)
        if e[0] != seq:
            print('  WARNING missing sequence %d (stored %d)' % (seq,e[0]));continue
        _,loops,flags,ep3,usb,ctx,fg,callbacks=e
        token=(flags>>12)&3; endp=(flags>>8)&15
        tag={0:'OUT',1:'SOF',2:'IN',3:'SETUP'}[token]
        print('  #%05d loops=%-10d FG/ST=0x%04X token=%-5s EP=%d TOG_OK=%d busy=%d len=%d TX_CTRL=0x%02X fg_events=%d callbacks=%d' %
              (seq,loops,flags,tag,endp,1 if flags&0x4000 else 0,ctx&0xff,ep3&0xff,(ep3>>8)&0xff,fg,callbacks))
    daddr=find_symbol(map_text,'m8d_usb_trace')
    o=daddr-RAM_BASE
    m=struct.unpack_from('<15I', data, o)
    if m[:5] != (0x4d385452,1,2364,36,32):
        raise ValueError('M8D callback trace ABI mismatch')
    print('M8D callback trace: fg_count=%d irq_count=%d main_loops=%d outstanding=%d max=%d' %
          (m[5],m[6],m[7],m[8],m[9]))
    print('IRQ entry count can include events not delivered to application callbacks.')
    print('USBPcap is host URB capture, not an electrical-level USB token analyzer.')
    return {'ep3_in':in3,'ep3_in_tog_ok':good,'ep3_in_tog_bad':bad,'callback_events':m[6]}


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('archive')
    p.add_argument('--show',type=int,default=30)
    a=p.parse_args()
    if not 1<=a.show<=64: p.error('--show must be 1..64')
    with zipfile.ZipFile(a.archive) as z:
        def read(name):
            matches=[n for n in z.namelist() if n.rsplit('/',1)[-1]==name]
            if len(matches)!=1: raise ValueError('Expected one %s in ZIP' % name)
            return z.read(matches[0])
        sram=read('sram-20kb.bin')
        maps=read('m8e_usb_cdc_irq_trace.map').decode('utf-8',errors='replace')
    decode(sram,maps,a.show)

if __name__=='__main__':
    try: main()
    except (ValueError,OSError,zipfile.BadZipFile,struct.error) as e:
        print('ERROR: %s' % e,file=sys.stderr);sys.exit(1)

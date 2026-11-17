#!/usr/bin/env python3
"""M43B-5 SRAM event journal decoder (M43B-3 binary ABI); never modifies the raw SRAM dump."""
import argparse
import csv
import hashlib
import json
import struct
from pathlib import Path

RAM_BASE = 0x20000000
RAM_BYTES = 20 * 1024
MAGIC = 0x33425350
END_MAGIC = 0xA53CB35A
HEADER_BYTES = 64
RECORD_BYTES = 16
CAPACITY = 64
SIZE = HEADER_BYTES + RECORD_BYTES * CAPACITY
HEADERS = ('magic','version','header_bytes','record_bytes','capacity','session',
           'head','count','total','freeze_reason','freeze_tick','active','lost',
           'attach_tick','reserved1','end_magic')
EVENTS = {1:'PD_TRACE',2:'TX_END',3:'RX_ARM',4:'RX_HEADER',5:'GOODCRC',
          6:'TX_GOODCRC',7:'RETRY_STATE',8:'HARD_RESET',9:'FAULT_STATE',
          10:'PHY_TX_LAUNCH',11:'PHY_RX_IRQ',12:'PHY_RX_SAMPLE',
          13:'PHY_RX_ARM_REG',14:'PHY_CRC_TIMEOUT',
          15:'REQ_BEGIN',16:'REQ_WINDOW',17:'REQ_END',18:'HR_INDEX'}
REASONS = {0:'NOT_FROZEN',1:'TX_FAILED',2:'SOURCE_HARD_RESET',
           3:'POLICY_FAILURE',4:'NORMAL_DETACH',5:'VBUS_FAULT'}

def decode(raw, address):
    if len(raw) != RAM_BYTES:
        raise ValueError(f'Expected {RAM_BYTES} SRAM bytes, found {len(raw)}')
    offset = address - RAM_BASE
    if address % 4 != 0 or offset < 0 or offset + SIZE > len(raw):
        raise ValueError(f'Symbol outside CH32X035 SRAM: 0x{address:08x}')
    vals = struct.unpack_from('<16I', raw, offset)
    d = dict(zip(HEADERS, vals))
    if (d['magic'],d['version'],d['header_bytes'],d['record_bytes'],
        d['capacity'],d['end_magic']) != (MAGIC,1,HEADER_BYTES,RECORD_BYTES,CAPACITY,END_MAGIC):
        raise ValueError('Missing/invalid M43B-3 SRAM signature. Possible MCU reset, wrong ELF or dump.')
    head,count,total = d['head'],d['count'],d['total']
    if head >= CAPACITY or count > CAPACITY or total < count or head != total % CAPACITY:
        raise ValueError('Invalid ring indices; SRAM capture incomplete or damaged')
    rows = []
    for i in range(count):
        slot=(head-count+i)%CAPACITY
        tick,arg0,arg1,seq,event,flags = struct.unpack_from('<IIIHBB',raw,offset+HEADER_BYTES+slot*RECORD_BYTES)
        expected_seq = (total-count+i)&0xffff
        if seq != expected_seq:
            raise ValueError(f'Bad record sequence at slot {slot}: {seq} != {expected_seq}')
        decoded = ''
        if event == 1:
            detail=arg0 & 0xff
            aux=(arg0>>8)&0xffff
            decoded=f'PDTR={flags} detail={detail} aux=0x{aux:04x}'
        elif event == 4:
            decoded=f'hdr=0x{arg0&0xffff:04x} bytes={(arg0>>16)&0xffff}'
        elif event == 5:
            decoded=f'match={flags&1} waiting={(flags>>1)&1} armed={(flags>>2)&1} expected={arg1&255} retry={(arg1>>8)&255}'
        elif event == 7:
            decoded=f'retry_before={(arg1>>16)&255}'
        elif event == 10:
            decoded=(f'tx_hdr=0x{arg0&65535:04x} tx_bytes={(arg0>>16)&255} '
                     f'tx_sel=0x{(arg0>>24)&255:02x} '
                     f'irq_status=0x{arg1&65535:04x} ctrl=0x{(arg1>>16)&65535:04x} '
                     f'wait={flags&1} armed={(flags>>1)&1} retry={(flags>>2)&3}')
        elif event == 11:
            decoded=(f'irq_status=0x{arg0&65535:04x} bytes={(arg0>>16)&255} '
                     f'cc={(arg0>>24)&255} header=0x{arg1&65535:04x} '
                     f'ctrl=0x{(arg1>>16)&65535:04x} '
                     f'wait={flags&1} armed={(flags>>1)&1} sop={(flags>>2)&1}')
        elif event == 12:
            payload=(arg0.to_bytes(4,"little")+(arg1&65535).to_bytes(2,"little"))
            decoded=(f'dma_0_5={payload.hex()} config=0x{(arg1>>16)&255:02x} '
                     f'retry={(arg1>>24)&255}')
        elif event == 13:
            decoded=(f'ctrl=0x{arg0&255:02x} config=0x{(arg0>>8)&255:02x} '
                     f'rxclk=0x{(arg0>>16)&255:02x} cc={(arg0>>24)&255} '
                     f'status=0x{arg1&65535:04x} expected={(arg1>>16)&255} '
                     f'retry={(arg1>>24)&255} wait={flags&1} armed={(flags>>1)&1}')
        elif event == 14:
            decoded=(f'status=0x{arg0&65535:04x} bytes={(arg0>>16)&255} '
                     f'cc={(arg0>>24)&255} ctrl=0x{arg1&65535:04x} '
                     f'retry={(arg1>>16)&255} expected={(arg1>>24)&255} '
                     f'wait={flags&1} armed={(flags>>1)&1}')
        elif event == 15:
            phases={0:'INITIAL',1:'AFTER_SOURCE_HR',2:'PPS_REFRESH'}
            decoded=(f'req_seq={arg0&65535} header=0x{(arg0>>16)&65535:04x} '
                     f'phase={phases.get(flags,"UNKNOWN")} irq_base={arg1}')
        elif event == 16:
            outcomes={1:'GOODCRC_MATCH',2:'CRC_TIMEOUT',3:'SOURCE_HR_ABORT',4:'SUPERSEDED'}
            decoded=(f'req_seq={arg0&65535} retry={(arg0>>16)&255} '
                     f'expected={(arg0>>24)&255} outcome={outcomes.get(flags,"UNKNOWN")} '
                     f'fs_crc_irq={arg1}')
        elif event == 17:
            outcomes={1:'GOODCRC_MATCH',2:'TX_FAILED',3:'SOURCE_HR_ABORT',4:'SUPERSEDED'}
            decoded=(f'req_seq={arg0&65535} retries={(arg0>>16)&255} '
                     f'outcome={outcomes.get(flags,"UNKNOWN")} total_fs_crc_irq={arg1}')
        elif event == 18:
            decoded=f'source_hr_ordinal={flags} status=0x{arg0&65535:04x} ctrl=0x{arg1&65535:04x}'
        rows.append({'ordinal':total-count+i,'slot':slot,'seq':seq,'tick':tick,
                     'event':EVENTS.get(event,f'UNKNOWN_{event}'),'code':event,
                     'flags':flags,'arg0':f'0x{arg0:08x}','arg1':f'0x{arg1:08x}',
                     'decode':decoded})
    return d, rows

def main():
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--ram',type=Path,required=True)
    ap.add_argument('--address',type=lambda x:int(x,0),required=True,
                    help='m43b3_sram address from the exact firmware ELF nm output')
    ap.add_argument('--out',type=Path,required=True,help='New, nonexistent output directory')
    args=ap.parse_args()
    if args.out.exists():
        ap.error(f'Output already exists; refusing overwrite: {args.out}')
    raw=args.ram.read_bytes()
    d,rows=decode(raw,args.address)   # validate BEFORE creating output directory
    if d['freeze_reason']==0 or d['active']!=0:
        raise SystemExit('Capture not frozen. Disconnect PD, keep MCU powered, then reread; nothing written.')
    args.out.mkdir(parents=True,exist_ok=False)
    csvfile=args.out/'events.csv'
    with csvfile.open('w',encoding='utf-8-sig',newline='') as f:
        w=csv.DictWriter(f,fieldnames=list(rows[0]) if rows else
                        ['ordinal','slot','seq','tick','event','code','flags','arg0','arg1','decode'],
                        lineterminator='\n')
        w.writeheader();w.writerows(rows)
    summary=dict(d)
    summary['freeze_name']=REASONS.get(d['freeze_reason'],'UNKNOWN')
    summary['symbol_address']=f'0x{args.address:08x}'
    summary['raw_sha256']=hashlib.sha256(raw).hexdigest()
    summary['source_hr_count']=d['reserved1']
    reqs={}
    for row in rows:
        if row['code'] not in (15,16,17):
            continue
        # request sequence is carried in the low 16 bits of arg0.
        arg0=int(row['arg0'],16)
        arg1=int(row['arg1'],16)
        rid=arg0&65535
        v=reqs.setdefault(rid, {'req_seq':rid,'phase':'UNKNOWN_OR_OVERWRITTEN',
                                'windows':[],'status':'INCOMPLETE_OR_OVERWRITTEN'})
        if row['code']==15:
            v['phase']={0:'INITIAL',1:'AFTER_SOURCE_HR',2:'PPS_REFRESH'}.get(row['flags'],'UNKNOWN')
            v['header']=f'0x{(arg0>>16)&65535:04x}'
        elif row['code']==16:
            v['windows'].append({'retry':(arg0>>16)&255,'outcome':
                {1:'GOODCRC_MATCH',2:'CRC_TIMEOUT',3:'SOURCE_HR_ABORT',4:'SUPERSEDED'}.get(row['flags'],'UNKNOWN'),
                'fs_crc_irq':arg1})
        elif row['code']==17:
            v['status']={1:'GOODCRC_MATCH',2:'TX_FAILED',3:'SOURCE_HR_ABORT',4:'SUPERSEDED'}.get(row['flags'],'UNKNOWN')
            v['total_fs_crc_irq']=arg1
            v['retries']=(arg0>>16)&255
    summary['requests_retained']=list(reqs.values())
    summary['oldest_events_overwritten']=d['total']-d['count']
    summary['events_retained']=len(rows)
    summary['event_names']={name:sum(r['event']==name for r in rows) for name in EVENTS.values()}
    (args.out/'summary.json').write_text(json.dumps(summary,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
    print(f"SRAM DECODE PASS: {len(rows)} events, freeze={summary['freeze_name']}, session={d['session']}")
    print(f"CSV: {csvfile}")
    print(f"JSON: {args.out/'summary.json'}")
if __name__=='__main__':
    main()

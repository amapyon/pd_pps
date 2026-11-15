#!/usr/bin/env python3
"""Read-only interpretation of M41 PDTRACE log. No hardware access."""
import argparse, re
from pathlib import Path

CODES={3:'IRQ_RESET',4:'IRQ_TX_END',7:'RX_HEADER',9:'TX_GOODCRC',
       10:'RX_GOODCRC',12:'TX_COMMAND',13:'TX_RETRY',14:'TX_FAILED',
       29:'M41_IRQ_REG',30:'M41_RX_DMA',31:'M41_RX_ARM',
       32:'M41_CRC_TIMEOUT',33:'M41_CRC_CTX',34:'M41_RX_GATED'}
EV = re.compile(r'PDTRACE idx=(\d+) count=(\d+) tick=([0-9a-fA-F]{8}) code=(\d+) detail=(\d+) aux=([0-9a-fA-F]{4}) value=([0-9a-fA-F]{8})')

def explain(kind,detail,aux,value):
    if kind in (29,31,32):
        control=(value>>16)&0xffff; config=value&0xffff
        flags=aux&0xfc
        return f'STATUS(R8)=0x{aux&0xff:02x} flags=0x{flags:02x} CONTROL(R8)=0x{control&0xff:02x} CONFIG(R16)=0x{config:04x} TX_EN={control&1} RX_STATE={(control>>2)&7} byte_count8={detail}'
    if kind==30:
        n=(value>>16)&0x1ff; header=value&0xffff
        mid=(header>>9)&7; ndo=(header>>12)&7; typ=header&0x1f
        return f'STATUS(R8)=0x{aux&0xff:02x} AUX={detail} RX_bytes={n} header=0x{header:04x} mid={mid} ndo={ndo} type={typ}'
    if kind==33:
        return f'expected_mid={detail} tx_mid={aux&0xff} wait_armed={(aux>>8)&1} cycles_since_TX_END={value} (~{value/48000:.3f}ms @48MHz)'
    if kind==34:
        return f'RX rejected by gate reason={detail} raw_bytes={value} STATUS(R8)=0x{aux&0xff:02x}'
    if kind==10:
        return f'GoodCRC mid={detail} expected_mid={value&0xff} match={(value>>8)&1}'
    if kind==12:
        return f'PD command={detail} header=0x{aux:04x} RDO=0x{value:08x}'
    if kind==13:
        return f'retry_no={detail} packet_bytes={value}'
    if kind==14:
        return f'retries={detail}'
    return f'detail={detail} aux=0x{aux:04x} value=0x{value:08x}'

def main():
    ap=argparse.ArgumentParser()
    ap.add_argument('log',type=Path,help='Capture-M41-Trace.ps1 output')
    ap.add_argument('--out',type=Path,help='Optional decoded .txt output')
    args=ap.parse_args()
    records=[]
    for line in args.log.read_text(encoding='utf-8-sig').splitlines():
        m=EV.search(line)
        if m:
            i,c,tick,kind,d,aux,val=m.groups()
            records.append((int(i),int(tick,16),int(kind),int(d),int(aux,16),int(val,16)))
    if not records:
        raise SystemExit('No PDTRACE records in the supplied file.')
    base=records[0][1]
    lines=[]
    for idx,tick,kind,detail,aux,value in records:
        elapsed=((tick-base)&0xffffffff)/48000.0
        lines.append(f'{idx:3} +{elapsed:9.3f}ms {CODES.get(kind,"CODE_"+str(kind)):18} {explain(kind,detail,aux,value)}')
    output='\n'.join(lines)+'\n'
    print(output,end='')
    if args.out:
        args.out.write_text(output,encoding='utf-8')
        print('Saved:',args.out)
if __name__=='__main__': main()

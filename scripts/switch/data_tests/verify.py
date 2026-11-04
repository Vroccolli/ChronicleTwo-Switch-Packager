"""Independent standard-library ZIP fixtures plus codec payload and corruption checks."""
from pathlib import Path
import binascii
import io
import struct
import subprocess
import sys
import zipfile

exe, root = sys.argv[1], Path(sys.argv[2])
root.mkdir(parents=True, exist_ok=True)
cases = 0
def run(name, data, valid=True, expected=None):
    global cases
    path=root/(name+'.zip'); path.write_bytes(data)
    result=subprocess.run([exe,str(path)],capture_output=True,text=True,encoding="utf-8",timeout=5)
    assert (result.returncode==0)==valid,(name,result.returncode,result.stdout,result.stderr)
    if expected is not None:
        lines=result.stdout.splitlines(); assert lines[0]=='count '+str(len(expected)),(name,lines)
        assert set(lines[1:])=={f'{n}\t{len(b)}\t{binascii.crc32(b):08x}' for n,b in expected.items()},(name,lines)
    cases+=1; print(name+': PASS')
def stored(entries, force64=False):
    stream=io.BytesIO()
    with zipfile.ZipFile(stream,'w',compression=zipfile.ZIP_STORED) as archive:
        for name, value in entries.items():
            with archive.open(name,'w',force_zip64=force64) as out: out.write(value)
    return stream.getvalue()
entries={'first.txt':b'hello archive','dir/second.bin':bytes(range(256)),'empty':b''}
baseline=stored(entries)
run('stored',baseline,expected=entries)
run('local-zip64',stored(entries,True),expected=entries)
run('empty',stored({}),expected={})
run('utf8',stored({'dir/日本語.txt':b'UTF8'}),expected={'dir/日本語.txt':b'UTF8'})
payload=b'compressed data '*1024
source=root/'input.bin'; source.write_bytes(payload); compressed=root/'compressed.bin'
subprocess.run([exe,'--zstd',str(source),str(compressed)],check=True,timeout=5)
name=b'data.bin'; zstd=compressed.read_bytes(); crc=binascii.crc32(payload)
local=struct.pack('<IHHHHHIIIHH',0x04034b50,63,0,93,0,0,crc,len(zstd),len(payload),len(name),0)+name+zstd
central=struct.pack('<IHHHHHHIIIHHHHHII',0x02014b50,63,63,0,93,0,0,crc,len(zstd),len(payload),len(name),0,0,0,0,0,0)+name
end=struct.pack('<IHHHHIIH',0x06054b50,0,0,1,1,len(central),len(local),0)
run('zstandard',local+central+end,expected={'data.bin':payload})
# A real ZIP64 end/locator and size/offset extra, even though this fixture is small.
extra=struct.pack('<HHQQQ',1,24,len(payload),len(zstd),0)
central64=bytearray(central[:46]); struct.pack_into('<II',central64,20,0xffffffff,0xffffffff)
struct.pack_into('<H',central64,30,len(extra)); struct.pack_into('<I',central64,42,0xffffffff)
central64=bytes(central64)+name+extra
zip64off=len(local)+len(central64)
record=struct.pack('<IQHHIIQQQQ',0x06064b50,44,63,63,0,0,1,1,len(central64),len(local))
locator=struct.pack('<IIQI',0x07064b50,0,zip64off,1)
end64=struct.pack('<IHHHHIIH',0x06054b50,0,0,65535,65535,0xffffffff,0xffffffff,0)
run('zip64',local+central64+record+locator+end64,expected={'data.bin':payload})
run('truncated',baseline[:-1],False)
run('traversal',stored({'../bad':b'x'}),False)
run('backslash',stored({'dir/bad':b'x'}).replace(b'dir/bad',b'dir\\bad'),False)
run('duplicate-case',stored({'same':b'x','SAME':b'y'}),False)
corrupt=bytearray(baseline); corrupt[30+len('first.txt')]=0
run('bad-crc',corrupt,False)
corrupt=bytearray(baseline); corrupt[30]=ord('X')
run('local-name-mismatch',corrupt,False)
central_at=baseline.index(b'PK\x01\x02')
corrupt=bytearray(baseline); struct.pack_into('<H',corrupt,central_at+8,1)
run('encrypted',corrupt,False)
corrupt=bytearray(baseline); struct.pack_into('<H',corrupt,central_at+10,8)
run('unsupported-codec',corrupt,False)
corrupt=bytearray(baseline); struct.pack_into('<I',corrupt,central_at+42,0xfffffffe)
run('bad-local-offset',corrupt,False)
corrupt=bytearray(local+central64+record+locator+end64)
struct.pack_into('<Q',corrupt,len(local)+46+len(name)+4+16,0xffffffffffffffff)
run('zip64-offset-overflow',corrupt,False)
print(f'{cases} archive scenarios passed')

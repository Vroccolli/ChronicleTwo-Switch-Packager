"""Compare the portable codec exhaustively to the PC's actual Windows API path."""
import ctypes
import subprocess
import sys

kernel=ctypes.WinDLL('kernel32',use_last_error=True)
decode=kernel.MultiByteToWideChar
decode.argtypes=[ctypes.c_uint,ctypes.c_ulong,ctypes.c_char_p,ctypes.c_int,ctypes.c_wchar_p,ctypes.c_int]
encode=kernel.WideCharToMultiByte
encode.argtypes=[ctypes.c_uint,ctypes.c_ulong,ctypes.c_wchar_p,ctypes.c_int,ctypes.c_char_p,ctypes.c_int,ctypes.c_char_p,ctypes.POINTER(ctypes.c_int)]
requests=[]; expected=[]
def add_decode(data):
    wide=ctypes.create_unicode_buffer(4)
    count=decode(932,8,data,len(data),wide,4)
    value=wide[:count].encode('utf-8').hex() if count and b'\0' not in data else 'ERROR'
    requests.append('D'+data.hex()); expected.append(value)
for value in range(1,256): add_decode(bytes([value]))
for lead in [*range(0x81,0xa0),*range(0xe0,0xfd)]:
    for trail in range(256): add_decode(bytes([lead,trail]))
for cp in range(1,65536):
    if 0xd800<=cp<=0xdfff: continue
    char=chr(cp); buffer=ctypes.create_string_buffer(4); substituted=ctypes.c_int()
    count=encode(932,0x400,char,1,buffer,4,None,ctypes.byref(substituted))
    requests.append('E'+char.encode('utf-8').hex())
    expected.append(buffer.raw[:count].hex() if count and not substituted.value else 'ERROR')
for invalid in [b'\0',b'\x80',b'\xc0\x80',b'\xc2',b'\xed\xa0\x80',b'\xf0\x80\x80\x80',b'\xf4\x90\x80\x80']:
    requests.append('E'+invalid.hex()); expected.append('ERROR')
requests+=['E','D','E'+chr(0x1f600).encode('utf-8').hex()]; expected+=['','','ERROR']
result=subprocess.run([sys.argv[1]],input='\n'.join(requests)+'\n',text=True,capture_output=True,timeout=15)
assert result.returncode==0,result.stderr
actual=result.stdout.splitlines(); assert len(actual)==len(expected),(len(actual),len(expected))
for index,(a,e) in enumerate(zip(actual,expected)):
    assert a==e,(requests[index],a,e)
print(f'{len(expected)} encoding cases match the Windows PC baseline')

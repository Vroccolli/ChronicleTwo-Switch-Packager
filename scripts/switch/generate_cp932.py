"""Pin the Windows PC reference's strict CP932 mappings for portable Switch use."""
from pathlib import Path
import ctypes
import hashlib
import json
import os

ROOT = Path(__file__).resolve().parents[2]


def main():
    if os.name != "nt": raise RuntimeError("Regeneration requires the Windows PC reference APIs; builds use the committed table")
    kernel = ctypes.WinDLL("kernel32", use_last_error=True)
    decode = kernel.MultiByteToWideChar
    decode.argtypes = [ctypes.c_uint,ctypes.c_ulong,ctypes.c_char_p,ctypes.c_int,ctypes.c_wchar_p,ctypes.c_int]
    encode = kernel.WideCharToMultiByte
    encode.argtypes = [ctypes.c_uint,ctypes.c_ulong,ctypes.c_wchar_p,ctypes.c_int,ctypes.c_char_p,ctypes.c_int,ctypes.c_char_p,ctypes.POINTER(ctypes.c_int)]
    decoded, encoded = [], []
    wide = ctypes.create_unicode_buffer(4)
    for code in range(1,65536):
        data = bytes([code]) if code<256 else code.to_bytes(2,"big")
        count = decode(932,8,data,len(data),wide,4)
        if count==1 and not (0xd800<=ord(wide[0])<=0xdfff): decoded.append((code,ord(wide[0])))
    for cp in range(1,65536):
        if 0xd800<=cp<=0xdfff: continue
        buffer=ctypes.create_string_buffer(4); substituted=ctypes.c_int()
        count=encode(932,0x400,chr(cp),1,buffer,4,None,ctypes.byref(substituted))
        if count in (1,2) and not substituted.value:
            encoded.append((cp,int.from_bytes(buffer.raw[:count],"big")))
    text='// Generated from the Windows PC reference CP932 APIs. See generate_cp932.py.\n'
    text+='struct Cp932Pair { uint16_t key, value; };\n'
    for name,table in [('DecodeMap',decoded),('EncodeMap',encoded)]:
        text+=f'static constexpr Cp932Pair {name}[] = {{\n'
        for at in range(0,len(table),8): text+='    '+','.join(f'{{0x{k:04x},0x{v:04x}}}' for k,v in table[at:at+8])+',\n'
        text+='};\n'
    path=ROOT/'port/switch/data/cp932_table.inc'
    path.write_bytes(text.encode("ascii"))
    metadata=dict(codepage=932,decoder_flags="MB_ERR_INVALID_CHARS",encoder_flags="WC_NO_BEST_FIT_CHARS; reject substitution",
        decode_entries=len(decoded),encode_entries=len(encoded),sha256=hashlib.sha256(path.read_bytes()).hexdigest())
    path.with_suffix('.json').write_bytes((json.dumps(metadata,indent=2)+'\n').encode("ascii"))
    print(json.dumps(metadata))


if __name__ == '__main__': main()

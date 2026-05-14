import struct
import sys

for f in sys.argv[1:]:
    with open(f, 'rb') as fh:
        data = bytearray(fh.read())

    pe_off = struct.unpack_from('<I', data, 0x3C)[0]
    char_off = pe_off + 22
    old = struct.unpack_from('<H', data, char_off)[0]
    new = 0x2022  # EXECUTABLE | LARGE_ADDRESS_AWARE | DLL

    struct.pack_into('<H', data, char_off, new)

    with open(f, 'wb') as fh:
        fh.write(data)

    print(f'{f}: chars 0x{old:04X} -> 0x{new:04X}')

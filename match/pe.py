"""pe.py -- a small PE reader: sections, header fields and the import table."""
import struct


class PE:
    def __init__(self, path):
        self.b = b = open(path, 'rb').read()
        pe = struct.unpack_from('<I', b, 0x3c)[0]
        self.opt = opt = pe + 24
        self.hdr = {
            'timestamp': hex(struct.unpack_from('<I', b, pe + 8)[0]),
            'linker': '%d.%02d' % (b[opt + 2], b[opt + 3]),
            'entry': hex(struct.unpack_from('<I', b, opt + 16)[0]),
            'image size': hex(struct.unpack_from('<I', b, opt + 56)[0]),
            'checksum': hex(struct.unpack_from('<I', b, opt + 64)[0]),
            'stack reserve': hex(struct.unpack_from('<I', b, opt + 72)[0]),
            'heap reserve': hex(struct.unpack_from('<I', b, opt + 80)[0]),
        }
        n = struct.unpack_from('<H', b, pe + 6)[0]
        off = opt + struct.unpack_from('<H', b, pe + 20)[0]
        self.secs = {}
        for _ in range(n):
            nm = b[off:off + 8].rstrip(b'\0').decode()
            vs, va, rs, ra = struct.unpack_from('<IIII', b, off + 8)
            self.secs[nm] = (va, vs, ra, rs)
            off += 40

    def data(self, name):
        va, vs, ra, rs = self.secs[name]
        return self.b[ra:ra + min(vs, rs) if vs else rs]

    def off(self, rva):
        for va, vs, ra, rs in self.secs.values():
            if va <= rva < va + max(vs, rs):
                return rva - va + ra

    def imports(self):
        """[(DLL name upper-case, function)] in import-directory order."""
        out = []
        o = self.off(struct.unpack_from('<I', self.b, self.opt + 104)[0])
        if o is None:
            return out
        while True:
            ilt, _, _, nrva, iat = struct.unpack_from('<IIIII', self.b, o)
            if nrva == 0:
                break
            no = self.off(nrva)
            dll = self.b[no:self.b.index(b'\0', no)].decode().upper()
            t = self.off(ilt or iat)
            while struct.unpack_from('<I', self.b, t)[0]:
                e = struct.unpack_from('<I', self.b, t)[0]
                if e & 0x80000000:
                    out.append((dll, '#%d' % (e & 0xffff)))
                else:
                    h = self.off(e) + 2
                    out.append((dll, self.b[h:self.b.index(b'\0', h)].decode()))
                t += 4
            o += 20
        return out

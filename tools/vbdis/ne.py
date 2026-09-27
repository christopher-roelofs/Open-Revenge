"""Minimal NE (16-bit Windows) executable parser."""
import struct

class Seg:
    def __init__(s, idx, off, size, flags, minalloc, data, relocs):
        s.idx, s.off, s.size, s.flags, s.minalloc, s.data, s.relocs = idx, off, size, flags, minalloc, data, relocs
    @property
    def is_data(s): return bool(s.flags & 1)

class NE:
    def __init__(self, path):
        d = self.d = open(path, 'rb').read()
        self.ne = ne = struct.unpack_from('<I', d, 0x3c)[0]
        assert d[ne:ne+2] == b'NE'
        (self.entry_off, self.entry_len) = struct.unpack_from('<HH', d, ne+4)
        self.flags = struct.unpack_from('<H', d, ne+0x0c)[0]
        self.autodata = struct.unpack_from('<H', d, ne+0x0e)[0]
        self.cs_ip = struct.unpack_from('<HH', d, ne+0x14)
        nseg, nmod, nnres = struct.unpack_from('<HHH', d, ne+0x1c)
        segtab, rsrctab, resnametab, modreftab, impnametab = struct.unpack_from('<HHHHH', d, ne+0x22)
        self.nonres_off = struct.unpack_from('<I', d, ne+0x2c)[0]
        self.align = struct.unpack_from('<H', d, ne+0x32)[0]
        nres = struct.unpack_from('<H', d, ne+0x34)[0]
        sh = self.align
        self.segs = []
        for i in range(nseg):
            off, size, fl, mina = struct.unpack_from('<HHHH', d, ne+segtab+i*8)
            foff = off << sh
            size = size or 0x10000 if off else 0
            data = d[foff:foff+size] if off else b''
            relocs = []
            if fl & 0x100 and off:
                ro = foff + size
                n = struct.unpack_from('<H', d, ro)[0]
                for j in range(n):
                    relocs.append(struct.unpack_from('<BBHHH', d, ro+2+j*8))
            self.segs.append(Seg(i+1, foff, size, fl, mina, data, relocs))
        # module refs
        self.modules = []
        for i in range(nmod):
            o = struct.unpack_from('<H', d, ne+modreftab+i*2)[0]
            self.modules.append(self._pstr(ne+impnametab+o))
        self.impnametab = ne+impnametab
        # resident / nonresident names
        self.names = {}
        o = ne+resnametab
        first = True
        while d[o]:
            s = self._pstr(o); ordn = struct.unpack_from('<H', d, o+1+d[o])[0]
            if not first: self.names[ordn] = s
            first = False
            o += 1+d[o]+2
        o = self.nonres_off; first = True
        while o < len(d) and d[o]:
            s = self._pstr(o); ordn = struct.unpack_from('<H', d, o+1+d[o])[0]
            if not first: self.names[ordn] = s
            first = False
            o += 1+d[o]+2
        # entry table
        self.entries = {}
        o = ne+self.entry_off; ordn = 1
        while True:
            cnt = d[o]; 
            if cnt == 0: break
            ind = d[o+1]; o += 2
            for k in range(cnt):
                if ind == 0: pass
                elif ind == 0xff:
                    fl, _, segn, off = struct.unpack_from('<BHBH', d, o); o += 6
                    self.entries[ordn] = (segn, off)
                else:
                    fl, off = struct.unpack_from('<BH', d, o); o += 3
                    self.entries[ordn] = (ind, off)
                ordn += 1
        # resources
        self.resources = []
        if rsrctab != resnametab:
            o = ne+rsrctab
            rsh = struct.unpack_from('<H', d, o)[0]; o += 2
            while True:
                tid = struct.unpack_from('<H', d, o)[0]
                if tid == 0: break
                cnt = struct.unpack_from('<H', d, o+2)[0]; o += 8
                tname = tid & 0x7fff if tid & 0x8000 else self._pstr(ne+rsrctab+tid)
                for k in range(cnt):
                    roff, rlen, rfl, rid = struct.unpack_from('<HHHH', d, o); o += 12
                    rname = rid & 0x7fff if rid & 0x8000 else self._pstr(ne+rsrctab+rid)
                    self.resources.append((tname, rname, roff << rsh, rlen << rsh))

    def _pstr(self, o):
        return self.d[o+1:o+1+self.d[o]].decode('latin1')

    def import_name(self, off):
        return self._pstr(self.impnametab+off)

if __name__ == '__main__':
    import sys
    n = NE(sys.argv[1])
    print('modules', n.modules, 'cs:ip', n.cs_ip, 'autodata', n.autodata)
    for s in n.segs:
        print(f'seg{s.idx}: foff={s.off:#x} size={s.size:#x} flags={s.flags:#x} min={s.minalloc:#x} relocs={len(s.relocs)}')
    for r in n.resources: print('res', r[0], r[1], hex(r[2]), hex(r[3]))
    print('entries', len(n.entries), 'names', len(n.names))

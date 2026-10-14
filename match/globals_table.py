"""globals_table.py -- match/globals.tsv: every global, under both of its spellings.

Columns: address, name (what src/ and include/ call it: gName once understood, DAT_<address>
until then), match (the identifier the byte-match build compiles it under; never changes -- the
symbol tables in this folder were fitted with it) and kind (var: defined in src/data/globals.c;
field: a macro for a member of another global; unit: defined in the unit that owns it).
match/owners.tsv says which unit's data/bss block each defined global sits in.
"""
import os
import re

TABLE = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'globals.tsv')
IDENT = re.compile(r'\b[A-Za-z_]\w*\b')


class Row:
    __slots__ = ('address', 'name', 'match', 'kind')

    def __init__(self, address, name, match, kind):
        self.address, self.name, self.match, self.kind = address, name, match, kind


_rows = None
_by = None


def load():
    global _rows, _by
    if _rows is None:
        _rows, _by = [], {}
        with open(TABLE, encoding='utf-8') as f:
            f.readline()
            for line in f:
                c = line.rstrip('\n').split('\t')
                if len(c) < 4:
                    continue
                r = Row(int(c[0], 16), c[1], c[2], c[3])
                _rows.append(r)
                if re.match(r'^[A-Za-z_]\w*$', r.name):      # a member spelling is not an identifier
                    _by[r.name] = r
                _by[r.match] = r
                _by['DAT_%08x' % r.address] = r
    return _rows


def _split(ident):
    load()
    r = _by.get(ident)
    if r is not None:
        return r, ''
    if ident.endswith('_r'):
        r = _by.get(ident[:-2])
        if r is not None:
            return r, '_r'
    return None, ''


def row(ident):
    r, _ = _split(ident)
    if r is None and ident.startswith('_'):
        r, _ = _split(ident[1:])
    return r


def addr_of(ident):
    r = row(ident)
    return r.address if r else None


def name_of(ident):
    r, sfx = _split(ident)
    return r.name + sfx if r else ident


def match_of(ident):
    r, sfx = _split(ident)
    return r.match + sfx if r else ident


_owners = None


def owners():
    """{global name: unit file} from match/owners.tsv: TI.EXE's .data and bss hold each unit's
    globals as one block in link order, and this says whose block each global sits in. A global
    several units use goes to the neighbour whose block it is inside."""
    global _owners
    if _owners is None:
        _owners = {}
        with open(os.path.join(os.path.dirname(TABLE), 'owners.tsv'), encoding='utf-8') as f:
            f.readline()
            for line in f:
                c = line.rstrip('\n').split('\t')
                if len(c) >= 2:
                    _owners[c[0]] = c[1]
    return _owners

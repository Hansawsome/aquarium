#!/usr/bin/env python3
"""Encoding-aware literal scanner for packaged build verification.

WHY THIS EXISTS: macOS /usr/bin/strings has no -e flag, so it only ever sees
8-bit strings. Unreal 5.5+ TCHAR literals are UTF-16, so every TEXT("...")
token -- every dev flag, every module name -- is invisible to `strings`.
A `strings | grep -c` check over a UE binary returns 0 whether the token is
absent or merely UTF-16, i.e. it is an absence check that cannot bite.
Measured on the M6 Development package: all 11 dev flags ascii=n utf16le=Y.

Usage: aq_scan.py <token> <path> [path ...]
Prints one "<count>\t<path>" line per file with >0 hits; exit 0 always.
Both UTF-8 and UTF-16LE encodings of the token are counted.
"""
import mmap, os, sys

def count(path, tok):
    try:
        with open(path, 'rb') as f:
            if os.path.getsize(path) == 0:
                return 0
            m = mmap.mmap(f.fileno(), 0, access=mmap.ACCESS_READ)
            try:
                return m.read().count(tok.encode()) + m[:].count(tok.encode('utf-16-le'))
            finally:
                m.close()
    except (OSError, ValueError):
        return 0

def walk(paths):
    for p in paths:
        if os.path.isdir(p):
            for root, _, files in os.walk(p):
                for fn in files:
                    fp = os.path.join(root, fn)
                    if not os.path.islink(fp):
                        yield fp
        elif os.path.isfile(p):
            yield p

def main():
    tok, paths = sys.argv[1], sys.argv[2:]
    for fp in walk(paths):
        n = count(fp, tok)
        if n:
            print(f"{n}\t{fp}")

if __name__ == '__main__':
    main()

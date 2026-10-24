#!/usr/bin/env python3
"""Restore platform-independent audited ko3 glyph metrics, keeping generated GX pixels."""
from pathlib import Path
import base64
import hashlib
import lzma

ROOT = Path(__file__).resolve().parents[2]
EXPECTED_SHA = '5d72e7941a22b4a5c6a608b0fb6ed1d3d9e65a9ee23fbb0fac1349418127bbd1'
content = lzma.decompress(base64.b85decode((ROOT/'tools/ko3/map-manifest.b85').read_bytes()))
if hashlib.sha256(content).hexdigest() != EXPECTED_SHA:
    raise SystemExit('Audited ko3 font manifest hash mismatch')
(ROOT/'res/fonts/map-ko.json').write_bytes(content)
print('Restored audited cross-platform Korean font metrics')

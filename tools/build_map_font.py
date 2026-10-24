"""Build the native map's display-only Korean glyph atlas.

Run after changing res/ko.json names or strings. Requires Pillow and fontTools
only when rebuilding these committed assets; the mod has no font rasterizer
dependency. Every requested glyph must exist in the bundled OFL font.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import math
from pathlib import Path
import struct

from fontTools.ttLib import TTFont
from PIL import Image, ImageDraw, ImageFont


FONT_SIZE = 24
CELL_SIZE = 32
PADDING = 2
PAGE_SIZE = 1024
MAX_PAGES = 4


def fnv1a32(data: bytes) -> int:
    result = 2166136261
    for byte in data:
        result = ((result ^ byte) * 16777619) & 0xFFFFFFFF
    return result


def pack_rgba8(image: Image.Image) -> bytes:
    """GX's 4x4 tiles contain an AR plane followed by a GB plane."""
    width, height = image.size
    if width % 4 or height % 4 or not (4 <= width <= 1024 and 4 <= height <= 1024):
        raise ValueError("Invalid GX atlas dimensions")
    header = bytearray(32)
    header[0:2] = bytes((6, 2))
    struct.pack_into('>HH', header, 2, width, height)
    header[20:22] = bytes((1, 1))  # Linear filters, clamped edges, no mipmaps.
    header[24] = 1
    struct.pack_into('>I', header, 28, 32)
    pixels = image.convert('RGBA').load()
    output = bytearray(header)
    for y in range(0, height, 4):
        for x in range(0, width, 4):
            block = [pixels[x + dx, y + dy] for dy in range(4) for dx in range(4)]
            output.extend(value for r, g, b, a in block for value in (a, r))
            output.extend(value for r, g, b, a in block for value in (g, b))
    return bytes(output)


def requested_codepoints(locale: dict) -> list[int]:
    if locale.get('version') != 1 or locale.get('language') != 'ko':
        raise ValueError('Unsupported localization resource')
    chars = set(range(32, 127)) | {0x2026}  # Safe ASCII fallback and truncation.
    for category in ('names', 'strings'):
        entries = locale[category]
        if not isinstance(entries, dict):
            raise ValueError(f'Invalid {category} section')
        for value in entries.values():
            if not isinstance(value, str):
                raise ValueError(f'Non-text value in {category}')
            for ch in value:
                codepoint = ord(ch)
                if ch in '\r\n\t':
                    continue
                if codepoint < 32 or 0x7F <= codepoint < 0xA0 or 0xD800 <= codepoint <= 0xDFFF:
                    raise ValueError(f'Unsupported control character U+{codepoint:04X}')
                chars.add(codepoint)
    return sorted(chars)


def build(font_path: Path, locale: dict) -> tuple[dict, list[Image.Image], list[bytes]]:
    codepoints = requested_codepoints(locale)
    cells_per_row = PAGE_SIZE // CELL_SIZE
    page_capacity = cells_per_row ** 2
    if len(codepoints) > page_capacity * MAX_PAGES:
        raise ValueError(f'{len(codepoints)} glyphs exceed the {MAX_PAGES}-page limit')
    with TTFont(font_path) as font_data:
        cmap = font_data.getBestCmap()
        missing = [f'U+{cp:04X} {chr(cp)!r}' for cp in codepoints if cp not in cmap]
    if missing:
        raise ValueError('Bundled font lacks requested glyphs: ' + ', '.join(missing))
    font = ImageFont.truetype(str(font_path), FONT_SIZE)
    masks = []
    for page_index in range(math.ceil(len(codepoints) / page_capacity)):
        count = min(page_capacity, len(codepoints) - page_index * page_capacity)
        rows = math.ceil(count / cells_per_row)
        height = 1 << (max(CELL_SIZE, rows * CELL_SIZE) - 1).bit_length()
        masks.append(Image.new('L', (PAGE_SIZE, height)))
    drawers = [ImageDraw.Draw(mask) for mask in masks]
    glyphs = []
    for index, codepoint in enumerate(codepoints):
        ch = chr(codepoint)
        # This font's ASCII grave is a blank outline. Its fullwidth grave has
        # the same visible accent; center that mark at the ASCII advance.
        source = '\uFF40' if codepoint == 0x60 else ch
        page_index, cell = divmod(index, page_capacity)
        cell_x = (cell % cells_per_row) * CELL_SIZE
        cell_y = (cell // cells_per_row) * CELL_SIZE
        left, top, right, bottom = font.getbbox(source, anchor='ls')
        width, height = right - left, bottom - top
        advance = float(font.getlength(ch))
        bearing_x = left + (advance - font.getlength(source)) / 2 if source != ch else left
        if not (0 < advance <= CELL_SIZE * 2) or width > CELL_SIZE - PADDING * 2 or height > CELL_SIZE - PADDING * 2:
            raise ValueError(f'Glyph U+{codepoint:04X} does not fit its padded cell')
        if codepoint == 32:
            width = height = 0
        elif width <= 0 or height <= 0 or font.getmask(source).getbbox() is None:
            raise ValueError(f'Glyph U+{codepoint:04X} has no drawable pixels')
        x, y = cell_x + PADDING, cell_y + PADDING
        if width and height:
            drawers[page_index].text((x - left, y - top), source, font=font, fill=255, anchor='ls')
        glyphs.append({
            'codepoint': codepoint, 'page': page_index, 'x': x, 'y': y,
            'width': width, 'height': height, 'bearing_x': bearing_x,
            'bearing_y': top, 'advance': advance,
        })
    images, textures, pages = [], [], []
    for index, mask in enumerate(masks):
        image = Image.new('RGBA', mask.size, (255, 255, 255, 0))
        image.putalpha(mask)
        texture = pack_rgba8(image)
        resource = 'fonts/map-ko.bti' if index == 0 else f'fonts/map-ko-{index}.bti'
        pages.append({'resource': resource, 'width': image.width, 'height': image.height,
                      'byte_size': len(texture), 'fnv1a32': fnv1a32(texture)})
        images.append(image)
        textures.append(texture)
    relevant = {key: locale[key] for key in ('names', 'strings')}
    manifest = {
        'version': 1, 'format': 'GX_RGBA8', 'font': font_path.name,
        'font_sha256': hashlib.sha256(font_path.read_bytes()).hexdigest(),
        'text_sha256': hashlib.sha256(json.dumps(relevant, ensure_ascii=False, sort_keys=True,
                                               separators=(',', ':')).encode()).hexdigest(),
        'font_size': FONT_SIZE, 'cell_size': CELL_SIZE, 'line_height': CELL_SIZE,
        'glyph_count': len(glyphs), 'pages': pages, 'glyphs': glyphs,
        'substitutions': [{'codepoint': 0x60, 'source_codepoint': 0xFF40,
                           'reason': 'The bundled font has a blank ASCII grave; its fullwidth grave supplies the visible mark.'}],
    }
    return manifest, images, textures


def write_previews(directory: Path, manifest: dict, images: list[Image.Image], font_path: Path, locale: dict) -> None:
    """Synthetic images, not a capture or a claim of in-game verification."""
    directory.mkdir(parents=True, exist_ok=True)
    for index, atlas in enumerate(images):
        preview = Image.new('RGBA', atlas.size, (28, 29, 24, 255))
        preview.alpha_composite(atlas)
        grid = ImageDraw.Draw(preview)
        for x in range(0, atlas.width, CELL_SIZE):
            grid.line((x, 0, x, atlas.height), fill=(63, 65, 53, 255))
        for y in range(0, atlas.height, CELL_SIZE):
            grid.line((0, y, atlas.width, y), fill=(63, 65, 53, 255))
        preview.convert('RGB').save(directory / f'map-ko-atlas-{index}.png')
    glyphs = {entry['codepoint']: entry for entry in manifest['glyphs']}
    canvas = Image.new('RGBA', (720, 480), (32, 31, 25, 255))
    draw = ImageDraw.Draw(canvas)
    heading = ImageFont.truetype(str(font_path), 16)
    draw.text((20, 14), 'SYNTHETIC PREVIEW - NOT A GAME SCREENSHOT', font=heading, fill=(249, 218, 144, 255))
    draw.text((20, 38), 'Generated from the committed glyph atlas; native-size layout shown at 2x.', font=ImageFont.truetype(str(font_path), 12), fill=(210, 207, 194, 255))
    draw.rectangle((140, 80, 704, 460), fill=(62, 60, 45, 255), outline=(161, 144, 93, 255), width=2)
    for box in ((185, 180, 290, 255), (350, 155, 445, 300), (500, 250, 655, 330)):
        draw.rectangle(box, outline=(132, 127, 89, 255), width=3)
    draw.line((290, 218, 350, 218), fill=(132, 127, 89, 255), width=5)
    draw.line((445, 275, 500, 275), fill=(132, 127, 89, 255), width=5)
    for x, y in ((220, 220), (390, 210), (595, 290)):
        draw.rectangle((x - 5, y - 4, x + 5, y + 4), fill=(238, 209, 82, 255))

    def width(text: str, size: float) -> float:
        return sum(glyphs[ord(ch)]['advance'] * size / FONT_SIZE for ch in text)

    def text(value: str, x: float, baseline: float, size: float, ink: tuple[int, int, int, int]) -> None:
        scale = size / FONT_SIZE
        for ch in value:
            glyph = glyphs[ord(ch)]
            if glyph['width'] and glyph['height']:
                rect = (glyph['x'], glyph['y'], glyph['x'] + glyph['width'], glyph['y'] + glyph['height'])
                alpha = images[glyph['page']].crop(rect).getchannel('A')
                target = (max(1, round(glyph['width'] * scale)), max(1, round(glyph['height'] * scale)))
                alpha = alpha.resize(target, Image.Resampling.LANCZOS)
                image = Image.new('RGBA', target, ink)
                image.putalpha(alpha)
                canvas.alpha_composite(image, (round(x + glyph['bearing_x'] * scale), round(baseline + glyph['bearing_y'] * scale)))
            x += glyph['advance'] * scale

    translate = lambda key: locale['strings'].get(key, key)
    ink, gold, green = (232, 226, 205, 255), (198, 181, 112, 255), (113, 231, 141, 255)
    draw.rectangle((20, 245, 132, 325), fill=(20, 18, 12, 255))
    draw.rectangle((20, 245, 22, 325), fill=gold)
    for key, y, size, color in (('Dungeon checks', 262, 12, gold), ('Accessible', 282, 11, ink), ('Remaining', 299, 11, ink), ('All floors', 316, 11, ink)):
        text(translate(key), 28, y, size, color)
    text('12', 109, 282, 12, green)
    text('37', 109, 299, 12, ink)
    draw.rectangle((152, 122, 287, 144), fill=(20, 18, 12, 255))
    draw.rectangle((157, 126, 216, 140), fill=(75, 72, 53, 255))
    text(translate('Left click'), 160, 137, 12, ink)
    text(translate('Check Info'), 225, 137, 12, gold)
    draw.rectangle((152, 394, 692, 452), fill=(20, 18, 12, 255))
    key = 'Palace of Twilight East Wing Second Room Southwest Chest'
    value = '[' + translate('OPEN') + '] ' + locale['names'].get(key, key)
    lines, current = [], ''
    for ch in value:
        if current and width(current + ch, 16) > 520:
            split = current.rfind(' ')
            if split > 0:
                lines.append(current[:split])
                current = current[split + 1:] + ch
            else:
                lines.append(current)
                current = ch
        else:
            current += ch
    lines.append(current)
    for index, line in enumerate(lines[:2]):
        text(line, 160, 415 + index * 20, 16, green)
    canvas.convert('RGB').resize((1440, 960), Image.Resampling.NEAREST).save(directory / 'map-ko-synthetic-overlay.png')


def main() -> None:
    root = Path(__file__).resolve().parents[1]
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--locale', type=Path, default=root / 'res/ko.json')
    parser.add_argument('--font', type=Path, default=root / 'res/fonts/NanumGothic-Regular.ttf')
    parser.add_argument('--output-dir', type=Path, default=root / 'res/fonts')
    parser.add_argument('--preview-dir', type=Path)
    args = parser.parse_args()
    locale = json.loads(args.locale.read_text(encoding='utf-8'))
    manifest, images, textures = build(args.font, locale)
    args.output_dir.mkdir(parents=True, exist_ok=True)
    for page, texture in zip(manifest['pages'], textures):
        (args.output_dir / Path(page['resource']).name).write_bytes(texture)
    (args.output_dir / 'map-ko.json').write_text(json.dumps(manifest, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
    if args.preview_dir:
        write_previews(args.preview_dir, manifest, images, args.font, locale)
    print(f"Built {manifest['glyph_count']} glyphs in {len(images)} page(s): " +
          ', '.join(f'{image.width}x{image.height}' for image in images))


if __name__ == '__main__':
    main()

#!/usr/bin/env python3
"""
extract_assets.py - Extract game assets from Rodent's Revenge (Windows 3.1)

Extracts sprite sheets, icons, level data, and game constants from the
original rodent.exe and field100.dll files. Users must supply their own
copy of the game (from the Windows Entertainment Pack).

Developer tool only: the game reads its graphics straight from rodent.exe
(src/assets.c), and `rodent --dump-assets DIR` writes the images modders
can replace.  This script additionally splits tiles and dumps strings.

Usage:
    python3 extract_assets.py <game_dir> [output_dir]

    game_dir:   Directory containing rodent.exe (and optionally field100.dll)
    output_dir: Where to write extracted assets (default: ./assets)

Output:
    assets/
        sprites_small_color.png    - 108x36 color sprite sheet (12x12 tiles)
        sprites_small_mono.png     - 108x36 mono sprite sheet
        sprites_large_color.png    - 144x48 color sprite sheet (16x16 tiles)
        sprites_large_mono.png     - 144x48 mono sprite sheet
        tiles_small/               - Individual 12x12 tile PNGs
        tiles_large/               - Individual 16x16 tile PNGs
        icon.png                   - Game icon (32x32)
        timer.png                  - Timer display bitmap (32x32)
        level_data.json            - Level parameters and game constants
        strings.json               - All game strings (menus, messages)
        tile_map.json              - Tile ID to name mapping

Requires: Pillow (pip install Pillow)
"""

import argparse
import json
import os
import struct
import sys

try:
    from PIL import Image
except ImportError:
    print("Error: Pillow is required. Install with: pip install Pillow")
    sys.exit(1)


# ---- BMP Parsing ----

def decode_bmp_4bpp(data, offset):
    """Decode a 4bpp BMP from raw data at given offset.
    Handles both 'BM' file header and raw BITMAPINFOHEADER."""

    has_file_header = data[offset:offset+2] == b'BM'
    if has_file_header:
        info_off = offset + 14
    else:
        info_off = offset

    # BITMAPINFOHEADER
    header_size = struct.unpack_from('<I', data, info_off)[0]
    width = struct.unpack_from('<i', data, info_off + 4)[0]
    height = struct.unpack_from('<i', data, info_off + 8)[0]
    bpp = struct.unpack_from('<H', data, info_off + 14)[0]
    num_colors = struct.unpack_from('<I', data, info_off + 32)[0]

    if bpp != 4:
        raise ValueError(f"Expected 4bpp, got {bpp}bpp")

    if num_colors == 0:
        num_colors = 16

    bottom_up = height > 0
    height = abs(height)

    # Read palette
    pal_off = info_off + header_size
    palette = []
    for i in range(num_colors):
        b, g, r, _ = data[pal_off + i*4 : pal_off + i*4 + 4]
        palette.append((r, g, b))

    # Pad palette to 16 entries
    while len(palette) < 16:
        palette.append((0, 0, 0))

    # Read pixel data
    pix_off = pal_off + num_colors * 4
    if has_file_header:
        pix_off = offset + struct.unpack_from('<I', data, offset + 10)[0]

    row_bytes = ((width * 4 + 31) // 32) * 4  # padded to 4-byte boundary

    img = Image.new('RGB', (width, height))
    pixels = img.load()

    for row in range(height):
        if bottom_up:
            src_row = height - 1 - row
        else:
            src_row = row

        row_off = pix_off + src_row * row_bytes
        for col in range(width):
            byte_idx = col // 2
            byte_val = data[row_off + byte_idx]
            if col % 2 == 0:
                pix_val = (byte_val >> 4) & 0x0F
            else:
                pix_val = byte_val & 0x0F
            pixels[col, row] = palette[pix_val]

    return img, palette


def decode_bmp_1bpp(data, offset):
    """Decode a 1bpp BMP (for icon masks)."""

    has_file_header = data[offset:offset+2] == b'BM'
    info_off = offset + 14 if has_file_header else offset

    header_size = struct.unpack_from('<I', data, info_off)[0]
    width = struct.unpack_from('<i', data, info_off + 4)[0]
    height = struct.unpack_from('<i', data, info_off + 8)[0]
    bpp = struct.unpack_from('<H', data, info_off + 14)[0]

    if bpp != 1:
        raise ValueError(f"Expected 1bpp, got {bpp}bpp")

    bottom_up = height > 0
    height = abs(height)

    # Palette
    pal_off = info_off + header_size
    palette = []
    for i in range(2):
        b, g, r, _ = data[pal_off + i*4 : pal_off + i*4 + 4]
        palette.append((r, g, b))

    pix_off = pal_off + 2 * 4
    if has_file_header:
        pix_off = offset + struct.unpack_from('<I', data, offset + 10)[0]

    row_bytes = ((width + 31) // 32) * 4

    img = Image.new('RGB', (width, height))
    pixels = img.load()

    for row in range(height):
        src_row = (height - 1 - row) if bottom_up else row
        row_off = pix_off + src_row * row_bytes
        for col in range(width):
            byte_idx = col // 8
            bit_idx = 7 - (col % 8)
            bit_val = (data[row_off + byte_idx] >> bit_idx) & 1
            pixels[col, row] = palette[bit_val]

    return img, palette


# ---- Asset Location Map ----

# All offsets determined by binary analysis of rodent.exe
# These are file offsets to BMP file headers ("BM" magic)
ASSETS = {
    'sprites_small_color': {'offset': 0xae58, 'size': 0x856, 'type': 'bmp4',
                             'desc': 'Small color sprite sheet (108x36, 12x12 tiles)'},
    'sprites_small_mono':  {'offset': 0xb6d6, 'size': 0x856, 'type': 'bmp4',
                             'desc': 'Small mono sprite sheet (108x36, 12x12 tiles)'},
    'sprites_large_color': {'offset': 0xbf54, 'size': 0xdf6, 'type': 'bmp4',
                             'desc': 'Large color sprite sheet (144x48, 16x16 tiles)'},
    'sprites_large_mono':  {'offset': 0xcd72, 'size': 0xdf6, 'type': 'bmp4',
                             'desc': 'Large mono sprite sheet (144x48, 16x16 tiles)'},
    'timer':               {'offset': 0x138f, 'size': 0x276, 'type': 'bmp4',
                             'desc': 'Timer display bitmap (32x32)'},
}

# Icon resources (BITMAPINFOHEADER without BM header)
ICONS = {
    'icon_4bpp':   {'offset': 0x0500, 'size': 0x468, 'type': 'bmp4',
                     'desc': 'Game icon 32x32 @ 4bpp (with 32x32 mask below)'},
    'icon_4bpp_2': {'offset': 0x0800, 'size': 0x468, 'type': 'bmp4',
                     'desc': 'Game icon 32x32 @ 4bpp (alternate)'},
}

# Tile layout: 9 columns x 3 rows in sprite sheet.
# Tile ID encoding (from field100.dll FLDDRAW): low nibble = sheet column,
# high nibble = sheet row.  Names are the chXXX constants of RODENT2.FRM.
TILE_MAP = {
    0x00: 'open', 0x01: 'block', 0x02: 'wall', 0x03: 'vert', 0x04: 'horz',
    0x05: 'hole', 0x06: 'trap', 0x07: 'cheese', 0x08: 'holeguy',
    0x10: 'kat', 0x11: 'yarn', 0x12: 'yarn2', 0x13: 'yarn3', 0x14: 'yarn_last',
    0x15: 'guygrey', 0x16: 'grey', 0x17: 'katsleep', 0x18: 'bevel1',
    0x20: 'guy', 0x21: 'dead1', 0x22: 'dead2', 0x23: 'dead3', 0x24: 'dead4',
    0x25: 'dead', 0x26: 'bevel2', 0x27: 'bevel3', 0x28: 'bevel4',
}


# ---- String Extraction ----

def extract_strings(data):
    """Extract game strings from rodent.exe."""
    strings = {}

    # Known string locations (from VB1 form data analysis)
    known_strings = {
        'app_caption':     (0x96e6, 16),   # "Rodent's Revenge"
        'app_exe':         (0x96fa, 6),    # "Rodent"
        'app_help':        (0x9704, 10),   # "Rodent.hlp"
        'help_commands':   (0x9712, 8),    # "Commands"
        'help_how_to_play':(0x971e, 11),   # "How to Play"
        'ini_file':        (0x972e, 11),   # "entpack.ini"
        'key_size':        (0x973e, 4),    # "Size"
        'key_skill':       (0x9746, 5),    # "Skill"
        'key_room':        (0x9750, 4),    # "Room"
        'enter_level':     (0x9758, 28),   # "Enter Starting Level: (1-50)"
        'game_over':       (0x9778, 11),   # " Game Over "
        'paused':          (0x9788, 9),    # " Paused. "
        'continue':        (0x9796, 23),   # " Press F3 To Continue."
        'ok_new_game':     (0x97c6, 21),   # "Ok to start new game?"
        'created_with':    (0x97e0, 13),   # "Created with "
        'visual_basic':    (0x97f0, 29),   # "Microsoft(R) Visual Basic(TM)"
        'err_version':     (0x9812, 34),   # "Incorrect version of 'weputil.dll'."
        'err_cant_find':   (0x983a, 26),   # "Cannot find 'weputil.dll'."
        'err_install':     (0x9858, 39),   # "Please install Entertainment Pack again."
    }

    for key, (offset, length) in known_strings.items():
        if offset + length <= len(data):
            raw = data[offset:offset+length]
            # Trim to first non-printable char
            s = ''
            for b in raw:
                if 32 <= b < 127:
                    s += chr(b)
                else:
                    break
            strings[key] = s

    # Menu strings
    menu_items = []
    menu_strings = [
        (0x16ef, '&Game'), (0x1706, '&New Game'), (0x1724, '&Pause'),
        (0x173e, '&High Scores...'), (0x176f, 'E&xit'),
        (0x1787, '&Options'), (0x17a3, '&Level...'),
        (0x17d5, 'Sm&all Size'), (0x17f8, 'Lar&ge Size'),
        (0x182d, 'S&nail'), (0x184a, '&Slow'), (0x1866, '&Medium'),
        (0x1884, '&Fast'), (0x18a0, '&Blazing'),
        (0x18ba, '&Help'), (0x18d3, '&Index'), (0x18ed, '&How to Play'),
        (0x190a, '&Commands'), (0x1925, '&Using Help'),
        (0x1953, "&About Rodent's Revenge..."),
    ]
    for offset, expected in menu_strings:
        raw = data[offset:offset+len(expected)]
        text = raw.decode('ascii', errors='replace')
        menu_items.append({'offset': offset, 'text': text})

    strings['menu_items'] = menu_items

    # VB1 function names (for documentation)
    func_names = []
    func_table_region = data[0x9a62:0x9ee0]
    i = 0
    while i < len(func_table_region):
        start = i
        while i < len(func_table_region) and 32 <= func_table_region[i] < 127:
            i += 1
        if i - start >= 3:
            name = func_table_region[start:i].decode('ascii')
            if not name.startswith('sz') or len(name) < 20:
                func_names.append(name)
        i += 1
    strings['vb1_functions'] = func_names

    return strings


# ---- Level Data Extraction ----

def extract_level_data():
    """Return level configuration data.

    The exact values are embedded in VB1 p-code data segments which are
    not fully decodable. These are reasonable approximations based on
    gameplay analysis. The level wall layouts come from the sprite sheet
    bitmaps (read at runtime via GetPixel)."""

    levels = []
    for lvl in range(1, 51):
        levels.append({
            'level': lvl,
            'initial_cats': min(3 + (lvl - 1) // 3, 10),
            'cats_to_clear': min(3 + (lvl - 1) // 4, 10),
            'wall_pattern_col': (lvl - 1) % 10,
            'wall_pattern_row': (lvl - 1) // 10,
            'note': 'Approximate values; exact data in VB1 p-code'
        })

    return {
        'levels': levels,
        'grid_size': {'cols': 33, 'rows': 33},
        'tile_sizes': {
            'small': {'width': 12, 'height': 12},
            'large': {'width': 16, 'height': 16},
        },
        'sprite_sheet': {
            'cols': 9,
            'rows': 3,
            'total_tiles': 27,
        },
        'speed_levels': ['Snail', 'Slow', 'Medium', 'Fast', 'Blazing'],
        'initial_lives': 3,
        'max_enemies': 50,
        'directions': {
            'N': 0, 'NE': 1, 'E': 2, 'SE': 3,
            'S': 4, 'SW': 5, 'W': 6, 'NW': 7,
        },
        'direction_offsets': {
            'dx': [0, 1, 1, 1, 0, -1, -1, -1],
            'dy': [-1, -1, 0, 1, 1, 1, 0, -1],
        },
        'game_modes': [
            'DEMO', 'BEGIN_GAME', 'BEGIN_LEVEL', 'RESTART',
            'BEGIN', 'PLAY', 'PLAY_FAST', 'YARN',
            'DYING', 'END', 'END_LEVEL', 'END_GAME', 'LAST',
        ],
    }


# ---- Tile Splitting ----

def split_tiles(sheet_img, tile_w, tile_h, tile_map, output_dir):
    """Split a sprite sheet into individual tile images."""
    os.makedirs(output_dir, exist_ok=True)

    sheet_w, sheet_h = sheet_img.size
    cols = sheet_w // tile_w
    rows = sheet_h // tile_h

    manifest = []

    for row in range(rows):
        for col in range(cols):
            tile_id = row * 0x10 + col
            name = tile_map.get(tile_id, f'tile_{tile_id:02x}')
            tile = sheet_img.crop((
                col * tile_w, row * tile_h,
                (col + 1) * tile_w, (row + 1) * tile_h
            ))
            filename = f'{tile_id:02x}_{name}.png'
            tile.save(os.path.join(output_dir, filename))
            manifest.append({
                'id': tile_id,
                'id_hex': f'0x{tile_id:02x}',
                'name': name,
                'file': filename,
                'sheet_col': col,
                'sheet_row': row,
            })

    return manifest


# ---- Main ----

def validate_exe(data):
    """Check that this looks like a valid rodent.exe."""
    # Check for NE header
    if len(data) < 0x100:
        return False, "File too small"

    ne_off = struct.unpack_from('<H', data, 0x3C)[0]
    if ne_off + 2 > len(data) or data[ne_off:ne_off+2] != b'NE':
        return False, "Not a valid NE executable"

    # Check for game signature strings
    if b"Rodent's Revenge" not in data:
        return False, "Does not appear to be Rodent's Revenge"

    if b'FIELD100.DLL' not in data and b'field100.dll' not in data:
        return False, "Missing FIELD100.DLL reference"

    return True, "OK"


def main():
    parser = argparse.ArgumentParser(
        description="Extract assets from Rodent's Revenge (Windows 3.1)")
    parser.add_argument('game_dir',
        help="Directory containing rodent.exe")
    parser.add_argument('output_dir', nargs='?', default='assets',
        help="Output directory (default: ./assets)")
    parser.add_argument('--no-split', action='store_true',
        help="Don't split sprite sheets into individual tiles")
    parser.add_argument('--verbose', '-v', action='store_true',
        help="Print detailed extraction info")
    args = parser.parse_args()

    # Find rodent.exe
    exe_path = None
    for name in ['rodent.exe', 'RODENT.EXE', 'Rodent.exe']:
        path = os.path.join(args.game_dir, name)
        if os.path.isfile(path):
            exe_path = path
            break

    if exe_path is None:
        print(f"Error: rodent.exe not found in {args.game_dir}")
        print("Please provide a directory containing the original game files.")
        sys.exit(1)

    with open(exe_path, 'rb') as f:
        data = f.read()

    valid, msg = validate_exe(data)
    if not valid:
        print(f"Error: {exe_path}: {msg}")
        sys.exit(1)

    print(f"Found: {exe_path} ({len(data)} bytes)")

    os.makedirs(args.output_dir, exist_ok=True)

    # ---- Extract sprite sheets ----
    print("\nExtracting sprite sheets...")
    sheet_images = {}
    for name, info in ASSETS.items():
        if args.verbose:
            print(f"  {name}: {info['desc']}")

        try:
            img, palette = decode_bmp_4bpp(data, info['offset'])
            out_path = os.path.join(args.output_dir, f'{name}.png')
            img.save(out_path)
            sheet_images[name] = img
            print(f"  {out_path} ({img.size[0]}x{img.size[1]})")
        except Exception as e:
            print(f"  Warning: Failed to extract {name}: {e}")

    # ---- Extract icon ----
    print("\nExtracting icons...")
    for name, info in ICONS.items():
        try:
            # Icons have BITMAPINFOHEADER directly (no BM header)
            # Height is 2x (image + mask), so we take top half
            img, palette = decode_bmp_4bpp(data, info['offset'])
            # The icon bitmap has height=64 (32 image + 32 mask)
            # Take bottom 32 rows (image is bottom-up, mask is top half)
            icon = img.crop((0, 0, 32, 32))
            out_path = os.path.join(args.output_dir, f'{name}.png')
            icon.save(out_path)
            print(f"  {out_path} (32x32)")
        except Exception as e:
            print(f"  Warning: Failed to extract {name}: {e}")

    # ---- Split tiles ----
    if not args.no_split:
        print("\nSplitting tiles...")

        if 'sprites_small_color' in sheet_images:
            manifest = split_tiles(
                sheet_images['sprites_small_color'],
                12, 12, TILE_MAP,
                os.path.join(args.output_dir, 'tiles_small'))
            manifest_path = os.path.join(args.output_dir, 'tiles_small', 'manifest.json')
            with open(manifest_path, 'w') as f:
                json.dump(manifest, f, indent=2)
            print(f"  tiles_small/: {len(manifest)} tiles")

        if 'sprites_large_color' in sheet_images:
            manifest = split_tiles(
                sheet_images['sprites_large_color'],
                16, 16, TILE_MAP,
                os.path.join(args.output_dir, 'tiles_large'))
            manifest_path = os.path.join(args.output_dir, 'tiles_large', 'manifest.json')
            with open(manifest_path, 'w') as f:
                json.dump(manifest, f, indent=2)
            print(f"  tiles_large/: {len(manifest)} tiles")

    # ---- Extract strings ----
    print("\nExtracting strings...")
    strings = extract_strings(data)
    strings_path = os.path.join(args.output_dir, 'strings.json')
    with open(strings_path, 'w') as f:
        json.dump(strings, f, indent=2)
    print(f"  {strings_path} ({len(strings)} entries)")

    # ---- Level data ----
    print("\nExtracting level data...")
    level_data = extract_level_data()
    level_path = os.path.join(args.output_dir, 'level_data.json')
    with open(level_path, 'w') as f:
        json.dump(level_data, f, indent=2)
    print(f"  {level_path}")

    # ---- Tile map ----
    tile_map_out = {f'0x{k:02x}': v for k, v in sorted(TILE_MAP.items())}
    tile_map_path = os.path.join(args.output_dir, 'tile_map.json')
    with open(tile_map_path, 'w') as f:
        json.dump(tile_map_out, f, indent=2)
    print(f"  {tile_map_path}")

    # ---- Summary ----
    print(f"\nDone! Assets written to: {args.output_dir}/")
    print(f"  Sprite sheets: {len(sheet_images)}")
    if not args.no_split and 'sprites_small_color' in sheet_images:
        print(f"  Individual tiles: 27 small + 27 large")
    print(f"  Icons: {len(ICONS)}")
    print(f"  JSON data: 3 files")


if __name__ == '__main__':
    main()

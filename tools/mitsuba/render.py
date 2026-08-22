import sys
import os
import json
from concurrent.futures import ThreadPoolExecutor
import numpy as np
import mitsuba as mi
from PIL import Image

from tonemap import apply_tonemap

if 'cuda_ad_rgb' in mi.variants():
    mi.set_variant('cuda_ad_rgb')
else:
    mi.set_variant('scalar_rgb')

script_dir = os.path.dirname(os.path.abspath(__file__))
cache_dir = os.path.join(script_dir, 'tex_cache')

def cache_path(source, suffix):
    folder = os.path.basename(os.path.dirname(source))
    name = os.path.splitext(os.path.basename(source))[0]
    return os.path.join(cache_dir, folder + '_' + name + suffix + '.png')

def decode_dds(source):
    out = cache_path(source, '')
    if os.path.exists(out):
        return out

    with open(source, 'rb') as dds_file:
        four_cc = dds_file.read(88)[84:88]

    pixels = np.array(Image.open(source))

    # BC5 (ATI2) reconstruct normal (blue channel)
    if four_cc == b'ATI2':
        x = pixels[:, :, 0] / 255.0 * 2.0 - 1.0
        y = pixels[:, :, 1] / 255.0 * 2.0 - 1.0
        z = np.sqrt(np.clip(1.0 - x * x - y * y, 0.0, 1.0))
        pixels[:, :, 2] = (z * 0.5 + 0.5) * 255.0

    # Mitsuba needs at least 2x2 textures
    height, width = pixels.shape[0], pixels.shape[1]
    if height < 2 or width < 2:
        pixels = np.tile(pixels, (2 // height, 2 // width, 1))

    Image.fromarray(pixels).save(out, compress_level=1)
    return out

def extract_channel(source, channel):
    out = cache_path(source, '_' + channel)
    if os.path.exists(out):
        return out

    index = {'r': 0, 'g': 1, 'b': 2}[channel]
    pixels = np.array(Image.open(source).convert('RGB'))
    Image.fromarray(pixels[:, :, index]).save(out, compress_level=1)
    return out

def find_bitmaps(node, found):
    if isinstance(node, dict):
        if node.get('type') == 'bitmap':
            found.append(node)
        for value in node.values():
            find_bitmaps(value, found)
    elif isinstance(node, list):
        for value in node:
            find_bitmaps(value, found)
    return found

def resolve_textures(scene_dict):
    os.makedirs(cache_dir, exist_ok=True)
    bitmaps = find_bitmaps(scene_dict, [])

    # Decode all .dds in parallel
    dds_files = sorted({node['filename'] for node in bitmaps if node['filename'].lower().endswith('.dds')})
    print(f'Decoding {len(dds_files)} textures')
    with ThreadPoolExecutor(32) as pool:
        for source in dds_files:
            pool.submit(decode_dds, source)

    # Resolve real filenames and extract channels if any
    for node in bitmaps:
        if node['filename'].lower().endswith('.dds'):
            node['filename'] = cache_path(node['filename'], '')
        if 'channel' in node:
            node['filename'] = extract_channel(node['filename'], node.pop('channel'))

# Convert my custom lookat and matrix to mitsuba's format
def build_transforms(node):
    if isinstance(node, dict):
        if node.get('type') == 'lookat':
            return mi.ScalarTransform4f().look_at(origin=node['origin'], target=node['target'], up=node['up'])
        if node.get('type') == 'matrix':
            values = node['value']
            rows = [values[i * 4:i * 4 + 4] for i in range(4)]
            return mi.ScalarTransform4f(rows)
        return {key: build_transforms(value) for key, value in node.items()}
    if isinstance(node, list):
        return [build_transforms(value) for value in node]
    return node

scene_path = sys.argv[1] if len(sys.argv) > 1 else os.path.join(script_dir, 'test/scene.json')
out_path = sys.argv[2] if len(sys.argv) > 2 else os.path.splitext(scene_path)[0] + '.png'

tonemap = {'exposure': 1.0, 'tonemapper': 0}
if scene_path.lower().endswith('.xml'):
    scene = mi.load_file(scene_path)
else:
    with open(scene_path) as json_file:
        scene_dict = json.load(json_file)
    tonemap = scene_dict.pop('tonemap', tonemap)
    resolve_textures(scene_dict)
    scene = mi.load_dict(build_transforms(scene_dict))

rendered = np.array(mi.render(scene, spp=256))
rendered = apply_tonemap(rendered, tonemap['exposure'], tonemap['tonemapper'])

bitmap = mi.Bitmap(np.ascontiguousarray(rendered.astype(np.float32)))
bitmap.convert(mi.Bitmap.PixelFormat.RGB, mi.Struct.Type.UInt8, srgb_gamma=False).write(out_path)
print(f'Wrote {out_path}')

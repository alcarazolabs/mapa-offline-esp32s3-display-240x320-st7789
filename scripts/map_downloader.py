import os
import requests
import math
import time
from PIL import Image

zoom = 16

center_lat = -23.550706
center_lon = -46.633382

lat_min = center_lat - 0.01
lat_max = center_lat + 0.01
lon_min = center_lon - 0.01
lon_max = center_lon + 0.01

png_dir = "tiles"
rgb_dir = "tiles_rgb565"

os.makedirs(png_dir, exist_ok=True)
os.makedirs(rgb_dir, exist_ok=True)

headers = {
    # Unique ID + contact email (recommended by OSM)
    "User-Agent": "MyPythonMappingApp/1.0 (my_correo@gmail.com)"
}


def deg2num(lat, lon, zoom):
    lat_rad = math.radians(lat)
    n = 2 ** zoom
    xtile = int((lon + 180.0) / 360.0 * n)
    ytile = int(
        (1.0 - math.log(math.tan(lat_rad) + 1.0 / math.cos(lat_rad)) / math.pi)
        / 2.0 * n
    )
    return xtile, ytile

def convert_to_rgb565(png_path, rgb_path):
    img = Image.open(png_path).convert("RGB")
    img = img.resize((256, 256))

    pixels = img.load()

    with open(rgb_path, "wb") as f:
        for y in range(256):
            for x in range(256):
                r, g, b = pixels[x, y]

                r5 = r >> 3
                g6 = g >> 2
                b5 = b >> 3

                value = (r5 << 11) | (g6 << 5) | b5
                f.write(value.to_bytes(2, byteorder="little"))

x1, y1 = deg2num(lat_min, lon_min, zoom)
x2, y2 = deg2num(lat_max, lon_max, zoom)

x_min = min(x1, x2)
x_max = max(x1, x2)
y_min = min(y1, y2)
y_max = max(y1, y2)

total_tiles = (x_max - x_min + 1) * (y_max - y_min + 1)

print("================================")
print("Zoom level:", zoom)
print("Total tiles:", total_tiles)
print("Estimated RGB565 size:", total_tiles * 128 / 1024, "MB")
print("================================")

if total_tiles > 200:
    print("Warning: Large area for ESP32")
    input("Press ENTER to continue...")

session = requests.Session()
count = 0

for x in range(x_min, x_max + 1):
    for y in range(y_min, y_max + 1):

        png_name = f"{zoom}_{x}_{y}.png"
        rgb_name = f"{zoom}_{x}_{y}.rgb565"

        png_path = os.path.join(png_dir, png_name)
        rgb_path = os.path.join(rgb_dir, rgb_name)

        if os.path.exists(rgb_path):
            continue

        if not os.path.exists(png_path):
            url = f"https://tile.openstreetmap.org/{zoom}/{x}/{y}.png"
            try:
                r = session.get(url, timeout=10, headers=headers)
                if r.status_code == 200:
                    with open(png_path, "wb") as f:
                        f.write(r.content)
                    print("Downloaded:", png_name)
                else:
                    print("Download failed:", png_name)
                    continue
            except Exception as e:
                print("Download error:", png_name, e)
                continue

            time.sleep(0.1)

        try:
            convert_to_rgb565(png_path, rgb_path)
            count += 1
            print(f"Converted ({count}/{total_tiles}):", rgb_name)
        except Exception as e:
            print("Conversion error:", png_name, e)

print("Process completed!")
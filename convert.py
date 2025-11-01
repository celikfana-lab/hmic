from PIL import Image
import lzma
import os
from collections import defaultdict

# 🩵 Load the sacred image
img_path = input("Enter image file path: ").strip()
img = Image.open(img_path).convert("RGB")  # 💀 no alpha dimension — pure RGB realm
w, h = img.size

# 😈 Ask the user what flavor of chaos they desire
mode = input("Choose format (HMIC / HMIC7): ").strip().upper()

# 🧾 Build the HMIC text data in memory
data = []
data.append("info{")
data.append(f"DISPLAY={w}X{h}")
data.append("FPS=1")
data.append("F=1")
data.append("LOOP=Y")
data.append("}\n")

# 🌈 BEGIN FRAME BLOCK
frame = 1
data.append(f"F{frame}{{")

# 🧠 Group consecutive horizontal pixels by color — RLE style
color_groups = defaultdict(list)

for y in range(h):
    x = 0
    while x < w:
        r, g, b = img.getpixel((x, y))  # 🩸 only RGB, no a
        start_x = x

        # 🚀 detect horizontal runs of same color
        while x + 1 < w:
            nr, ng, nb = img.getpixel((x + 1, y))
            if (nr, ng, nb) != (r, g, b):
                break
            x += 1

        end_x = x
        if start_x == end_x:
            color_groups[(r, g, b)].append(f"P={start_x+1}x{y+1}")
        else:
            color_groups[(r, g, b)].append(f"PL={start_x+1}x{y+1}-{end_x+1}x{y+1}")

        x += 1

# 🎨 Write grouped rgb() blocks
for (r, g, b), cmds in color_groups.items():
    data.append(f"  rgb({r},{g},{b}){{")
    for cmd in cmds:
        data.append(f"    {cmd}")
    data.append("  }")

# 🔚 END FRAME BLOCK
data.append("}")

# 🧵 Join the chaos script
text_data = "\n".join(data)

# 🚀 Output depending on mode
base_name = os.path.splitext(os.path.basename(img_path))[0]
if mode == "HMIC":
    out_file = f"{base_name}.hmic"
    with open(out_file, "w", encoding="utf-8") as f:
        f.write(text_data)
    print(f"✅ HMIC file created successfully — {out_file} blessed 💚")

elif mode == "HMIC7":
    out_file = f"{base_name}.hmic7"
    compressed_data = lzma.compress(text_data.encode("utf-8"), format=lzma.FORMAT_XZ)
    with open(out_file, "wb") as f:
        f.write(compressed_data)
    print(f"🌀 HMIC7 file created — LZMA2 chewed {out_file} like bubblegum 💾🔥")

else:
    print("❌ invalid format, Miku has left the chat 😭")
    exit()

# 🧮 File size flex
if os.path.exists(f"{base_name}.hmic"):
    print(f"HMIC size: {os.path.getsize(f'{base_name}.hmic') / 1024:.2f} KB")
if os.path.exists(f"{base_name}.hmic7"):
    print(f"HMIC7 size: {os.path.getsize(f'{base_name}.hmic7') / 1024:.2f} KB")

print("💥 Conversion complete — behold the pure RGB chaos, alpha banished 💥")

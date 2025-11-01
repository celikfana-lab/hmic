from PIL import Image
import zstandard as zstd
import os
from collections import defaultdict

# 🩵 Load the sacred image/gif
img_path = input("Enter image file path: ").strip()
img = Image.open(img_path)

# 🎬 Check if it's an animated GIF
is_animated = hasattr(img, 'n_frames') and img.n_frames > 1

if is_animated:
    print(f"🎬 ANIMATED GIF DETECTED!! {img.n_frames} frames of pure chaos incoming 🔥")
    n_frames = img.n_frames
    frame_duration = img.info.get('duration', 100)
    fps = max(1, int(1000 / frame_duration))
    loop = img.info.get('loop', 0) == 0
else:
    print(f"📸 Single frame image detected — classic mode 💚")
    n_frames = 1
    fps = 1
    loop = True

w, h = img.size

mode = input("Choose format (HMIC / HMIC7): ").strip().upper()

# 🎨 Extract all frames and their pixel data
frames_data = []
for frame_idx in range(n_frames):
    if is_animated:
        img.seek(frame_idx)
    current_frame = img.convert("RGB")
    
    frame_pixels = {}
    for y in range(h):
        for x in range(w):
            r, g, b = current_frame.getpixel((x, y))
            frame_pixels[(x, y)] = (r, g, b)
    
    frames_data.append(frame_pixels)
    print(f"[DEBUG] 📦 Extracted frame {frame_idx + 1}/{n_frames}")

# 🎯 Convert frame lists to frame ranges
def frames_to_range_string(frame_list):
    if not frame_list:
        return ""
    if len(frame_list) == 1:
        return str(frame_list[0])
    
    ranges = []
    start = frame_list[0]
    end = frame_list[0]
    
    for i in range(1, len(frame_list)):
        if frame_list[i] == end + 1:
            end = frame_list[i]
        else:
            ranges.append(f"{start}-{end}" if start != end else str(start))
            start = frame_list[i]
            end = frame_list[i]
    
    ranges.append(f"{start}-{end}" if start != end else str(start))
    return ",".join(ranges)

# 🧠 COMPLETELY REWRITTEN ALGORITHM - Process each frame independently first, then optimize!!
print("[DEBUG] 🔥 Building per-frame pixel data...")

# Step 1: Build complete frame data (every pixel must be in SOME frame)
frame_commands = [defaultdict(list) for _ in range(n_frames)]

for frame_idx in range(n_frames):
    print(f"[DEBUG] 🎨 Processing frame {frame_idx + 1}/{n_frames}...")
    processed_rows = 0
    
    for y in range(h):
        x = 0
        while x < w:
            pixel_color = frames_data[frame_idx][(x, y)]
            
            # Find horizontal run of same color
            run_length = 1
            while x + run_length < w and frames_data[frame_idx][(x + run_length, y)] == pixel_color:
                run_length += 1
            
            # Generate command
            end_x = x + run_length - 1
            if run_length == 1:
                cmd = f"P={x+1}x{y+1}"
            else:
                cmd = f"PL={x+1}x{y+1}-{end_x+1}x{y+1}"
            
            frame_commands[frame_idx][pixel_color].append((cmd, x, end_x, y))
            x += run_length
        
        processed_rows += 1
        if processed_rows % 50 == 0:
            print(f"[DEBUG]   Row {processed_rows}/{h}")

# Step 2: NOW optimize by finding temporal patterns (optional multi-frame compression)
print("[DEBUG] 🚀 Optimizing with temporal compression...")

# Track which commands have been merged into temporal blocks
merged_commands = [set() for _ in range(n_frames)]
temporal_commands = defaultdict(lambda: defaultdict(list))

# Try to find commands that appear in consecutive frames
for frame_idx in range(n_frames - 1):
    for color, cmd_list in frame_commands[frame_idx].items():
        for cmd_data in cmd_list:
            cmd, x, end_x, y = cmd_data
            
            # Skip if already merged
            if cmd_data in merged_commands[frame_idx]:
                continue
            
            # Check if same command exists in next frames
            consecutive_frames = [frame_idx + 1]  # Start with current frame (1-indexed)
            
            # Look ahead for matching commands
            for next_frame_idx in range(frame_idx + 1, n_frames):
                # Check if this exact pixel range has the same color in next frame
                match_found = False
                if color in frame_commands[next_frame_idx]:
                    for next_cmd_data in frame_commands[next_frame_idx][color]:
                        next_cmd, next_x, next_end_x, next_y = next_cmd_data
                        if next_x == x and next_end_x == end_x and next_y == y:
                            if next_cmd_data not in merged_commands[next_frame_idx]:
                                consecutive_frames.append(next_frame_idx + 1)
                                merged_commands[next_frame_idx].add(next_cmd_data)
                                match_found = True
                                break
                
                if not match_found:
                    break  # Stop if we don't find consecutive match
            
            # If we found a multi-frame pattern, use it!
            if len(consecutive_frames) > 1:
                frame_range_str = frames_to_range_string(consecutive_frames)
                temporal_commands[frame_range_str][color].append(cmd)
                merged_commands[frame_idx].add(cmd_data)

print(f"[DEBUG] ✅ Created {len(temporal_commands)} temporal command groups")

# 🧾 Build the HMIC text data
data = []
data.append("info{")
data.append(f"DISPLAY={w}X{h}")
data.append(f"FPS={fps}")
data.append(f"F={n_frames}")
data.append(f"LOOP={'Y' if loop else 'N'}")
data.append("}\n")

# 🔥 Write temporal blocks first (multi-frame commands)
print("[DEBUG] 🎯 Writing temporal multi-frame blocks...")
for frame_range_str in sorted(temporal_commands.keys()):
    color_commands = temporal_commands[frame_range_str]
    data.append(f"F{frame_range_str}{{")
    for color, cmds in color_commands.items():
        r, g, b = color
        data.append(f"  rgb({r},{g},{b}){{")
        for cmd in cmds:
            data.append(f"    {cmd}")
        data.append(f"  }}")
    data.append("}")

# 🌈 Write individual frame blocks (commands not in temporal blocks)
print("[DEBUG] 🎨 Writing individual frame blocks...")
for frame_idx in range(n_frames):
    frame_num = frame_idx + 1
    has_content = False
    frame_data = [f"F{frame_num}{{"]
    
    # Write commands that weren't merged
    for color, cmd_list in frame_commands[frame_idx].items():
        color_written = False
        for cmd_data in cmd_list:
            if cmd_data not in merged_commands[frame_idx]:
                if not color_written:
                    has_content = True
                    r, g, b = color
                    frame_data.append(f"  rgb({r},{g},{b}){{")
                    color_written = True
                
                cmd = cmd_data[0]  # Extract just the command string
                frame_data.append(f"    {cmd}")
        
        if color_written:
            frame_data.append(f"  }}")
    
    frame_data.append("}")
    
    if has_content:
        data.extend(frame_data)
        print(f"[DEBUG] ✅ Wrote frame {frame_num}")

# 🧵 Join the chaos script
text_data = "\n".join(data)

# 🚀 Output
base_name = os.path.splitext(os.path.basename(img_path))[0]
if mode == "HMIC":
    out_file = f"{base_name}.hmic"
    with open(out_file, "w", encoding="utf-8") as f:
        f.write(text_data)
    print(f"✅ HMIC file created successfully — {out_file} blessed 💚")

elif mode == "HMIC7":
    out_file = f"{base_name}.hmic7"
    cctx = zstd.ZstdCompressor(level=19)
    compressed_data = cctx.compress(text_data.encode("utf-8"))
    with open(out_file, "wb") as f:
        f.write(compressed_data)
    print(f"🌀 HMIC7 file created — Zstd absolutely DEVOURED {out_file} no crumbs left 💾🔥")

else:
    print("❌ invalid format, Miku has left the chat 😭")
    exit()

# 🧮 File size flex
if os.path.exists(f"{base_name}.hmic"):
    print(f"HMIC size: {os.path.getsize(f'{base_name}.hmic') / 1024:.2f} KB")
if os.path.exists(f"{base_name}.hmic7"):
    print(f"HMIC7 size: {os.path.getsize(f'{base_name}.hmic7') / 1024:.2f} KB")

if is_animated:
    print(f"🎬 Animation info: {n_frames} frames @ {fps} FPS, Loop={'YES' if loop else 'NO'} 🔥")

# 🔥 VERIFICATION STATS
total_commands = sum(len(cmds) for color_dict in temporal_commands.values() for cmds in color_dict.values())
for frame_idx in range(n_frames):
    for cmd_list in frame_commands[frame_idx].values():
        for cmd_data in cmd_list:
            if cmd_data not in merged_commands[frame_idx]:
                total_commands += 1

print(f"📊 Total commands generated: {total_commands}")
print(f"📊 Image dimensions: {w}x{h} = {w*h} pixels per frame")
print(f"📊 Total frames: {n_frames}")

print("💥 Conversion complete — behold the pure RGB chaos, alpha banished 💥")
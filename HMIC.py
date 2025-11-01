import pygame, re, lzma

DISPLAY_SIZE = (5, 5)
PIXEL_SIZE = 100
FPS = 2
TOTAL_FRAMES = 9
LOOP = True


def parse_color(color_str):
    color_str = color_str.strip()
    if color_str.startswith("#") and len(color_str) == 7:
        return tuple(int(color_str[i:i + 2], 16) for i in (1, 3, 5))
    if color_str.lower().startswith("rgb("):
        nums = re.findall(r"\d+", color_str)
        if len(nums) == 3:
            return tuple(map(int, nums))
    return (255, 255, 255)


def parse_header(content):
    global DISPLAY_SIZE, FPS, TOTAL_FRAMES, LOOP
    m = re.search(r"info\s*\{([^}]*)\}", content, re.I | re.S)
    if not m:
        return
    for line in m.group(1).splitlines():
        line = line.strip()
        if not line or "=" not in line:
            continue
        key, value = [s.strip().upper() for s in line.split("=", 1)]
        if key == "DISPLAY":
            DISPLAY_SIZE = tuple(map(int, value.split("X")))
        elif key == "FPS":
            FPS = int(value)
        elif key == "F":
            TOTAL_FRAMES = int(value)
        elif key == "LOOP":
            LOOP = value == "Y"


def parse_pl(plist):
    """Support PL= and P= pixel lines."""
    pixels = []
    if plist.startswith("PL="):
        # Range form: PL=1x1-10x1
        p1, p2 = plist[3:].split("-")
        x1, y1 = map(int, p1.split("x"))
        x2, y2 = map(int, p2.split("x"))
        if y1 == y2:  # horizontal line
            for x in range(x1, x2 + 1):
                pixels.append((x, y1))
        elif x1 == x2:  # vertical line
            for y in range(y1, y2 + 1):
                pixels.append((x1, y))
        else:
            # diagonal fallback
            dx = 1 if x2 > x1 else -1
            dy = 1 if y2 > y1 else -1
            steps = max(abs(x2 - x1), abs(y2 - y1))
            for i in range(steps + 1):
                pixels.append((x1 + i * dx, y1 + i * dy))
    else:
        # multiple pixels: P=1x1,2x1,3x1
        pts = plist[2:].split(",")
        for p in pts:
            if "x" in p:
                x, y = map(int, p.split("x"))
                pixels.append((x, y))
    return pixels


def parse_frame_block(text):
    frame_m = re.match(r"F(\d+)(?:-(\d+))?\s*\{(.*)\}", text, re.S | re.I)
    if not frame_m:
        return []
    start, end, body = frame_m.groups()
    start, end = int(start), int(end or start)
    cmds = []

    # find rgb/hex color blocks
    color_pat = re.compile(r"(rgb\([^)]+\)|#[0-9A-F]{6})\s*\{(.*?)\}", re.I | re.S)
    for color_str, inner in color_pat.findall(body):
        color = parse_color(color_str)
        pixels = []
        for ln in inner.splitlines():
            ln = ln.strip()
            if ln.lower().startswith("p=") or ln.lower().startswith("pl="):
                pixels.extend(parse_pl(ln))
        if pixels:
            cmds.append((start, end, pixels, color))
    return cmds


def load_hmic(filename, compressed=False):
    if compressed:
        with lzma.open(filename, "rt", encoding="utf-8") as f:
            content = f.read()
    else:
        with open(filename, encoding="utf-8") as f:
            content = f.read()

    parse_header(content)
    cmds = []

    # Parse all F{...} blocks
    frame_blocks = re.findall(r"F\d+(?:-\d+)?\s*\{[^{}]*(?:\{[^{}]*\}[^{}]*)*\}", content, re.S | re.I)
    for fb in frame_blocks:
        cmds.extend(parse_frame_block(fb))

    # Also parse direct line commands (fallback mode)
    for line in content.splitlines():
        line = line.strip()
        if not line:
            continue
        if line.lower().startswith("f="):
            # legacy line form
            m = re.match(r"f=(\d+)-?(\d+)?\s+(p[l]?=[^#\s]+)\s+(#[0-9A-F]{6}|rgb\([^)]+\))", line, re.I)
            if not m:
                continue
            start, end, plist, color_str = m.groups()
            start, end = int(start), int(end or start)
            pixels = parse_pl(plist)
            color = parse_color(color_str)
            cmds.append((start, end, pixels, color))

    return cmds


def play_hmic_gui(filename, compressed=False):
    pygame.init()
    cmds = load_hmic(filename, compressed)
    WINDOW_SIZE = 800
    global PIXEL_SIZE
    PIXEL_SIZE = WINDOW_SIZE // max(DISPLAY_SIZE)

    screen = pygame.display.set_mode((WINDOW_SIZE, WINDOW_SIZE))
    pygame.display.set_caption("HMIC - hatsune miku is cringe")
    clock = pygame.time.Clock()

    frame = 1
    running = True
    while running:
        for e in pygame.event.get():
            if e.type == pygame.QUIT:
                running = False

        buffer = {}
        for (start, end, pixels, color) in cmds:
            if start <= frame <= end:
                for p in pixels:
                    buffer[p] = color

        screen.fill((0, 0, 0))
        ox = (WINDOW_SIZE - DISPLAY_SIZE[0] * PIXEL_SIZE) // 2
        oy = (WINDOW_SIZE - DISPLAY_SIZE[1] * PIXEL_SIZE) // 2
        for (x, y), color in buffer.items():
            pygame.draw.rect(screen, color,
                             (ox + (x - 1) * PIXEL_SIZE,
                              oy + (y - 1) * PIXEL_SIZE,
                              PIXEL_SIZE, PIXEL_SIZE))

        pygame.display.flip()
        frame += 1
        clock.tick(FPS)
        if frame > TOTAL_FRAMES:
            frame = 1 if LOOP else 9999999

    pygame.quit()

if __name__ == "__main__":
    print("🌀 HMIC 7.4 chaos RESURRECTION MODE 🌀")
    ftype = input("File type [1] HMIC / [2] HMIC7? ").strip()
    fname = input("Path to file: ").strip()
    compressed = ftype == "2" or fname.lower().endswith(".hmic7")
    play_hmic_gui(fname, compressed)

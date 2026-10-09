#!/usr/bin/env python3
"""Chess pages for the piececraft book. Events call this. No sockets.

A page is an 8x8 map.txt. Each cell is one EVENT row. A piece row is
cmds=select. An empty row is cmds=land. Selecting a piece writes
move_range_matrix.txt (# = legal). Landing on a # moves that piece.
Capturing the king ends the game and updates Elo. The computer is the
same move, played for black when mode=computer.
"""
import os, random, sys

HERE = os.path.dirname(os.path.abspath(__file__))
PCHQ = os.path.dirname(HERE)
BOOK = os.path.join(PCHQ, "pieces", "system", "maps", "chess")
HOUSE = os.path.dirname(os.path.dirname(PCHQ))
K = 32

WHITE = set("PNBRQK")
BLACK = set("pnbrqk")

def desk_dir(name):
    return os.path.join(BOOK, name)

def read_map(desk):
    path = os.path.join(desk_dir(desk), "map.txt")
    return [list(line.rstrip("\n")) for line in open(path)]

def write_map(desk, rows):
    with open(os.path.join(desk_dir(desk), "map.txt"), "w") as f:
        for row in rows:
            f.write("".join(row) + "\n")

def read_events(desk):
    rows = []
    for line in open(os.path.join(desk_dir(desk), "events.pdl")):
        if not line.startswith("EVENT"):
            continue
        x = y = g = cmd = ""
        parts = line.split("|")
        for tok in parts[1].split():
            if tok.startswith("x="): x = int(tok[2:])
            elif tok.startswith("y="): y = int(tok[2:])
            elif tok.startswith("glyph="): g = tok[6:]
        for tok in parts[2].split():
            if tok.startswith("cmds="): cmd = tok[5:]
        rows.append({"x": x, "y": y, "g": g, "cmd": cmd, "raw": line})
    return rows

def write_events(desk, rows):
    path = os.path.join(desk_dir(desk), "events.pdl")
    head = []
    for line in open(path):
        if line.startswith("EVENT"):
            break
        head.append(line if line.endswith("\n") else line + "\n")
    names = {"N": "New Game", "C": "Computer", "P": "Player", "Q": "Quit",
             "E": "Elo", "K": "King", "Q2": "Queen"}
    with open(path, "w") as f:
        f.writelines(head)
        for ev in rows:
            f.write("EVENT        | x=%d y=%d glyph=%s type=piece | trigger=on-click cmds=%s\n"
                    % (ev["x"], ev["y"], ev["g"], ev["cmd"]))
    bar = os.path.join(desk_dir(desk), "bar.txt")
    piece_name = {
        "P": "Pawn", "N": "Knight", "B": "Bishop", "R": "Rook", "Q": "Queen", "K": "King",
        "p": "Pawn", "n": "Knight", "b": "Bishop", "r": "Rook", "q": "Queen", "k": "King",
        ".": "Land", "N": "New Game", "C": "Computer", "P": "Player", "Q": "Quit", "E": "Elo",
    }
    # Title uses capital letters that collide with pieces. Title glyphs are N,C,P,Q,E
    # and the board uses the same letters for pieces. The command tells them apart.
    with open(bar, "w") as f:
        for i, ev in enumerate(rows, 1):
            if ev["cmd"] == "select":
                label = {"P":"Pawn","N":"Knight","B":"Bishop","R":"Rook","Q":"Queen","K":"King",
                         "p":"Pawn","n":"Knight","b":"Bishop","r":"Rook","q":"Queen","k":"King"}.get(ev["g"], ev["g"])
            elif ev["cmd"] == "land":
                label = "Land"
            else:
                label = {"start_standard": "Standard", "start_computer": "Computer",
                         "start_player": "Player", "stop_game": "Quit", "show_elo": "Elo"}.get(ev["cmd"], ev["g"])
            f.write("%d\t%s\t%d\t%d\ton-click\n" % (i, label, ev["x"], ev["y"]))

def kv_path(name):
    return os.path.join(BOOK, name)

def read_kv(path):
    d = {}
    if os.path.isfile(path):
        for line in open(path):
            if "=" in line:
                k, v = line.strip().split("=", 1)
                d[k] = v
    return d

def write_kv(path, d):
    with open(path, "w") as f:
        for k in sorted(d):
            f.write("%s=%s\n" % (k, d[k]))

def side_of(g):
    if g in WHITE: return "white"
    if g in BLACK: return "black"
    return ""

def in_b(x, y, rows):
    return 0 <= y < len(rows) and 0 <= x < len(rows[0])

def ray(rows, x, y, dirs, stop_color):
    out = []
    h, w = len(rows), len(rows[0])
    for dx, dy in dirs:
        cx, cy = x + dx, y + dy
        while 0 <= cx < w and 0 <= cy < h:
            g = rows[cy][cx]
            if g == ".":
                out.append((cx, cy))
            else:
                if side_of(g) and side_of(g) != stop_color:
                    out.append((cx, cy))
                break
            cx += dx
            cy += dy
    return out

def legal(rows, x, y):
    g = rows[y][x]
    color = side_of(g)
    if not color:
        return []
    out = []
    if g in "Nn":
        for dx, dy in ((1,2),(2,1),(-1,2),(-2,1),(1,-2),(2,-1),(-1,-2),(-2,-1)):
            cx, cy = x + dx, y + dy
            if in_b(cx, cy, rows) and side_of(rows[cy][cx]) != color:
                out.append((cx, cy))
    elif g in "Kk":
        for dx in (-1, 0, 1):
            for dy in (-1, 0, 1):
                if dx == 0 and dy == 0: continue
                cx, cy = x + dx, y + dy
                if in_b(cx, cy, rows) and side_of(rows[cy][cx]) != color:
                    out.append((cx, cy))
    elif g in "Rr":
        out = ray(rows, x, y, ((1,0),(-1,0),(0,1),(0,-1)), color)
    elif g in "Bb":
        out = ray(rows, x, y, ((1,1),(1,-1),(-1,1),(-1,-1)), color)
    elif g in "Qq":
        out = ray(rows, x, y, ((1,0),(-1,0),(0,1),(0,-1),(1,1),(1,-1),(-1,1),(-1,-1)), color)
    elif g in "Pp":
        step = -1 if g == "P" else 1
        start = 6 if g == "P" else 1
        nx, ny = x, y + step
        if in_b(nx, ny, rows) and rows[ny][nx] == ".":
            out.append((nx, ny))
            nx2, ny2 = x, y + step * 2
            if y == start and in_b(nx2, ny2, rows) and rows[ny2][nx2] == ".":
                out.append((nx2, ny2))
        for dx in (-1, 1):
            cx, cy = x + dx, y + step
            if in_b(cx, cy, rows) and side_of(rows[cy][cx]) not in ("", color):
                out.append((cx, cy))
    return out

def write_matrix(desk, rows, cells):
    h, w = len(rows), len(rows[0])
    marks = {(x, y) for x, y in cells}
    lines = []
    for y in range(h):
        lines.append("".join("#" if (x, y) in marks else "." for x in range(w)))
    text = "\n".join(lines) + "\n"
    with open(os.path.join(desk_dir(desk), "move_range_matrix.txt"), "w") as f:
        f.write(text)
    world = read_kv(os.path.join(PCHQ, "pieces", "world_01", "state.txt"))
    if world.get("map_id") == "chess":
        disp = os.path.join(PCHQ, "pieces", "display")
        os.makedirs(disp, exist_ok=True)
        with open(os.path.join(disp, "move_range_matrix.txt"), "w") as f:
            f.write(text)

def clear_matrix(desk):
    for path in (os.path.join(desk_dir(desk), "move_range_matrix.txt"),
                 os.path.join(PCHQ, "pieces", "display", "move_range_matrix.txt")):
        world = read_kv(os.path.join(PCHQ, "pieces", "world_01", "state.txt"))
        if path.endswith("display/move_range_matrix.txt") and world.get("map_id") != "chess":
            continue
        if os.path.isfile(path):
            os.remove(path)

def expected(me, opp):
    return 1.0 / (1.0 + 10 ** ((opp - me) / 400.0))

def apply_elo(winner):
    elo = read_kv(kv_path("elo.pdl"))
    w = float(elo.get("white", "1200"))
    b = float(elo.get("black", "1200"))
    if winner == "white":
        sw, sb = 1.0, 0.0
    elif winner == "black":
        sw, sb = 0.0, 1.0
    else:
        sw, sb = 0.5, 0.5
    ew, eb = expected(w, b), expected(b, w)
    w2 = round(w + K * (sw - ew))
    b2 = round(b + K * (sb - eb))
    games = int(elo.get("games", "0")) + 1
    write_kv(kv_path("elo.pdl"), {"white": w2, "black": b2, "games": games, "last": winner})
    post_line("result white=%s black=%s winner=%s" % (w2, b2, winner))
    return w2, b2

def post_line(content):
    net = os.path.join(BOOK, "net")
    os.makedirs(net, exist_ok=True)
    with open(os.path.join(net, "outbox.txt"), "a") as f:
        f.write("DATA|local|%s\n" % content)

def apply_move(desk, rows, evs, src, dst):
    sx, sy = src
    dx, dy = dst
    g = rows[sy][sx]
    captured = rows[dy][dx]
    rows[dy][dx] = g
    rows[sy][sx] = "."
    write_map(desk, rows)
    mover = None
    for ev in evs:
        if ev["x"] == sx and ev["y"] == sy and ev["cmd"] == "select" and ev["g"] == g:
            mover = ev
            break
    for ev in evs:
        if ev is mover:
            continue
        if ev["x"] == dx and ev["y"] == dy and captured != "." and ev["g"] == captured:
            ev["g"] = "."
            ev["cmd"] = "land"
    if mover:
        mover["x"], mover["y"] = dx, dy
    if not any(ev["x"] == sx and ev["y"] == sy for ev in evs):
        evs.append({"x": sx, "y": sy, "g": ".", "cmd": "land"})
    write_events(desk, evs)
    return captured

def all_moves(rows, color):
    moves = []
    for y, row in enumerate(rows):
        for x, g in enumerate(row):
            if side_of(g) == color:
                for dst in legal(rows, x, y):
                    moves.append(((x, y), dst, g))
    return moves

def do_select(desk, eid):
    st = read_kv(kv_path("state.pdl"))
    if st.get("result", "none") not in ("", "none"):
        return
    evs = read_events(desk)
    ev = evs[eid - 1]
    rows = read_map(desk)
    if side_of(ev["g"]) != st.get("turn", "white"):
        return
    cells = legal(rows, ev["x"], ev["y"])
    write_matrix(desk, rows, cells)
    st["selected"] = str(eid)
    st["sel_x"] = str(ev["x"])
    st["sel_y"] = str(ev["y"])
    write_kv(kv_path("state.pdl"), st)

def do_land(desk, eid):
    st = read_kv(kv_path("state.pdl"))
    if st.get("result", "none") not in ("", "none"):
        return
    if not st.get("selected"):
        return
    evs = read_events(desk)
    dest = evs[eid - 1]
    rows = read_map(desk)
    sx, sy = int(st["sel_x"]), int(st["sel_y"])
    cells = legal(rows, sx, sy)
    if (dest["x"], dest["y"]) not in cells:
        return
    captured = apply_move(desk, rows, evs, (sx, sy), (dest["x"], dest["y"]))
    clear_matrix(desk)
    st["selected"] = ""
    winner = ""
    if captured in "Kk":
        winner = "white" if captured == "k" else "black"
        st["result"] = winner
        st["turn"] = "none"
        apply_elo(winner)
    else:
        st["turn"] = "black" if st.get("turn") == "white" else "white"
    write_kv(kv_path("state.pdl"), st)
    if st.get("mode") == "computer" and st.get("turn") == "black" and not winner:
        do_computer(desk)

def do_computer(desk):
    st = read_kv(kv_path("state.pdl"))
    rows = read_map(desk)
    moves = all_moves(rows, "black")
    if not moves:
        st["result"] = "white"
        st["turn"] = "none"
        write_kv(kv_path("state.pdl"), st)
        apply_elo("white")
        return
    captures = [m for m in moves if rows[m[1][1]][m[1][0]] != "."]
    src, dst, g = random.choice(captures or moves)
    evs = read_events(desk)
    captured = apply_move(desk, rows, evs, src, dst)
    clear_matrix(desk)
    if captured == "K":
        st["result"] = "black"
        st["turn"] = "none"
        apply_elo("black")
    else:
        st["turn"] = "white"
        st["result"] = "none"
    st["selected"] = ""
    write_kv(kv_path("state.pdl"), st)

def main(argv):
    cmd = argv[1]
    if cmd == "select":
        do_select(argv[2], int(argv[3]))
    elif cmd == "land":
        do_land(argv[2], int(argv[3]))
    elif cmd == "computer":
        do_computer(argv[2])
    elif cmd == "show_elo":
        elo = read_kv(kv_path("elo.pdl"))
        print("white %s  black %s  games %s" % (elo.get("white"), elo.get("black"), elo.get("games")))
    elif cmd == "offer":
        elo = read_kv(kv_path("elo.pdl"))
        post_line("offer game=chess elo=%s" % elo.get("white", "1200"))
    else:
        sys.exit(2)

if __name__ == "__main__":
    main(sys.argv)

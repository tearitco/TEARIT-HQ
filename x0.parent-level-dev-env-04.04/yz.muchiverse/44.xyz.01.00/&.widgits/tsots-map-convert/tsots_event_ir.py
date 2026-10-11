"""RPG Maker MV event commands -> events-hq pages (event.ir.pdl + event.pal + cmd_N.sh).

A one-off converter (house rule: Python only for converters). It does what Events (hq) does when it saves a page, for the
commands that have a registry entry, so the page opens in Events (hq) and plays with play_event.sh:

  * the registry is #.ref/menu/event_commands.registry.pdl (COMMAND blocks: PARAMS, TEMPLATE, PAL). Nothing is hardcoded
    about what an op looks like; a TEMPLATE command becomes a cmd_N.sh wrapper, a PAL command becomes its pal lines.
  * if / else / end are compiled like khtpm_events_hq_manager.c does (switch read, bne to the else label).
  * MV codes with no registry command are NOT dropped: they become a `comment` node "mv <code> <params>" (visible in Events (hq)).
  * Show Choices branches (402/403/404) are kept as guarded blocks on a switch `choice_<n>` that nothing sets yet (mr_show_choices
    writes choice_result.txt, the registry `if` only reads switches). Counted as `guarded` in the stats.

Mapped codes: 101/401 Show Text, 102 Show Choices, 108/408 Comment, 111/411/412 If/Else/End (switch conditions; other condition
kinds become an `if` on a switch that is never set, so their body is skipped, not run wrongly), 115 Exit Event, 117 Call Common Event,
121 Control Switches, 123 Control Self Switch, 125 Change Gold, 201 Transfer Player, 205 Set Movement Route, 221/222 Fadeout/Fadein,
223 Tint Screen, 224 Flash, 225 Shake, 230 Wait, 250 Play SE, 301 Battle, 302 Shop, 357+ and the rest -> comment.
"""
import os
import re

REG = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "../../#.ref/menu/event_commands.registry.pdl"))
STATS = {}


def bump(k, n=1):
    STATS[k] = STATS.get(k, 0) + n


def load_registry():
    text = open(REG, encoding="utf-8").read()
    reg = {}
    for m in re.finditer(r"COMMAND (\w+)\n(.*?)\nEND", text, re.S):
        name, body = m.groups()
        p = re.search(r"^\s*PARAMS (.*)$", body, re.M)
        t = re.search(r"^\s*TEMPLATE (.*)$", body, re.M)
        pal = re.findall(r"^\s*PAL (.*)$", body, re.M)
        reg[name] = {"params": [x.strip() for x in p.group(1).split(",")] if p else [],
                     "template": t.group(1) if t else None, "pal": pal}
    return reg


REGISTRY = load_registry()


def expand(tpl, fields):
    """{key} substitution; [ ... {key}] is dropped whole when that value is empty (registry syntax)."""
    def opt(m):
        seg = m.group(1)
        k = re.search(r"\{(\w+)\}", seg)
        if k and str(fields.get(k.group(1), "")) == "":
            return ""
        return seg
    tpl = re.sub(r"\[([^\]]*\{\w+\}[^\]]*)\]", opt, tpl)
    return re.sub(r"\{(\w+)\}", lambda m: str(fields.get(m.group(1), "")), tpl)


def sh_q(s):
    return "'" + str(s).replace("'", "'\\''") + "'"


def node(t, **f):
    return (t, {k: str(v) for k, v in f.items()})


def comment(code, params):
    bump("comment")
    bump("comment_mv%d" % code)
    short = " ".join(str(p)[:40] for p in params[:4]).replace("|", "/").replace("\n", " ")
    return node("comment", text="mv %d %s" % (code, short))


def text_of(cmds, i):
    bits, j = [], i + 1
    while j < len(cmds) and int(cmds[j].get("code") or 0) == 401:
        p = cmds[j].get("parameters") or [""]
        bits.append(str(p[0] if p else ""))
        j += 1
    return " ".join(b for b in bits if b).strip(), j


def convert(cmds):
    """list of MV commands -> list of (type, fields) nodes."""
    out, i = [], 0
    choice_stack = []          # open Show Choices blocks: next branch index guard
    open_choice = [False]      # a guarded choice branch is open (its `end` is still to write)
    while i < len(cmds):
        c = cmds[i]
        code = int(c.get("code") or 0)
        p = c.get("parameters") or []
        i += 1
        if code == 0:
            continue
        if code == 101:
            txt, i = text_of(cmds, i - 1)
            out.append(node("show_text", text=txt or "(empty)", speaker=""))
            bump("show_text")
        elif code == 102:
            ch = p[0] if p and isinstance(p[0], list) else []
            out.append(node("show_choices", choices="|".join(str(x).replace("|", "/") for x in ch), default="0"))
            choice_stack.append(0)
            bump("show_choices")
        elif code in (402, 403):
            if choice_stack:
                n = int(p[0]) if (code == 402 and p) else 9
                if open_choice[0]:
                    out.append(node("end"))
                out.append(node("if", switch_name="choice_%d" % n, compare="on"))
                open_choice[0] = True
                bump("guarded")
        elif code == 404:
            if open_choice[0]:
                out.append(node("end"))
                open_choice[0] = False
            if choice_stack:
                choice_stack.pop()
        elif code in (108, 408):
            out.append(node("comment", text=str(p[0] if p else "")[:120].replace("|", "/")))
            bump("comment_native")
        elif code == 111:
            if p and p[0] == 0:
                out.append(node("if", switch_name="S%s" % p[1], compare="on" if (len(p) < 3 or p[2] == 0) else "off"))
                bump("if")
            else:
                out.append(node("if", switch_name="unsupported_cond_%s" % (p[0] if p else "?"), compare="on"))
                bump("if_unsupported")
        elif code == 411:
            out.append(node("else"))
        elif code == 412:
            out.append(node("end"))
        elif code == 115:
            out.append(node("exit_event"))
            bump("exit_event")
        elif code == 117:
            out.append(node("call_common_event", event_name="common_%s" % (p[0] if p else "0"), trigger=""))
            bump("call_common_event")
        elif code == 121:
            a, b, v = (p + [0, 0, 0])[:3]
            for sid in range(int(a), int(b) + 1):
                out.append(node("control_switch", switch_name="S%d" % sid, switch_value="1" if int(v) == 0 else "0"))
            bump("control_switch")
        elif code == 122:
            a, b, op, otype, v = (p + [0, 0, 0, 0, 0])[:5]
            if int(op) == 0 and int(otype) == 0:
                for vid in range(int(a), int(b) + 1):
                    out.append(node("control_variable", var_name="V%d" % vid, var_value=str(int(v))))
                bump("control_variable")
            else:
                out.append(comment(code, p))
        elif code == 126:
            iid, op, otype, v = (p + [0, 0, 0, 0])[:4]
            if int(otype) == 0:
                out.append(node("change_items", item_id=str(iid), amount=("-" if int(op) == 1 else "") + str(abs(int(v)))))
                bump("change_items")
            else:
                out.append(comment(code, p))
        elif code == 123:
            out.append(node("control_self_switch", letter=str(p[0]) if p else "A", value="1" if (len(p) > 1 and int(p[1]) == 0) else "0"))
            bump("control_self_switch")
        elif code == 125:
            op, otype, val = (p + [0, 0, 0])[:3]
            if int(otype) == 0:
                out.append(node("change_gold", amount=("-" if int(op) == 1 else "") + str(abs(int(val)))))
                bump("change_gold")
            else:
                out.append(comment(code, p))
        elif code == 201:
            if p and int(p[0]) == 0:
                out.append(node("transfer_player", map="map%03d" % int(p[1]), xy="%s,%s" % (p[2], p[3])))
                bump("transfer_player")
            else:
                out.append(comment(code, p))
        elif code == 205:
            rc = c.get("parameters") or []
            n = 0
            while i < len(cmds) and int(cmds[i].get("code") or 0) == 505:
                n += 1
                i += 1
            out.append(node("set_move_route", target=str(rc[0] if rc else 0), route="%d steps" % n))
            bump("set_move_route")
        elif code == 221:
            out.append(node("fadeout_screen", frames="30")); bump("fadeout_screen")
        elif code == 222:
            out.append(node("fadein_screen", frames="30")); bump("fadein_screen")
        elif code == 223:
            tone = p[0] if p and isinstance(p[0], list) else [0, 0, 0, 0]
            out.append(node("tint_screen", tint=",".join(str(int(x)) for x in tone), frames=str(p[1] if len(p) > 1 else 60)))
            bump("tint_screen")
        elif code == 224:
            col = p[0] if p and isinstance(p[0], list) else [255, 255, 255, 170]
            out.append(node("flash_screen", color=",".join(str(int(x)) for x in col), frames=str(p[1] if len(p) > 1 else 30)))
            bump("flash_screen")
        elif code == 225:
            out.append(node("shake_screen", power=str(p[0] if p else 5), frames=str(p[2] if len(p) > 2 else 30)))
            bump("shake_screen")
        elif code == 230:
            out.append(node("wait", ms=str(int((int(p[0]) if p else 60) * 1000 / 60))))
            bump("wait")
        elif code == 250:
            se = p[0] if p and isinstance(p[0], dict) else {}
            out.append(node("play_se", path=str(se.get("name", ""))))
            bump("play_se")
        elif code == 301:
            out.append(node("battle_processing", troop=str(p[1] if len(p) > 1 else 0), can_escape=str(p[2] if len(p) > 2 else 0)))
            bump("battle_processing")
        elif code == 302:
            out.append(node("shop_processing", goods=str(p[:4])))
            bump("shop_processing")
        elif code in (401, 405, 505, 605, 655):
            continue            # text / scroll text / route / script continuation lines: consumed with their head
        else:
            out.append(comment(code, p))
    if open_choice[0]:
        out.append(node("end"))
    return out


def write_page(page_dir, ev_name, nodes, state_dir):
    """event.ir.pdl, event.pal and cmd_N.sh from the nodes, like the events-hq compiler."""
    for f in os.listdir(page_dir):
        if f.startswith("cmd_") or f in ("event.ir.pdl", "event.pal"):
            os.remove(os.path.join(page_dir, f))
    ir = ["SECTION      | KEY                | VALUE\n----------------------------------------\n",
          "META         | piece_id           | %s\n" % ev_name, "STATE        | source               | events-hq\n"]
    pal = ["# event.pal - COMPILED from event.ir.pdl (tsots_event_ir.py, shape of khtpm_events_hq_manager.c output)\n"]
    stack, lc = [], 0
    for nid, (t, f) in enumerate(nodes, start=1):
        spec = REGISTRY.get(t)
        params = "|".join("%s=%s" % (k, f.get(k, "")) for k in (spec["params"] if spec else f.keys()))
        ir.append("NODE         | id=%d type=%s | %s\n" % (nid, t, params))
        if t == "if":
            lc += 1
            fr = {"end": "_endif_%d" % lc, "else": "_else_%d" % lc, "has_else": False}
            stack.append(fr)
            pal += ["li x15, 6\n", 'ecall "%s/switches.txt" "%s"\n' % (state_dir, f.get("switch_name", "")),
                    "li x2, %s\n" % ("1" if f.get("compare") == "on" else "0"), "bne x12, x2, %s\n" % fr["else"]]
            continue
        if t == "else":
            if stack:
                pal += ["j %s\n" % stack[-1]["end"], "%s:\n" % stack[-1]["else"]]
                stack[-1]["has_else"] = True
            continue
        if t == "end":
            if stack:
                fr = stack.pop()
                if not fr["has_else"]:
                    pal.append("%s:\n" % fr["else"])
                pal.append("%s:\n" % fr["end"])
            continue
        if t in ("comment", "exit_event", "label", "jump_to_label", "loop", "break_loop"):
            if t == "exit_event":
                pal.append("halt\n")
            continue
        if spec and spec["template"]:
            body = ("#!/bin/sh\ncd \"$(dirname \"$0\")/../../..\" || exit 1\nENT=\"${MUCHI_TARGET_ENT:-$PWD}\"\nD=\"$ENT\"\n"
                    "while [ \"$D\" != \"/\" ] && [ ! -d \"$D/xyzfs\" ]; do D=\"$(dirname \"$D\")\"; done\n" + expand(spec["template"], {k: v.replace("'", "'\\''") for k, v in f.items()}) + "\n")
            path = os.path.join(page_dir, "cmd_%d.sh" % nid)
            open(path, "w", encoding="utf-8").write(body)
            os.chmod(path, 0o755)
            pal.append("exec cmd_%d.sh\n" % nid)
        elif spec and spec["pal"]:
            for line in spec["pal"]:
                pal.append(expand(line, dict(f, STATE_DIR=state_dir)) + "\n")
        else:
            pal.append("# (no registry command for %s)\n" % t)
    while stack:
        fr = stack.pop()
        if not fr["has_else"]:
            pal.append("%s:\n" % fr["else"])
        pal.append("%s:\n" % fr["end"])
    pal.append("halt\n")
    open(os.path.join(page_dir, "event.ir.pdl"), "w", encoding="utf-8").writelines(ir)
    open(os.path.join(page_dir, "event.pal"), "w", encoding="utf-8").writelines(pal)


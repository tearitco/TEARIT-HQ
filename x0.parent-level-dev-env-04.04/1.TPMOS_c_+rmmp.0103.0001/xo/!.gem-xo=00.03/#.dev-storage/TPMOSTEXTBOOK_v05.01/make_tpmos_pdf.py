import os
import re
from fpdf import FPDF
from fpdf.enums import XPos, YPos

# CONFIGURATION
SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
BASE_DIR = SCRIPT_DIR
OUTPUT_PDF = os.path.join(SCRIPT_DIR, "TPMOS_TEXTBOOK_v05.00.pdf")

# PATH TO FONTS
LATIN_FONT = "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf"
LATIN_FONT_BOLD = "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf"
CJK_FONT = "/usr/share/fonts/truetype/droid/DroidSansFallbackFull.ttf"

# EMOJI SUBSTITUTION MAPPING
EMOJI_MAP = {
    "📚": "[B 书]", "📖": "[B 阅]", "🎓": "[GRAD 学]", "🚀": "[LAUNCH 发]", "🌌": "[COSMOS 宇]",
    "🌀": "[REC 归]", "🏛": "[BLD 馆]", "🏗": "[BLD 建]", "🤖": "[BOT 机]", "🧬": "[DNA 命]",
    "🔬": "[SCI 究]", "🧪": "[LAB 验]", "🌍": "[EARTH 地]", "🌎": "[EARTH 球]", "🛡": "[SEC 安]",
    "⛓": "[NET 链]", "🖥": "[CPU 算]", "🔋": "[PWR 电]", "🧠": "[MIND 脑]", "🎭": "[VIEW 戏]",
    "💰": "[$ 钱]", "💎": "[VAL 宝]", "⚖": "[LAW 法]", "💼": "[BIZ 业]", "📈": "[MKT↑ 升]",
    "📉": "[MKT↓ 降]", "📣": "[PR 宣]", "🤝": "[NET 协]", "🪄": "[PEN 笔]", "💓": "[PULSE 心]",
    "🛠": "[TOOL 工]", "⚙": "[TOOL 齿]", "🎨": "[GL 艺]", "📝": "[DOC 记]", "📋": "[DOC 单]",
    "📺": "[TERM 视]", "📟": "[TERM 显]", "🗺": "[MAP 图]", "🚪": "[EXIT 门]", "⚔": "[WAR 战]",
    "🧘‍♂️": "[ZEN 禅]", "🌟": "[* 星]", "✨": "[* 闪]", "🕯": "[IMM 烛]", "🍼": "[KISS 奶]",
    "⚡": "[FAST 快]", "🌓": "[MODE 模式]", "🏃": "[SPRINT 跑]", "👣": "[PATH 迹]", "🏭": "[APP 厂]",
    "🐾": "[PET 爪]", "👤": "[USER 人]", "💨": "[FAST 速]", "🌉": "[BRIDGE 桥]", "👻": "[GHOST 鬼]",
    "🏘": "[COM 村]", "🌑": "[LUNA 阴]", "🌕": "[LUNA 阳]", "🛰": "[EXP 卫]", "📐": "[GEOM 角]",
    "🏁": "[GO 终]", "🎰": "[LUCK 赌]", "🐠": "[SEA 鱼]", "✅": "[OK 是]", "❌": "[FAIL 错]",
    "💡": "[TIPS 明]", "⚠": "[WARN 警]", "♾": "[INF 无]", "🧹": "[CLEAN 洁]", "💤": "[IDLE 眠]",
    "😴": "[IDLE 睡]", "🚫": "[NO 禁]", "🧨": "[FIRE 炮]", "🏢": "[CORP 司]", "⛩": "[GATE 门]",
    "🖇": "[LINK 连]", "🔗": "[LINK 接]", "👁": "[EYE 眼]", "✎": "[PEN 笔]", "⚐": "[GO 旗]",
    "◑": "[MODE 态]", "◎": "[REC 圆]", "⌂": "[BLD 宅]", "☤": "[DNA 医]", "⚗": "[SCI 壶]",
    "⊕": "[EARTH 环]", "⛨": "[SEC 盾]", "⌨": "[CPU 键]", "☍": "[IDLE 连]", "⎚": "[TERM 屏]",
    "⌗": "[MAP 井]", "☯": "[ZEN 道]", "∞": "[INF 穷]", "☩": "[CLEAN 十]", "⦸": "[NO 止]",
    "⎗": "[OUT 出]", "⊿": "[GEOM 形]", "⚄": "[LUCK 点]", "🐟": "[SEA 鱼]", "🪲": "[BUG 虫]",
    "☑": "[OK 对]", "☒": "[FAIL 误]", "📢": "[PR 喇]", "🕮": "[B 书]", "🪞": "[MIRROR 镜]",
    "🧱": "[PIECE 砖]"
}

def clean_emojis(text):
    if not text: return ""
    for emoji, sub in EMOJI_MAP.items():
        text = text.replace(emoji, sub)
    return text

class TPMOS_PDF(FPDF):
    def header(self):
        if self.page_no() > 1:
            self.set_font("Main", "B", 10)
            self.set_text_color(100, 100, 100)
            header_text = clean_emojis("📚 TPMOS TEXTBOOK v05.00 🎓  ")
            self.cell(0, 10, header_text, align="R", new_x=XPos.LMARGIN, new_y=YPos.NEXT)
            self.ln(5)

    def footer(self):
        self.set_y(-15)
        self.set_font("Main", "", 8)
        self.set_text_color(150, 150, 150)
        self.cell(0, 10, f"Page {self.page_no()}", align="C")

    def add_code_block(self, code_text):
        """Add a code block with monospace font and line-by-line background"""
        self.set_font("Main", "", 8)
        self.set_fill_color(240, 240, 240)
        self.set_draw_color(220, 220, 220)
        self.set_text_color(20, 20, 120)

        lines = code_text.split('\n')
        line_height = 4
        
        # Draw start of block
        self.ln(2)
        
        for line in lines:
            clean_line = clean_emojis(line).replace('\t', '    ')
            # Handle extremely long lines
            if len(clean_line) > 110:
                clean_line = clean_line[:107] + "..."
            
            # Check for page break before each line
            if self.get_y() + line_height > 275:
                self.add_page()
                # Repeat formatting after page break
                self.set_font("Main", "", 8)
                self.set_fill_color(240, 240, 240)
                self.set_text_color(20, 20, 120)

            # Draw line with fill
            self.cell(0, line_height, clean_line, fill=True, new_x=XPos.LMARGIN, new_y=YPos.NEXT)

        self.set_text_color(0, 0, 0)
        self.ln(2)

def create_pdf():
    pdf = TPMOS_PDF()
    pdf.add_font("Main", "", LATIN_FONT)
    pdf.add_font("Main", "B", LATIN_FONT_BOLD)
    pdf.add_font("CJK", "", CJK_FONT)
    pdf.set_fallback_fonts(["CJK"])
    pdf.set_font("Main", size=11)
    pdf.set_auto_page_break(auto=True, margin=15)

    # 1. COVER PAGE
    pdf.add_page()
    pdf.set_font("Main", "B", 24)
    pdf.ln(60)
    pdf.cell(0, 20, clean_emojis("📚 TPMOS_TEXTBOOK 🎓"), align="C", new_x=XPos.LMARGIN, new_y=YPos.NEXT)
    pdf.set_font("Main", "B", 16)
    pdf.cell(0, 10, clean_emojis("The Definitive Guide to the Mono-OS (基于件的系统指南)"), align="C", new_x=XPos.LMARGIN, new_y=YPos.NEXT)
    pdf.ln(10)
    pdf.set_font("Main", "", 12)
    pdf.cell(0, 10, "Version 05.00 (Exo-Sovereignty Edition)", align="C", new_x=XPos.LMARGIN, new_y=YPos.NEXT)
    pdf.ln(80)
    pdf.set_font("Main", "", 10)
    pdf.cell(0, 10, clean_emojis("Softness wins. The empty center of the flexbox holds ten thousand things. 🧘‍♂️"), align="C", new_x=XPos.LMARGIN, new_y=YPos.NEXT)

    # 2. TOC PAGE
    index_path = os.path.join(BASE_DIR, "INDEX.md")
    with open(index_path, "r", encoding="utf-8") as f:
        index_content = f.read()

    pdf.add_page()
    pdf.set_font("Main", "B", 18)
    pdf.cell(0, 10, clean_emojis("📋 Table of Contents (目录)"), align="L", new_x=XPos.LMARGIN, new_y=YPos.NEXT)
    pdf.ln(5)
    pdf.set_font("Main", "", 11)

    index_lines = index_content.split("\n")
    for line in index_lines:
        line = line.strip()
        if not line or line.startswith("# ") or "Dependency Graph" in line or "```" in line or "graph TD" in line:
            continue
        if "subgraph" in line or "style" in line: continue

        match = re.search(r"\[(.*?)\]", line)
        if match:
            title = clean_emojis(match.group(1))
            pdf.set_x(pdf.l_margin + 5)
            pdf.multi_cell(0, 7, f"• {title}", new_x=XPos.LMARGIN, new_y=YPos.NEXT)
        elif line.startswith("### "):
            pdf.ln(2)
            pdf.set_font("Main", "B", 12)
            pdf.cell(0, 10, clean_emojis(line[4:]), new_x=XPos.LMARGIN, new_y=YPos.NEXT)
            pdf.set_font("Main", "", 11)

    chapters = re.findall(r"\[(.*?)\]\((.*?\.md)\)", index_content)

    # 3. ADD CHAPTERS
    for title, filename in chapters:
        if filename == "INDEX.md": continue
        file_path = os.path.join(BASE_DIR, filename)
        if not os.path.exists(file_path): continue

        pdf.add_page()
        with open(file_path, "r", encoding="utf-8") as f:
            content = f.read()

        lines = content.split("\n")
        in_code_block = False
        code_buffer = []
        for line in lines:
            if line.startswith("```"):
                if in_code_block:
                    pdf.add_code_block('\n'.join(code_buffer))
                    code_buffer = []
                    in_code_block = False
                else:
                    in_code_block = True
                continue

            if in_code_block:
                code_buffer.append(line)
                continue

            line = line.strip()
            if not line:
                pdf.ln(4)
                continue

            if line.startswith("---"):
                pdf.ln(1)
                pdf.line(pdf.l_margin, pdf.get_y(), 210 - pdf.r_margin, pdf.get_y())
                pdf.ln(1)
                continue

            clean_line = clean_emojis(line)
            if line.startswith("# "):
                pdf.set_font("Main", "B", 16)
                pdf.multi_cell(0, 9, clean_line[2:], new_x=XPos.LMARGIN, new_y=YPos.NEXT)
                pdf.ln(4)
            elif line.startswith("## "):
                pdf.set_font("Main", "B", 14)
                pdf.multi_cell(0, 8, clean_line[3:], new_x=XPos.LMARGIN, new_y=YPos.NEXT)
                pdf.ln(2)
            elif line.startswith("### "):
                pdf.set_font("Main", "B", 12)
                pdf.multi_cell(0, 7, clean_line[4:], new_x=XPos.LMARGIN, new_y=YPos.NEXT)
                pdf.ln(1)
            elif line.startswith("* "):
                pdf.set_font("Main", "", 11)
                pdf.multi_cell(0, 6, f"  • {clean_line[2:]}", new_x=XPos.LMARGIN, new_y=YPos.NEXT)
            elif line.startswith("> "):
                pdf.set_font("Main", "B", 10)
                pdf.set_text_color(80, 80, 80)
                pdf.multi_cell(0, 6, clean_line[2:], new_x=XPos.LMARGIN, new_y=YPos.NEXT)
                pdf.set_text_color(0, 0, 0)
            else:
                pdf.set_font("Main", "", 11)
                pdf.multi_cell(0, 6, clean_line.replace("**", ""), new_x=XPos.LMARGIN, new_y=YPos.NEXT)

    # 4. SAVE PDF
    print(f"Generating {OUTPUT_PDF}...")
    pdf.output(OUTPUT_PDF)
    print("Done! ✅")

if __name__ == "__main__":
    create_pdf()

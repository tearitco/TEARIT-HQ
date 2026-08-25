# 🏗️ Emoji Studio Architecture Blueprint 🎨

Welcome to the **Emoji Studio**! This document is a high-level "brain-dump" 🧠 of how this voxel-powered emoji factory is built. It's designed to be modular, efficient, and slightly chaotic (in a good way). 🚀

---

## 🛰️ The Big Picture: Two Worlds Colliding

The system is split into two distinct processes that talk to each other through the file system. 🤝

1.  **The Host (`emoji-studio_host`)**: The "Brain" 🧠. It handles the UI, the 3D rendering, and tells the Muscle what to do.
2.  **The Extractor (`emoji-xtract`)**: The "Muscle" 💪. A standalone tool that takes a giant PNG atlas and carves out tiny voxel CSVs.

---

## 🧠 The Host: Command & Control

The Host is a **C/OpenGL/GLUT** application. It's built on a "State Machine" model. ⚙️

### 1. 📋 Emoji Registry
-   **Data Source**: `parsed_emojis.txt`.
-   **Mechanism**: On startup, the Host parses this list to map **Codepoints** (Hex) ➡️ **Names** ➡️ **Atlas Indices**.
-   **Memory**: Stored in a fixed-size array (`MAX_EMOJIS = 4000`) for fast lookup.

### 2. 🎨 2D Layer: The Emoji Picker
-   **Technology**: `FreeType` + `OpenGL Textures`. 🔡
-   **Rendering**: 
    -   Loads `NotoColorEmoji.ttf`. 🌈
    -   Uses `FT_LOAD_COLOR` to get raw **BGRA** bitmaps.
    -   **Texture Caching**: We only render what's on screen! We use a "Slot Cache" (`PICKER_COLS * PICKER_ROWS`) to store OpenGL textures. When you scroll, we reuse the slots! ♻️

### 3. 🧊 3D Layer: The Voxel Engine
-   **Projection**: 45° Perspective view. 📐
-   **Rendering Strategy**: 
    -   Reads an $N \times N$ CSV file.
    -   **Extrusion**: For every pixel in the 2D CSV, it draws a 3D column (a cube scaled on the Y-axis). 🧱
    -   **Coordinate Math**: Everything is centered. If the resolution is 64, voxels are small. If resolution is 8, voxels are CHUNKY. But the overall model size is always a $2.0 \times 2.0 \times 2.0$ OpenGL volume. 📦

---

## 💪 The Extractor: The Data Processor

This is a surgical tool. You give it an index, and it gives you a voxel grid. ✂️

### 1. 📂 Input: The Atlas
-   Loads `emoji_atlas.png` (a 1D strip of 64x64 emojis). 🎞️
-   Calculates `x_offset = index * 64`.

### 2. 📉 Downsampling: The "Simplifier"
-   It takes the 64x64 original emoji and "crushes" it down to $N \times N$ ($8, 16, 32,$ or $64$).
-   **Box Sampling**: It averages the colors (RGBA) in a grid to ensure transparency looks good even at low resolutions. 🧪

### 3. 📄 Output: The Voxel CSV
-   Writes a file with a metadata header (Resolution, Scale, Transform).
-   Followed by lines of `r,g,b,a` data. 📝

---

## 🔄 The Flow: "The Voxel Pipeline" 🎢

1.  **Selection**: User clicks an emoji in the 2D Picker. 🖱️
2.  **Orchestration**: Host checks if `pieces/<name>/voxels_<res>.csv` exists. 🔍
3.  **Step 1: Mini-Atlas Generation**: If missing, Host calls `emoji-gen-atlas`.
    -   `./emoji-gen-atlas <symbol> <pieces/name/mini_atlas.png>`
    -   This creates a 64x64 PNG of just that emoji using FreeType. 🖌️
4.  **Step 2: Voxel Extraction**: Host calls `emoji-xtract` on the *mini-atlas*.
    -   `./emoji-xtract <mini_atlas.png> 0 <res> <voxels.csv>`
    -   The index is always 0 because the atlas only has one emoji! 🎯
5.  **Load**: Host reads the fresh CSV and populates its `voxel_data` buffer. 📥
6.  **Display**: The 3D loop sees new data and renders the voxel model! ✨

---

## 🛠️ Data Structures for the Future

If you're rewriting this in **Assembly** or a lower-level language, keep these in mind:

-   **`RGBA_Pixel`**: 4 bytes (Red, Green, Blue, Alpha). Keep it packed! 🎨
-   **`EmojiEntry`**: A struct with Fixed-length strings for Names and Codepoints. 📑
-   **`Camera`**: Floats for `rotX`, `rotY`, and `zoom`. 🎥

## 📁 File System Layout
-   `/pieces/` : Persistent cache of generated voxel grids. 💾
-   `parsed_emojis.txt` : The Source of Truth (Emoji List). 📚

---

**Stay Voxelated!** 🧊💎

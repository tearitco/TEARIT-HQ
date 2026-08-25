# Emoji Studio Standalone Demo

This is a standalone, portable version of the Emoji Studio application. It operates using the **True Piece Method (TPM)**, decoupling the state manager from the graphical renderer.

## 📂 Project Structure
- `emoji-studio_host.c`: The graphical renderer (OpenGL/GLUT). Handles 2D UI and 3D voxel extrusion.
- `manager/emoji-studio_manager.c`: The state master. Orchestrates extraction via standalone Ops.
- `emoji-xtract.c`: The standalone Op. Extracts pixels from the atlas and generates `voxels.csv`.
- `lib/`: Contains the `stb_image` header libraries.
- `session/`: Runtime command history and pulse markers.
- `pieces/`: Persistent storage for extracted emoji voxel data.

## 🛠 Compilation
You can compile all components using the following commands:

```bash
# Compile the Op
gcc -O2 emoji-xtract.c -o emoji-xtract -lm

# Compile the Manager
gcc -O2 manager/emoji-studio_manager.c -o manager/emoji-studio_manager

# Compile the Host
gcc -O2 emoji-studio_host.c -o emoji-studio_host -lGL -lGLU -lglut -lm
```

## 🚀 Running the Demo
1. **Start the Manager:** (Runs in the background)
   ```bash
   ./manager/emoji-studio_manager &
   ```
2. **Start the Host:**
   ```bash
   ./emoji-studio_host
   ```

## 🎮 Controls
- **Mouse Click:**
  - Click an emoji in the left sidebar to extract and view it in 3D.
  - Click resolution buttons (8, 16, 32, 64) to change voxel density.
- **Keyboard (3D Camera):**
  - `W` / `S`: Zoom In / Out
  - `A` / `D`: Rotate Yaw
  - `Q` / `E`: Rotate Pitch

## 🧩 TPM Principles
- **Soul (Piece):** Every emoji is a directory in `pieces/`.
- **Brain (Manager):** The manager doesn't render; it writes `gui_state.txt`.
- **TV (Host):** The host doesn't know "why" pixels change; it just draws what `gui_state.txt` and `voxels.csv` describe.

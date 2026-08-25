# Chatbot Web Interface

## Overview
This project provides a web interface for the C-based LLM chatbot. The interface allows users to interact with the chatbot through a browser, providing configuration options and real-time response display.

## Components

### Frontend
- `index.php` - Main HTML interface with Bootstrap styling
- `css/style.css` - Custom CSS styling
- `js/app.js` - JavaScript for API communication and UI interactions

### Backend
- `api.php` - PHP API endpoints to interface with the C chatbot

## Features

1. **Configuration Panel**
   - Curriculum file selection
   - Response length control (1-100 tokens)
   - Temperature control (0.1-10.0)

2. **Chat Interface**
   - Prompt input with multi-line support
   - Real-time response display with typing effect
   - Clear chat functionality

3. **Debug Information**
   - Show debug logs from `debug_chain.txt`
   - Real-time debug information

## Setup

1. Ensure the C chatbot is compiled:
   ```bash
   cd +x && bash xsh.compile-all.+x]🔘️.sh
   ```

2. Make sure `vocab_model.txt` or other curriculum files exist in the root directory

3. Start a PHP development server:
   ```bash
   php -S localhost:8000
   ```

4. Navigate to `http://localhost:8000/web-interface/`

## API Endpoints

- `GET /api/curricula` - Get list of available curriculum files
- `POST /api/chat` - Execute chatbot with given parameters
- `GET /api/debug` - Get debug information

## Usage

1. Select a curriculum file (e.g., `vocab_model.txt`)
2. Enter your prompt in the input field
3. Adjust response length and temperature as needed
4. Click "Generate Response"
5. View the chatbot response in real-time
6. Use "Show Debug" to view detailed debug information

## File Structure

```
web-interface/
├── index.php          # Main HTML interface
├── api.php            # PHP API backend
├── css/
│   └── style.css      # Styling
└── js/
    └── app.js         # JavaScript functionality
```

## Dependencies

- PHP 7.0+ with `exec()` and `popen()` enabled
- GCC compiler (for fallback C compilation if needed)
- Web browser with JavaScript enabled

## Troubleshooting

- If you get "Failed to execute chatbot" error, verify that:
  - The `+x/chatbot_moe_v1.+x` executable exists and is executable
  - The selected curriculum file exists
  - PHP has permission to execute shell commands

- If CSS/JS files are not loading, check file permissions and paths

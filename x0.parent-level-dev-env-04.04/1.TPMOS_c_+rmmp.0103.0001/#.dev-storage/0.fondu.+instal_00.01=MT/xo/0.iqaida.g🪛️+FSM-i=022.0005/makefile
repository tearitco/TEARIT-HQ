CC = gcc
CFLAGS = -std=c11 -Wall -Wextra -O2 -D_DEFAULT_SOURCE -pthread
TOOL_DIR = tools
TOOLS = $(TOOL_DIR)/json_state $(TOOL_DIR)/cmd_exec $(TOOL_DIR)/json_parser $(TOOL_DIR)/file_ops \
        $(TOOL_DIR)/list_dir $(TOOL_DIR)/search_in_files $(TOOL_DIR)/edit_file $(TOOL_DIR)/web_search \
        $(TOOL_DIR)/complete_path $(TOOL_DIR)/gemini_payload_builder

all: agent $(TOOLS)

agent: agent.c
	$(CC) $(CFLAGS) -o $@ $<

$(TOOL_DIR)/%: $(TOOL_DIR)/%.c
	$(CC) $(CFLAGS) -o $@ $<

clean:
	rm -f agent $(TOOLS)
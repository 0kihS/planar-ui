---
name: planar
description: Interact with the planar infinite-canvas Wayland compositor — query windows, navigate, arrange, spawn apps, manage groups
category: desktop
---

# Planar Compositor Tools

Use the planar compositor helper module to interact with the user's canvas. Import it in execute_code:

```python
import sys; sys.path.insert(0, os.path.expanduser("~/.hermes"))
from planar import *
```

## When to Use

- User asks "what windows are open" or "show me the desktop"
- User wants you to open/arrange/focus windows
- User says "go to the firefox window" or "what's on screen"
- Any task involving window management, workspace navigation, or spawning apps

## Event Log (start this first!)

The event log daemon streams compositor events to a JSONL file. Start it once per session:
```python
start_event_logger()      # Start daemon (idempotent)
event_log_status()        # Check if running
stop_event_logger()       # Stop daemon

# Read events
read_events(limit=20)           # Last 20 events
read_events(since=last_id)      # Only new events
read_events(event="window_focus") # Filter by type
last_event_id()                 # Latest event ID
```

Event types: `window_open`, `window_close`, `window_focus`, `workspace`

## Quick Reference

### Queries (non-destructive)
```python
overview()          # Human-readable canvas summary
get_windows()       # List all windows
get_workspaces()    # Workspace info
get_focused()       # Currently focused window
find_window("ff")   # Search by title/app_id substring
```

### Navigation
```python
goto_window(id)         # Pan canvas + focus window
focus_window(id)        # Focus without panning
switch_workspace(n)     # Switch to workspace (1-based)
```

### Window Management
```python
move_window(id, x, y)           # Move to coordinates
resize_window(id, w, h)         # Resize
move_window_to_workspace(id, n) # Move window to workspace
close_window(id)                # Close specific window
send_keys(id, "text")           # Send keystrokes
```

### Spawning
```python
spawn("firefox")        # Launch app on canvas
```

### Screen Capture + Vision
```python
result = capture_screen()           # ~50KB JPEG, SCPs to VPS
# Then use vision_analyze(result["vps_path"], "What's on screen?") to see it
```

### Groups
```python
group_create("name")            # Create group
group_add(group_id, window_id)  # Add window to group
group_move(group_id, x, y)      # Move entire group
group_color(group_id, r, g, b)  # Set border color
```

### Selection
```python
select_add(id)    # Add to selection
select_toggle(id) # Toggle in/out
group_create_from_selection() # Group selected windows
```

## Response Format

All `get_*` functions return parsed data. Movement/management functions return `{"ok": true/false}`.

## Workflow Patterns

1. **"Show me what's open"** → call `overview()`, summarize for user
2. **"Go to X"** → call `find_window("X")`, then `goto_window(id)`
3. **"Arrange my windows"** → query with `get_windows()`, calculate positions, use `move_window()` calls
4. **"Open X and put it on workspace 2"** → `spawn("X")`, then `move_window_to_workspace(id, 2)`

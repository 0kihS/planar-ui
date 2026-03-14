# Planar

An infinite-canvas Wayland compositor with an integrated AI agent panel.

Planar implements a zoomable/pannable workspace where windows exist on an infinite 2D canvas. The agent UI panel connects to a [Hermes Agent](https://github.com/NousResearch/hermes-agent) instance over WebSocket, giving you an AI assistant that can see, navigate, and manage your desktop.

https://github.com/user-attachments/assets/fb8c2155-425d-4ef5-ab65-1374d1667eef

## Components

| Directory | What | Language |
|---|---|---|
| `compositor/` | Wayland compositor built on wlroots-0.19 + scenefx | C |
| `ui/` | GTK4 layer-shell agent panel (chat, window list, status) | C |
| `agent/` | Hermes Agent platform adapter + skill for compositor control | Python |

## Building

### Dependencies

**Compositor:**
- wlroots (0.19.x)
- wayland-server, wayland-protocols
- xkbcommon
- meson, ninja

**Agent UI:**
- gtk4
- gtk4-layer-shell
- webkitgtk-6.0
- libsoup-3.0
- json-glib-1.0
- meson, ninja

<details>
<summary>Arch Linux</summary>

```bash
# Compositor
sudo pacman -S wlroots wayland wayland-protocols libxkbcommon meson ninja

# Agent UI
sudo pacman -S gtk4 gtk4-layer-shell webkitgtk-6.0 libsoup3 json-glib
```
</details>

### Compositor

```bash
cd compositor
meson setup build
meson compile -C build
sudo meson install -C build
```

This installs the `planar` compositor and the `planarctl` CLI tool.

### Agent UI

```bash
cd ui
meson setup build
meson compile -C build
sudo meson install -C build
```

This installs `planar-agent-ui`, which runs as a layer-shell overlay on top of the compositor.

## Agent Setup

The agent panel communicates with a [Hermes Agent](https://github.com/NousResearch/hermes-agent) instance over WebSocket. You need a running Hermes gateway with the Planar platform adapter enabled.

### Quick setup (if you already have Hermes installed)

```bash
cd agent
./install.sh
```

This copies the platform adapter and skill into your Hermes installation.

### Manual setup

1. **Install Hermes Agent** following the [upstream instructions](https://github.com/NousResearch/hermes-agent).

2. **Copy the platform adapter** into Hermes:
   ```bash
   cp agent/platforms/planar.py <hermes-agent>/gateway/platforms/
   ```

3. **Install the Planar skill:**
   ```bash
   mkdir -p ~/.hermes/skills/planar
   cp agent/skills/planar/SKILL.md ~/.hermes/skills/planar/
   ```

4. **Enable the platform** in your Hermes `config.yaml`:
   ```yaml
   platforms:
     planar:
       enabled: true
   ```

5. **Install the websockets dependency:**
   ```bash
   pip install websockets
   ```

6. Start the Hermes gateway. The Planar adapter listens on `ws://0.0.0.0:5000` by default (configurable via `PLANAR_WS_PORT` and `PLANAR_WS_HOST` environment variables).

### Connecting the UI

The agent UI connects to `ws://<host>:5050` by default. If your Hermes instance runs on a remote server, make sure the WebSocket port is reachable. The connection URL is set in `ui/ipc/agent-ipc.c` (`DEFAULT_WS_URL`).

## Usage

1. Start the compositor: `planar`
2. Start the agent UI: `planar-agent-ui`
3. Ensure your Hermes gateway is running with the Planar platform enabled.

### planarctl

Runtime control via IPC:

```bash
planarctl workspace 2              # Switch workspace
planarctl killactive               # Close focused window
planarctl move_workspace 100 0     # Pan canvas
planarctl set border_width 6       # Configure decorations
planarctl set border_color 0.5 0.2 0.8 1.0
planarctl get workspaces           # Query state
planarctl get focused              # Query focused window
planarctl -e                       # Subscribe to events
```

### Configuration

Config file: `~/.config/planar/config.toml`

Supports keybindings and startup commands. See `compositor/CLAUDE.md` for full documentation of internal commands.

## Architecture

```
┌─────────────────────────────────────────────────┐
│                  Wayland Clients                 │
└──────────────────────┬──────────────────────────┘
                       │ xdg-shell / layer-shell
┌──────────────────────▼──────────────────────────┐
│              Planar Compositor                   │
│  infinite canvas · workspaces · decorations      │
│  IPC socket: $XDG_RUNTIME_DIR/planar.*.sock     │
└──────┬──────────────────────────────────┬───────┘
       │ unix socket IPC                  │ layer-shell
┌──────▼──────┐                   ┌───────▼───────┐
│  planarctl  │                   │ Agent UI Panel │
│  (CLI tool) │                   │  GTK4 overlay  │
└─────────────┘                   └───────┬───────┘
                                          │ WebSocket
                                  ┌───────▼───────┐
                                  │ Hermes Gateway │
                                  │ planar adapter │
                                  └───────┬───────┘
                                          │
                                  ┌───────▼───────┐
                                  │  Hermes Agent  │
                                  │  (AI + tools)  │
                                  └───────────────┘
```

## License

MIT

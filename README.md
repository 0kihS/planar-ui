# Planar Agent UI

GTK4 layer-shell panel for the [Planar](https://github.com/0kihS/planar) infinite-canvas Wayland compositor, with an integrated AI agent powered by [Hermes Agent](https://github.com/NousResearch/hermes-agent).

The panel provides a chat interface, window list, status bar, and notification system — all rendered as a layer-shell overlay on the compositor. The agent can see, navigate, and manage your desktop through the Hermes gateway.

## Building

### Dependencies

- gtk4
- gtk4-layer-shell
- webkitgtk-6.0
- libsoup-3.0
- json-glib-1.0
- meson, ninja

<details>
<summary>Arch Linux</summary>

```bash
sudo pacman -S gtk4 gtk4-layer-shell webkitgtk-6.0 libsoup3 json-glib meson ninja
```
</details>

### Build & Install

```bash
cd ui
meson setup build
meson compile -C build
sudo meson install -C build
```

This installs `planar-agent-ui`.

## Agent Setup

The UI communicates with a [Hermes Agent](https://github.com/NousResearch/hermes-agent) instance over WebSocket. You need a running Hermes gateway with the Planar platform adapter enabled.

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

### Connecting

The UI connects to `ws://<host>:5050` by default. If your Hermes instance runs on a remote server, make sure the WebSocket port is reachable. The connection URL is set in `ui/ipc/agent-ipc.c` (`DEFAULT_WS_URL`).

## Usage

1. Start the [Planar compositor](https://github.com/0kihS/planar): `planar`
2. Start the agent UI: `planar-agent-ui`
3. Ensure your Hermes gateway is running with the Planar platform enabled.

## Architecture

```
┌──────────────────────────────────────────┐
│           Planar Compositor              │
│  github.com/0kihS/planar                │
└──────────────────────┬───────────────────┘
                       │ layer-shell
               ┌───────▼───────┐
               │ Agent UI Panel │  <-- this repo
               │  GTK4 overlay  │
               └───────┬───────┘
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

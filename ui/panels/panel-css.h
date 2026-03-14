#ifndef PANEL_CSS_H
#define PANEL_CSS_H

static const char *PANEL_CSS =
    "@import url('https://fonts.googleapis.com/css2?family=Share+Tech+Mono&family=Orbitron:wght@400;700;900&family=Rajdhani:wght@300;400;600;700&display=swap');"
    ":root {"
    "  --red: #e8001a; --orange: #ff6a00; --amber: #ffb300;"
    "  --teal: #00e5ff; --teal-dim: #00e5ff55; --teal-glow: 0 0 8px #00e5ff66;"
    "  --yellow: #ffe100;"
    "  --bg: #070707; --bg2: rgba(10,10,14,0.88); --bg3: #111114;"
    "  --text: #ff9040; --text-dim: #ff6a0077;"
    "  --glow: 0 0 8px #ff6a0088; --glow-red: 0 0 8px #e8001a88;"
    "  --mono: 'Share Tech Mono', monospace; --display: 'Orbitron', monospace;"
    "  --caution: repeating-linear-gradient(135deg, var(--yellow) 0px, var(--yellow) 4px, #111 4px, #111 8px);"
    "  --grid: repeating-linear-gradient(0deg, transparent, transparent 19px, rgba(0,229,255,0.04) 19px, rgba(0,229,255,0.04) 20px),"
    "           repeating-linear-gradient(90deg, transparent, transparent 19px, rgba(0,229,255,0.04) 19px, rgba(0,229,255,0.04) 20px);"
    "}"
    "* { box-sizing: border-box; margin: 0; padding: 0; }"
    "body { background: var(--bg); color: var(--text); font-family: var(--mono); font-size: 12px; overflow: hidden; }"

    /* ── Panels ───────────────────────────────────────────────── */
    ".panel { background: var(--bg2); border: 1px solid rgba(232,0,26,0.45); "
    "  border-radius: 3px; box-shadow: inset 0 0 40px rgba(0,0,0,0.5), 0 0 12px rgba(232,0,26,0.15); "
    "  position: relative; backdrop-filter: blur(12px); -webkit-backdrop-filter: blur(12px); "
    "  background-image: var(--grid); }"

    /* ── Scanline overlay ─────────────────────────────────────── */
    ".panel::after { content: ''; position: absolute; inset: 0; pointer-events: none; z-index: 999; "
    "  border-radius: 3px; "
    "  background: repeating-linear-gradient(0deg, transparent 0px, transparent 2px, rgba(0,0,0,0.06) 2px, rgba(0,0,0,0.06) 4px); }"

    /* ── Headers ──────────────────────────────────────────────── */
    ".header { position: relative; height: 28px; overflow: hidden; margin-bottom: 0; }"
    ".header-fill { position: absolute; inset: 0; background: var(--red); transform: skewX(-12deg) translateX(-4px); "
    "  box-shadow: var(--glow-red); }"
    ".header-fill.amber { background: var(--orange); box-shadow: var(--glow); }"
    ".header-fill.teal-fill { background: var(--teal); box-shadow: var(--teal-glow); }"
    ".header-text { position: relative; z-index: 1; font-family: var(--display); font-size: 9px; font-weight: 700; "
    "  letter-spacing: 0.18em; color: #000; line-height: 28px; padding: 0 12px; text-transform: uppercase; }"
    ".caution-bar { height: 3px; background: var(--caution); opacity: 0.7; }"

    /* ── Content area ─────────────────────────────────────────── */
    ".content { flex: 1; overflow-y: auto; padding: 8px; }"
    ".content::-webkit-scrollbar { width: 5px; background: transparent; }"
    ".content::-webkit-scrollbar-thumb { background: linear-gradient(180deg, var(--red), var(--orange)); border-radius: 3px; }"

    /* ── Window items ─────────────────────────────────────────── */
    ".window-item { display: flex; align-items: center; gap: 8px; padding: 5px 10px; "
    "  border-bottom: 1px solid rgba(232,0,26,0.12); cursor: pointer; "
    "  transition: all 0.2s ease; font-size: 11px; border-left: 2px solid transparent; }"
    ".window-item:hover { background: rgba(0,229,255,0.06); border-left-color: var(--teal); }"
    ".window-item.focused { background: rgba(0,229,255,0.08); border-left: 2px solid var(--teal); }"
    ".win-id { font-family: var(--display); font-size: 8px; color: var(--teal-dim); min-width: 28px; letter-spacing: 0.05em; }"
    ".win-name { flex: 1; color: var(--text); text-shadow: 0 0 6px #ff6a0044; font-size: 11px; }"
    ".win-pos { font-size: 8px; color: var(--text-dim); }"
    ".win-hex { color: var(--teal); font-size: 7px; margin-right: 2px; }"

    /* ── Chat messages ────────────────────────────────────────── */
    ".msg { margin-bottom: 10px; animation: fadeUp 0.3s ease; }"
    "@keyframes fadeUp { from { opacity: 0; transform: translateY(6px); } to { opacity: 1; transform: translateY(0); } }"
    ".msg-meta { font-size: 8px; color: var(--teal-dim); letter-spacing: 0.1em; margin-bottom: 3px; font-family: var(--display); }"
    ".msg-bubble { padding: 8px 12px; border-left: 2px solid var(--red); background: rgba(232,0,26,0.05); "
    "  font-size: 12px; line-height: 1.6; border-radius: 0 2px 2px 0; }"
    ".msg-bubble.user { border-left-color: var(--orange); background: rgba(255,106,0,0.05); color: var(--amber); }"
    ".msg-bubble.system { border-left-color: var(--teal); background: rgba(0,229,255,0.03); color: #556; font-size: 10px; }"
    ".msg-bubble.tool { border-left-color: var(--teal); background: rgba(0,229,255,0.04); color: var(--teal); font-size: 11px; font-family: var(--mono); }"
    ".msg-bubble.agent { border-left-color: var(--amber); background: rgba(255,179,0,0.05); color: var(--amber); }"

    /* ── Log items ────────────────────────────────────────────── */
    ".log-item { padding: 5px 8px; border-bottom: 1px solid rgba(0,229,255,0.06); font-size: 10px; "
    "  animation: fadeUp 0.25s ease; border-left: 2px solid var(--orange); }"
    ".log-item.tool-entry { border-left-color: var(--teal); }"
    ".log-time { color: var(--teal-dim); font-size: 8px; margin-bottom: 1px; font-family: var(--display); letter-spacing: 0.08em; }"
    ".log-event { color: var(--orange); letter-spacing: 0.05em; text-shadow: 0 0 6px #ff6a0044; font-size: 10px; }"
    ".log-event.tool-name { color: var(--teal); text-shadow: 0 0 6px #00e5ff44; }"
    ".log-detail { color: #445; font-size: 9px; margin-top: 1px; white-space: nowrap; overflow: hidden; text-overflow: ellipsis; }"

    /* ── Sparkline ────────────────────────────────────────────── */
    ".spark-container { padding: 6px 8px; border-bottom: 1px solid rgba(0,229,255,0.1); "
    "  display: flex; flex-direction: column; gap: 4px; }"
    ".spark-label { font-family: var(--display); font-size: 7px; color: var(--teal-dim); "
    "  letter-spacing: 0.15em; text-transform: uppercase; }"
    ".sparkline { height: 24px; display: flex; align-items: flex-end; gap: 1px; }"
    ".spark-bar { width: 2px; background: var(--teal); border-radius: 1px 1px 0 0; "
    "  min-height: 1px; transition: height 0.3s ease; opacity: 0.8; }"
    ".spark-bar.active { opacity: 1; box-shadow: 0 0 4px var(--teal); }"
    ".spark-counters { display: flex; gap: 12px; font-size: 8px; font-family: var(--display); "
    "  letter-spacing: 0.1em; color: var(--teal-dim); padding: 4px 8px; "
    "  border-bottom: 1px solid rgba(0,229,255,0.08); }"
    ".spark-counters span { color: var(--teal); }"

    /* ── Stat rows ────────────────────────────────────────────── */
    ".stat-row { display: flex; justify-content: space-between; align-items: center; font-size: 10px; "
    "  padding: 4px 0; border-bottom: 1px solid rgba(0,229,255,0.06); }"
    ".stat-label { color: var(--text); letter-spacing: 0.08em; font-size: 9px; font-weight: 700; }"
    ".stat-val { color: var(--teal); font-family: var(--display); font-size: 9px; letter-spacing: 0.05em; }"
    ".stat-val.online { color: #0f6; }"
    ".stat-val.offline { color: var(--red); }"

    /* ── Hex decorative element ───────────────────────────────── */
    ".hex-badge { width: 28px; height: 32px; clip-path: polygon(50%% 0%%, 100%% 25%%, 100%% 75%%, 50%% 100%%, 0%% 75%%, 0%% 25%%); "
    "  background: rgba(0,229,255,0.12); border: 1px solid var(--teal); display: flex; align-items: center; "
    "  justify-content: center; font-family: var(--display); font-size: 7px; color: var(--teal); }"

    /* ── Input area ───────────────────────────────────────────── */
    ".input-area { background: var(--bg3); border: 1px solid rgba(232,0,26,0.3); color: var(--text); "
    "  font-family: var(--mono); font-size: 13px; padding: 8px 12px; width: 100%; resize: none; outline: none; "
    "  border-radius: 3px; box-shadow: inset 0 0 10px rgba(0,0,0,0.3); transition: all 0.2s ease; }"
    ".input-area:focus { border-color: var(--teal); box-shadow: inset 0 0 10px rgba(0,229,255,0.06), var(--teal-glow); }"
    ".send-btn { background: var(--red); border: none; color: #000; font-family: var(--display); font-size: 9px; "
    "  font-weight: 700; letter-spacing: 0.15em; padding: 8px 14px; cursor: pointer; text-transform: uppercase; "
    "  box-shadow: var(--glow-red); border-radius: 2px; transition: all 0.2s ease; }"
    ".send-btn:hover { background: var(--orange); transform: scale(1.03); }"
    ".qcmd { font-family: var(--mono); font-size: 9px; padding: 3px 10px; background: transparent; "
    "  border: 1px solid rgba(0,229,255,0.25); color: var(--teal-dim); cursor: pointer; letter-spacing: 0.05em; "
    "  margin-right: 4px; border-radius: 2px; transition: all 0.2s ease; }"
    ".qcmd:hover { border-color: var(--teal); color: var(--teal); background: rgba(0,229,255,0.06); "
    "  box-shadow: var(--teal-glow); }"
    ".quick-cmds { display: flex; gap: 4px; flex-wrap: wrap; margin-bottom: 8px; }"

    /* ── Topbar ────────────────────────────────────────────────── */
    ".topbar { background: var(--bg2); border-bottom: 1px solid rgba(232,0,26,0.4); display: flex; "
    "  align-items: center; padding: 0 12px; height: 100%%; gap: 0; "
    "  box-shadow: 0 2px 16px rgba(0,0,0,0.6), 0 1px 0 rgba(232,0,26,0.2); "
    "  background-image: var(--grid); backdrop-filter: blur(12px); }"
    ".topbar-caution { width: 6px; height: 100%%; background: var(--caution); margin-right: 10px; flex-shrink: 0; }"
    ".topbar-title { font-family: var(--display); font-size: 13px; font-weight: 900; letter-spacing: 0.25em; "
    "  text-transform: uppercase; color: var(--red); text-shadow: var(--glow-red); }"
    ".topbar-sep { width: 1px; height: 18px; background: rgba(232,0,26,0.25); margin: 0 12px; flex-shrink: 0; }"
    ".topbar-sub { font-size: 9px; color: var(--teal-dim); letter-spacing: 0.1em; font-family: var(--display); }"
    ".topbar-sub .val { color: var(--teal); }"
    ".topbar-status { display: flex; align-items: center; gap: 6px; }"
    ".status-dot { width: 6px; height: 6px; border-radius: 50%%; background: var(--teal); "
    "  box-shadow: 0 0 6px var(--teal); animation: pulse 2s ease-in-out infinite; flex-shrink: 0; }"
    ".status-dot.offline { background: var(--red); box-shadow: 0 0 6px var(--red); }"
    "@keyframes pulse { 0%%,100%% { opacity: 1; transform: scale(1); } 50%% { opacity: 0.4; transform: scale(0.7); } }"
    ".toggle-btn { background: transparent; border: 1px solid rgba(232,0,26,0.3); color: var(--text-dim); "
    "  font-family: var(--display); font-size: 8px; padding: 3px 8px; cursor: pointer; letter-spacing: 0.12em; "
    "  margin-left: 4px; border-radius: 2px; transition: all 0.2s ease; position: relative; }"
    ".toggle-btn:hover { border-color: var(--teal); color: var(--teal); }"
    ".toggle-btn.active { background: var(--red); color: #000; border-color: var(--red); }"
    ".toggle-btn.active::after { content: ''; position: absolute; bottom: -3px; left: 0; right: 0; "
    "  height: 2px; background: var(--caution); }"
    ".toggle-btn.inactive { opacity: 0.4; }"

    /* ── Status panel extras ──────────────────────────────────── */
    ".diag-section { padding: 8px; }"
    ".diag-header { font-family: var(--display); font-size: 7px; color: var(--teal); font-weight: 700; "
    "  letter-spacing: 0.2em; margin-bottom: 6px; text-transform: uppercase; }"
    ".conn-indicator { display: flex; align-items: center; gap: 6px; padding: 4px 0; }"
    ".conn-bar { flex: 1; height: 3px; background: var(--bg3); border-radius: 2px; overflow: hidden; }"
    ".conn-bar-fill { height: 100%%; background: linear-gradient(90deg, var(--teal), var(--teal)); "
    "  border-radius: 2px; transition: width 0.5s ease; }"
    ".conn-bar-fill.disconnected { background: linear-gradient(90deg, var(--red), var(--red)); }"
    ;

#endif

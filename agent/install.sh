#!/usr/bin/env bash
#
# Install Planar platform adapter + skill into an existing Hermes Agent setup.
# Also patches toolsets.py and gateway/run.py for clarify/reasoning support.
#
# Usage:
#   ./install.sh                      # auto-detect ~/.hermes
#   ./install.sh /path/to/hermes      # specify hermes home
#

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# Find hermes home
if [ $# -ge 1 ]; then
    HERMES_HOME="$1"
elif [ -d "$HOME/.hermes" ]; then
    HERMES_HOME="$HOME/.hermes"
else
    echo "Error: Could not find Hermes installation."
    echo "Either install Hermes first or pass the path: $0 /path/to/.hermes"
    exit 1
fi

# Find hermes-agent directory (contains the gateway code)
if [ -d "$HERMES_HOME/hermes-agent" ]; then
    AGENT_DIR="$HERMES_HOME/hermes-agent"
elif [ -d "$HERMES_HOME/gateway" ]; then
    AGENT_DIR="$HERMES_HOME"
else
    echo "Error: Could not find hermes-agent in $HERMES_HOME"
    exit 1
fi

echo "Hermes home:  $HERMES_HOME"
echo "Agent dir:    $AGENT_DIR"
echo ""

# Install platform adapter
PLATFORMS_DIR="$AGENT_DIR/gateway/platforms"
if [ ! -d "$PLATFORMS_DIR" ]; then
    echo "Error: $PLATFORMS_DIR does not exist"
    exit 1
fi

echo "Installing platform adapter..."
cp "$SCRIPT_DIR/platforms/planar.py" "$PLATFORMS_DIR/planar.py"
echo "  -> $PLATFORMS_DIR/planar.py"

# Install skill
SKILLS_DIR="$HERMES_HOME/skills/planar"
mkdir -p "$SKILLS_DIR"
echo "Installing skill..."
cp "$SCRIPT_DIR/skills/planar/SKILL.md" "$SKILLS_DIR/SKILL.md"
echo "  -> $SKILLS_DIR/SKILL.md"

# Apply patches (idempotent — safe to re-run)
echo ""
echo "Patching toolsets.py..."
python3 "$SCRIPT_DIR/patches/patch_toolsets.py" "$AGENT_DIR"

echo "Patching gateway/run.py..."
python3 "$SCRIPT_DIR/patches/patch_run.py" "$AGENT_DIR"

# Check websockets dependency
echo ""
if python3 -c "import websockets" 2>/dev/null; then
    echo "websockets: OK"
else
    echo "websockets: NOT FOUND"
    echo "  Install with: pip install websockets"
fi

echo ""
echo "Done. Make sure 'planar' is enabled in your Hermes config.yaml:"
echo ""
echo "  platforms:"
echo "    planar:"
echo "      enabled: true"

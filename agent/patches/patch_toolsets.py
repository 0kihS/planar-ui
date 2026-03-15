#!/usr/bin/env python3
"""Add 'hermes-planar' toolset to the Hermes TOOLSETS registry.

Inserts after the last existing toolset entry, using _HERMES_CORE_TOOLS
(the same tool list used by cli, telegram, discord, etc.).
"""
import sys
import os

def patch(agent_dir):
    path = os.path.join(agent_dir, "toolsets.py")
    if not os.path.isfile(path):
        print(f"  SKIP: {path} not found")
        return False

    with open(path, "r") as f:
        content = f.read()

    if '"hermes-planar"' in content:
        print("  OK: hermes-planar toolset already present")
        return True

    # Insert before the last closing brace of TOOLSETS dict, after the last
    # existing entry.  Find a known entry to anchor after.
    anchor = '"hermes-email"'
    if anchor not in content:
        # Try any toolset entry as anchor
        for candidate in ['"hermes-discord"', '"hermes-telegram"', '"hermes-cli"']:
            if candidate in content:
                anchor = candidate
                break
        else:
            print("  ERROR: Could not find anchor toolset entry")
            return False

    # Find the closing '},' of the anchor entry's block and insert after it
    idx = content.index(anchor)
    # Walk forward to find the block's closing '},\n\n'
    depth = 0
    i = idx
    block_end = None
    while i < len(content):
        if content[i] == '{':
            depth += 1
        elif content[i] == '}':
            depth -= 1
            if depth == 0:
                # Find the comma and newlines after
                j = i + 1
                while j < len(content) and content[j] in ' ,\n':
                    j += 1
                block_end = j
                break
        i += 1

    if block_end is None:
        print("  ERROR: Could not parse toolset block structure")
        return False

    insert = '''
    "hermes-planar": {
        "description": "Planar compositor UI toolset - full access via layer-shell panels",
        "tools": _HERMES_CORE_TOOLS,
        "includes": []
    },

'''
    content = content[:block_end] + insert + content[block_end:]

    with open(path, "w") as f:
        f.write(content)
    print("  OK: Added hermes-planar toolset")
    return True


if __name__ == "__main__":
    agent_dir = sys.argv[1] if len(sys.argv) > 1 else "."
    sys.exit(0 if patch(agent_dir) else 1)

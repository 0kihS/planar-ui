#!/usr/bin/env python3
"""Patch gateway/run.py to wire clarify_callback and reasoning_callback
for the Planar platform adapter.

Inserts callback definitions before the AIAgent constructor and adds
the callback kwargs to the constructor call.
"""
import sys
import os
import re

CALLBACK_BLOCK = '''
            # --- Planar UI callback wiring ---
            _planar_for_cb = self.adapters.get(Platform.PLANAR)
            _cb_loop = _loop_for_step  # reuse loop captured in async context
            _cb_chat_id = source.chat_id

            def _gateway_clarify_callback(question, choices):
                if not _planar_for_cb:
                    return "No Planar UI connected"
                try:
                    asyncio.run_coroutine_threadsafe(
                        _planar_for_cb.send_clarify(
                            question, choices or [], timeout=120, chat_id=_cb_chat_id,
                        ),
                        _cb_loop,
                    ).result(timeout=5)
                except Exception as e:
                    return f"Failed to send clarify: {e}"
                try:
                    future = asyncio.run_coroutine_threadsafe(
                        _planar_for_cb.wait_for_clarify_response(timeout=120),
                        _cb_loop,
                    )
                    return future.result(timeout=125)
                except Exception as e:
                    return f"Clarify timed out: {e}"

            def _gateway_reasoning_callback(text):
                if not _planar_for_cb:
                    return
                try:
                    asyncio.run_coroutine_threadsafe(
                        _planar_for_cb.send_reasoning(text, chat_id=_cb_chat_id),
                        _cb_loop,
                    )
                except Exception:
                    pass

'''


def patch(agent_dir):
    path = os.path.join(agent_dir, "gateway", "run.py")
    if not os.path.isfile(path):
        print(f"  SKIP: {path} not found")
        return False

    with open(path, "r") as f:
        content = f.read()

    modified = False

    # --- Part 1: Insert callback definitions before AIAgent constructor ---
    if "_gateway_clarify_callback" in content:
        print("  OK: Clarify callback already present")
    else:
        # Find the AIAgent constructor call inside run_sync
        marker = "            agent = AIAgent("
        idx = content.find(marker)
        if idx == -1:
            print("  ERROR: Could not find 'agent = AIAgent(' in run.py")
            return False

        content = content[:idx] + CALLBACK_BLOCK + content[idx:]
        modified = True
        print("  OK: Inserted callback definitions")

    # --- Part 2: Add kwargs to AIAgent constructor ---
    if "clarify_callback=" in content and "reasoning_callback=" in content:
        print("  OK: AIAgent kwargs already present")
    else:
        # Find the closing ')' of the AIAgent constructor
        # Look for the last kwarg line before the closing ')'
        pattern = r"(            agent = AIAgent\(.*?)(            \))"
        match = re.search(pattern, content, re.DOTALL)
        if not match:
            print("  ERROR: Could not parse AIAgent constructor")
            return False

        constructor_body = match.group(1)
        if "clarify_callback=" not in constructor_body:
            insert_kwargs = (
                "                clarify_callback=_gateway_clarify_callback if _planar_for_cb else None,\n"
                "                reasoning_callback=_gateway_reasoning_callback if _planar_for_cb else None,\n"
            )
            # Insert before the closing ')'
            content = content[:match.start(2)] + insert_kwargs + content[match.start(2):]
            modified = True
            print("  OK: Added clarify/reasoning kwargs to AIAgent")

    if modified:
        with open(path, "w") as f:
            f.write(content)
    return True


if __name__ == "__main__":
    agent_dir = sys.argv[1] if len(sys.argv) > 1 else "."
    sys.exit(0 if patch(agent_dir) else 1)

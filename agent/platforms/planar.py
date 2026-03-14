"""
Planar UI platform adapter.

WebSocket server that accepts connections from the Planar compositor's
agent UI panel. Messages are JSON frames over WebSocket.

Client -> Server:
    {"type": "message", "text": "hello world"}
    {"type": "command", "cmd": "new"}

Server -> Client:
    {"type": "response", "text": "...", "session_id": "..."}
    {"type": "progress", "tool": "terminal", "preview": "ls -la"}
    {"type": "system", "text": "Connected to Hermes gateway"}
    {"type": "error", "text": "..."}
"""

import asyncio
import json
import logging
import os
import re
import uuid
from typing import Any, Dict, Optional

try:
    import websockets
    from websockets.asyncio.server import serve as ws_serve
    WEBSOCKETS_AVAILABLE = True
except ImportError:
    WEBSOCKETS_AVAILABLE = False

from gateway.config import Platform, PlatformConfig
from gateway.platforms.base import (
    BasePlatformAdapter,
    MessageEvent,
    MessageType,
    SendResult,
)
from gateway.session import SessionSource

logger = logging.getLogger(__name__)

# Pattern to extract tool name and preview from progress text like "💻 terminal: \"ls -la\""
_PROGRESS_RE = re.compile(r'^.{1,4}\s+(\w+)(?:[:.]\s*"?(.*?)"?\s*)?$')


def check_planar_requirements() -> bool:
    if not WEBSOCKETS_AVAILABLE:
        logger.warning("Planar: websockets library not installed")
        return False
    return True


class PlanarAdapter(BasePlatformAdapter):
    """WebSocket server adapter for the Planar compositor UI."""

    MAX_MESSAGE_LENGTH = 16384

    def __init__(self, config: PlatformConfig):
        super().__init__(config, Platform.PLANAR)
        self._port = int(os.getenv("PLANAR_WS_PORT", "5000"))
        self._host = os.getenv("PLANAR_WS_HOST", "0.0.0.0")
        self._server = None
        self._clients: Dict[Any, str] = {}  # ws -> connection_id
        self._msg_counter = 0

    async def connect(self) -> bool:
        if not WEBSOCKETS_AVAILABLE:
            return False
        try:
            self._server = await ws_serve(
                self._handle_client,
                self._host,
                self._port,
            )
            self._running = True
            logger.info("[%s] WebSocket server listening on %s:%d", self.name, self._host, self._port)
            return True
        except Exception as e:
            logger.error("[%s] Failed to start: %s", self.name, e)
            return False

    async def disconnect(self) -> None:
        self._running = False
        if self._server:
            self._server.close()
            await self._server.wait_closed()
            self._server = None
        self._clients.clear()

    def _parse_progress_text(self, text: str) -> dict:
        """Parse progress text like '💻 terminal: \"ls -la\"' into a progress frame."""
        # Try last line for accumulated progress
        lines = text.strip().split("\n")
        last = lines[-1] if lines else text
        m = _PROGRESS_RE.match(last)
        if m:
            return {"type": "progress", "tool": m.group(1), "preview": m.group(2) or ""}
        return {"type": "progress", "tool": "", "preview": last}

    async def send(
        self,
        chat_id: str,
        content: str,
        reply_to: Optional[str] = None,
        metadata: Optional[Dict[str, Any]] = None,
    ) -> SendResult:
        if metadata and metadata.get("is_progress"):
            frame = self._parse_progress_text(content)
        else:
            frame = {"type": "response", "text": content}
            if metadata and metadata.get("session_id"):
                frame["session_id"] = metadata["session_id"]
        self._msg_counter += 1
        msg_id = f"planar-{self._msg_counter}"
        await self._broadcast(chat_id, frame)
        return SendResult(success=True, message_id=msg_id)

    async def edit_message(
        self,
        chat_id: str,
        message_id: str,
        content: str,
        metadata: Optional[Dict[str, Any]] = None,
    ) -> SendResult:
        """Edit = update the progress indicator with latest tool status."""
        frame = self._parse_progress_text(content)
        await self._broadcast(chat_id, frame)
        return SendResult(success=True, message_id=message_id)

    async def send_typing(self, chat_id: str, metadata=None) -> None:
        pass  # Progress is handled through send/edit_message

    async def get_chat_info(self, chat_id: str) -> dict:
        return {"name": "Planar UI", "type": "dm"}

    async def _broadcast(self, chat_id: str, frame: dict) -> SendResult:
        data = json.dumps(frame)
        closed = []
        for ws, cid in list(self._clients.items()):
            if cid == chat_id or chat_id == "planar-ui":
                try:
                    await ws.send(data)
                except Exception:
                    closed.append(ws)
        for ws in closed:
            self._clients.pop(ws, None)
        return SendResult(success=True)

    async def _handle_client(self, ws):
        conn_id = f"planar-{uuid.uuid4().hex[:8]}"
        self._clients[ws] = conn_id
        logger.info("[%s] Client connected: %s", self.name, conn_id)

        try:
            await ws.send(json.dumps({
                "type": "system",
                "text": "Connected to Hermes gateway",
            }))

            async for raw in ws:
                try:
                    msg = json.loads(raw)
                except (json.JSONDecodeError, TypeError):
                    await ws.send(json.dumps({"type": "error", "text": "Invalid JSON"}))
                    continue

                msg_type = msg.get("type", "")

                if msg_type == "message":
                    text = msg.get("text", "").strip()
                    if not text:
                        continue
                    await self._dispatch(text, conn_id, ws)

                elif msg_type == "command":
                    cmd = msg.get("cmd", "").strip()
                    if cmd:
                        await self._dispatch(f"/{cmd}", conn_id, ws)

                else:
                    await ws.send(json.dumps({"type": "error", "text": f"Unknown type: {msg_type}"}))

        except websockets.exceptions.ConnectionClosed:
            pass
        except Exception as e:
            logger.error("[%s] Client error: %s", self.name, e)
        finally:
            self._clients.pop(ws, None)
            logger.info("[%s] Client disconnected: %s", self.name, conn_id)

    async def _dispatch(self, text: str, conn_id: str, ws):
        source = SessionSource(
            platform=Platform.PLANAR,
            chat_id=conn_id,
            chat_type="dm",
            user_id="planar-ui",
            user_name="planar",
        )
        event = MessageEvent(
            text=text,
            source=source,
            message_type=MessageType.TEXT,
        )
        if self._message_handler:
            try:
                response = await self._message_handler(event)
                if response:
                    await ws.send(json.dumps({"type": "response", "text": response}))
            except Exception as e:
                logger.error("[%s] Handler error: %s", self.name, e)
                await ws.send(json.dumps({"type": "error", "text": str(e)}))

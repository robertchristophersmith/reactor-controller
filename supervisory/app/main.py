from fastapi import FastAPI, WebSocket, WebSocketDisconnect, HTTPException
from fastapi.staticfiles import StaticFiles
from contextlib import asynccontextmanager
from typing import List
from .orchestrator import orchestrator
from .database import engine, Base

@asynccontextmanager
async def lifespan(app: FastAPI):
    # Startup
    await orchestrator.start()
    yield
    # Shutdown
    # orchestrator.stop()

app = FastAPI(title="Reactor Controller", lifespan=lifespan)

# Helper for CORS if needed (e.g. dev)
from fastapi.middleware.cors import CORSMiddleware
app.add_middleware(
    CORSMiddleware,
    allow_origins=["*"],
    allow_methods=["*"],
    allow_headers=["*"],
)

# --- API Endpoints ---

@app.post("/api/control/state/{state_id}")
async def set_state(state_id: int):
    # 0=Standby, 1=Warmup, 2=Working, etc.
    await orchestrator.set_state(state_id)
    return {"status": "command_sent", "state": state_id}

@app.post("/api/control/setpoint")
async def set_setpoint(zone: int, value: float, rate: float = 0.0):
    # Zone: 0=Gas, 1=Vap, 2=Reactor
    await orchestrator.send_setpoint(zone, value, rate)
    return {"status": "command_sent", "zone": zone, "value": value, "rate": rate}

@app.post("/api/control/flow")
async def set_flow(value: float):
    await orchestrator.send_flow(value)
    return {"status": "command_sent", "value": value}

@app.get("/api/history")
async def get_history():
    return list(orchestrator.live_buffer)

# --- WebSocket ---

@app.websocket("/ws")
async def websocket_endpoint(websocket: WebSocket):
    await websocket.accept()
    queue = await orchestrator.subscribe()
    try:
        while True:
            data = await queue.get()
            await websocket.send_json(data)
    except WebSocketDisconnect:
        orchestrator.unsubscribe(queue)

# --- Static Files ---
import os
static_dir = os.path.join(os.path.dirname(__file__), "static")
if not os.path.exists(static_dir):
    os.makedirs(static_dir)

app.mount("/", StaticFiles(directory=static_dir, html=True), name="static")

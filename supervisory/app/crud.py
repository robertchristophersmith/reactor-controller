from sqlalchemy.orm import Session
from .database import ProcessLog
from datetime import datetime, timedelta

def create_log(db: Session, data: dict, uptime: float, state: int):
    # Map dictionary keys (from JSON) to Model fields
    # JSON keys: t_gas, t_feed... -> Model: temp_gas, temp_feed...
    
    # Sensors
    s = data.get("sensors", {})
    h = data.get("heaters", {})
    sp = data.get("sp", {})
    pump = data.get("pump", {})
    
    db_log = ProcessLog(
        uptime=uptime,
        control_state=state,
        
        temp_gas=s.get("t_gas", 0.0),
        temp_feed=s.get("t_feed", 0.0),
        temp_vap=s.get("t_vap", 0.0),
        temp_r_i1=s.get("t_r_i1", 0.0),
        temp_r_i2=s.get("t_r_i2", 0.0),
        temp_r_e1=s.get("t_r_e1", 0.0),
        temp_r_e2=s.get("t_r_e2", 0.0),
        
        pressure_feed=s.get("p_feed", 0.0),
        pressure_reac=s.get("p_reac", 0.0),
        flow_rate=s.get("flow", 0.0),
        h2_ppm=s.get("h2", 0.0),
        
        heater_gas=h.get("gas", 0.0),
        heater_vap=h.get("vap", 0.0),
        heater_reac=h.get("reac", 0.0),
        
        sp_gas=sp.get("gas", 0.0),
        sp_vap=sp.get("vap", 0.0),
        sp_reac=sp.get("reac", 0.0),
        
        weight=s.get("weight", 0.0),
        pump_speed=pump.get("speed", 0)
    )
    db.add(db_log)
    db.commit()
    return db_log

def get_history_downsampled(db: Session, hours: int, max_points: int = 300):
    cutoff = datetime.utcnow() - timedelta(hours=hours)
    logs = db.query(ProcessLog).filter(ProcessLog.timestamp >= cutoff).order_by(ProcessLog.timestamp.asc()).all()
    if not logs:
        return []
        
    step = max(1, len(logs) // max_points)
    sampled = logs[::step]
    
    result = []
    for log in sampled:
        result.append({
            "ts": log.timestamp.strftime("%H:%M:%S"),
            "uptime": log.uptime,
            "state": log.control_state,
            "sensors": {
                "weight": log.weight,
            },
            "pump": {
                "speed": log.pump_speed
            }
        })
    return result

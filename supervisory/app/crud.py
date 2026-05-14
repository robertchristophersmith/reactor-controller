from sqlalchemy.orm import Session
from .database import Logs1s, Logs1m, Logs10m, RunsMetadata
from datetime import datetime, timedelta

def create_log(db: Session, data: dict, uptime: float, state: int):
    # Map dictionary keys (from JSON) to Model fields
    # JSON keys: t_gas, t_feed... -> Model: temp_gas, temp_feed...
    
    # Sensors
    s = data.get("sensors", {})
    h = data.get("heaters", {})
    sp = data.get("sp", {})
    pump = data.get("pump", {})
    
    db_log = Logs1s(
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
    
    # Decide which table to query based on time horizon
    if hours <= 1:
        Table = Logs1s
    elif hours <= 6:
        Table = Logs1m
    else:
        Table = Logs10m
        
    logs = db.query(Table).filter(Table.timestamp >= cutoff).order_by(Table.timestamp.asc()).all()
    if not logs:
        return []
        
    # We still decimate if somehow there are more than max_points, but ideally the rollups handle it
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
                "t_gas": log.temp_gas,
                "t_vap": log.temp_vap,
                "t_r_i1": log.temp_r_i1,
                "t_r_i2": log.temp_r_i2,
            },
            "pump": {
                "speed": log.pump_speed
            }
        })
    return result

from sqlalchemy import text

def perform_1m_rollup(db: Session):
    sql = text("""
        INSERT INTO logs_1m (
            timestamp, uptime, control_state, temp_gas, temp_feed, temp_vap,
            temp_r_i1, temp_r_i2, temp_r_e1, temp_r_e2, pressure_feed, pressure_reac,
            flow_rate, h2_ppm, weight, pump_speed, heater_gas, heater_vap, heater_reac,
            sp_gas, sp_vap, sp_reac
        )
        SELECT 
            MAX(timestamp), AVG(uptime), MAX(control_state), AVG(temp_gas), AVG(temp_feed), AVG(temp_vap),
            AVG(temp_r_i1), AVG(temp_r_i2), AVG(temp_r_e1), AVG(temp_r_e2), AVG(pressure_feed), AVG(pressure_reac),
            AVG(flow_rate), AVG(h2_ppm), AVG(weight), AVG(pump_speed), AVG(heater_gas), AVG(heater_vap), AVG(heater_reac),
            AVG(sp_gas), AVG(sp_vap), AVG(sp_reac)
        FROM logs_1s
        WHERE timestamp >= datetime('now', '-1 minute')
    """)
    db.execute(sql)
    db.commit()

def perform_10m_rollup(db: Session):
    sql = text("""
        INSERT INTO logs_10m (
            timestamp, uptime, control_state, temp_gas, temp_feed, temp_vap,
            temp_r_i1, temp_r_i2, temp_r_e1, temp_r_e2, pressure_feed, pressure_reac,
            flow_rate, h2_ppm, weight, pump_speed, heater_gas, heater_vap, heater_reac,
            sp_gas, sp_vap, sp_reac
        )
        SELECT 
            MAX(timestamp), AVG(uptime), MAX(control_state), AVG(temp_gas), AVG(temp_feed), AVG(temp_vap),
            AVG(temp_r_i1), AVG(temp_r_i2), AVG(temp_r_e1), AVG(temp_r_e2), AVG(pressure_feed), AVG(pressure_reac),
            AVG(flow_rate), AVG(h2_ppm), AVG(weight), AVG(pump_speed), AVG(heater_gas), AVG(heater_vap), AVG(heater_reac),
            AVG(sp_gas), AVG(sp_vap), AVG(sp_reac)
        FROM logs_1m
        WHERE timestamp >= datetime('now', '-10 minutes')
    """)
    db.execute(sql)
    db.commit()

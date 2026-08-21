from sqlalchemy.orm import Session
from .database import Logs1s, Logs1m, Logs10m, RunsMetadata, ErrorLog
from datetime import datetime, timedelta

def create_error_log(db: Session, sensor: str, exact_error: str) -> ErrorLog:
    log = ErrorLog(
        timestamp=datetime.utcnow(),
        sensor=sensor,
        exact_error=exact_error,
        cleared_timestamp=None
    )
    db.add(log)
    db.commit()
    db.refresh(log)
    return log

def resolve_error_log(db: Session, log_id: int):
    log = db.query(ErrorLog).filter(ErrorLog.id == log_id).first()
    if log and log.cleared_timestamp is None:
        log.cleared_timestamp = datetime.utcnow()
        db.commit()

def get_error_logs(db: Session, limit: int = 100):
    logs = db.query(ErrorLog).order_by(ErrorLog.id.desc()).limit(limit).all()
    result = []
    for log in logs:
        result.append({
            "id": log.id,
            "timestamp": log.timestamp.strftime("%Y-%m-%d %H:%M:%S"),
            "sensor": log.sensor,
            "exact_error": log.exact_error,
            "cleared_timestamp": log.cleared_timestamp.strftime("%Y-%m-%d %H:%M:%S") if log.cleared_timestamp else None,
            "active": log.cleared_timestamp is None
        })
    return result

def update_last_run_config(db: Session, sp: dict, pump: dict):
    run_meta = db.query(RunsMetadata).order_by(RunsMetadata.id.desc()).first()
    if run_meta:
        if "feed_pre" in sp: run_meta.last_sp_feed_pre = float(sp.get("feed_pre") or 0.0)
        if "liq_reac" in sp: run_meta.last_sp_liq_reac = float(sp.get("liq_reac") or 0.0)
        if "gas_reac" in sp: run_meta.last_sp_gas_reac = float(sp.get("gas_reac") or 0.0)
        
        if "mode" in pump: run_meta.last_pump_mode = str(pump.get("mode") or "manual")
        if "speed" in pump: run_meta.last_pump_speed = int(pump.get("speed") or 0)
        if "dir" in pump: run_meta.last_pump_dir = int(pump.get("dir") or 0)
        if "auto_min" in pump: run_meta.last_auto_min = float(pump.get("auto_min") or 0.0)
        if "auto_max" in pump: run_meta.last_auto_max = float(pump.get("auto_max") or 0.0)
        if "auto_rec_rpm" in pump: run_meta.last_auto_rec_rpm = int(pump.get("auto_rec_rpm") or 0)
        if "auto_dir" in pump: run_meta.last_auto_dir = int(pump.get("auto_dir") or 0)
        db.commit()

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
        
        temp_feed_res=s.get("t_feed_res", 0.0),
        temp_feed_pre=s.get("t_feed_pre", 0.0),
        temp_liq_reac=s.get("t_liq_reac", 0.0),
        temp_gas_reac_int=s.get("t_gas_reac_int", 0.0),
        temp_gas_reac_ext=s.get("t_gas_reac_ext", 0.0),
        
        h2_ppm=s.get("h2", 0.0),
        
        heater_feed_pre=h.get("feed_pre", 0.0),
        heater_liq_reac=h.get("liq_reac", 0.0),
        heater_gas_reac=h.get("gas_reac", 0.0),
        
        sp_feed_pre=sp.get("feed_pre", 0.0),
        sp_liq_reac=sp.get("liq_reac", 0.0),
        sp_gas_reac=sp.get("gas_reac", 0.0),
        
        weight=s.get("weight", 0.0),
        pump_speed=pump.get("speed", 0)
    )
    db.add(db_log)
    update_last_run_config(db, sp, pump)
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
                "t_feed_res": log.temp_feed_res,
                "t_feed_pre": log.temp_feed_pre,
                "t_liq_reac": log.temp_liq_reac,
                "t_gas_reac_int": log.temp_gas_reac_int,
                "t_gas_reac_ext": log.temp_gas_reac_ext,
                "h2": log.h2_ppm,
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
            timestamp, uptime, control_state, temp_feed_res, temp_feed_pre, temp_liq_reac,
            temp_gas_reac_int, temp_gas_reac_ext, h2_ppm, weight, pump_speed,
            heater_feed_pre, heater_liq_reac, heater_gas_reac,
            sp_feed_pre, sp_liq_reac, sp_gas_reac
        )
        SELECT 
            MAX(timestamp), AVG(uptime), MAX(control_state), AVG(temp_feed_res), AVG(temp_feed_pre), AVG(temp_liq_reac),
            AVG(temp_gas_reac_int), AVG(temp_gas_reac_ext), AVG(h2_ppm), AVG(weight), AVG(pump_speed),
            AVG(heater_feed_pre), AVG(heater_liq_reac), AVG(heater_gas_reac),
            AVG(sp_feed_pre), AVG(sp_liq_reac), AVG(sp_gas_reac)
        FROM logs_1s
        WHERE timestamp >= datetime('now', '-1 minute')
        HAVING MAX(timestamp) IS NOT NULL
    """)
    db.execute(sql)
    db.commit()

def perform_10m_rollup(db: Session):
    sql = text("""
        INSERT INTO logs_10m (
            timestamp, uptime, control_state, temp_feed_res, temp_feed_pre, temp_liq_reac,
            temp_gas_reac_int, temp_gas_reac_ext, h2_ppm, weight, pump_speed,
            heater_feed_pre, heater_liq_reac, heater_gas_reac,
            sp_feed_pre, sp_liq_reac, sp_gas_reac
        )
        SELECT 
            MAX(timestamp), AVG(uptime), MAX(control_state), AVG(temp_feed_res), AVG(temp_feed_pre), AVG(temp_liq_reac),
            AVG(temp_gas_reac_int), AVG(temp_gas_reac_ext), AVG(h2_ppm), AVG(weight), AVG(pump_speed),
            AVG(heater_feed_pre), AVG(heater_liq_reac), AVG(heater_gas_reac),
            AVG(sp_feed_pre), AVG(sp_liq_reac), AVG(sp_gas_reac)
        FROM logs_1m
        WHERE timestamp >= datetime('now', '-10 minutes')
        HAVING MAX(timestamp) IS NOT NULL
    """)
    db.execute(sql)
    db.commit()

import os
import sys
import argparse
from datetime import datetime, timedelta
from sqlalchemy import create_engine, text, func
from sqlalchemy.orm import sessionmaker

# Ensure supervisory package can be imported
sys.path.insert(0, os.path.abspath(os.path.join(os.path.dirname(__file__), "..")))
from supervisory.app.database import Base, Logs1s, Logs1m, Logs10m

def run_backfill_and_prune(db_path: str, raw_retention_hours: float = 1.5, rollup_1m_retention_hours: float = 8.0):
    if not os.path.isabs(db_path):
        db_path = os.path.abspath(db_path)

    print(f"=== Database Backfill & Retention Policy Migration ===")
    print(f"Target Database: {db_path}")

    initial_size = os.path.getsize(db_path) if os.path.exists(db_path) else 0
    print(f"Initial DB Size: {initial_size / (1024 * 1024):.2f} MB ({initial_size:,} bytes)")

    db_url = f"sqlite:///{db_path}"
    engine = create_engine(db_url, connect_args={"check_same_thread": False})
    Base.metadata.create_all(bind=engine)
    Session = sessionmaker(bind=engine)

    with Session() as db:
        initial_1s_count = db.query(Logs1s).count()
        initial_1m_count = db.query(Logs1m).count()
        initial_10m_count = db.query(Logs10m).count()

        print(f"\n--- Initial Row Counts ---")
        print(f"  Logs1s (1-second raw):     {initial_1s_count:,}")
        print(f"  Logs1m (1-minute rollup):   {initial_1m_count:,}")
        print(f"  Logs10m (10-minute rollup): {initial_10m_count:,}")

        if initial_1s_count == 0 and initial_1m_count == 0:
            print("\nNo historical logs found in database. Exiting.")
            return

        # 1. Backfill 1-Minute Rollups from 1-Second Logs
        print("\n[Step 1/4] Aggregating 1-second raw data into 1-minute rollup buckets...")
        sql_1m = text("""
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
            GROUP BY strftime('%Y-%m-%d %H:%M:00', timestamp)
            HAVING MAX(timestamp) IS NOT NULL
        """)
        db.execute(sql_1m)
        db.commit()

        post_1m_count = db.query(Logs1m).count()
        print(f"  -> Generated {post_1m_count - initial_1m_count:,} new 1-minute rollup entries (Total: {post_1m_count:,}).")

        # 2. Backfill 10-Minute Rollups from 1-Minute Rollups
        print("\n[Step 2/4] Aggregating 1-minute data into 10-minute rollup buckets...")
        sql_10m = text("""
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
            GROUP BY (strftime('%s', timestamp) / 600)
            HAVING MAX(timestamp) IS NOT NULL
        """)
        db.execute(sql_10m)
        db.commit()

        post_10m_count = db.query(Logs10m).count()
        print(f"  -> Generated {post_10m_count - initial_10m_count:,} new 10-minute rollup entries (Total: {post_10m_count:,}).")

        # 3. Apply Retention Truncation
        print(f"\n[Step 3/4] Applying Data Retention Policy...")
        latest_ts = db.query(func.max(Logs1s.timestamp)).scalar()
        if not latest_ts:
            latest_ts = db.query(func.max(Logs1m.timestamp)).scalar()
        if not latest_ts:
            latest_ts = datetime.utcnow()

        raw_cutoff = latest_ts - timedelta(hours=raw_retention_hours)
        rollup_cutoff = latest_ts - timedelta(hours=rollup_1m_retention_hours)

        print(f"  Latest Data Timestamp:        {latest_ts.isoformat()}Z")
        print(f"  1-Second Raw Cutoff (-{raw_retention_hours}h):    {raw_cutoff.isoformat()}Z")
        print(f"  1-Minute Rollup Cutoff (-{rollup_1m_retention_hours}h): {rollup_cutoff.isoformat()}Z")

        del_1s = db.query(Logs1s).filter(Logs1s.timestamp < raw_cutoff).delete()
        del_1m = db.query(Logs1m).filter(Logs1m.timestamp < rollup_cutoff).delete()
        db.commit()

        print(f"  -> Pruned {del_1s:,} expired 1-second raw records.")
        print(f"  -> Pruned {del_1m:,} expired 1-minute rollup records.")

        final_1s_count = db.query(Logs1s).count()
        final_1m_count = db.query(Logs1m).count()
        final_10m_count = db.query(Logs10m).count()

    # 4. Reclaim disk space via VACUUM
    print("\n[Step 4/4] Executing VACUUM to reclaim disk space and defragment database...")
    with engine.connect() as conn:
        conn.execution_options(isolation_level="AUTOCOMMIT")
        conn.execute(text("VACUUM;"))

    final_size = os.path.getsize(db_path) if os.path.exists(db_path) else 0

    print(f"\n=== Migration Complete ===")
    print(f"Final DB Size: {final_size / (1024 * 1024):.2f} MB ({final_size:,} bytes) [Space Saved: {(initial_size - final_size) / (1024 * 1024):.2f} MB]")
    print(f"Final Records Retained:")
    print(f"  Logs1s (Last {raw_retention_hours} hours):     {final_1s_count:,}")
    print(f"  Logs1m (Last {rollup_1m_retention_hours} hours):     {final_1m_count:,}")
    print(f"  Logs10m (Long-Term History): {final_10m_count:,}")

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="One-off backfill and retention compaction for reactor logs.")
    parser.add_argument("--db-path", default="./reactor_logs.db", help="Path to SQLite reactor logs database.")
    parser.add_argument("--raw-retention-hours", type=float, default=1.5, help="Hours of 1-second raw data to retain.")
    parser.add_argument("--rollup-1m-retention-hours", type=float, default=8.0, help="Hours of 1-minute rollup data to retain.")
    args = parser.parse_args()

    run_backfill_and_prune(args.db_path, args.raw_retention_hours, args.rollup_1m_retention_hours)

from sqlalchemy import create_engine
from sqlalchemy.orm import DeclarativeBase, Mapped, mapped_column, sessionmaker
from sqlalchemy import text
from datetime import datetime
from .config import settings

# Database Setup
engine = create_engine(settings.DATABASE_URL, connect_args={"check_same_thread": False})
SessionLocal = sessionmaker(autocommit=False, autoflush=False, bind=engine)

class Base(DeclarativeBase):
    pass

from typing import Optional

class ErrorLog(Base):
    __tablename__ = "error_logs"

    id: Mapped[int] = mapped_column(primary_key=True)
    timestamp: Mapped[datetime] = mapped_column(default=datetime.utcnow, index=True)
    sensor: Mapped[str] = mapped_column()
    exact_error: Mapped[str] = mapped_column()
    cleared_timestamp: Mapped[Optional[datetime]] = mapped_column(nullable=True, default=None)

class RunsMetadata(Base):
    __tablename__ = "runs_metadata"
    
    id: Mapped[int] = mapped_column(primary_key=True)
    run_name: Mapped[str] = mapped_column(unique=True)
    start_time: Mapped[datetime] = mapped_column(default=datetime.utcnow)
    status: Mapped[str] = mapped_column(default="active")

class LogMixin:
    # A mixin to share columns between 1s, 1m, and 10m tables
    timestamp: Mapped[datetime] = mapped_column(default=datetime.utcnow, index=True)
    uptime: Mapped[float] = mapped_column()
    
    # State
    control_state: Mapped[int] = mapped_column()

    # Sensors - Temperatures
    temp_feed_res: Mapped[float] = mapped_column()
    temp_feed_pre: Mapped[float] = mapped_column()
    temp_liq_reac: Mapped[float] = mapped_column()
    temp_gas_reac_int: Mapped[float] = mapped_column()
    temp_gas_reac_ext: Mapped[float] = mapped_column()

    # Sensors - Analog
    h2_ppm: Mapped[float] = mapped_column()
    
    # New Columns
    weight: Mapped[float] = mapped_column(default=0.0)
    pump_speed: Mapped[int] = mapped_column(default=0)

    # Actuators (Heater Duty Cycles)
    heater_feed_pre: Mapped[float] = mapped_column()
    heater_liq_reac: Mapped[float] = mapped_column()
    heater_gas_reac: Mapped[float] = mapped_column()

    # Setpoints
    sp_feed_pre: Mapped[float] = mapped_column()
    sp_liq_reac: Mapped[float] = mapped_column()
    sp_gas_reac: Mapped[float] = mapped_column()

class Logs1s(Base, LogMixin):
    __tablename__ = "logs_1s"
    id: Mapped[int] = mapped_column(primary_key=True)

class Logs1m(Base, LogMixin):
    __tablename__ = "logs_1m"
    id: Mapped[int] = mapped_column(primary_key=True)

class Logs10m(Base, LogMixin):
    __tablename__ = "logs_10m"
    id: Mapped[int] = mapped_column(primary_key=True)

def init_db():
    Base.metadata.create_all(bind=engine)

def get_db():
    db = SessionLocal()
    try:
        yield db
    finally:
        db.close()

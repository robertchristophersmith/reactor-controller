from sqlalchemy import create_engine
from sqlalchemy.orm import DeclarativeBase, Mapped, mapped_column, sessionmaker
from datetime import datetime
from .config import settings

# Database Setup
engine = create_engine(settings.DATABASE_URL, connect_args={"check_same_thread": False})
SessionLocal = sessionmaker(autocommit=False, autoflush=False, bind=engine)

class Base(DeclarativeBase):
    pass

class ProcessLog(Base):
    __tablename__ = "process_log"

    id: Mapped[int] = mapped_column(primary_key=True)
    timestamp: Mapped[datetime] = mapped_column(default=datetime.utcnow, index=True)
    uptime: Mapped[float] = mapped_column()
    
    # State
    control_state: Mapped[int] = mapped_column()

    # Sensors - Temperatures
    temp_gas: Mapped[float] = mapped_column()
    temp_feed: Mapped[float] = mapped_column()
    temp_vap: Mapped[float] = mapped_column()
    temp_r_i1: Mapped[float] = mapped_column()
    temp_r_i2: Mapped[float] = mapped_column()
    temp_r_e1: Mapped[float] = mapped_column()
    temp_r_e2: Mapped[float] = mapped_column()

    # Sensors - Analog
    pressure_feed: Mapped[float] = mapped_column()
    pressure_reac: Mapped[float] = mapped_column()
    flow_rate: Mapped[float] = mapped_column()
    h2_ppm: Mapped[float] = mapped_column()

    # Actuators (Heater Duty Cycles)
    heater_gas: Mapped[float] = mapped_column()
    heater_vap: Mapped[float] = mapped_column()
    heater_reac: Mapped[float] = mapped_column()

    # Setpoints
    sp_gas: Mapped[float] = mapped_column()
    sp_vap: Mapped[float] = mapped_column()
    sp_reac: Mapped[float] = mapped_column()

def init_db():
    Base.metadata.create_all(bind=engine)

def get_db():
    db = SessionLocal()
    try:
        yield db
    finally:
        db.close()

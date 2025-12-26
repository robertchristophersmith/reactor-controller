import os

class Settings:
    SERIAL_PORT: str = os.getenv("SERIAL_PORT", "socket://localhost:9999")
    SERIAL_BAUD: int = 115200
    DATABASE_URL: str = "sqlite:///./reactor_logs.db"
    
settings = Settings()

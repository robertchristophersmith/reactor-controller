import os

class Settings:
    import platform
    # Default to socket mock on Windows/Mac (Dev), but real serial on Linux (Pi)
    _default_port = "socket://localhost:9999"
    if platform.system() == "Linux":
        _default_port = "/dev/ttyACM0"
    
    SERIAL_PORT: str = os.getenv("SERIAL_PORT", _default_port)
    SERIAL_BAUD: int = 115200
    DATABASE_URL: str = "sqlite:///./reactor_logs.db"
    
settings = Settings()

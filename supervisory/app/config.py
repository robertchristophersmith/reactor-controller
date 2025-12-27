import os

class Settings:
    import platform
    # Default to socket mock on Windows/Mac (Dev), but real serial on Linux (Pi)
    _default_port = "socket://localhost:9999"
    if platform.system() == "Linux":
         # Try standard Arduino ports
         if os.path.exists("/dev/ttyACM0"):
             _default_port = "/dev/ttyACM0"
         elif os.path.exists("/dev/ttyUSB0"):
             _default_port = "/dev/ttyUSB0"
         else:
             _default_port = "/dev/ttyACM0" # Fallback
    
    SERIAL_PORT: str = os.getenv("SERIAL_PORT", _default_port)
    SERIAL_BAUD: int = 115200
    DATABASE_URL: str = "sqlite:///./reactor_logs.db"
    
settings = Settings()

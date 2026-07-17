FROM python:3.12-slim

WORKDIR /app

# Copy requirements and install dependencies
COPY supervisory/requirements.txt .
RUN pip install --no-cache-dir -r requirements.txt

# Copy the supervisory application files
COPY supervisory/ ./supervisory/

# Expose port 8000 for FastAPI
EXPOSE 8000

# Start the application
CMD ["uvicorn", "supervisory.app.main:app", "--host", "0.0.0.0", "--port", "8000"]

import uvicorn

if __name__ == "__main__":
    # Run the application defined in app/main.py
    # "app.main:app" works because this script is in the 'supervisory' folder,
    # so 'app' is a subpackage available in the path.
    uvicorn.run("app.main:app", host="0.0.0.0", port=8000, reload=True)

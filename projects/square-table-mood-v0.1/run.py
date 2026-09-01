from app import app
import extensions

if __name__ == "__main__":
    print("Square Table AI Mood Controller")
    print("Mood Feed + Local DHT11 enabled")
    print("Serial ports ready")
    app.run(host="127.0.0.1", port=8790, debug=False)

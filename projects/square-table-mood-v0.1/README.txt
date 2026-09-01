# Square Table AI Mood Controller v0.1.1 — correction

This is a correction to the v0.1 controller.

The important change is explicit COM-port selection and a real PySerial connection.

1. Plug in the Pico.
2. Run `startup.bat`.
3. Open http://127.0.0.1:8790
4. Click Refresh.
5. Select COM6 (or whatever Windows assigns).
6. Click Connect.
7. Try a mood button.

The Pico firmware is not changed by this package.

Expected connection log:

Found serial ports: COM6
Opening COM6...
Connected to COM6
Sent: HAPPY

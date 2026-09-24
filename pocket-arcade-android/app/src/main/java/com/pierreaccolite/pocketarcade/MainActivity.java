package com.pierreaccolite.pocketarcade;

import android.app.Activity;
import android.app.AlertDialog;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.hardware.usb.UsbDevice;
import android.hardware.usb.UsbManager;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.view.Gravity;
import android.view.View;
import android.view.Window;
import android.view.WindowManager;
import android.widget.Button;
import android.widget.LinearLayout;
import android.widget.TextView;

import com.hoho.android.usbserial.driver.UsbSerialDriver;
import com.hoho.android.usbserial.driver.UsbSerialPort;
import com.hoho.android.usbserial.driver.UsbSerialProber;
import com.hoho.android.usbserial.util.SerialInputOutputManager;

import java.nio.charset.StandardCharsets;
import java.util.List;
import java.util.Locale;

public class MainActivity extends Activity implements SerialInputOutputManager.Listener {

    private static final String ACTION_USB_PERMISSION =
            "com.pierreaccolite.pocketarcade.USB_PERMISSION";

    private static final int BAUD = 115200;

    private UsbManager usbManager;
    private UsbSerialPort serialPort;
    private SerialInputOutputManager ioManager;

    private final Handler mainHandler = new Handler(Looper.getMainLooper());
    private final StringBuilder rxBuffer = new StringBuilder();

    private TextView connectionText;
    private TextView joystickText;
    private TextView eventText;
    private TextView deviceText;
    private TextView titleText;

    private boolean permissionPending = false;

    private final BroadcastReceiver usbReceiver = new BroadcastReceiver() {
        @Override
        public void onReceive(Context context, Intent intent) {
            if (ACTION_USB_PERMISSION.equals(intent.getAction())) {
                permissionPending = false;
                UsbDevice device = intent.getParcelableExtra(UsbManager.EXTRA_DEVICE);
                boolean granted = intent.getBooleanExtra(
                        UsbManager.EXTRA_PERMISSION_GRANTED, false);

                if (granted && device != null) {
                    connectToDevice(device);
                } else {
                    setConnection("USB permission denied");
                }
            } else if (UsbManager.ACTION_USB_DEVICE_ATTACHED.equals(intent.getAction())) {
                scanAndConnect();
            } else if (UsbManager.ACTION_USB_DEVICE_DETACHED.equals(intent.getAction())) {
                closeSerial();
                setConnection("USB disconnected");
            }
        }
    };

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        requestFullscreen();
        buildUi();

        usbManager = (UsbManager) getSystemService(Context.USB_SERVICE);

        IntentFilter filter = new IntentFilter();
        filter.addAction(ACTION_USB_PERMISSION);
        filter.addAction(UsbManager.ACTION_USB_DEVICE_ATTACHED);
        filter.addAction(UsbManager.ACTION_USB_DEVICE_DETACHED);

        if (android.os.Build.VERSION.SDK_INT >= 33) {
            registerReceiver(usbReceiver, filter, Context.RECEIVER_NOT_EXPORTED);
        } else {
            registerReceiver(usbReceiver, filter);
        }

        scanAndConnect();
    }

    private void requestFullscreen() {
        requestWindowFeature(Window.FEATURE_NO_TITLE);
        getWindow().setFlags(
                WindowManager.LayoutParams.FLAG_FULLSCREEN,
                WindowManager.LayoutParams.FLAG_FULLSCREEN
        );
        getWindow().getDecorView().setSystemUiVisibility(
                View.SYSTEM_UI_FLAG_FULLSCREEN
                        | View.SYSTEM_UI_FLAG_HIDE_NAVIGATION
                        | View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY
                        | View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN
                        | View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION
                        | View.SYSTEM_UI_FLAG_LAYOUT_STABLE
        );
    }

    private TextView label(String text, int size) {
        TextView v = new TextView(this);
        v.setText(text);
        v.setTextColor(0xFFFFFFFF);
        v.setTextSize(size);
        v.setGravity(Gravity.CENTER_VERTICAL);
        v.setPadding(16, 8, 16, 8);
        return v;
    }

    private Button arcadeButton(String text) {
        Button b = new Button(this);
        b.setText(text);
        b.setTextSize(16);
        b.setAllCaps(false);
        b.setTextColor(0xFFFFFFFF);
        b.setOnClickListener(v -> sendCommand(text));
        return b;
    }

    private void buildUi() {
        LinearLayout root = new LinearLayout(this);
        root.setOrientation(LinearLayout.VERTICAL);
        root.setPadding(20, 12, 20, 12);
        root.setBackgroundColor(0xFF05070A);

        titleText = label("POCKET ARCADE", 28);
        titleText.setGravity(Gravity.CENTER);
        root.addView(titleText, new LinearLayout.LayoutParams(
                LinearLayout.LayoutParams.MATCH_PARENT, 60));

        connectionText = label("USB: searching...", 16);
        connectionText.setGravity(Gravity.CENTER);
        root.addView(connectionText, new LinearLayout.LayoutParams(
                LinearLayout.LayoutParams.MATCH_PARENT, 42));

        LinearLayout info = new LinearLayout(this);
        info.setOrientation(LinearLayout.VERTICAL);
        info.setPadding(12, 8, 12, 8);
        info.setBackgroundColor(0xFF111820);

        deviceText = label("Device: none", 16);
        joystickText = label("Joystick: X 0  Y 0  BTN 0", 20);
        eventText = label("Event: waiting", 16);

        info.addView(deviceText);
        info.addView(joystickText);
        info.addView(eventText);

        root.addView(info, new LinearLayout.LayoutParams(
                LinearLayout.LayoutParams.MATCH_PARENT, 150));

        LinearLayout buttons = new LinearLayout(this);
        buttons.setGravity(Gravity.CENTER);
        buttons.addView(arcadeButton("START"), weightParams());
        buttons.addView(arcadeButton("SELECT"), weightParams());
        buttons.addView(arcadeButton("MENU"), weightParams());
        buttons.addView(arcadeButton("BACK"), weightParams());

        root.addView(buttons, new LinearLayout.LayoutParams(
                LinearLayout.LayoutParams.MATCH_PARENT, 72));

        TextView hint = label(
                "USB GAME PLATFORM  •  115200 baud  •  Controller data streams every 50 ms",
                13);
        hint.setGravity(Gravity.CENTER);
        root.addView(hint, new LinearLayout.LayoutParams(
                LinearLayout.LayoutParams.MATCH_PARENT, 48));

        setContentView(root);
    }

    private LinearLayout.LayoutParams weightParams() {
        LinearLayout.LayoutParams p = new LinearLayout.LayoutParams(
                0, LinearLayout.LayoutParams.MATCH_PARENT, 1f);
        p.setMargins(6, 4, 6, 4);
        return p;
    }

    private void scanAndConnect() {
        if (usbManager == null) return;

        List<UsbSerialDriver> drivers =
                UsbSerialProber.getDefaultProber().findAllDrivers(usbManager);

        if (drivers.isEmpty()) {
            setConnection("USB: no supported serial device");
            deviceText.setText("Device: waiting for ESP32");
            return;
        }

        UsbSerialDriver driver = drivers.get(0);
        UsbDevice device = driver.getDevice();

        deviceText.setText(String.format(
                Locale.US,
                "Device: VID %04X  PID %04X  %s",
                device.getVendorId(),
                device.getProductId(),
                device.getDeviceName()));

        if (!usbManager.hasPermission(device)) {
            if (!permissionPending) {
                permissionPending = true;
                Intent permissionIntent = new Intent(ACTION_USB_PERMISSION);
                permissionIntent.setPackage(getPackageName());
                usbManager.requestPermission(device,
                        android.app.PendingIntent.getBroadcast(
                                this,
                                0,
                                permissionIntent,
                                android.app.PendingIntent.FLAG_UPDATE_CURRENT
                                        | android.app.PendingIntent.FLAG_MUTABLE));
            }
            setConnection("USB: requesting permission...");
            return;
        }

        connectToDevice(device);
    }

    private void connectToDevice(UsbDevice device) {
        try {
            UsbSerialDriver driver =
                    UsbSerialProber.getDefaultProber().probeDevice(device);

            if (driver == null || driver.getPorts().isEmpty()) {
                setConnection("USB: no compatible serial port");
                return;
            }

            closeSerial();

            serialPort = driver.getPorts().get(0);
            android.hardware.usb.UsbDeviceConnection connection =
                    usbManager.openDevice(device);

            if (connection == null) {
                setConnection("USB: open failed");
                return;
            }

            serialPort.open(connection);
            serialPort.setParameters(
                    BAUD,
                    8,
                    UsbSerialPort.STOPBITS_1,
                    UsbSerialPort.PARITY_NONE);

            ioManager = new SerialInputOutputManager(serialPort, this);
            ioManager.start();

            setConnection("USB: CONNECTED");
            writeLine("PING");

            mainHandler.postDelayed(() -> {
                writeLine("STATUS");
                writeLine("INPUT");
            }, 300);

        } catch (Exception e) {
            setConnection("USB: " + e.getClass().getSimpleName());
            closeSerial();
        }
    }

    private void writeLine(String command) {
        if (serialPort == null) return;

        try {
            byte[] data = (command + "\n").getBytes(StandardCharsets.UTF_8);
            serialPort.write(data, 1000);
        } catch (Exception e) {
            setConnection("USB write error");
        }
    }

    private void sendCommand(String command) {
        writeLine(command);
        eventText.setText("Command: " + command);
    }

    @Override
    public void onNewData(byte[] data) {
        String chunk = new String(data, StandardCharsets.UTF_8);
        mainHandler.post(() -> processIncoming(chunk));
    }

    private void processIncoming(String chunk) {
        rxBuffer.append(chunk);

        int newline;
        while ((newline = rxBuffer.indexOf("\n")) >= 0) {
            String line = rxBuffer.substring(0, newline).trim();
            rxBuffer.delete(0, newline + 1);

            if (!line.isEmpty()) {
                handleLine(line);
            }
        }
    }

    private void handleLine(String line) {
        if (line.startsWith("READY,")) {
            setConnection("USB: READY");
        } else if (line.equals("PONG")) {
            setConnection("USB: CONNECTED");
        } else if (line.startsWith("STATUS,")) {
            eventText.setText(line);
        } else if (line.startsWith("JOY,")) {
            String[] p = line.split(",");
            if (p.length >= 4) {
                joystickText.setText(String.format(
                        Locale.US,
                        "Joystick: X %s   Y %s   BTN %s",
                        p[1], p[2], p[3]));
            }
        } else if (line.startsWith("EVENT,")) {
            eventText.setText("Event: " + line.substring(6));
        } else if (line.startsWith("HEARTBEAT,")) {
            // Heartbeats prove the transport is alive but do not need to
            // overwrite the main event display.
        } else if (line.startsWith("ERROR,")) {
            eventText.setText(line);
        }
    }

    private void setConnection(String text) {
        if (connectionText != null) {
            connectionText.setText(text);
        }
    }

    @Override
    public void onRunError(Exception e) {
        mainHandler.post(() -> {
            setConnection("USB I/O stopped");
            closeSerial();
        });
    }

    private void closeSerial() {
        if (ioManager != null) {
            try {
                ioManager.stop();
            } catch (Exception ignored) {
            }
            ioManager = null;
        }

        if (serialPort != null) {
            try {
                serialPort.close();
            } catch (Exception ignored) {
            }
            serialPort = null;
        }
    }

    @Override
    protected void onDestroy() {
        super.onDestroy();

        try {
            unregisterReceiver(usbReceiver);
        } catch (Exception ignored) {
        }

        closeSerial();
    }
}

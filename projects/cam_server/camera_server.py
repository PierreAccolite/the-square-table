import os
import time
import threading
import urllib.request
from collections import deque

import cv2
from flask import Flask, Response, jsonify, request, render_template_string

try:
    import mediapipe as mp
    from mediapipe.tasks import python
    from mediapipe.tasks.python import vision
except ImportError:
    mp = None
    python = None
    vision = None

HOST = "0.0.0.0"
PORT = 8080
CAMERA_SCAN_MAX = 10
CAMERA_SCAN_CACHE_SECONDS = 5.0
BASE_DIR = os.path.dirname(os.path.abspath(__file__))
MODEL_FILE = os.path.join(BASE_DIR, "hand_landmarker.task")
MODEL_URL = "https://storage.googleapis.com/mediapipe-models/hand_landmarker/hand_landmarker/float16/1/hand_landmarker.task"

app = Flask(__name__)

state_lock = threading.Lock()
camera_lock = threading.Lock()
frame_lock = threading.Lock()

camera = None
camera_index = None
latest_jpeg = None
frame_sequence = 0
running = True
hand_landmarker = None
last_timestamp_ms = 0
camera_scan_cache = []
camera_scan_time = 0.0

hand_data = {
    "detected": False,
    "x": 0.5,
    "y": 0.5,
    "gesture": "None",
    "finger_count": 0,
    "confidence": 0.0,
    "calibrated_x": 0.5,
    "calibrated_y": 0.5,
    "fps": 0.0,
}
calibration = {"active": False, "x": 0.5, "y": 0.5}

HTML = r"""
<!doctype html>
<html><head>
<meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>Camera + Hand Lab + Flyer</title>
<style>
*{box-sizing:border-box}body{margin:0;background:#101318;color:#eee;font-family:Arial,sans-serif}.wrap{max-width:1200px;margin:auto;padding:18px}h1{margin:0 0 14px;font-size:26px}.tabs{display:flex;gap:8px;margin-bottom:12px;flex-wrap:wrap}button,select,input{font:inherit}button{background:#252c36;color:#fff;border:1px solid #3b4655;border-radius:7px;padding:9px 13px;cursor:pointer}button:hover{background:#303947}button.active{background:#3b4f68}select{background:#181e26;color:#fff;border:1px solid #3b4655;border-radius:7px;padding:9px}.panel{display:none;background:#171c23;border:1px solid #2b333e;border-radius:10px;padding:14px}.panel.active{display:block}.row{display:flex;gap:10px;align-items:center;flex-wrap:wrap;margin-bottom:12px}.status{color:#9fb4c9}#video{display:block;width:100%;max-width:960px;background:#000;border-radius:8px}.grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(180px,1fr));gap:10px}.card{background:#11161c;border:1px solid #2b333e;border-radius:8px;padding:12px}.big{font-size:24px;font-weight:bold;margin-top:4px}canvas{width:100%;max-width:900px;border-radius:8px;background:#8fd3ff;display:block}.small{font-size:13px;color:#9ba7b4}label{display:flex;gap:8px;align-items:center}input[type=range]{width:180px}
</style></head><body><div class="wrap">
<h1>Camera + Hand Lab + Flyer</h1>
<div class="tabs"><button class="tab active" data-tab="cameraPanel">Camera</button><button class="tab" data-tab="handPanel">Hand Lab</button><button class="tab" data-tab="gamePanel">Flyer</button></div>
<div id="cameraPanel" class="panel active"><div class="row"><select id="cameraSelect"></select><button id="refreshBtn">Refresh cameras</button><button id="connectBtn">Connect</button><button id="disconnectBtn">Disconnect</button><span id="cameraStatus" class="status">Ready.</span></div><img id="video" src="/video_feed" alt="Camera feed"><p class="small">The server listens on port 8080. Other machines on the LAN can use the server's IP address.</p></div>
<div id="handPanel" class="panel"><div class="row"><button id="calibrateBtn">Calibrate neutral position</button><span id="calStatus" class="status">Move your hand into a comfortable neutral position, then calibrate.</span></div><div class="grid"><div class="card">Detected<div id="detected" class="big">No</div></div><div class="card">Gesture<div id="gesture" class="big">None</div></div><div class="card">Fingers<div id="fingers" class="big">0</div></div><div class="card">X<div id="x" class="big">0.50</div></div><div class="card">Y<div id="y" class="big">0.50</div></div><div class="card">Confidence<div id="confidence" class="big">0%</div></div><div class="card">Server FPS<div id="fps" class="big">0</div></div></div></div>
<div id="gamePanel" class="panel"><div class="row"><button id="startGame">Start / Restart</button><label>Sensitivity <input id="sensitivity" type="range" min="50" max="200" value="100"><span id="sensValue">100%</span></label><span class="status">Open Palm / Thumbs Up = flap. Hand Y = altitude.</span></div><canvas id="game" width="900" height="600"></canvas><p class="small">Keyboard SPACE also works as a backup control during testing.</p></div>
</div>
<script>
const $=id=>document.getElementById(id);
document.querySelectorAll('.tab').forEach(b=>b.onclick=()=>{document.querySelectorAll('.tab').forEach(x=>x.classList.remove('active'));document.querySelectorAll('.panel').forEach(x=>x.classList.remove('active'));b.classList.add('active');$(b.dataset.tab).classList.add('active')});

async function loadCameras(){try{const r=await fetch('/api/cameras',{cache:'no-store'});const d=await r.json();const s=$('cameraSelect');s.innerHTML='';d.cameras.forEach(c=>{const o=document.createElement('option');o.value=c.index;o.textContent=c.name;s.appendChild(o)});if(d.cameras.length){if(d.current!==null&&d.current!==undefined&&d.cameras.some(c=>c.index===d.current))s.value=d.current;else s.value=d.cameras[0].index} $('cameraStatus').textContent=d.current!==null&&d.current!==undefined?'Camera connected.':'Ready. Select a camera and press Connect.'}catch(e){console.error(e);$('cameraStatus').textContent='Could not scan cameras.'}}
async function selectCamera(){const value=$('cameraSelect').value;if(value===''){ $('cameraStatus').textContent='Please select a camera.';return }$('cameraStatus').textContent='Connecting...';try{const r=await fetch('/select_camera',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({camera:value})});const d=await r.json();$('cameraStatus').textContent=d.ok?'Connected to camera '+value:d.error}catch(e){$('cameraStatus').textContent='Camera connection failed.'}}
async function disconnectCamera(){ $('cameraStatus').textContent='Disconnecting...';try{const r=await fetch('/disconnect',{method:'POST'});const d=await r.json();$('cameraStatus').textContent=d.ok?'Disconnected.':d.error}catch(e){$('cameraStatus').textContent='Disconnect failed.'}}
$('refreshBtn').onclick=loadCameras;$('connectBtn').onclick=selectCamera;$('disconnectBtn').onclick=disconnectCamera;
$('calibrateBtn').onclick=async()=>{const r=await fetch('/calibrate',{method:'POST'});const d=await r.json();$('calStatus').textContent=d.ok?'Calibration captured.':'No hand detected - try again.'};
async function pollHand(){try{const d=await(await fetch('/api/hand',{cache:'no-store'})).json();$('detected').textContent=d.detected?'Yes':'No';$('gesture').textContent=d.gesture;$('fingers').textContent=d.finger_count;$('x').textContent=d.calibrated_x.toFixed(2);$('y').textContent=d.calibrated_y.toFixed(2);$('confidence').textContent=Math.round(d.confidence*100)+'%';$('fps').textContent=d.fps.toFixed(1);game.handY=d.calibrated_y;game.gesture=d.gesture;game.detected=d.detected}catch(e){}}
setInterval(pollHand,80);

const canvas=$('game'),ctx=canvas.getContext('2d');
const game={running:false,over:false,score:0,best:Number(localStorage.getItem('flyerBest')||0),x:180,y:300,vy:0,gravity:.38,flap:-7.2,pipes:[],speed:3.2,spawn:0,lastGesture:'None',handY:.5,gesture:'None',detected:false,sensitivity:1};
function resetGame(){game.running=true;game.over=false;game.score=0;game.x=180;game.y=300;game.vy=0;game.pipes=[];game.spawn=0;game.lastGesture='None'}
function flap(){if(game.over||!game.running){resetGame();return}game.vy=game.flap}
$('startGame').onclick=resetGame;$('sensitivity').oninput=e=>{$('sensValue').textContent=e.target.value+'%';game.sensitivity=Number(e.target.value)/100};document.addEventListener('keydown',e=>{if(e.code==='Space'){e.preventDefault();flap()}});
function addPipe(){const gap=155,top=90+Math.random()*260;game.pipes.push({x:canvas.width+30,top:top,bottom:top+gap,passed:false})}
function collide(p){const r=18;return game.x+r>p.x&&game.x-r<p.x+65&&(game.y-r<p.top||game.y+r>p.bottom)}
function endGame(){game.over=true;game.running=false;if(game.score>game.best){game.best=game.score;localStorage.setItem('flyerBest',game.best)}}
function update(){if(!game.running||game.over)return;const target=100+game.handY*400;if(game.detected)game.y=game.y+(target-game.y)*.18*game.sensitivity;else{game.vy+=game.gravity;game.y+=game.vy}game.spawn--;if(game.spawn<=0){addPipe();game.spawn=105}for(const p of game.pipes){p.x-=game.speed;if(!p.passed&&p.x+65<game.x){p.passed=true;game.score++}if(collide(p))endGame()}game.pipes=game.pipes.filter(p=>p.x>-90);if(game.y<18||game.y>canvas.height-18)endGame();if(game.detected&&(game.gesture==='Open Palm'||game.gesture==='Thumbs Up')&&game.lastGesture!=='Open Palm'&&game.lastGesture!=='Thumbs Up')flap();game.lastGesture=game.gesture}
function draw(){ctx.clearRect(0,0,canvas.width,canvas.height);const grd=ctx.createLinearGradient(0,0,0,canvas.height);grd.addColorStop(0,'#8fd3ff');grd.addColorStop(1,'#d9f3ff');ctx.fillStyle=grd;ctx.fillRect(0,0,canvas.width,canvas.height);ctx.fillStyle='#7dbb55';ctx.fillRect(0,canvas.height-35,canvas.width,35);ctx.fillStyle='#4b8f3c';ctx.fillRect(0,canvas.height-35,canvas.width,5);ctx.fillStyle='#3e9b55';for(const p of game.pipes){ctx.fillRect(p.x,0,65,p.top);ctx.fillRect(p.x,p.bottom,65,canvas.height-p.bottom-35);ctx.fillRect(p.x-6,p.top-14,77,14);ctx.fillRect(p.x-6,p.bottom,77,14)}ctx.fillStyle='#f3c542';ctx.beginPath();ctx.arc(game.x,game.y,18,0,Math.PI*2);ctx.fill();ctx.fillStyle='#fff';ctx.beginPath();ctx.arc(game.x+7,game.y-6,6,0,Math.PI*2);ctx.fill();ctx.fillStyle='#111';ctx.beginPath();ctx.arc(game.x+9,game.y-6,2.5,0,Math.PI*2);ctx.fill();ctx.fillStyle='#e87922';ctx.beginPath();ctx.moveTo(game.x+17,game.y);ctx.lineTo(game.x+31,game.y+5);ctx.lineTo(game.x+17,game.y+9);ctx.fill();ctx.fillStyle='#10202b';ctx.font='bold 28px Arial';ctx.fillText('Score: '+game.score,20,40);ctx.font='18px Arial';ctx.fillText('Best: '+game.best,20,66);if(!game.running){ctx.fillStyle='rgba(0,0,0,.45)';ctx.fillRect(0,0,canvas.width,canvas.height);ctx.fillStyle='#fff';ctx.textAlign='center';ctx.font='bold 42px Arial';ctx.fillText(game.over?'Game Over':'Flappy-style Flyer',canvas.width/2,250);ctx.font='22px Arial';ctx.fillText(game.over?'Press Start / Restart or SPACE':'Press Start / Restart',canvas.width/2,295);ctx.textAlign='left'}}
function loop(){update();draw();requestAnimationFrame(loop)}
loadCameras();pollHand();loop();
</script></body></html>
"""


def download_model():
    if os.path.exists(MODEL_FILE):
        return True
    print("MediaPipe hand model not found.")
    print("Downloading hand_landmarker.task ...")
    try:
        urllib.request.urlretrieve(MODEL_URL, MODEL_FILE)
        print("Hand model downloaded.")
        return True
    except Exception as exc:
        print(f"Could not download hand model: {exc}")
        return False


def init_hand_landmarker():
    global hand_landmarker
    if mp is None or python is None or vision is None:
        print("MediaPipe is not installed.")
        return False
    if not download_model():
        return False
    try:
        base_options = python.BaseOptions(model_asset_path=MODEL_FILE)
        options = vision.HandLandmarkerOptions(
            base_options=base_options,
            running_mode=vision.RunningMode.VIDEO,
            num_hands=1,
            min_hand_detection_confidence=0.5,
            min_hand_presence_confidence=0.5,
            min_tracking_confidence=0.5,
        )
        hand_landmarker = vision.HandLandmarker.create_from_options(options)
        print("MediaPipe Hand Landmarker ready.")
        return True
    except Exception as exc:
        print(f"MediaPipe initialization failed: {exc}")
        hand_landmarker = None
        return False


def open_camera(index):
    global camera, camera_index, latest_jpeg, frame_sequence, camera_scan_time
    try:
        index = int(index)
    except (TypeError, ValueError):
        return False

    with camera_lock:
        if camera is not None and camera.isOpened() and camera_index == index:
            print(f"Camera {index} is already connected; leaving it open.")
            return True

        if camera is not None:
            print(f"Releasing camera {camera_index}...")
            try:
                camera.release()
            except Exception:
                pass
            camera = None
            camera_index = None

        print(f"Opening camera {index}...")
        new_camera = cv2.VideoCapture(index, cv2.CAP_DSHOW)
        if not new_camera.isOpened():
            new_camera.release()
            print(f"Could not open camera {index}.")
            return False

        new_camera.set(cv2.CAP_PROP_FRAME_WIDTH, 1280)
        new_camera.set(cv2.CAP_PROP_FRAME_HEIGHT, 720)
        new_camera.set(cv2.CAP_PROP_FPS, 30)
        camera = new_camera
        camera_index = index

        with frame_lock:
            latest_jpeg = None
            frame_sequence += 1

        camera_scan_time = 0.0
        print(f"Camera {index} connected.")
        return True


def close_camera():
    global camera, camera_index, latest_jpeg, frame_sequence, camera_scan_time
    with camera_lock:
        if camera is not None:
            print(f"Closing camera {camera_index}...")
            try:
                camera.release()
            except Exception:
                pass
        camera = None
        camera_index = None
        with frame_lock:
            latest_jpeg = None
            frame_sequence += 1
        camera_scan_time = 0.0


def scan_cameras(force=False):
    global camera_scan_cache, camera_scan_time
    now = time.monotonic()
    with camera_lock:
        current = camera_index
        if not force and now - camera_scan_time < CAMERA_SCAN_CACHE_SECONDS:
            return list(camera_scan_cache), current

        cameras = []
        for index in range(CAMERA_SCAN_MAX):
            if current is not None and index == current:
                cameras.append({"index": index, "name": f"Camera {index}"})
                continue
            test = None
            try:
                test = cv2.VideoCapture(index, cv2.CAP_DSHOW)
                if test.isOpened():
                    cameras.append({"index": index, "name": f"Camera {index}"})
            except Exception as exc:
                print(f"Camera scan error {index}: {exc}")
            finally:
                if test is not None:
                    try:
                        test.release()
                    except Exception:
                        pass

        camera_scan_cache = cameras
        camera_scan_time = now
        return list(cameras), current


def clamp(v, lo=0.0, hi=1.0):
    return max(lo, min(hi, v))


def distance(a, b):
    dx = a.x - b.x
    dy = a.y - b.y
    return (dx * dx + dy * dy) ** 0.5


def classify_hand(landmarks):
    wrist = landmarks[0]
    thumb_tip, thumb_mcp = landmarks[4], landmarks[2]
    index_tip, index_pip = landmarks[8], landmarks[6]
    middle_tip, middle_pip = landmarks[12], landmarks[10]
    ring_tip, ring_pip = landmarks[16], landmarks[14]
    pinky_tip, pinky_pip = landmarks[20], landmarks[18]

    fingers = 0
    if distance(index_tip, wrist) > distance(index_pip, wrist) * 1.12: fingers += 1
    if distance(middle_tip, wrist) > distance(middle_pip, wrist) * 1.12: fingers += 1
    if distance(ring_tip, wrist) > distance(ring_pip, wrist) * 1.12: fingers += 1
    if distance(pinky_tip, wrist) > distance(pinky_pip, wrist) * 1.12: fingers += 1

    thumb_extended = distance(thumb_tip, wrist) > distance(thumb_mcp, wrist) * 1.12
    if thumb_extended: fingers += 1

    if thumb_extended and fingers == 1:
        gesture = "Thumbs Up" if thumb_tip.y < thumb_mcp.y - 0.06 else "Thumb"
    elif fingers == 5:
        gesture = "Open Palm"
    elif fingers == 0:
        gesture = "Fist"
    elif fingers == 2 and distance(index_tip, middle_tip) < distance(index_tip, wrist) * 0.45:
        gesture = "Peace"
    elif fingers == 1:
        gesture = "Point"
    else:
        gesture = f"{fingers} Fingers"
    return gesture, fingers


def process_frame(frame):
    global last_timestamp_ms
    if hand_landmarker is None:
        return frame

    rgb = cv2.cvtColor(frame, cv2.COLOR_BGR2RGB)
    image = mp.Image(image_format=mp.ImageFormat.SRGB, data=rgb)
    timestamp_ms = int(time.monotonic() * 1000)
    if timestamp_ms <= last_timestamp_ms:
        timestamp_ms = last_timestamp_ms + 1
    last_timestamp_ms = timestamp_ms

    try:
        result = hand_landmarker.detect_for_video(image, timestamp_ms)
    except Exception as exc:
        print(f"Hand detection error: {exc}")
        return frame

    if not result.hand_landmarks:
        with state_lock:
            hand_data.update({"detected": False, "gesture": "None", "finger_count": 0, "confidence": 0.0})
        return frame

    landmarks = result.hand_landmarks[0]
    index_tip = landmarks[8]
    x, y = clamp(index_tip.x), clamp(index_tip.y)
    gesture, fingers = classify_hand(landmarks)
    try:
        confidence = float(result.handedness[0][0].score)
    except Exception:
        confidence = 1.0

    with state_lock:
        cal_x, cal_y = calibration["x"], calibration["y"]

    calibrated_x = clamp(0.5 + (x - cal_x) * 1.8)
    calibrated_y = clamp(0.5 + (y - cal_y) * 1.8)

    with state_lock:
        hand_data.update({"detected": True, "x": x, "y": y, "gesture": gesture, "finger_count": fingers, "confidence": confidence, "calibrated_x": calibrated_x, "calibrated_y": calibrated_y})

    h, w = frame.shape[:2]
    for lm in landmarks:
        cv2.circle(frame, (int(clamp(lm.x) * w), int(clamp(lm.y) * h)), 4, (0, 255, 0), -1)
    cv2.circle(frame, (int(x * w), int(y * h)), 10, (0, 200, 255), 2)
    cv2.putText(frame, f"{gesture}  {confidence:.0%}", (20, 40), cv2.FONT_HERSHEY_SIMPLEX, 1, (0, 255, 255), 2, cv2.LINE_AA)
    return frame


def camera_worker():
    global latest_jpeg, frame_sequence
    fps_times = deque(maxlen=30)

    while running:
        frame = None
        with camera_lock:
            cam = camera
            if cam is not None:
                try:
                    ok, frame = cam.read()
                except Exception as exc:
                    print(f"Camera read error: {exc}")
                    ok, frame = False, None
                if not ok:
                    frame = None
                if frame is not None:
                    frame = cv2.flip(frame, 1)
                    frame = process_frame(frame)
                    now = time.monotonic()
                    fps_times.append(now)
                    if len(fps_times) >= 2:
                        elapsed = fps_times[-1] - fps_times[0]
                        fps = (len(fps_times) - 1) / elapsed if elapsed > 0 else 0.0
                        with state_lock:
                            hand_data["fps"] = fps
                    ok, encoded = cv2.imencode('.jpg', frame, [int(cv2.IMWRITE_JPEG_QUALITY), 82])
                    if ok:
                        with frame_lock:
                            latest_jpeg = encoded.tobytes()
                            frame_sequence += 1

        time.sleep(0.001 if frame is not None else 0.05)


def mjpeg_generator():
    last_sent = -1
    while running:
        with frame_lock:
            data = latest_jpeg
            sequence = frame_sequence
        if data is None:
            time.sleep(0.05)
            continue
        if sequence == last_sent:
            time.sleep(0.005)
            continue
        last_sent = sequence
        yield (b'--frame\r\nContent-Type: image/jpeg\r\nCache-Control: no-cache, no-store, must-revalidate\r\nPragma: no-cache\r\n\r\n' + data + b'\r\n')


@app.route('/')
def index():
    return render_template_string(HTML)


@app.route('/api/cameras')
def api_cameras():
    cameras, current = scan_cameras()
    return jsonify({"cameras": cameras, "current": current})


@app.post('/select_camera')
def select_camera():
    data = request.get_json(silent=True) or {}
    try:
        index = int(data['camera'])
    except (KeyError, TypeError, ValueError):
        return jsonify({"ok": False, "error": "Invalid camera index."}), 400
    if not open_camera(index):
        return jsonify({"ok": False, "error": f"Could not open camera {index}."}), 400
    return jsonify({"ok": True, "camera": index})


@app.post('/disconnect')
def disconnect():
    close_camera()
    with state_lock:
        hand_data.update({"detected": False, "gesture": "None", "finger_count": 0, "confidence": 0.0, "fps": 0.0})
    return jsonify({"ok": True})


@app.get('/api/hand')
def api_hand():
    with state_lock:
        return jsonify(dict(hand_data))


@app.post('/calibrate')
def calibrate():
    with state_lock:
        if not hand_data['detected']:
            return jsonify({"ok": False, "error": "No hand detected."}), 400
        calibration['x'] = hand_data['x']
        calibration['y'] = hand_data['y']
        calibration['active'] = True
    return jsonify({"ok": True})


@app.get('/video_feed')
def video_feed():
    return Response(mjpeg_generator(), mimetype='multipart/x-mixed-replace; boundary=frame', headers={'Cache-Control': 'no-cache, no-store, must-revalidate', 'Pragma': 'no-cache', 'Expires': '0'})


def main():
    print('=' * 60)
    print('Camera + Hand Lab + Flyer')
    print('=' * 60)
    if mp is None:
        print('ERROR: MediaPipe is not installed.')
        print('Run: py -m pip install mediapipe')
        return
    if not init_hand_landmarker():
        print('WARNING: Hand tracking is unavailable.')
        print('The camera server will still start.')

    # Deliberately do NOT open Camera 0 here.
    # The web interface owns camera connection/disconnection.
    print('Camera is waiting for web interface connection.')
    threading.Thread(target=camera_worker, daemon=True).start()
    print(f'Open locally: http://127.0.0.1:{PORT}')
    print(f'LAN access:   http://<THIS-PC-IP>:{PORT}')
    print('Press Ctrl+C to stop.')
    try:
        app.run(host=HOST, port=PORT, threaded=True, debug=False, use_reloader=False)
    finally:
        close_camera()


if __name__ == '__main__':
    main()

import os, time, threading, urllib.request, math
import cv2
from flask import Flask, Response, jsonify, request, render_template_string

try:
    import mediapipe as mp
    from mediapipe.tasks import python
    from mediapipe.tasks.python import vision
except ImportError:
    mp = python = vision = None

HOST, PORT = "0.0.0.0", 8080
BASE_DIR = os.path.dirname(os.path.abspath(__file__))
HAND_MODEL = os.path.join(BASE_DIR, "hand_landmarker.task")
FACE_MODEL = os.path.join(BASE_DIR, "face_landmarker.task")
HAND_URL = "https://storage.googleapis.com/mediapipe-models/hand_landmarker/hand_landmarker/float16/1/hand_landmarker.task"
FACE_URL = "https://storage.googleapis.com/mediapipe-models/face_landmarker/face_landmarker/float16/1/face_landmarker.task"

app = Flask(__name__)
camera_lock = threading.Lock()
frame_lock = threading.Lock()
state_lock = threading.Lock()
camera = None
camera_index = None
latest_jpeg = None
frame_sequence = 0
running = True
hand_landmarker = face_landmarker = None
last_timestamp_ms = 0
scan_cache, scan_time = [], 0.0
fps = 0.0
fps_frames, fps_time = 0, time.monotonic()
calibration = {"active": False, "x": 0.5, "y": 0.5}

def blank_hand():
    return {"detected": False, "label": "Unknown", "x": 0.5, "y": 0.5, "pinch": False, "pinch_distance": 0.0, "fingers": 0, "gesture": "None", "roll": 0.0, "landmarks": []}

hand_data = {"detected": False, "hands_detected": 0, "x": 0.5, "y": 0.5, "gesture": "None", "finger_count": 0, "confidence": 0.0, "calibrated_x": 0.5, "calibrated_y": 0.5, "fps": 0.0, "left": None, "right": None, "face_detected": False, "blink_left": False, "blink_right": False, "blink": False, "wink": "None", "mouth_open": False, "smile": 0.0, "head_yaw": 0.0, "head_pitch": 0.0, "head_roll": 0.0, "head_direction": "Centre"}

def get_model(path, url, name):
    if os.path.exists(path): return True
    print(f"{name} missing; downloading...")
    try:
        urllib.request.urlretrieve(url, path)
        print(f"{name} ready.")
        return True
    except Exception as e:
        print(f"{name} download failed: {e}")
        return False

def init_trackers():
    global hand_landmarker, face_landmarker
    if not mp:
        print("MediaPipe is not installed.")
        return False
    if get_model(HAND_MODEL, HAND_URL, "Hand model"):
        try:
            hand_landmarker = vision.HandLandmarker.create_from_options(vision.HandLandmarkerOptions(base_options=python.BaseOptions(model_asset_path=HAND_MODEL), running_mode=vision.RunningMode.VIDEO, num_hands=2, min_hand_detection_confidence=0.5, min_hand_presence_confidence=0.5, min_tracking_confidence=0.5))
            print("Hand Landmarker ready: 2 hands.")
        except Exception as e: print(f"Hand tracker failed: {e}")
    if get_model(FACE_MODEL, FACE_URL, "Face model"):
        try:
            face_landmarker = vision.FaceLandmarker.create_from_options(vision.FaceLandmarkerOptions(base_options=python.BaseOptions(model_asset_path=FACE_MODEL), running_mode=vision.RunningMode.VIDEO, num_faces=1, min_face_detection_confidence=0.5, min_face_presence_confidence=0.5, min_tracking_confidence=0.5, output_face_blendshapes=True))
            print("Face Landmarker ready.")
        except Exception as e: print(f"Face tracker failed: {e}")
    return bool(hand_landmarker or face_landmarker)

def open_camera(index):
    global camera, camera_index, latest_jpeg, frame_sequence, scan_time
    try: index = int(index)
    except (TypeError, ValueError): return False
    with camera_lock:
        if camera is not None and camera.isOpened() and camera_index == index: return True
        if camera is not None:
            try: camera.release()
            except Exception: pass
        camera = cv2.VideoCapture(index, cv2.CAP_DSHOW)
        if not camera.isOpened():
            camera.release(); camera = None; camera_index = None; return False
        camera.set(cv2.CAP_PROP_FRAME_WIDTH, 1280); camera.set(cv2.CAP_PROP_FRAME_HEIGHT, 720); camera.set(cv2.CAP_PROP_FPS, 30)
        camera_index = index
        with frame_lock: latest_jpeg = None; frame_sequence += 1
        scan_time = 0.0
        print(f"Camera {index} connected.")
        return True

def close_camera():
    global camera, camera_index, latest_jpeg, frame_sequence, scan_time
    with camera_lock:
        if camera is not None:
            try: camera.release()
            except Exception: pass
        camera = camera_index = None
        with frame_lock: latest_jpeg = None; frame_sequence += 1
        scan_time = 0.0

def scan_cameras(force=False):
    global scan_cache, scan_time
    now = time.monotonic()
    with camera_lock:
        current = camera_index
        if not force and now - scan_time < 5: return list(scan_cache), current
        found = [{"index": current, "name": f"Camera {current}"}] if current is not None else []
        for i in range(10):
            if i == current: continue
            p = cv2.VideoCapture(i, cv2.CAP_DSHOW)
            if p.isOpened(): found.append({"index": i, "name": f"Camera {i}"})
            p.release()
        found.sort(key=lambda x:x["index"]); scan_cache, scan_time = found, now
        return list(found), current

def dist(a,b): return math.sqrt((a.x-b.x)**2+(a.y-b.y)**2+(a.z-b.z)**2)

def hand_info(lm, label):
    s={"index":lm[8].y<lm[6].y,"middle":lm[12].y<lm[10].y,"ring":lm[16].y<lm[14].y,"pinky":lm[20].y<lm[18].y}
    s["thumb"]=dist(lm[4],lm[9])>dist(lm[3],lm[9])*1.08
    count=sum(s.values()); ratio=dist(lm[4],lm[8])/max(dist(lm[0],lm[9]),.001); pinch=ratio<.42
    if pinch: gesture="Pinch"
    elif count==0: gesture="Fist"
    elif count==5: gesture="Open Palm"
    elif s["index"] and not s["middle"] and not s["ring"] and not s["pinky"]: gesture="Point"
    elif s["index"] and s["middle"] and not s["ring"] and not s["pinky"]: gesture="Peace"
    elif s["thumb"] and count==1: gesture="Thumbs Up"
    else: gesture=f"{count} Fingers"
    return {"detected":True,"label":label,"x":sum(p.x for p in lm)/21,"y":sum(p.y for p in lm)/21,"pinch":pinch,"pinch_distance":round(ratio,3),"fingers":count,"gesture":gesture,"roll":round(math.degrees(math.atan2(lm[17].y-lm[5].y,lm[17].x-lm[5].x)),1),"landmarks":[{"x":round(p.x,4),"y":round(p.y,4)} for p in lm]}

def pose(face,w,h):
    import numpy as np
    ids=[1,33,263,61,291,152]; img=np.array([[face[i].x*w,face[i].y*h] for i in ids],dtype="double")
    model=np.array([[0,0,0],[-45,-32,35],[45,-32,35],[-35,35,20],[35,35,20],[0,65,5]],dtype="double"); f=float(w); cam=np.array([[f,0,w/2],[0,f,h/2],[0,0,1]],dtype="double")
    try:
        ok,rv,_=cv2.solvePnP(model,img,cam,np.zeros((4,1)),flags=cv2.SOLVEPNP_ITERATIVE)
        if not ok:return 0,0,0
        r,_=cv2.Rodrigues(rv); sy=math.sqrt(r[0,0]**2+r[1,0]**2)
        return math.degrees(math.atan2(r[1,0],r[0,0])), math.degrees(math.atan2(-r[2,0],sy)), math.degrees(math.atan2(r[2,1],r[2,2]))
    except Exception:return 0,0,0

def blend(cats,name):
    for c in cats:
        if c.category_name==name:return float(c.score)
    return 0.0

def process(frame):
    global hand_data,last_timestamp_ms
    rgb=cv2.cvtColor(frame,cv2.COLOR_BGR2RGB); image=mp.Image(image_format=mp.ImageFormat.SRGB,data=rgb); ts=max(int(time.monotonic()*1000),last_timestamp_ms+1); last_timestamp_ms=ts
    hr=hand_landmarker.detect_for_video(image,ts) if hand_landmarker else None; fr=face_landmarker.detect_for_video(image,ts) if face_landmarker else None; hands=[]
    if hr and hr.hand_landmarks:
        for i,lm in enumerate(hr.hand_landmarks[:2]):
            label=hr.handedness[i][0].category_name if hr.handedness else "Unknown"; hands.append(hand_info(lm,label))
            for p in lm: cv2.circle(frame,(int(p.x*frame.shape[1]),int(p.y*frame.shape[0])),3,(0,220,80),-1)
    left=next((x for x in hands if x["label"]=="Left"),None); right=next((x for x in hands if x["label"]=="Right"),None); primary=right or left or (hands[0] if hands else None)
    fs={"face_detected":False,"blink_left":False,"blink_right":False,"blink":False,"wink":"None","mouth_open":False,"smile":0.0,"head_yaw":0.0,"head_pitch":0.0,"head_roll":0.0,"head_direction":"Centre"}
    if fr and fr.face_landmarks:
        face=fr.face_landmarks[0]; fs["face_detected"]=True; yaw,pitch,roll=pose(face,frame.shape[1],frame.shape[0]); fs.update(head_yaw=round(yaw,1),head_pitch=round(pitch,1),head_roll=round(roll,1))
        if yaw < -12: fs["head_direction"]="Left"
        elif yaw > 12: fs["head_direction"]="Right"
        elif pitch < -10: fs["head_direction"]="Up"
        elif pitch > 10: fs["head_direction"]="Down"
        def ear(ids):
            p=[face[i] for i in ids]; return (dist(p[1],p[5])+dist(p[2],p[4]))/(2*max(dist(p[0],p[3]),.0001))
        le,re=ear([33,160,158,133,153,144]),ear([362,385,387,263,373,380]); fs["blink_left"]=le<.18; fs["blink_right"]=re<.18; fs["blink"]=fs["blink_left"] and fs["blink_right"]
        if fs["blink_left"]!=fs["blink_right"]: fs["wink"]="Left" if fs["blink_left"] else "Right"
        if fr.face_blendshapes:
            cats=fr.face_blendshapes[0]; fs["mouth_open"]=blend(cats,"jawOpen")>.35; fs["smile"]=round(max(blend(cats,"mouthSmileLeft"),blend(cats,"mouthSmileRight")),3)
        for i in [1,33,263,61,291,152]:
            p=face[i]; cv2.circle(frame,(int(p.x*frame.shape[1]),int(p.y*frame.shape[0])),3,(255,180,0),-1)
    with state_lock:
        d=dict(hand_data); d.update(fs); d["hands_detected"]=len(hands); d["detected"]=bool(hands); d["left"],d["right"]=left,right
        if primary:
            d.update(x=primary["x"],y=primary["y"],gesture=primary["gesture"],finger_count=primary["fingers"],confidence=1.0)
            d["calibrated_x"]=max(0,min(1,primary["x"]-calibration["x"]+.5)) if calibration["active"] else primary["x"]; d["calibrated_y"]=max(0,min(1,primary["y"]-calibration["y"]+.5)) if calibration["active"] else primary["y"]
        else: d.update(gesture="None",finger_count=0,confidence=0.0)
        d["fps"]=fps; hand_data=d
    return frame

def worker():
    global latest_jpeg,frame_sequence,fps,fps_frames,fps_time
    while running:
        with camera_lock:
            cam=camera
            if cam is None or not cam.isOpened(): frame=enc=None
            else:
                ok,frame=cam.read()
                if ok:
                    frame=cv2.flip(frame,1); frame=process(frame); ok,enc=cv2.imencode(".jpg",frame,[int(cv2.IMWRITE_JPEG_QUALITY),82])
                else: enc=None
        if frame is not None and enc is not None and ok:
            with frame_lock: latest_jpeg=enc.tobytes(); frame_sequence+=1
            fps_frames+=1; now=time.monotonic()
            if now-fps_time>=1: fps=fps_frames/(now-fps_time); fps_frames=0; fps_time=now
        else: time.sleep(.05)

def stream():
    last=-1
    while running:
        with frame_lock: seq,jpeg=frame_sequence,latest_jpeg
        if jpeg is not None and seq!=last:
            last=seq; yield b"--frame\r\nContent-Type: image/jpeg\r\nCache-Control: no-cache\r\n\r\n"+jpeg+b"\r\n"
        else: time.sleep(.01)

@app.route("/")
def index(): return render_template_string(HTML)
@app.route("/api/cameras")
def cameras(): c,i=scan_cameras(); return jsonify(cameras=c,current=i)
@app.route("/select_camera",methods=["POST"])
def select():
    i=(request.get_json(silent=True) or {}).get("camera")
    if i is None:return jsonify(ok=False,error="No camera selected."),400
    ok=open_camera(i); return (jsonify(ok=True,camera=int(i)) if ok else jsonify(ok=False,error=f"Could not open camera {i}.")),200 if ok else 500
@app.route("/disconnect",methods=["POST"])
def disconnect(): close_camera(); return jsonify(ok=True)
@app.route("/api/hand")
def api_hand():
    with state_lock:return jsonify(dict(hand_data))
@app.route("/calibrate",methods=["POST"])
def calibrate():
    with state_lock:
        p=hand_data.get("right") or hand_data.get("left")
        if not p:return jsonify(ok=False)
        calibration.update(active=True,x=p["x"],y=p["y"]); hand_data["calibrated_x"]=hand_data["calibrated_y"]=.5
    return jsonify(ok=True)
@app.route("/video_feed")
def video_feed():return Response(stream(),mimetype="multipart/x-mixed-replace; boundary=frame",headers={"Cache-Control":"no-cache,no-store,must-revalidate","Pragma":"no-cache"})

HTML = r'''<!doctype html><html><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>Square Table Human Tracker</title><style>*{box-sizing:border-box}body{margin:0;background:#101318;color:#eee;font-family:Arial,sans-serif}.wrap{max-width:1280px;margin:auto;padding:18px}h1{margin:0 0 14px}.tabs{display:flex;gap:8px;flex-wrap:wrap;margin-bottom:12px}button,select,input{font:inherit}button{background:#252c36;color:#fff;border:1px solid #3b4655;border-radius:7px;padding:9px 13px;cursor:pointer}button.active{background:#3b4f68}select{background:#181e26;color:#fff;border:1px solid #3b4655;border-radius:7px;padding:9px}.panel{display:none;background:#171c23;border:1px solid #2b333e;border-radius:10px;padding:14px}.panel.active{display:block}.row{display:flex;gap:10px;align-items:center;flex-wrap:wrap;margin-bottom:12px}.status{color:#9fb4c9}#video{display:block;width:100%;max-width:1100px;background:#000;border-radius:8px}.grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(155px,1fr));gap:10px}.card,.hand{background:#11161c;border:1px solid #2b333e;border-radius:8px;padding:12px}.big{font-size:22px;font-weight:bold;margin-top:4px}.mono{font-family:Consolas,monospace;font-size:14px;line-height:1.5}.small{font-size:13px;color:#9ba7b4}label{display:flex;gap:8px;align-items:center}input[type=range]{width:180px}canvas{width:100%;max-width:900px;border-radius:8px;display:block}</style></head><body><div class="wrap"><h1>Square Table — Camera + Human Tracker</h1><div class="tabs"><button class="tab active" data-tab="cam">Camera</button><button class="tab" data-tab="lab">Human Lab</button><button class="tab" data-tab="fly">Flyer</button></div><div id="cam" class="panel active"><div class="row"><select id="cams"></select><button id="refresh">Refresh cameras</button><button id="connect">Connect</button><button id="disconnect">Disconnect</button><span id="status" class="status">Ready.</span></div><img id="video" src="/video_feed" alt="Camera feed"><p class="small">Two-hand + face tracking runs server-side. Refresh never probes the active camera.</p></div><div id="lab" class="panel"><div class="row"><button id="cal">Calibrate neutral hand position</button><span id="calstatus" class="status">Centre the primary hand before calibrating.</span></div><div class="grid"><div class="card">Hands<div id="hands" class="big">0</div></div><div class="card">Primary gesture<div id="gesture" class="big">None</div></div><div class="card">Fingers<div id="fingers" class="big">0</div></div><div class="card">X<div id="x" class="big">0.50</div></div><div class="card">Y<div id="y" class="big">0.50</div></div><div class="card">FPS<div id="fps" class="big">0</div></div></div><div class="hand"><b>Left hand</b><div id="left" class="mono">Not detected</div></div><br><div class="hand"><b>Right hand</b><div id="right" class="mono">Not detected</div></div><br><div class="hand"><b>Head / Face</b><div class="grid"><div class="card">Face<div id="face" class="big">No</div></div><div class="card">Blink<div id="blink" class="big">No</div></div><div class="card">Wink<div id="wink" class="big">None</div></div><div class="card">Direction<div id="dir" class="big">Centre</div></div><div class="card">Yaw<div id="yaw" class="big">0°</div></div><div class="card">Pitch<div id="pitch" class="big">0°</div></div><div class="card">Roll<div id="roll" class="big">0°</div></div><div class="card">Mouth<div id="mouth" class="big">Closed</div></div><div class="card">Smile<div id="smile" class="big">0%</div></div></div></div></div><div id="fly" class="panel"><div class="row"><button id="start">Start / Restart</button><label>Sensitivity <input id="sens" type="range" min="50" max="200" value="100"><span id="sensv">100%</span></label><span class="status">Primary hand Y = altitude; Open Palm / Thumbs Up = flap.</span></div><canvas id="game" width="900" height="600"></canvas><p class="small">SPACE is also available.</p></div></div><script>const $=id=>document.getElementById(id);document.querySelectorAll('.tab').forEach(b=>b.onclick=()=>{document.querySelectorAll('.tab').forEach(x=>x.classList.remove('active'));document.querySelectorAll('.panel').forEach(x=>x.classList.remove('active'));b.classList.add('active');$(b.dataset.tab).classList.add('active')});function ht(h){return h?`Gesture: ${h.gesture} | Fingers: ${h.fingers} | Pinch: ${h.pinch?'YES':'No'} | Ratio: ${h.pinch_distance.toFixed(3)} | X: ${h.x.toFixed(2)} Y: ${h.y.toFixed(2)} Roll: ${h.roll.toFixed(1)}°`:'Not detected'}async function cams(){try{let d=await(await fetch('/api/cameras',{cache:'no-store'})).json(),s=$('cams');s.innerHTML='';d.cameras.forEach(c=>{let o=document.createElement('option');o.value=c.index;o.textContent=c.name;s.appendChild(o)});if(d.cameras.length)s.value=d.current??d.cameras[0].index;$('status').textContent=d.current!=null?'Camera connected.':'Ready.'}catch(e){$('status').textContent='Could not scan cameras.'}}$('refresh').onclick=cams;$('connect').onclick=async()=>{let v=$('cams').value;if(v==='')return;$('status').textContent='Connecting...';let d=await(await fetch('/select_camera',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({camera:v})})).json();$('status').textContent=d.ok?'Connected to camera '+v:d.error};$('disconnect').onclick=async()=>{await fetch('/disconnect',{method:'POST'});$('status').textContent='Disconnected.'};$('cal').onclick=async()=>{$('calstatus').textContent=(await(await fetch('/calibrate',{method:'POST'})).json()).ok?'Calibration captured.':'No hand detected.'};function poll(){fetch('/api/hand',{cache:'no-store'}).then(r=>r.json()).then(d=>{$('hands').textContent=d.hands_detected;$('gesture').textContent=d.gesture;$('fingers').textContent=d.finger_count;$('x').textContent=d.calibrated_x.toFixed(2);$('y').textContent=d.calibrated_y.toFixed(2);$('fps').textContent=d.fps.toFixed(1);$('left').textContent=ht(d.left);$('right').textContent=ht(d.right);$('face').textContent=d.face_detected?'Yes':'No';$('blink').textContent=d.blink?'YES':'No';$('wink').textContent=d.wink;$('dir').textContent=d.head_direction;$('yaw').textContent=d.head_yaw.toFixed(1)+'°';$('pitch').textContent=d.head_pitch.toFixed(1)+'°';$('roll').textContent=d.head_roll.toFixed(1)+'°';$('mouth').textContent=d.mouth_open?'Open':'Closed';$('smile').textContent=Math.round(d.smile*100)+'%';g.yh=d.calibrated_y;g.g=d.gesture;g.detected=d.detected}).catch(()=>{})}setInterval(poll,80);const c=$('game'),ctx=c.getContext('2d'),g={run:false,over:false,x:180,y:300,vy:0,yh:.5,g:'None',detected:false,score:0,best:Number(localStorage.getItem('flyerBest')||0),pipes:[],spawn:0,sens:1,last:'None'};function reset(){g.run=true;g.over=false;g.score=0;g.x=180;g.y=300;g.vy=0;g.pipes=[];g.spawn=0;g.last='None'}function flap(){if(g.over||!g.run){reset();return}g.vy=-7.2}$('start').onclick=reset;$('sens').oninput=e=>{$('sensv').textContent=e.target.value+'%';g.sens=Number(e.target.value)/100};document.addEventListener('keydown',e=>{if(e.code==='Space'){e.preventDefault();flap()}});function upd(){if(!g.run)return;let target=100+g.yh*400;if(g.detected)g.y+=(target-g.y)*.18*g.sens;else{g.vy+=.38;g.y+=g.vy}if(--g.spawn<=0){let t=90+Math.random()*260;g.pipes.push({x:c.width+30,t,b:t+155,p:false});g.spawn=105}for(let p of g.pipes){p.x-=3.2;if(!p.p&&p.x+65<g.x){p.p=true;g.score++}if(g.x+18>p.x&&g.x-18<p.x+65&&(g.y-18<p.t||g.y+18>p.b))g.over=g.run=false}g.pipes=g.pipes.filter(p=>p.x>-90);if(g.y<18||g.y>582)g.over=g.run=false;if(g.detected&&(g.g==='Open Palm'||g.g==='Thumbs Up')&&g.last!=='Open Palm'&&g.last!=='Thumbs Up')flap();g.last=g.g}function draw(){ctx.clearRect(0,0,c.width,c.height);let q=ctx.createLinearGradient(0,0,0,c.height);q.addColorStop(0,'#8fd3ff');q.addColorStop(1,'#d9f3ff');ctx.fillStyle=q;ctx.fillRect(0,0,c.width,c.height);ctx.fillStyle='#7dbb55';ctx.fillRect(0,565,900,35);for(let p of g.pipes){ctx.fillRect(p.x,0,65,p.t);ctx.fillRect(p.x,p.b,65,565-p.b)}ctx.fillStyle='#f3c542';ctx.beginPath();ctx.arc(g.x,g.y,18,0,7);ctx.fill();ctx.fillStyle='#10202b';ctx.font='bold 28px Arial';ctx.fillText('Score: '+g.score,20,40);if(!g.run){ctx.fillStyle='rgba(0,0,0,.45)';ctx.fillRect(0,0,900,600);ctx.fillStyle='#fff';ctx.textAlign='center';ctx.font='bold 42px Arial';ctx.fillText(g.over?'Game Over':'Flappy-style Flyer',450,260);ctx.font='22px Arial';ctx.fillText(g.over?'Press Start / Restart or SPACE':'Press Start / Restart',450,305);ctx.textAlign='left'}}function loop(){upd();draw();requestAnimationFrame(loop)}cams();poll();loop();</script></body></html>'''

if __name__ == "__main__":
    print("="*60)
    print("Square Table Camera + Human Tracker")
    print("="*60)
    print("Camera waits for web interface connection.")
    init_trackers()
    threading.Thread(target=worker,daemon=True).start()
    try:
        app.run(host=HOST,port=PORT,threaded=True,debug=False,use_reloader=False)
    finally:
        running=False
        close_camera()

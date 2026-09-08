import os, time, math, threading, urllib.request, ctypes
from collections import deque

import cv2
from flask import Flask, Response, jsonify, request, render_template_string

try:
    import mediapipe as mp
    from mediapipe.tasks import python
    from mediapipe.tasks.python import vision
except ImportError:
    mp = python = vision = None

HOST, PORT = "0.0.0.0", 8080
BASE = os.path.dirname(os.path.abspath(__file__))
HAND_MODEL = os.path.join(BASE, "hand_landmarker.task")
FACE_MODEL = os.path.join(BASE, "face_landmarker.task")
HAND_URL = "https://storage.googleapis.com/mediapipe-models/hand_landmarker/hand_landmarker/float16/1/hand_landmarker.task"
FACE_URL = "https://storage.googleapis.com/mediapipe-models/face_landmarker/face_landmarker/float16/1/face_landmarker.task"

app = Flask(__name__)
cam_lock = threading.Lock()
frame_lock = threading.Lock()
state_lock = threading.Lock()

camera = None
camera_index = None
latest_jpeg = None
frame_seq = 0
running = True
hand_tracker = face_tracker = None
ts = 0
fps = 0.0
fps_samples = deque(maxlen=30)
cal = {"active": False, "x": 0.5, "y": 0.5}
mouse_enabled = False
mouse_down = False

ball = {"x": 0.5, "y": 0.5, "vx": 0.0, "vy": 0.0, "held": False,
        "holder": "None", "lastx": 0.5, "lasty": 0.5, "lastt": 0.0}

state = {
    "detected": False, "hands_detected": 0, "gesture": "None", "finger_count": 0,
    "confidence": 0.0, "x": 0.5, "y": 0.5, "calibrated_x": 0.5, "calibrated_y": 0.5,
    "fps": 0.0, "left": None, "right": None, "face_detected": False,
    "blink_left": False, "blink_right": False, "blink": False, "wink": "None",
    "mouth_open": False, "smile": 0.0, "head_yaw": 0.0, "head_pitch": 0.0,
    "head_roll": 0.0, "head_direction": "Centre", "mouse_enabled": False,
    "mouse_down": False,
}

HTML = r'''<!doctype html>
<html><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>Square Table Human Interface</title>
<style>
*{box-sizing:border-box}body{margin:0;background:#101318;color:#eee;font-family:Arial,sans-serif}.wrap{max-width:1250px;margin:auto;padding:18px}h1{margin:0 0 12px}.tabs,.row{display:flex;gap:8px;align-items:center;flex-wrap:wrap;margin-bottom:12px}button,select{font:inherit;background:#252c36;color:#fff;border:1px solid #3b4655;border-radius:7px;padding:9px 13px}button{cursor:pointer}button.active{background:#3b4f68}.panel{display:none;background:#171c23;border:1px solid #2b333e;border-radius:10px;padding:14px}.panel.active{display:block}.status{color:#9fb4c9}.warn{color:#ffd166}.small{font-size:13px;color:#9ba7b4}.grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(170px,1fr));gap:10px}.card{background:#11161c;border:1px solid #2b333e;border-radius:8px;padding:12px}.big{font-size:22px;font-weight:bold;margin-top:4px}#video{display:block;width:100%;max-width:1100px;background:#000;border-radius:8px}canvas{width:100%;max-width:900px;border-radius:8px;display:block;background:#0c0f13}.hint{padding:10px 12px;background:#11161c;border-radius:8px;margin:10px 0}.green{color:#8ee7a1}
</style></head><body><div class="wrap">
<h1>Square Table — Human Interface</h1>
<div class="tabs">
<button class="tab active" data-p="camera">Camera</button><button class="tab" data-p="lab">Human Lab</button><button class="tab" data-p="objects">Objects</button><button class="tab" data-p="flyer">Flyer</button><button class="tab" data-p="mouse">Windows Mouse</button>
</div>
<div id="camera" class="panel active"><div class="row"><select id="cams"></select><button id="refresh">Refresh cameras</button><button id="connect">Connect</button><button id="disconnect">Disconnect</button><span id="cs" class="status">Ready.</span></div><img id="video" src="/video_feed"><p class="small">Live image shows both-hand landmarks, gesture labels, face/head indicators and the throwable ball. Pinch the ball to grab it, move, then release to throw.</p></div>
<div id="lab" class="panel"><div class="row"><button id="cal">Calibrate neutral hand</button><span id="calstatus" class="status">Optional.</span></div><div class="grid"><div class="card">Hands<div id="hands" class="big">0</div></div><div class="card">Primary gesture<div id="gesture" class="big">None</div></div><div class="card">Left hand<div id="left" class="big">—</div></div><div class="card">Right hand<div id="right" class="big">—</div></div><div class="card">Left pinch<div id="lp" class="big">—</div></div><div class="card">Right pinch<div id="rp" class="big">—</div></div><div class="card">Head<div id="head" class="big">Centre</div></div><div class="card">Yaw / Pitch / Roll<div id="angles" class="big">0 / 0 / 0</div></div><div class="card">Blink<div id="blink" class="big">No</div></div><div class="card">Wink<div id="wink" class="big">None</div></div><div class="card">Mouth<div id="mouth" class="big">Closed</div></div><div class="card">Smile<div id="smile" class="big">0%</div></div></div></div>
<div id="objects" class="panel"><div class="hint"><b>Two-hand object manipulation</b><br><span class="green">Pinch one hand</span> to grab and move the object. <span class="green">Pinch both hands</span> and move them apart to <b>stretch</b>, together to compress, and around each other to rotate. Release both hands to let it go.</div><div class="row"><button id="resetObject">Reset object</button><span id="objstatus" class="status">Waiting for hands.</span></div><div class="grid"><div class="card">Mode<div id="objmode" class="big">Idle</div></div><div class="card">Length<div id="objlen" class="big">100%</div></div><div class="card">Rotation<div id="objrot" class="big">0°</div></div><div class="card">Hands<div id="objhands" class="big">0</div></div></div><canvas id="objectCanvas" width="900" height="560"></canvas></div>
<div id="flyer" class="panel"><button id="start">Start / Restart</button><span class="status"> Open Palm / Thumbs Up = flap; hand Y controls altitude.</span><canvas id="game" width="900" height="600"></canvas></div>
<div id="mouse" class="panel"><div class="row"><button id="mtoggle">Enable Windows Mouse</button><span id="ms" class="status">Mouse control is OFF.</span></div><div class="grid"><div class="card">Pointer<div id="ptr" class="big">0%, 0%</div></div><div class="card">Action<div id="act" class="big">Idle</div></div><div class="card">Pinch<div id="mp" class="big">Open</div></div></div><p class="small warn">OFF by default. When enabled: move hand = pointer, pinch = left-click/hold, move while pinched = drag, release = drop.</p></div>
</div>
<script>
const $=x=>document.getElementById(x);
document.querySelectorAll('.tab').forEach(b=>b.onclick=()=>{document.querySelectorAll('.tab').forEach(x=>x.classList.remove('active'));document.querySelectorAll('.panel').forEach(x=>x.classList.remove('active'));b.classList.add('active');$(b.dataset.p).classList.add('active')});
async function load(){try{let d=await(await fetch('/api/cameras')).json(),s=$('cams');s.innerHTML='';d.cameras.forEach(c=>{let o=document.createElement('option');o.value=c.index;o.textContent=c.name;s.appendChild(o)});if(d.current!=null)s.value=d.current;$('cs').textContent=d.current!=null?'Camera connected.':'Ready.'}catch(e){$('cs').textContent='Camera scan failed.'}}
$('refresh').onclick=load;$('connect').onclick=async()=>{let d=await(await fetch('/select_camera',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({camera:$('cams').value})})).json();$('cs').textContent=d.ok?'Connected.':d.error};$('disconnect').onclick=async()=>{await fetch('/disconnect',{method:'POST'});$('cs').textContent='Disconnected.'};$('cal').onclick=async()=>{let d=await(await fetch('/calibrate',{method:'POST'})).json();$('calstatus').textContent=d.ok?'Calibration captured.':'No hand detected.'};
let mouseEnabled=false;$('mtoggle').onclick=async()=>{let d=await(await fetch('/mouse_mode',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({enabled:!mouseEnabled})})).json();mouseEnabled=d.enabled;$('mtoggle').textContent=mouseEnabled?'Disable Windows Mouse':'Enable Windows Mouse';$('ms').textContent=mouseEnabled?'Mouse control ENABLED.':'Mouse control is OFF.'};

const oc=$('objectCanvas'),octx=oc.getContext('2d');let obj={cx:.5,cy:.5,len:.30,thick:.10,angle:0,baseDist:null,baseAngle:null,baseLen:null,drag:false};
function resetObj(){obj={cx:.5,cy:.5,len:.30,thick:.10,angle:0,baseDist:null,baseAngle:null,baseLen:null,drag:false}}
$('resetObject').onclick=()=>resetObj();
function normAngle(a){while(a>Math.PI)a-=Math.PI*2;while(a<-Math.PI)a+=Math.PI*2;return a}
function updateObject(d){let hands=[d.left,d.right].filter(Boolean),pinched=hands.filter(h=>h.pinch);$('objhands').textContent=hands.length;if(pinched.length===2){let a=pinched[0],b=pinched[1],dx=b.x-a.x,dy=b.y-a.y,dist=Math.hypot(dx,dy),ang=Math.atan2(dy,dx),cx=(a.x+b.x)/2,cy=(a.y+b.y)/2;if(obj.baseDist===null){obj.baseDist=Math.max(dist,.04);obj.baseAngle=ang;obj.baseLen=obj.len}obj.cx=cx;obj.cy=cy;obj.len=Math.max(.10,Math.min(.90,obj.baseLen*(dist/obj.baseDist)));obj.angle=obj.baseAngle+normAngle(ang-obj.baseAngle);obj.drag=true;$('objmode').textContent='STRETCH + ROTATE';$('objstatus').textContent='Two-hand pinch active — spread or twist your hands.'}else if(pinched.length===1){let h=pinched[0];if(!obj.drag){obj.drag=true}obj.cx=h.x;obj.cy=h.y;obj.baseDist=obj.baseAngle=obj.baseLen=null;$('objmode').textContent='GRAB / MOVE';$('objstatus').textContent='One-hand pinch — moving object.'}else{obj.baseDist=obj.baseAngle=obj.baseLen=null;obj.drag=false;$('objmode').textContent='Idle';$('objstatus').textContent='Pinch one or both hands to manipulate.'}}
function drawObject(){octx.clearRect(0,0,900,560);octx.fillStyle='#0d1117';octx.fillRect(0,0,900,560);octx.strokeStyle='#252d38';octx.lineWidth=1;for(let x=0;x<=900;x+=45){octx.beginPath();octx.moveTo(x,0);octx.lineTo(x,560);octx.stroke()}for(let y=0;y<=560;y+=45){octx.beginPath();octx.moveTo(0,y);octx.lineTo(900,y);octx.stroke()}let cx=obj.cx*900,cy=obj.cy*560,L=obj.len*900,W=Math.max(32,obj.thick*560);octx.save();octx.translate(cx,cy);octx.rotate(obj.angle);octx.fillStyle='#3e86ff';octx.strokeStyle='#b9d4ff';octx.lineWidth=3;octx.beginPath();octx.roundRect(-L/2,-W/2,L,W,Math.min(W/2,18));octx.fill();octx.stroke();octx.fillStyle='#fff';octx.font='bold 20px Arial';octx.textAlign='center';octx.fillText('STRETCH ME',0,7);octx.restore();octx.fillStyle='#9ba7b4';octx.font='14px Arial';octx.fillText('Pinch both ends with two hands',20,28)}
function objectTick(){let d=window.lastHandData;if(d)updateObject(d);drawObject();let pct=Math.round(obj.len/.30*100);$('objlen').textContent=pct+'%';$('objrot').textContent=Math.round(obj.angle*180/Math.PI)+'°'}

async function poll(){try{let d=await(await fetch('/api/hand',{cache:'no-store'})).json();window.lastHandData=d;$('hands').textContent=d.hands_detected;$('gesture').textContent=d.gesture;$('left').textContent=d.left?d.left.gesture:'—';$('right').textContent=d.right?d.right.gesture:'—';$('lp').textContent=d.left?(d.left.pinch?'PINCHED':'Open'):'—';$('rp').textContent=d.right?(d.right.pinch?'PINCHED':'Open'):'—';$('head').textContent=d.head_direction;$('angles').textContent=d.head_yaw.toFixed(0)+' / '+d.head_pitch.toFixed(0)+' / '+d.head_roll.toFixed(0);$('blink').textContent=d.blink?'YES':'No';$('wink').textContent=d.wink;$('mouth').textContent=d.mouth_open?'Open':'Closed';$('smile').textContent=Math.round(d.smile*100)+'%';$('ptr').textContent=Math.round(d.x*100)+'%, '+Math.round(d.y*100)+'%';$('act').textContent=d.mouse_down?'DRAGGING':'Idle';$('mp').textContent=(d.right&&d.right.pinch)||(d.left&&d.left.pinch)?'PINCHED':'Open';game.handY=d.calibrated_y;game.detected=d.detected;game.gesture=d.gesture}catch(e){}}
setInterval(poll,70);

const cv=$('game'),ctx=cv.getContext('2d'),game={running:false,over:false,x:180,y:300,vy:0,score:0,pipes:[],spawn:0,last:'None',handY:.5,detected:false,gesture:'None'};function reset(){Object.assign(game,{running:true,over:false,x:180,y:300,vy:0,score:0,pipes:[],spawn:0,last:'None'})}function flap(){if(!game.running||game.over){reset();return}game.vy=-7.2}$('start').onclick=reset;document.onkeydown=e=>{if(e.code==='Space'){e.preventDefault();flap()}};function tick(){if(!game.running)return;game.y=game.detected?game.y+(100+game.handY*400-game.y)*.18:game.y+(game.vy+=.38);if(--game.spawn<=0){let t=90+Math.random()*260;game.pipes.push({x:930,t,b:t+155,hit:false});game.spawn=105}game.pipes.forEach(p=>{p.x-=3.2;if(!p.hit&&p.x+65<game.x){p.hit=true;game.score++}if(game.x+18>p.x&&game.x-18<p.x+65&&(game.y-18<p.t||game.y+18>p.b))game.over=true});game.pipes=game.pipes.filter(p=>p.x>-80);if(game.y<18||game.y>582)game.over=true;if(game.detected&&(game.gesture==='Open Palm'||game.gesture==='Thumbs Up')&&game.last!=='Open Palm'&&game.last!=='Thumbs Up')flap();game.last=game.gesture}
function draw(){ctx.clearRect(0,0,900,600);ctx.fillStyle='#9ddcff';ctx.fillRect(0,0,900,600);ctx.fillStyle='#65a34b';ctx.fillRect(0,565,900,35);ctx.fillStyle='#3e8b3b';game.pipes.forEach(p=>{ctx.fillRect(p.x,0,65,p.t);ctx.fillRect(p.x,p.b,65,565-p.b)});ctx.fillStyle='#f4c542';ctx.beginPath();ctx.arc(game.x,game.y,18,0,7);ctx.fill();ctx.fillStyle='#111';ctx.font='bold 28px Arial';ctx.fillText('Score: '+game.score,20,40);if(!game.running||game.over){ctx.fillStyle='rgba(0,0,0,.45)';ctx.fillRect(0,0,900,600);ctx.fillStyle='#fff';ctx.textAlign='center';ctx.font='bold 40px Arial';ctx.fillText(game.over?'Game Over':'Flyer',450,270);ctx.font='20px Arial';ctx.fillText('Start / Restart or SPACE',450,310);ctx.textAlign='left'}}
function loop(){tick();objectTick();draw();requestAnimationFrame(loop)}load();poll();loop();
</script></body></html>'''


def model(path, url, name):
    if os.path.exists(path):
        return True
    print(name + " missing; downloading...")
    try:
        urllib.request.urlretrieve(url, path)
        print(name + " ready.")
        return True
    except Exception as e:
        print(name + " download failed:", e)
        return False


def init():
    global hand_tracker, face_tracker
    if not mp:
        print("MediaPipe is not installed.")
        return False
    if model(HAND_MODEL, HAND_URL, "Hand model"):
        try:
            hand_tracker = vision.HandLandmarker.create_from_options(
                vision.HandLandmarkerOptions(
                    base_options=python.BaseOptions(model_asset_path=HAND_MODEL),
                    running_mode=vision.RunningMode.VIDEO, num_hands=2,
                    min_hand_detection_confidence=.5, min_hand_presence_confidence=.5,
                    min_tracking_confidence=.5))
            print("Hand tracker ready: 2 hands.")
        except Exception as e:
            print("Hand tracker failed:", e)
    if model(FACE_MODEL, FACE_URL, "Face model"):
        try:
            face_tracker = vision.FaceLandmarker.create_from_options(
                vision.FaceLandmarkerOptions(
                    base_options=python.BaseOptions(model_asset_path=FACE_MODEL),
                    running_mode=vision.RunningMode.VIDEO, num_faces=1,
                    min_face_detection_confidence=.5, min_face_presence_confidence=.5,
                    min_tracking_confidence=.5, output_face_blendshapes=True))
            print("Face tracker ready.")
        except Exception as e:
            print("Face tracker failed:", e)
    return bool(hand_tracker or face_tracker)


def clamp(v):
    return max(0.0, min(1.0, float(v)))


def D(a, b):
    return math.hypot(a.x - b.x, a.y - b.y)


def hand(lm, label):
    wrist = lm[0]
    fs = sum(D(lm[t], wrist) > D(lm[p], wrist) * 1.12 for t, p in ((8,6),(12,10),(16,14),(20,18)))
    thumb = D(lm[4], wrist) > D(lm[2], wrist) * 1.12
    fs += thumb
    ratio = D(lm[4], lm[8]) / max(D(lm[0], lm[9]), .001)
    pinch = ratio < .42
    if pinch: g = "Pinch"
    elif fs == 5: g = "Open Palm"
    elif fs == 0: g = "Fist"
    elif fs == 1: g = "Thumbs Up" if thumb and lm[4].y < lm[2].y else "Point"
    elif fs == 2: g = "Peace"
    else: g = f"{fs} Fingers"
    return {"detected": True, "label": label, "x": clamp(sum(p.x for p in lm)/21),
            "y": clamp(sum(p.y for p in lm)/21), "pinch": pinch,
            "pinch_distance": round(ratio, 3), "fingers": fs, "gesture": g,
            "roll": round(math.degrees(math.atan2(lm[17].y-lm[5].y, lm[17].x-lm[5].x)),1),
            "landmarks": [{"x":round(p.x,4),"y":round(p.y,4)} for p in lm]}


def blend(cats, name):
    return next((float(x.score) for x in cats if x.category_name == name), 0.0)


def pose(face, w, h):
    import numpy as np
    ids = [1,33,263,61,291,152]
    img = np.array([[face[i].x*w, face[i].y*h] for i in ids], dtype="double")
    obj = np.array([[0,0,0],[-45,-32,35],[45,-32,35],[-35,35,20],[35,35,20],[0,65,5]], dtype="double")
    f = float(w); cam = np.array([[f,0,w/2],[0,f,h/2],[0,0,1]], dtype="double")
    try:
        ok, rv, _ = cv2.solvePnP(obj, img, cam, np.zeros((4,1)), flags=cv2.SOLVEPNP_ITERATIVE)
        if not ok: return 0.,0.,0.
        r,_=cv2.Rodrigues(rv); sy=math.sqrt(r[0,0]**2+r[1,0]**2)
        return math.degrees(math.atan2(r[1,0],r[0,0])), math.degrees(math.atan2(-r[2,0],sy)), math.degrees(math.atan2(r[2,1],r[2,2]))
    except Exception:
        return 0.,0.,0.


def mouse_move(x,y):
    try:
        u=ctypes.windll.user32
        u.SetCursorPos(int(clamp(x)*(u.GetSystemMetrics(0)-1)), int(clamp(y)*(u.GetSystemMetrics(1)-1)))
    except Exception:
        pass


def mouse_btn(down):
    global mouse_down
    try:
        ctypes.windll.user32.mouse_event(2 if down else 4,0,0,0,0)
        mouse_down=down
    except Exception:
        mouse_down=down


def process(frame):
    global ts, fps
    h,w=frame.shape[:2]; rgb=cv2.cvtColor(frame,cv2.COLOR_BGR2RGB)
    hands=[]; face_data=None
    ts += 33
    if hand_tracker:
        try:
            result=hand_tracker.detect_for_video(mp.Image(image_format=mp.ImageFormat.SRGB,data=rgb),ts)
            for i,lm in enumerate(result.hand_landmarks):
                label="Right"
                if result.handedness and i < len(result.handedness):
                    label=result.handedness[i][0].display_name or result.handedness[i][0].category_name or label
                hands.append(hand(lm,label))
                pts=[(int(p.x*w),int(p.y*h)) for p in lm]
                for a,b in ((0,1),(1,2),(2,3),(3,4),(0,5),(5,6),(6,7),(7,8),(5,9),(9,10),(10,11),(11,12),(9,13),(13,14),(14,15),(15,16),(13,17),(17,18),(18,19),(19,20)):
                    cv2.line(frame,pts[a],pts[b],(90,210,255),2)
                for p in pts: cv2.circle(frame,p,3,(255,255,255),-1)
        except Exception:
            pass
    if face_tracker:
        try:
            fr=face_tracker.detect_for_video(mp.Image(image_format=mp.ImageFormat.SRGB,data=rgb),ts)
            if fr.face_landmarks:
                f=fr.face_landmarks[0]; yaw,pitch,roll=pose(f,w,h)
                cats=fr.face_blendshapes[0] if fr.face_blendshapes else []
                bl=blend(cats,"eyeBlinkLeft"); br=blend(cats,"eyeBlinkRight")
                face_data={"face_detected":True,"blink_left":bl>.55,"blink_right":br>.55,"blink":bl>.55 and br>.55,"wink":"Left" if bl>.55 and br<.25 else ("Right" if br>.55 and bl<.25 else "None"),"mouth_open":blend(cats,"jawOpen")>.35,"smile":max(blend(cats,"mouthSmileLeft"),blend(cats,"mouthSmileRight")),"head_yaw":yaw,"head_pitch":pitch,"head_roll":roll,"head_direction":"Left" if yaw < -15 else ("Right" if yaw > 15 else ("Up" if pitch < -12 else ("Down" if pitch > 12 else "Centre")))}
        except Exception:
            face_data=None
    left=next((x for x in hands if x["label"].lower().startswith("left")),None)
    right=next((x for x in hands if x["label"].lower().startswith("right")),None)
    if left is None and right is None and hands: right=hands[0]
    primary=(right or left)
    if primary:
        if cal["active"]:
            cx=clamp(primary["x"]-cal["x"]+.5); cy=clamp(primary["y"]-cal["y"]+.5)
        else: cx,cy=primary["x"],primary["y"]
    else: cx=cy=.5
    with state_lock:
        state.update({"detected":bool(hands),"hands_detected":len(hands),"gesture":primary["gesture"] if primary else "None","finger_count":primary["fingers"] if primary else 0,"confidence":1.0 if hands else 0.0,"x":cx,"y":cy,"calibrated_x":cx,"calibrated_y":cy,"fps":fps,"left":left,"right":right})
        if face_data: state.update(face_data)
        else: state.update({"face_detected":False,"blink_left":False,"blink_right":False,"blink":False,"wink":"None","mouth_open":False,"smile":0.0})
        state["mouse_enabled"]=mouse_enabled; state["mouse_down"]=mouse_down
    if mouse_enabled and primary:
        mouse_move(cx,cy)
        if primary["pinch"] and not mouse_down: mouse_btn(True)
        elif not primary["pinch"] and mouse_down: mouse_btn(False)
    update_ball(hands)
    return frame


def update_ball(hands):
    now=time.time(); pinched=[h for h in hands if h["pinch"]]
    if ball["held"]:
        holder=next((h for h in pinched if h["label"]==ball["holder"]),None)
        if holder:
            dt=max(now-ball["lastt"],.001); ball["vx"]=(holder["x"]-ball["lastx"])/dt; ball["vy"]=(holder["y"]-ball["lasty"])/dt; ball["x"],ball["y"]=holder["x"],holder["y"]; ball["lastx"],ball["lasty"],ball["lastt"]=ball["x"],ball["y"],now
        else:
            ball["held"]=False; ball["holder"]="None"
    elif pinched:
        h=pinched[0]
        if math.hypot(h["x"]-ball["x"],h["y"]-ball["y"])<.08:
            ball["held"]=True; ball["holder"]=h["label"]; ball["lastx"],ball["lasty"],ball["lastt"]=h["x"],h["y"],now
    else:
        ball["x"]+=ball["vx"]*.033; ball["y"]+=ball["vy"]*.033; ball["vy"]+=.7
        if ball["x"]<.03 or ball["x"]>.97: ball["vx"]*=-.8; ball["x"]=clamp(ball["x"])
        if ball["y"]<.03 or ball["y"]>.97: ball["vy"]*=-.8; ball["y"]=clamp(ball["y"])
        ball["vx"]*=.995; ball["vy"]*=.995


def camera_loop():
    global camera, latest_jpeg, frame_seq, fps
    while running:
        with cam_lock: cap=camera
        if cap is None:
            time.sleep(.05); continue
        ok,frame=cap.read()
        if not ok:
            time.sleep(.02); continue
        t=time.time()
        if hasattr(camera_loop,"last"):
            dt=t-camera_loop.last
            if dt>0: fps=.9*fps+.1*(1/dt)
        camera_loop.last=t
        frame=process(frame)
        cv2.circle(frame,(int(ball["x"]*frame.shape[1]),int(ball["y"]*frame.shape[0])),18,(0,220,255),3)
        if ball["held"]: cv2.putText(frame,"HELD",(int(ball["x"]*frame.shape[1])+20,int(ball["y"]*frame.shape[0])),cv2.FONT_HERSHEY_SIMPLEX,.6,(0,220,255),2)
        ok,jpeg=cv2.imencode('.jpg',frame,[int(cv2.IMWRITE_JPEG_QUALITY),82])
        if ok:
            with frame_lock: latest_jpeg=jpeg.tobytes(); frame_seq+=1


def camera_candidates():
    out=[]
    for i in range(10):
        cap=cv2.VideoCapture(i,cv2.CAP_DSHOW if os.name=='nt' else 0)
        ok=cap.isOpened(); cap.release()
        if ok: out.append({"index":i,"name":f"Camera {i}"})
    return out

@app.get('/')
def index(): return render_template_string(HTML)

@app.get('/api/cameras')
def api_cameras(): return jsonify({"cameras":camera_candidates(),"current":camera_index})

@app.post('/select_camera')
def select_camera():
    global camera,camera_index
    data=request.get_json(silent=True) or {}
    try: idx=int(data.get('camera'))
    except Exception: return jsonify({"ok":False,"error":"Invalid camera index."})
    cap=cv2.VideoCapture(idx,cv2.CAP_DSHOW if os.name=='nt' else 0)
    if not cap.isOpened(): return jsonify({"ok":False,"error":f"Could not open camera {idx}."})
    cap.set(cv2.CAP_PROP_FRAME_WIDTH,1280); cap.set(cv2.CAP_PROP_FRAME_HEIGHT,720)
    with cam_lock: old=camera; camera=cap; camera_index=idx
    if old is not None: old.release()
    return jsonify({"ok":True,"camera":idx})

@app.post('/disconnect')
def disconnect():
    global camera,camera_index
    with cam_lock: old=camera; camera=None; camera_index=None
    if old is not None: old.release()
    return jsonify({"ok":True})

@app.post('/calibrate')
def calibrate():
    if not state["detected"]: return jsonify({"ok":False})
    cal.update({"active":True,"x":state["x"],"y":state["y"]}); return jsonify({"ok":True})

@app.post('/mouse_mode')
def mouse_mode():
    global mouse_enabled
    data=request.get_json(silent=True) or {}; mouse_enabled=bool(data.get('enabled',False))
    if not mouse_enabled and mouse_down: mouse_btn(False)
    return jsonify({"ok":True,"enabled":mouse_enabled})

@app.get('/api/hand')
def api_hand():
    with state_lock: return jsonify(state)

@app.get('/video_feed')
def video_feed():
    def gen():
        last=-1
        while True:
            with frame_lock: seq=frame_seq; jpg=latest_jpeg
            if jpg is not None and seq!=last:
                last=seq; yield b'--frame\r\nContent-Type: image/jpeg\r\n\r\n'+jpg+b'\r\n'
            time.sleep(.02)
    return Response(gen(),mimetype='multipart/x-mixed-replace; boundary=frame')


def main():
    init()
    threading.Thread(target=camera_loop,daemon=True).start()
    print(f"Square Table camera server on http://127.0.0.1:{PORT}")
    app.run(host=HOST,port=PORT,threaded=True,use_reloader=False)

if __name__=='__main__': main()

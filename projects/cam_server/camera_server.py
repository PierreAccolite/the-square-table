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

HOST, PORT = '0.0.0.0', 8080
BASE = os.path.dirname(os.path.abspath(__file__))
HAND_MODEL = os.path.join(BASE, 'hand_landmarker.task')
FACE_MODEL = os.path.join(BASE, 'face_landmarker.task')
HAND_URL = 'https://storage.googleapis.com/mediapipe-models/hand_landmarker/hand_landmarker/float16/1/hand_landmarker.task'
FACE_URL = 'https://storage.googleapis.com/mediapipe-models/face_landmarker/face_landmarker/float16/1/face_landmarker.task'
app = Flask(__name__)
cam_lock = threading.Lock(); frame_lock = threading.Lock(); state_lock = threading.Lock()
camera = None; camera_index = None; latest_jpeg = None; running = True
hand_tracker = face_tracker = None; timestamp_ms = 0; fps = 0.0; fps_samples = deque(maxlen=30)
cal = {'active': False, 'x': .5, 'y': .5}; mouse_enabled = False; mouse_down = False
ball = {'x': .68, 'y': .52, 'vx': 0., 'vy': 0., 'held': False, 'holder': None, 'lastx': .68, 'lasty': .52, 'lastt': 0.}
racket = {'x': .35, 'y': .70, 'held': False, 'holder': None}
state = {'detected':False,'hands_detected':0,'gesture':'None','finger_count':0,'confidence':0.,'x':.5,'y':.5,'calibrated_x':.5,'calibrated_y':.5,'fps':0.,'left':None,'right':None,'face_detected':False,'blink_left':False,'blink_right':False,'blink':False,'wink':'None','mouth_open':False,'smile':0.,'head_yaw':0.,'head_pitch':0.,'head_roll':0.,'head_direction':'Centre','mouse_enabled':False,'mouse_down':False,'ball':ball,'racket':racket,'hand_distance':0.,'hand_distance_px':0.}

def model(path, url, name):
    if os.path.exists(path): return True
    print(name + ' missing; downloading...')
    try: urllib.request.urlretrieve(url, path); print(name + ' ready.'); return True
    except Exception as e: print(name + ' download failed:', e); return False

def init():
    global hand_tracker, face_tracker
    if not mp: return False
    if model(HAND_MODEL, HAND_URL, 'Hand model'):
        try:
            hand_tracker = vision.HandLandmarker.create_from_options(vision.HandLandmarkerOptions(base_options=python.BaseOptions(model_asset_path=HAND_MODEL),running_mode=vision.RunningMode.VIDEO,num_hands=2,min_hand_detection_confidence=.5,min_hand_presence_confidence=.5,min_tracking_confidence=.5))
            print('Hand tracker ready: 2 hands.')
        except Exception as e: print('Hand tracker failed:', e)
    if model(FACE_MODEL, FACE_URL, 'Face model'):
        try:
            face_tracker = vision.FaceLandmarker.create_from_options(vision.FaceLandmarkerOptions(base_options=python.BaseOptions(model_asset_path=FACE_MODEL),running_mode=vision.RunningMode.VIDEO,num_faces=1,min_face_detection_confidence=.5,min_face_presence_confidence=.5,min_tracking_confidence=.5,output_face_blendshapes=True))
            print('Face tracker ready.')
        except Exception as e: print('Face tracker failed:', e)
    return bool(hand_tracker or face_tracker)

def clamp(v): return max(0., min(1., float(v)))
def D(a,b): return math.hypot(a.x-b.x,a.y-b.y)
def hand(lm, label):
    wrist=lm[0]; fs=sum(D(lm[t],wrist)>D(lm[p],wrist)*1.12 for t,p in ((8,6),(12,10),(16,14),(20,18)))
    thumb=D(lm[4],wrist)>D(lm[2],wrist)*1.12; fs += int(thumb)
    ratio=D(lm[4],lm[8])/max(D(lm[0],lm[9]),.001); pinch=ratio<.42
    if pinch: g='Pinch'
    elif fs==5: g='Open Palm'
    elif fs==0: g='Fist'
    elif fs==1: g='Thumbs Up' if thumb and lm[4].y<lm[2].y else 'Point'
    elif fs==2: g='Peace'
    else: g=f'{fs} Fingers'
    return {'detected':True,'label':label,'x':clamp(sum(p.x for p in lm)/21),'y':clamp(sum(p.y for p in lm)/21),'pinch':pinch,'pinch_distance':round(ratio,3),'fingers':fs,'gesture':g,'roll':round(math.degrees(math.atan2(lm[17].y-lm[5].y,lm[17].x-lm[5].x)),1),'landmarks':[{'x':round(p.x,4),'y':round(p.y,4)} for p in lm]}

def blend(cats,n): return next((float(x.score) for x in cats if x.category_name==n),0.)
def pose(face,w,h):
    import numpy as np
    ids=[1,33,263,61,291,152]
    img=np.array([[face[i].x*w,face[i].y*h] for i in ids],dtype='double')
    obj=np.array([[0,0,0],[-45,-32,35],[45,-32,35],[-35,35,20],[35,35,20],[0,65,5]],dtype='double')
    f=float(w); cam=np.array([[f,0,w/2],[0,f,h/2],[0,0,1]],dtype='double')
    try:
        ok,rv,_=cv2.solvePnP(obj,img,cam,np.zeros((4,1)),flags=cv2.SOLVEPNP_ITERATIVE)
        if not ok:return 0.,0.,0.
        r,_=cv2.Rodrigues(rv); sy=math.sqrt(r[0,0]**2+r[1,0]**2)
        yaw=math.degrees(math.atan2(r[1,0],r[0,0])); pitch=math.degrees(math.atan2(-r[2,0],sy)); roll=math.degrees(math.atan2(r[2,1],r[2,2]))
        return -yaw,pitch,roll
    except:return 0.,0.,0.

def mouse_move(x,y):
    try:
        u=ctypes.windll.user32; u.SetCursorPos(int(clamp(x)*(u.GetSystemMetrics(0)-1)),int(clamp(y)*(u.GetSystemMetrics(1)-1)))
    except: pass

def mouse_btn(down):
    global mouse_down
    try: ctypes.windll.user32.mouse_event(2 if down else 4,0,0,0,0); mouse_down=down
    except: pass

def find_camera():
    found=[]
    for i in range(10):
        c=cv2.VideoCapture(i,cv2.CAP_DSHOW)
        if c.isOpened(): found.append({'index':i,'name':f'Camera {i}'}); c.release()
    return found

def process():
    global latest_jpeg,frame_seq,timestamp_ms,fps
    last=time.time()
    while running:
        with cam_lock: c=camera
        if c is None: time.sleep(.05); continue
        ok,frame=c.read()
        if not ok: time.sleep(.02); continue
        # Make the live view a normal selfie/mirror view. Tracking runs on the same image,
        # so screen-left and screen-right match what the user sees.
        frame=cv2.flip(frame,1)
        h,w=frame.shape[:2]; timestamp_ms += 33
        hands=[]; face_lm=None
        try:
            rgb=cv2.cvtColor(frame,cv2.COLOR_BGR2RGB); image=mp.Image(image_format=mp.ImageFormat.SRGB,data=rgb)
            if hand_tracker:
                r=hand_tracker.detect_for_video(image,timestamp_ms)
                for i,lm in enumerate(r.hand_landmarks):
                    # On mirrored input MediaPipe's handedness is the user's anatomical side.
                    handed=r.handedness[i][0].category_name if i < len(r.handedness) else 'Hand'
                    hands.append(hand(lm,handed))
            if face_tracker:
                fr=face_tracker.detect_for_video(image,timestamp_ms)
                if fr.face_landmarks: face_lm=fr.face_landmarks[0]
        except Exception as e: print('Tracking error:',e)
        # Assign left/right by visible screen position, eliminating handedness ambiguity.
        hands.sort(key=lambda q:q['x'])
        left=hands[0] if hands else None; right=hands[-1] if len(hands)>1 else None
        if len(hands)==2 and left is right: right=None
        cx=hands[-1]['x'] if hands else .5; cy=hands[-1]['y'] if hands else .5
        if cal['active'] and hands: cx=clamp(cx-cal['x']+.5); cy=clamp(cy-cal['y']+.5)
        yaw=pitch=roll=0.; face=False; bl=False; br=False; mouth=False; smile=0.; wink='None'; direction='Centre'
        if face_lm:
            face=True; yaw,pitch,roll=pose(face_lm,w,h); direction='Left' if yaw < -8 else 'Right' if yaw > 8 else 'Centre'
            cats=[]
            try: cats=face_tracker.detect_for_video(mp.Image(image_format=mp.ImageFormat.SRGB,data=cv2.cvtColor(frame,cv2.COLOR_BGR2RGB)),timestamp_ms).face_blendshapes[0]
            except: pass
            bl=blend(cats,'eyeBlinkLeft')>.45; br=blend(cats,'eyeBlinkRight')>.45; mouth=blend(cats,'jawOpen')>.30; smile=max(blend(cats,'mouthSmileLeft'),blend(cats,'mouthSmileRight')); wink='Left' if bl and not br else 'Right' if br and not bl else 'Both' if bl and br else 'None'
            pts=[(int(p.x*w),int(p.y*h)) for p in face_lm[0:468:12]]
            for x,y in pts: cv2.circle(frame,(x,y),1,(190,230,255),-1)
            fx,fy=int(face_lm[1].x*w),int(face_lm[1].y*h); cv2.putText(frame,f'HEAD {direction}  Y:{yaw:.0f} P:{pitch:.0f} R:{roll:.0f}',(max(8,fx-90),max(25,fy-35)),cv2.FONT_HERSHEY_SIMPLEX,.55,(255,220,80),2)
        for hand_data in hands:
            for p in hand_data['landmarks']: cv2.circle(frame,(int(p['x']*w),int(p['y']*h)),2,(80,220,120),-1)
            px,py=int(hand_data['x']*w),int(hand_data['y']*h); cv2.putText(frame,hand_data['label']+' '+hand_data['gesture'],(px-60,py-12),cv2.FONT_HERSHEY_SIMPLEX,.5,(80,220,120),2)
        dist=math.hypot(left['x']-right['x'],left['y']-right['y']) if left and right else 0.
        with state_lock:
            state.update({'detected':bool(hands),'hands_detected':len(hands),'gesture':hands[-1]['gesture'] if hands else 'None','finger_count':hands[-1]['fingers'] if hands else 0,'x':cx,'y':cy,'calibrated_x':cx,'calibrated_y':cy,'left':left,'right':right,'face_detected':face,'blink_left':bl,'blink_right':br,'blink':bl and br,'wink':wink,'mouth_open':mouth,'smile':smile,'head_yaw':yaw,'head_pitch':pitch,'head_roll':roll,'head_direction':direction,'mouse_enabled':mouse_enabled,'mouse_down':mouse_down,'hand_distance':dist,'hand_distance_px':dist*math.hypot(w,h)})
        update_racket_ball(left,right,frame,w,h)
        now=time.time(); fps=0.9*fps+0.1/(max(now-last,.001)); last=now; state['fps']=fps
        cv2.putText(frame,f'Hands: {len(hands)}  Distance: {dist*100:.1f}%  FPS: {fps:.1f}',(10,h-15),cv2.FONT_HERSHEY_SIMPLEX,.55,(220,220,220),2)
        with frame_lock: ok,jpeg=cv2.imencode('.jpg',frame,[int(cv2.IMWRITE_JPEG_QUALITY),88]); latest_jpeg=jpeg.tobytes() if ok else latest_jpeg

def update_racket_ball(left,right,frame,w,h):
    global racket,ball
    now=time.time(); hands=[x for x in (left,right) if x]
    # Pinch grabs the nearest virtual object. Racket is intentionally easy to pick up.
    for side in hands:
        dx=side['x']-racket['x']; dy=side['y']-racket['y']
        if side['pinch'] and math.hypot(dx,dy)<.13 and not ball['held']:
            racket.update(x=side['x'],y=side['y'],held=True,holder=side['label']); break
    if racket['held']:
        holder=next((x for x in hands if x['label']==racket['holder']),None)
        if holder and holder['pinch']: racket['x'],racket['y']=holder['x'],holder['y']
        else: racket.update(held=False,holder=None)
    if not ball['held']:
        ball['vy'] += .0008; ball['x']+=ball['vx']; ball['y']+=ball['vy']
        if ball['y']>.94: ball['y']=.94; ball['vy']*=-.72
        if ball['x']<.03 or ball['x']>.97: ball['vx']*=-.9; ball['x']=clamp(ball['x'])
    for side in hands:
        if side['pinch'] and math.hypot(side['x']-ball['x'],side['y']-ball['y'])<.07 and not racket['held']:
            ball['held']=True; ball['holder']=side['label']; ball['vx']=ball['vy']=0.; ball['lastx']=side['x']; ball['lasty']=side['y']; ball['lastt']=now; break
    if ball['held']:
        holder=next((x for x in hands if x['label']==ball['holder']),None)
        if holder and holder['pinch']:
            dt=max(now-ball['lastt'],.001); ball['vx']=(holder['x']-ball['lastx'])/dt*.018; ball['vy']=(holder['y']-ball['lasty'])/dt*.018; ball['x']=holder['x']; ball['y']=holder['y']; ball['lastx']=holder['x']; ball['lasty']=holder['y']; ball['lastt']=now
        else: ball['held']=False; ball['holder']=None
    # A held racket can hit the ball; velocity is based on racket movement.
    if racket['held'] and not ball['held'] and math.hypot(racket['x']-ball['x'],racket['y']-ball['y'])<.16:
        ball['vx']=(ball['x']-racket['x'])*.16; ball['vy']=(ball['y']-racket['y'])*.16-.012
    cv2.circle(frame,(int(ball['x']*w),int(ball['y']*h)),13,(40,190,255),-1); cv2.circle(frame,(int(ball['x']*w),int(ball['y']*h)),13,(255,255,255),2)
    rx,ry=int(racket['x']*w),int(racket['y']*h); cv2.ellipse(frame,(rx,ry),(18,34),-25,0,360,(220,220,220),3); cv2.line(frame,(rx-8,ry+28),(rx-25,ry+62),(180,180,180),6)
    cv2.putText(frame,'RACKET' if racket['held'] else 'Pinch racket',(rx-45,ry-40),cv2.FONT_HERSHEY_SIMPLEX,.45,(230,230,230),1)

def camera_thread(): process()

HTML=r'''<!doctype html><html><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>Square Table Human Interface</title><style>
*{box-sizing:border-box}body{margin:0;background:#101318;color:#eee;font-family:Arial,sans-serif}.wrap{max-width:1250px;margin:auto;padding:18px}h1{margin:0 0 12px}.tabs,.row{display:flex;gap:8px;align-items:center;flex-wrap:wrap;margin-bottom:12px}button,select{font:inherit;background:#252c36;color:#fff;border:1px solid #3b4655;border-radius:7px;padding:9px 13px}button{cursor:pointer}.tab.active{background:#3b4f68}.panel{display:none;background:#171c23;border:1px solid #2b333e;border-radius:10px;padding:14px}.panel.active{display:block}.status{color:#9fb4c9}.warn{color:#ffd166}.small{font-size:13px;color:#9ba7b4}.grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(165px,1fr));gap:10px}.card{background:#11161c;border:1px solid #2b333e;border-radius:8px;padding:12px}.big{font-size:22px;font-weight:bold;margin-top:4px}canvas{width:100%;max-width:900px;border-radius:8px;display:block;background:#0c0f13}.hint{padding:10px 12px;background:#11161c;border-radius:8px;margin:10px 0}.good{color:#8ee7a1}#video{display:block;width:100%;max-width:1100px;background:#000;border-radius:8px}
</style></head><body><div class="wrap"><h1>Square Table — Human Interface</h1><div class="tabs"><button class="tab active" data-p="camera">Camera</button><button class="tab" data-p="lab">Human Lab</button><button class="tab" data-p="objects">Objects</button><button class="tab" data-p="theremin">Theremin</button><button class="tab" data-p="flyer">Flyer</button><button class="tab" data-p="mouse">Windows Mouse</button></div>
<div id="camera" class="panel active"><div class="row"><select id="cams"></select><button id="refresh">Refresh cameras</button><button id="connect">Connect</button><button id="disconnect">Disconnect</button><span id="cs" class="status">Ready.</span></div><img id="video" src="/video_feed"><p class="small">Mirrored live view. Pinch the racket to pick it up and hit the ball. Pinch the ball to grab and throw it.</p></div>
<div id="lab" class="panel"><div class="row"><button id="cal">Calibrate neutral hand</button><span id="calstatus" class="status">Optional.</span></div><div class="grid"><div class="card">Hands<div id="hands" class="big">0</div></div><div class="card">Primary gesture<div id="gesture" class="big">None</div></div><div class="card">Left hand<div id="left" class="big">—</div></div><div class="card">Right hand<div id="right" class="big">—</div></div><div class="card">Left pinch<div id="lp" class="big">—</div></div><div class="card">Right pinch<div id="rp" class="big">—</div></div><div class="card">Hand space<div id="hdist" class="big">0%</div></div><div class="card">Head<div id="head" class="big">Centre</div></div><div class="card">Yaw / Pitch / Roll<div id="angles" class="big">0 / 0 / 0</div></div><div class="card">Blink<div id="blink" class="big">No</div></div><div class="card">Wink<div id="wink" class="big">None</div></div><div class="card">Mouth<div id="mouth" class="big">Closed</div></div><div class="card">Smile<div id="smile" class="big">0%</div></div></div></div>
<div id="objects" class="panel"><div class="hint"><b>Two-hand object manipulation</b><br><span class="good">Pinch one hand</span> to grab and move. <span class="good">Pinch both</span> to stretch/compress and rotate. Hand space and object length are shown live.</div><div class="row"><button id="resetObject">Reset object</button><span id="objstatus" class="status">Waiting for hands.</span></div><div class="grid"><div class="card">Mode<div id="objmode" class="big">Idle</div></div><div class="card">Length<div id="objlen" class="big">100%</div></div><div class="card">Rotation<div id="objrot" class="big">0°</div></div><div class="card">Hand space<div id="objspace" class="big">0%</div></div><div class="card">Hands<div id="objhands" class="big">0</div></div></div><canvas id="objectCanvas" width="900" height="560"></canvas></div>
<div id="theremin" class="panel"><div class="hint"><b>Virtual Theremin</b><br>Right hand X = pitch. Right hand Y = volume. Pinch = gate on/off. Left hand X = vibrato. Move slowly for smooth tones.</div><div class="row"><button id="thereminToggle">Start Theremin</button><span id="thereminStatus" class="status">Audio is off.</span></div><div class="grid"><div class="card">Pitch<div id="tpitch" class="big">—</div></div><div class="card">Volume<div id="tvol" class="big">—</div></div><div class="card">Vibrato<div id="tvib" class="big">—</div></div><div class="card">Gate<div id="tgate" class="big">OFF</div></div></div><canvas id="scope" width="900" height="260"></canvas></div>
<div id="flyer" class="panel"><button id="start">Start / Restart</button><span class="status"> Open Palm / Thumbs Up = flap; hand Y controls altitude.</span><canvas id="game" width="900" height="600"></canvas></div>
<div id="mouse" class="panel"><div class="row"><button id="mtoggle">Enable Windows Mouse</button><span id="ms" class="status">Mouse control is OFF.</span></div><div class="grid"><div class="card">Pointer<div id="ptr" class="big">0%, 0%</div></div><div class="card">Action<div id="act" class="big">Idle</div></div><div class="card">Pinch<div id="mp" class="big">Open</div></div></div><p class="small warn">OFF by default. Move hand = pointer; pinch = click/drag.</p></div></div>
<script>
const $=x=>document.getElementById(x);document.querySelectorAll('.tab').forEach(b=>b.onclick=()=>{document.querySelectorAll('.tab').forEach(x=>x.classList.remove('active'));document.querySelectorAll('.panel').forEach(x=>x.classList.remove('active'));b.classList.add('active');$(b.dataset.p).classList.add('active')});
async function load(){try{let d=await(await fetch('/api/cameras')).json(),s=$('cams');s.innerHTML='';d.cameras.forEach(c=>{let o=document.createElement('option');o.value=c.index;o.textContent=c.name;s.appendChild(o)});if(d.current!=null)s.value=d.current;$('cs').textContent=d.current!=null?'Camera connected.':'Ready.'}catch(e){$('cs').textContent='Camera scan failed.'}}$('refresh').onclick=load;$('connect').onclick=async()=>{let d=await(await fetch('/select_camera',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({camera:$('cams').value})})).json();$('cs').textContent=d.ok?'Connected.':d.error};$('disconnect').onclick=async()=>{await fetch('/disconnect',{method:'POST'});$('cs').textContent='Disconnected.'};$('cal').onclick=async()=>{let d=await(await fetch('/calibrate',{method:'POST'})).json();$('calstatus').textContent=d.ok?'Calibration captured.':'No hand detected.'};let mouseEnabled=false;$('mtoggle').onclick=async()=>{let d=await(await fetch('/mouse_mode',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({enabled:!mouseEnabled})})).json();mouseEnabled=d.enabled;$('mtoggle').textContent=mouseEnabled?'Disable Windows Mouse':'Enable Windows Mouse';$('ms').textContent=mouseEnabled?'Mouse control ENABLED.':'Mouse control is OFF.'};
let last=null;async function poll(){try{let d=await(await fetch('/api/hand',{cache:'no-store'})).json();last=d;$('hands').textContent=d.hands_detected;$('gesture').textContent=d.gesture;$('left').textContent=d.left?d.left.gesture:'—';$('right').textContent=d.right?d.right.gesture:'—';$('lp').textContent=d.left?(d.left.pinch?'PINCHED':'Open'):'—';$('rp').textContent=d.right?(d.right.pinch?'PINCHED':'Open'):'—';$('hdist').textContent=(d.hand_distance*100).toFixed(1)+'%';$('head').textContent=d.head_direction;$('angles').textContent=d.head_yaw.toFixed(0)+' / '+d.head_pitch.toFixed(0)+' / '+d.head_roll.toFixed(0);$('blink').textContent=d.blink?'YES':'No';$('wink').textContent=d.wink;$('mouth').textContent=d.mouth_open?'Open':'Closed';$('smile').textContent=Math.round(d.smile*100)+'%';$('ptr').textContent=Math.round(d.x*100)+'%, '+Math.round(d.y*100)+'%';$('act').textContent=d.mouse_down?'DRAGGING':'Idle';$('mp').textContent=(d.right&&d.right.pinch)||(d.left&&d.left.pinch)?'PINCHED':'Open';updateObject(d);updateTheremin(d)}catch(e){}}setInterval(poll,70);
const oc=$('objectCanvas'),octx=oc.getContext('2d');let obj={cx:.5,cy:.5,len:.30,angle:0,baseDist:null,baseAngle:null,baseLen:null};function resetObj(){obj={cx:.5,cy:.5,len:.30,angle:0,baseDist:null,baseAngle:null,baseLen:null}}$('resetObject').onclick=resetObj;function normAngle(a){while(a>Math.PI)a-=Math.PI*2;while(a<-Math.PI)a+=Math.PI*2;return a}function updateObject(d){let a=d.left,b=d.right;$('objhands').textContent=d.hands_detected;$('objspace').textContent=(d.hand_distance*100).toFixed(1)+'%';if(a&&b&&a.pinch&&b.pinch){let dx=b.x-a.x,dy=b.y-a.y,dist=Math.max(Math.hypot(dx,dy),.04),ang=Math.atan2(dy,dx);if(obj.baseDist===null){obj.baseDist=dist;obj.baseAngle=ang;obj.baseLen=obj.len}obj.cx=(a.x+b.x)/2;obj.cy=(a.y+b.y)/2;obj.len=Math.max(.10,Math.min(.90,obj.baseLen*dist/obj.baseDist));obj.angle=obj.baseAngle+normAngle(ang-obj.baseAngle);$('objmode').textContent='STRETCH + ROTATE';$('objstatus').textContent='Two-hand pinch — spread, compress or twist.'}else if((a&&a.pinch)||(b&&b.pinch)){let h=(a&&a.pinch)?a:b;obj.cx=h.x;obj.cy=h.y;obj.baseDist=obj.baseAngle=obj.baseLen=null;$('objmode').textContent='GRAB / MOVE';$('objstatus').textContent='One-hand pinch — moving object.'}else{obj.baseDist=obj.baseAngle=obj.baseLen=null;$('objmode').textContent='Idle';$('objstatus').textContent='Pinch one or both hands to manipulate.'}$('objlen').textContent=Math.round(obj.len/.30*100)+'%';$('objrot').textContent=Math.round(obj.angle*180/Math.PI)+'°';drawObject()}function drawObject(){octx.clearRect(0,0,900,560);octx.fillStyle='#0d1117';octx.fillRect(0,0,900,560);octx.strokeStyle='#252d38';for(let x=0;x<=900;x+=45){octx.beginPath();octx.moveTo(x,0);octx.lineTo(x,560);octx.stroke()}for(let y=0;y<=560;y+=45){octx.beginPath();octx.moveTo(0,y);octx.lineTo(900,y);octx.stroke()}let cx=obj.cx*900,cy=obj.cy*560,L=obj.len*900,W=65;octx.save();octx.translate(cx,cy);octx.rotate(obj.angle);octx.fillStyle='#3e86ff';octx.strokeStyle='#d5e5ff';octx.lineWidth=3;octx.beginPath();octx.roundRect(-L/2,-W/2,L,W,18);octx.fill();octx.stroke();octx.fillStyle='#fff';octx.font='bold 20px Arial';octx.textAlign='center';octx.fillText('STRETCH ME',0,7);octx.restore()}
let audio=null,osc=null,gain=null,lfo=null,lfoGain=null,thereminOn=false;function startTheremin(){audio=new (window.AudioContext||window.webkitAudioContext)();osc=audio.createOscillator();gain=audio.createGain();lfo=audio.createOscillator();lfoGain=audio.createGain();osc.type='sine';gain.gain.value=0;lfo.type='sine';lfo.frequency.value=5;lfoGain.gain.value=0;osc.connect(gain);gain.connect(audio.destination);lfo.connect(lfoGain);lfoGain.connect(osc.frequency);osc.start();lfo.start();thereminOn=true;$('thereminToggle').textContent='Stop Theremin';$('thereminStatus').textContent='Audio active.'}function stopTheremin(){if(audio){audio.close();audio=null}osc=gain=lfo=lfoGain=null;thereminOn=false;$('thereminToggle').textContent='Start Theremin';$('thereminStatus').textContent='Audio is off.';$('tgate').textContent='OFF'}$('thereminToggle').onclick=()=>thereminOn?stopTheremin():startTheremin();function updateTheremin(d){let h=d.right||d.left;if(!h){$('tgate').textContent='OFF';return}let freq=110*Math.pow(8,h.x);let vol=Math.max(0,Math.min(.22,1-h.y))*(h.pinch?1:.0);let vib=d.left&&d.right?d.left.x*.7:0;if(thereminOn){osc.frequency.setTargetAtTime(freq,audio.currentTime,.035);gain.gain.setTargetAtTime(vol,audio.currentTime,.04);lfoGain.gain.setTargetAtTime(vib*18,audio.currentTime,.05);lfo.frequency.setTargetAtTime(3+vib*8,audio.currentTime,.08)}$('tpitch').textContent=Math.round(freq)+' Hz';$('tvol').textContent=Math.round(vol*100)+'%';$('tvib').textContent=Math.round(vib*100)+'%';$('tgate').textContent=h.pinch?'ON':'OFF'}
const cv=$('game'),ctx=cv.getContext('2d'),game={running:false,over:false,x:180,y:300,vy:0,score:0,pipes:[],spawn:0,last:'None',handY:.5,detected:false,gesture:'None'};function reset(){Object.assign(game,{running:true,over:false,x:180,y:300,vy:0,score:0,pipes:[],spawn:0,last:'None'})}function flap(){if(!game.running||game.over){reset();return}game.vy=-7.2}$('start').onclick=reset;document.onkeydown=e=>{if(e.code==='Space'){e.preventDefault();flap()}};function tick(){if(!game.running)return;game.y=game.detected?game.y+(100+game.handY*400-game.y)*.18:game.y+(game.vy+=.38);if(--game.spawn<=0){let t=90+Math.random()*260;game.pipes.push({x:930,t,b:t+155,hit:false});game.spawn=105}game.pipes.forEach(p=>{p.x-=3.2;if(!p.hit&&p.x+65<game.x){p.hit=true;game.score++}if(game.x+18>p.x&&game.x-18<p.x+65&&(game.y-18<p.t||game.y+18>p.b))game.over=true});game.pipes=game.pipes.filter(p=>p.x>-80);if(game.y<18||game.y>582)game.over=true;if(game.detected&&(game.gesture==='Open Palm'||game.gesture==='Thumbs Up')&&game.last!=='Open Palm'&&game.last!=='Thumbs Up')flap();game.last=game.gesture}function draw(){ctx.clearRect(0,0,900,600);ctx.fillStyle='#9ddcff';ctx.fillRect(0,0,900,600);ctx.fillStyle='#65a34b';ctx.fillRect(0,565,900,35);ctx.fillStyle='#3e8b3b';game.pipes.forEach(p=>{ctx.fillRect(p.x,0,65,p.t);ctx.fillRect(p.x,p.b,65,565-p.b)});ctx.fillStyle='#f4c542';ctx.beginPath();ctx.arc(game.x,game.y,18,0,7);ctx.fill();ctx.fillStyle='#111';ctx.font='bold 28px Arial';ctx.fillText('Score: '+game.score,20,40);if(!game.running||game.over){ctx.fillStyle='rgba(0,0,0,.45)';ctx.fillRect(0,0,900,600);ctx.fillStyle='#fff';ctx.textAlign='center';ctx.font='bold 40px Arial';ctx.fillText(game.over?'Game Over':'Flyer',450,270);ctx.font='20px Arial';ctx.fillText('Start / Restart or SPACE',450,310);ctx.textAlign='left'}}function loop(){tick();draw();requestAnimationFrame(loop)}load();poll();loop();
</script></body></html>'''

@app.route('/')
def index(): return render_template_string(HTML)
@app.route('/api/cameras')
def cameras(): return jsonify({'cameras':find_camera(),'current':camera_index})
@app.route('/select_camera',methods=['POST'])
def select_camera():
    global camera,camera_index
    idx=int(request.json.get('camera',0))
    with cam_lock:
        if camera is not None: camera.release()
        camera=cv2.VideoCapture(idx,cv2.CAP_DSHOW); camera_index=idx if camera.isOpened() else None
    return jsonify({'ok':camera_index is not None,'error':None if camera_index is not None else 'Could not open camera'})
@app.route('/disconnect',methods=['POST'])
def disconnect():
    global camera,camera_index
    with cam_lock:
        if camera is not None: camera.release()
        camera=None; camera_index=None
    return jsonify({'ok':True})
@app.route('/video_feed')
def video_feed():
    def gen():
        while running:
            with frame_lock: jpg=latest_jpeg
            if jpg: yield b'--frame\r\nContent-Type: image/jpeg\r\n\r\n'+jpg+b'\r\n'
            time.sleep(.04)
    return Response(gen(),mimetype='multipart/x-mixed-replace; boundary=frame')
@app.route('/api/hand')
def api_hand():
    with state_lock: return jsonify(state)
@app.route('/calibrate',methods=['POST'])
def calibrate():
    if not state.get('detected'): return jsonify({'ok':False})
    cal.update(active=True,x=state['x'],y=state['y']); return jsonify({'ok':True})
@app.route('/mouse_mode',methods=['POST'])
def mouse_mode():
    global mouse_enabled
    mouse_enabled=bool(request.json.get('enabled',False))
    if not mouse_enabled and mouse_down: mouse_btn(False)
    return jsonify({'enabled':mouse_enabled})

def mouse_loop():
    global mouse_enabled
    while running:
        if mouse_enabled:
            with state_lock: d=dict(state)
            if d['detected']:
                mouse_move(d['x'],d['y'])
                pinch=(d['right'] and d['right']['pinch']) or (d['left'] and d['left']['pinch'])
                if pinch != mouse_down: mouse_btn(pinch)
        time.sleep(.03)

if __name__=='__main__':
    init(); threading.Thread(target=process,daemon=True).start(); threading.Thread(target=mouse_loop,daemon=True).start()
    print(f'Square Table Human Interface running on http://127.0.0.1:{PORT}')
    app.run(host=HOST,port=PORT,threaded=True,use_reloader=False)

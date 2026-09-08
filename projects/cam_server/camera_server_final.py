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
cam_lock, frame_lock, state_lock = threading.Lock(), threading.Lock(), threading.Lock()
camera = None; camera_index = None; latest_jpeg = None; frame_seq = 0; running = True
hand_tracker = face_tracker = None; ts = 0; fps = 0.0; fps_samples = deque(maxlen=30)
cal = {'active': False, 'x': .5, 'y': .5}; mouse_enabled = False; mouse_down = False
ball = {'x': .5, 'y': .5, 'vx': 0., 'vy': 0., 'held': False, 'holder': 'None', 'lastx': .5, 'lasty': .5, 'lastt': 0.}
state = {'detected':False,'hands_detected':0,'gesture':'None','finger_count':0,'confidence':0.,'x':.5,'y':.5,'calibrated_x':.5,'calibrated_y':.5,'fps':0.,'left':None,'right':None,'face_detected':False,'blink_left':False,'blink_right':False,'blink':False,'wink':'None','mouth_open':False,'smile':0.,'head_yaw':0.,'head_pitch':0.,'head_roll':0.,'head_direction':'Centre','mouse_enabled':False,'mouse_down':False}

HTML=r'''<!doctype html><html><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>Square Table Human Interface</title><style>*{box-sizing:border-box}body{margin:0;background:#101318;color:#eee;font-family:Arial}.wrap{max-width:1250px;margin:auto;padding:18px}h1{margin:0 0 12px}.tabs,.row{display:flex;gap:8px;align-items:center;flex-wrap:wrap;margin-bottom:12px}button,select{font:inherit;background:#252c36;color:#fff;border:1px solid #3b4655;border-radius:7px;padding:9px 13px}button{cursor:pointer}button.active{background:#3b4f68}.panel{display:none;background:#171c23;border:1px solid #2b333e;border-radius:10px;padding:14px}.panel.active{display:block}.status{color:#9fb4c9}.warn{color:#ffd166}.small{font-size:13px;color:#9ba7b4}#video{display:block;width:100%;max-width:1100px;background:#000;border-radius:8px}.grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(170px,1fr));gap:10px}.card{background:#11161c;border:1px solid #2b333e;border-radius:8px;padding:12px}.big{font-size:22px;font-weight:bold;margin-top:4px}canvas{width:100%;max-width:900px;border-radius:8px;display:block}</style></head><body><div class="wrap"><h1>Square Table — Human Interface</h1><div class="tabs"><button class="tab active" data-p="camera">Camera</button><button class="tab" data-p="lab">Human Lab</button><button class="tab" data-p="flyer">Flyer</button><button class="tab" data-p="mouse">Windows Mouse</button></div>
<div id="camera" class="panel active"><div class="row"><select id="cams"></select><button id="refresh">Refresh cameras</button><button id="connect">Connect</button><button id="disconnect">Disconnect</button><span id="cs" class="status">Ready.</span></div><img id="video" src="/video_feed"><p class="small">Live image now shows both-hand landmarks, live gestures, face/head indicators and the throwable ball. Pinch the ball to grab it, move, then release to throw.</p></div>
<div id="lab" class="panel"><div class="row"><button id="cal">Calibrate neutral hand</button><span id="calstatus" class="status">Optional.</span></div><div class="grid"><div class="card">Hands<div id="hands" class="big">0</div></div><div class="card">Primary gesture<div id="gesture" class="big">None</div></div><div class="card">Left hand<div id="left" class="big">—</div></div><div class="card">Right hand<div id="right" class="big">—</div></div><div class="card">Left pinch<div id="lp" class="big">—</div></div><div class="card">Right pinch<div id="rp" class="big">—</div></div><div class="card">Head<div id="head" class="big">Centre</div></div><div class="card">Yaw / Pitch / Roll<div id="angles" class="big">0 / 0 / 0</div></div><div class="card">Blink<div id="blink" class="big">No</div></div><div class="card">Wink<div id="wink" class="big">None</div></div><div class="card">Mouth<div id="mouth" class="big">Closed</div></div><div class="card">Smile<div id="smile" class="big">0%</div></div></div></div>
<div id="flyer" class="panel"><button id="start">Start / Restart</button><span class="status"> Open Palm / Thumbs Up = flap; hand Y controls altitude.</span><canvas id="game" width="900" height="600"></canvas></div>
<div id="mouse" class="panel"><div class="row"><button id="mtoggle">Enable Windows Mouse</button><span id="ms" class="status">Mouse control is OFF.</span></div><div class="grid"><div class="card">Pointer<div id="ptr" class="big">0%, 0%</div></div><div class="card">Action<div id="act" class="big">Idle</div></div><div class="card">Pinch<div id="mp" class="big">Open</div></div></div><p class="small warn">OFF by default. When enabled: move hand = pointer, pinch = left-click/hold, move while pinched = drag, release = drop.</p></div></div>
<script>const $=x=>document.getElementById(x);document.querySelectorAll('.tab').forEach(b=>b.onclick=()=>{document.querySelectorAll('.tab').forEach(x=>x.classList.remove('active'));document.querySelectorAll('.panel').forEach(x=>x.classList.remove('active'));b.classList.add('active');$(b.dataset.p).classList.add('active')});
async function load(){try{let d=await(await fetch('/api/cameras')).json(),s=$('cams');s.innerHTML='';d.cameras.forEach(c=>{let o=document.createElement('option');o.value=c.index;o.textContent=c.name;s.appendChild(o)});if(d.current!=null)s.value=d.current;$('cs').textContent=d.current!=null?'Camera connected.':'Ready.'}catch(e){$('cs').textContent='Camera scan failed.'}}$('refresh').onclick=load;$('connect').onclick=async()=>{let d=await(await fetch('/select_camera',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({camera:$('cams').value})})).json();$('cs').textContent=d.ok?'Connected.':d.error};$('disconnect').onclick=async()=>{await fetch('/disconnect',{method:'POST'});$('cs').textContent='Disconnected.'};$('cal').onclick=async()=>{let d=await(await fetch('/calibrate',{method:'POST'})).json();$('calstatus').textContent=d.ok?'Calibration captured.':'No hand detected.'};
let mouseEnabled=false;$('mtoggle').onclick=async()=>{let d=await(await fetch('/mouse_mode',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({enabled:!mouseEnabled})})).json();mouseEnabled=d.enabled;$('mtoggle').textContent=mouseEnabled?'Disable Windows Mouse':'Enable Windows Mouse';$('ms').textContent=mouseEnabled?'Mouse control ENABLED.':'Mouse control is OFF.'};
async function poll(){try{let d=await(await fetch('/api/hand',{cache:'no-store'})).json();$('hands').textContent=d.hands_detected;$('gesture').textContent=d.gesture;$('left').textContent=d.left?d.left.gesture:'—';$('right').textContent=d.right?d.right.gesture:'—';$('lp').textContent=d.left?(d.left.pinch?'PINCHED':'Open'):'—';$('rp').textContent=d.right?(d.right.pinch?'PINCHED':'Open'):'—';$('head').textContent=d.head_direction;$('angles').textContent=d.head_yaw.toFixed(0)+' / '+d.head_pitch.toFixed(0)+' / '+d.head_roll.toFixed(0);$('blink').textContent=d.blink?'YES':'No';$('wink').textContent=d.wink;$('mouth').textContent=d.mouth_open?'Open':'Closed';$('smile').textContent=Math.round(d.smile*100)+'%';$('ptr').textContent=Math.round(d.x*100)+'%, '+Math.round(d.y*100)+'%';$('act').textContent=d.mouse_down?'DRAGGING':'Idle';$('mp').textContent=(d.right&&d.right.pinch)||(d.left&&d.left.pinch)?'PINCHED':'Open';game.handY=d.calibrated_y;game.detected=d.detected;game.gesture=d.gesture}catch(e){}}setInterval(poll,70);
const cv=$('game'),ctx=cv.getContext('2d'),game={running:false,over:false,x:180,y:300,vy:0,score:0,pipes:[],spawn:0,last:'None',handY:.5,detected:false,gesture:'None'};function reset(){Object.assign(game,{running:true,over:false,x:180,y:300,vy:0,score:0,pipes:[],spawn:0,last:'None'})}function flap(){if(!game.running||game.over){reset();return}game.vy=-7.2}$('start').onclick=reset;document.onkeydown=e=>{if(e.code==='Space'){e.preventDefault();flap()}};function tick(){if(!game.running)return;game.y=game.detected?game.y+(100+game.handY*400-game.y)*.18:game.y+(game.vy+=.38);if(--game.spawn<=0){let t=90+Math.random()*260;game.pipes.push({x:930,t,b:t+155,hit:false});game.spawn=105}game.pipes.forEach(p=>{p.x-=3.2;if(!p.hit&&p.x+65<game.x){p.hit=true;game.score++}if(game.x+18>p.x&&game.x-18<p.x+65&&(game.y-18<p.t||game.y+18>p.b))game.over=true});game.pipes=game.pipes.filter(p=>p.x>-80);if(game.y<18||game.y>582)game.over=true;if(game.detected&&(game.gesture==='Open Palm'||game.gesture==='Thumbs Up')&&game.last!=='Open Palm'&&game.last!=='Thumbs Up')flap();game.last=game.gesture}function draw(){ctx.clearRect(0,0,900,600);ctx.fillStyle='#9ddcff';ctx.fillRect(0,0,900,600);ctx.fillStyle='#65a34b';ctx.fillRect(0,565,900,35);ctx.fillStyle='#3e8b3b';game.pipes.forEach(p=>{ctx.fillRect(p.x,0,65,p.t);ctx.fillRect(p.x,p.b,65,565-p.b)});ctx.fillStyle='#f4c542';ctx.beginPath();ctx.arc(game.x,game.y,18,0,7);ctx.fill();ctx.fillStyle='#111';ctx.font='bold 28px Arial';ctx.fillText('Score: '+game.score,20,40);if(!game.running||game.over){ctx.fillStyle='rgba(0,0,0,.45)';ctx.fillRect(0,0,900,600);ctx.fillStyle='#fff';ctx.textAlign='center';ctx.font='bold 40px Arial';ctx.fillText(game.over?'Game Over':'Flyer',450,270);ctx.font='20px Arial';ctx.fillText('Start / Restart or SPACE',450,310);ctx.textAlign='left'}}function loop(){tick();draw();requestAnimationFrame(loop)}load();poll();loop();</script></body></html>'''

def model(path,url,name):
    if os.path.exists(path): return True
    print(name+' missing; downloading...')
    try: urllib.request.urlretrieve(url,path); print(name+' ready.'); return True
    except Exception as e: print(name+' download failed:',e); return False

def init():
    global hand_tracker,face_tracker
    if not mp:return False
    if model(HAND_MODEL,HAND_URL,'Hand model'):
        try: hand_tracker=vision.HandLandmarker.create_from_options(vision.HandLandmarkerOptions(base_options=python.BaseOptions(model_asset_path=HAND_MODEL),running_mode=vision.RunningMode.VIDEO,num_hands=2,min_hand_detection_confidence=.5,min_hand_presence_confidence=.5,min_tracking_confidence=.5));print('Hand tracker ready: 2 hands.')
        except Exception as e:print('Hand tracker failed:',e)
    if model(FACE_MODEL,FACE_URL,'Face model'):
        try: face_tracker=vision.FaceLandmarker.create_from_options(vision.FaceLandmarkerOptions(base_options=python.BaseOptions(model_asset_path=FACE_MODEL),running_mode=vision.RunningMode.VIDEO,num_faces=1,min_face_detection_confidence=.5,min_face_presence_confidence=.5,min_tracking_confidence=.5,output_face_blendshapes=True));print('Face tracker ready.')
        except Exception as e:print('Face tracker failed:',e)
    return bool(hand_tracker or face_tracker)

def clamp(v):return max(0.,min(1.,v))
def D(a,b):return math.hypot(a.x-b.x,a.y-b.y)
def hand(lm,label):
    wrist=lm[0]; fs=sum(D(lm[t],wrist)>D(lm[p],wrist)*1.12 for t,p in ((8,6),(12,10),(16,14),(20,18))); thumb=D(lm[4],wrist)>D(lm[2],wrist)*1.12;fs+=thumb; ratio=D(lm[4],lm[8])/max(D(lm[0],lm[9]),.001);pinch=ratio<.42
    if pinch:g='Pinch'
    elif fs==5:g='Open Palm'
    elif fs==0:g='Fist'
    elif fs==1:g='Thumbs Up' if thumb and lm[4].y<lm[2].y else 'Point'
    elif fs==2:g='Peace'
    else:g=f'{fs} Fingers'
    return {'detected':True,'label':label,'x':clamp(sum(p.x for p in lm)/21),'y':clamp(sum(p.y for p in lm)/21),'pinch':pinch,'pinch_distance':round(ratio,3),'fingers':fs,'gesture':g,'roll':round(math.degrees(math.atan2(lm[17].y-lm[5].y,lm[17].x-lm[5].x)),1),'landmarks':[{'x':round(p.x,4),'y':round(p.y,4)} for p in lm]}
def blend(cats,n):return next((float(x.score) for x in cats if x.category_name==n),0.)
def pose(face,w,h):
    import numpy as np
    ids=[1,33,263,61,291,152];img=np.array([[face[i].x*w,face[i].y*h] for i in ids],dtype='double');obj=np.array([[0,0,0],[-45,-32,35],[45,-32,35],[-35,35,20],[35,35,20],[0,65,5]],dtype='double');f=float(w);cam=np.array([[f,0,w/2],[0,f,h/2],[0,0,1]],dtype='double')
    try:
        ok,rv,_=cv2.solvePnP(obj,img,cam,np.zeros((4,1)),flags=cv2.SOLVEPNP_ITERATIVE)
        if not ok:return 0.,0.,0.
        r,_=cv2.Rodrigues(rv);sy=math.sqrt(r[0,0]**2+r[1,0]**2);return math.degrees(math.atan2(r[1,0],r[0,0])),math.degrees(math.atan2(-r[2,0],sy)),math.degrees(math.atan2(r[2,1],r[2,2]))
    except:return 0.,0.,0.
def mouse_move(x,y):
    try:u=ctypes.windll.user32;u.SetCursorPos(int(clamp(x)*(u.GetSystemMetrics(0)-1)),int(clamp(y)*(u.GetSystemMetrics(1)-1)))
    except:pass
def mouse_btn(down):
    global mouse_down
    try:ctypes.windll.user32.mouse_event(2 if down else 4,0,0,0,0);mouse_down=down
    except:pass

def process(frame):
    global ts,mouse_down
    rgb=cv2.cvtColor(frame,cv2.COLOR_BGR2RGB);im=mp.Image(image_format=mp.ImageFormat.SRGB,data=rgb);ts=max(int(time.monotonic()*1000),ts+1);hr=hand_tracker.detect_for_video(im,ts) if hand_tracker else None;fr=face_tracker.detect_for_video(im,ts) if face_tracker else None;hands=[]
    if hr and hr.hand_landmarks:
        for i,lm in enumerate(hr.hand_landmarks[:2]):
            lab=hr.handedness[i][0].category_name if hr.handedness else 'Unknown';hands.append(hand(lm,lab))
            for p in lm:cv2.circle(frame,(int(p.x*frame.shape[1]),int(p.y*frame.shape[0])),3,(0,220,80),-1)
    left=next((h for h in hands if h['label']=='Left'),None);right=next((h for h in hands if h['label']=='Right'),None);primary=right or left or (hands[0] if hands else None)
    face={'face_detected':False,'blink_left':False,'blink_right':False,'blink':False,'wink':'None','mouth_open':False,'smile':0.,'head_yaw':0.,'head_pitch':0.,'head_roll':0.,'head_direction':'Centre'}
    if fr and fr.face_landmarks:
        fl=fr.face_landmarks[0];face['face_detected']=True;yaw,pitch,roll=pose(fl,frame.shape[1],frame.shape[0]);face.update(head_yaw=yaw,head_pitch=pitch,head_roll=roll);face['head_direction']='Left' if yaw<-12 else 'Right' if yaw>12 else 'Up' if pitch<-10 else 'Down' if pitch>10 else 'Centre'
        def ear(ids):
            p=[fl[i] for i in ids];return(D(p[1],p[5])+D(p[2],p[4]))/(2*max(D(p[0],p[3]),.0001))
        le,re=ear((33,160,158,133,153,144)),ear((362,385,387,263,373,380));face['blink_left']=le<.18;face['blink_right']=re<.18;face['blink']=face['blink_left'] and face['blink_right'];face['wink']='Left' if face['blink_left'] and not face['blink_right'] else 'Right' if face['blink_right'] and not face['blink_left'] else 'None'
        if fr.face_blendshapes:cats=fr.face_blendshapes[0];face['mouth_open']=blend(cats,'jawOpen')>.35;face['smile']=max(blend(cats,'mouthSmileLeft'),blend(cats,'mouthSmileRight'))
        for i in (1,33,263,61,291,152):p=fl[i];cv2.circle(frame,(int(p.x*frame.shape[1]),int(p.y*frame.shape[0])),3,(255,180,0),-1)
    now=time.monotonic();grab=next((h for h in hands if h['pinch'] and math.hypot(h['x']-ball['x'],h['y']-ball['y'])<.13),None)
    if grab:
        if not ball['held']:ball['vx']=ball['vy']=0.
        ball.update(held=True,holder=grab['label']);ball['x']=grab['x'];ball['y']=grab['y'];ball['vx']=(grab['x']-ball['lastx'])/max(now-ball['lastt'],.016)*.7;ball['vy']=(grab['y']-ball['lasty'])/max(now-ball['lastt'],.016)*.7;ball['lastx']=grab['x'];ball['lasty']=grab['y'];ball['lastt']=now
    elif ball['held']:
        ball['held']=False;ball['holder']='None';ball['vx']=max(-.04,min(.04,ball['vx']));ball['vy']=max(-.04,min(.04,ball['vy']))
    if not ball['held']:
        ball['x']+=ball['vx']*.016;ball['y']+=ball['vy']*.016;ball['vy']+=.00065;ball['vx']*=.992;ball['vy']*=.992;r=.035
        if ball['x']<r:ball['x']=r;ball['vx']=abs(ball['vx'])*.9
        if ball['x']>1-r:ball['x']=1-r;ball['vx']=-abs(ball['vx'])*.9
        if ball['y']<r:ball['y']=r;ball['vy']=abs(ball['vy'])*.9
        if ball['y']>1-r:ball['y']=1-r;ball['vy']=-abs(ball['vy'])*.9
    if mouse_enabled and primary:
        mouse_move(primary['x'],primary['y']);pin=primary['pinch']
        if pin and not mouse_down:mouse_btn(True)
        elif not pin and mouse_down:mouse_btn(False)
    elif not mouse_enabled and mouse_down:mouse_btn(False)
    with state_lock:
        state.update(face,hands_detected=len(hands),detected=bool(hands),left=left,right=right,mouse_enabled=mouse_enabled,mouse_down=mouse_down,fps=fps)
        if primary:state.update(gesture=primary['gesture'],finger_count=primary['fingers'],confidence=1.,x=primary['x'],y=primary['y'],calibrated_x=clamp(.5+(primary['x']-cal['x'])*1.8) if cal['active'] else primary['x'],calibrated_y=clamp(.5+(primary['y']-cal['y'])*1.8) if cal['active'] else primary['y'])
        else:state.update(gesture='None',finger_count=0,confidence=0.)
    h,w=frame.shape[:2];bx,by=int(ball['x']*w),int(ball['y']*h);br=max(10,int(min(w,h)*.035));cv2.circle(frame,(bx,by),br,(70,180,255),-1);cv2.circle(frame,(bx,by),br,(255,255,255),2);cv2.putText(frame,'GRABBED' if ball['held'] else 'PINCH ME',(bx-45,by-br-10),cv2.FONT_HERSHEY_SIMPLEX,.55,(255,255,255),2,cv2.LINE_AA)
    hud=[f"Hands: {len(hands)} | L:{left['gesture'] if left else '-'} | R:{right['gesture'] if right else '-'}",f"Head: {face['head_direction']}  Y:{face['head_yaw']:.0f} P:{face['head_pitch']:.0f} R:{face['head_roll']:.0f}",f"Blink:{'YES' if face['blink'] else 'No'}  Wink:{face['wink']}  Smile:{face['smile']:.0%}",f"Mouse:{'ON' if mouse_enabled else 'OFF'}"+('  DRAG' if mouse_down else '')]
    for i,t in enumerate(hud):cv2.putText(frame,t,(15,35+i*28),cv2.FONT_HERSHEY_SIMPLEX,.65,(0,255,255),2,cv2.LINE_AA)
    return frame

def worker():
    global latest_jpeg,frame_seq,fps
    while running:
        with cam_lock:
            c=camera
            if c is not None and c.isOpened():
                ok,f=c.read()
                if ok:
                    f=process(cv2.flip(f,1));ok,e=cv2.imencode('.jpg',f,[int(cv2.IMWRITE_JPEG_QUALITY),82]);now=time.monotonic();fps_samples.append(now);fps=(len(fps_samples)-1)/(fps_samples[-1]-fps_samples[0]) if len(fps_samples)>1 else 0.
                    if ok:
                        with frame_lock:latest_jpeg=e.tobytes();frame_seq+=1
            else:f=None
        time.sleep(.001 if f is not None else .05)
def stream():
    last=-1
    while running:
        with frame_lock:d,s=latest_jpeg,frame_seq
        if d is not None and s!=last:last=s;yield b'--frame\r\nContent-Type: image/jpeg\r\nCache-Control: no-cache,no-store,must-revalidate\r\n\r\n'+d+b'\r\n'
        else:time.sleep(.01)
def open_cam(i):
    global camera,camera_index,latest_jpeg,frame_seq
    try:i=int(i)
    except:return False
    with cam_lock:
        if camera is not None and camera.isOpened() and camera_index==i:return True
        if camera is not None:
            try:camera.release()
            except:pass
        c=cv2.VideoCapture(i,cv2.CAP_DSHOW)
        if not c.isOpened():c.release();return False
        c.set(cv2.CAP_PROP_FRAME_WIDTH,1280);c.set(cv2.CAP_PROP_FRAME_HEIGHT,720);c.set(cv2.CAP_PROP_FPS,30);camera=c;camera_index=i
        with frame_lock:latest_jpeg=None;frame_seq+=1
        return True
def close_cam():
    global camera,camera_index,latest_jpeg,frame_seq
    with cam_lock:
        if camera is not None:
            try:camera.release()
            except:pass
        camera=None;camera_index=None
        with frame_lock:latest_jpeg=None;frame_seq+=1
def scan():
    out=[]
    with cam_lock:
        cur=camera_index
        for i in range(10):
            if i==cur:out.append({'index':i,'name':f'Camera {i}'});continue
            p=None
            try:p=cv2.VideoCapture(i,cv2.CAP_DSHOW);ok=p.isOpened()
            except:ok=False
            if ok:out.append({'index':i,'name':f'Camera {i}'})
            if p is not None:
                try:p.release()
                except:pass
        return out,cur
@app.get('/')
def index():return render_template_string(HTML)
@app.get('/api/cameras')
def cameras():c,i=scan();return jsonify(cameras=c,current=i)
@app.post('/select_camera')
def select():
    try:i=int((request.get_json(silent=True) or {})['camera'])
    except:return jsonify(ok=False,error='Invalid camera index.'),400
    return (jsonify(ok=True,camera=i) if open_cam(i) else jsonify(ok=False,error=f'Could not open camera {i}.'),400)
@app.post('/disconnect')
def disconnect():close_cam();return jsonify(ok=True)
@app.get('/api/hand')
def api_hand():
    with state_lock:return jsonify(dict(state))
@app.post('/calibrate')
def calibrate():
    with state_lock:
        if not state['detected']:return jsonify(ok=False),400
        cal.update(active=True,x=state['x'],y=state['y'])
    return jsonify(ok=True)
@app.post('/mouse_mode')
def mouse_mode():
    global mouse_enabled
    mouse_enabled=bool((request.get_json(silent=True) or {}).get('enabled',False))
    if not mouse_enabled and mouse_down:mouse_btn(False)
    return jsonify(ok=True,enabled=mouse_enabled)
@app.get('/video_feed')
def video():return Response(stream(),mimetype='multipart/x-mixed-replace; boundary=frame',headers={'Cache-Control':'no-cache,no-store,must-revalidate','Pragma':'no-cache'})
def main():
    print('='*60);print('Square Table Human Interface');print('='*60)
    if not mp:print('ERROR: install MediaPipe: py -m pip install mediapipe');return
    init();print('Camera waiting for web interface connection.');threading.Thread(target=worker,daemon=True).start();print(f'Open: http://127.0.0.1:{PORT}')
    try:app.run(host=HOST,port=PORT,threaded=True,debug=False,use_reloader=False)
    finally:close_cam()
if __name__=='__main__':main()

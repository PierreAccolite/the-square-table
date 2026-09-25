const POCKET_ARCADE_BAUD=74880;
const POCKET_ARCADE_LINE={dataBits:8,stopBits:1,parity:"none",bufferSize:4096,flowControl:"none"};
let port=null,reader=null,writer=null,keepReading=false,receiveBuffer="";
let decoder=new TextDecoder("utf-8");
let lastPongAt=0;
const serialState={x:0,y:0,button:0};

const $=id=>document.getElementById(id);
const log=message=>{const box=$("console");if(!box)return;box.textContent+=message+"\n";box.scrollTop=box.scrollHeight;};

function setLink(connected,warn=false){
  $("status").textContent=connected?"USB LINK ONLINE":warn?"WEB SERIAL UNAVAILABLE":"USB DISCONNECTED";
  $("statusDot").classList.toggle("on",connected&&!warn);
  $("statusDot").classList.toggle("warn",warn);
  $("linkState").textContent=connected?"USB":"LOCAL";
}
function updateSerialStick(){
  const x=Math.max(-100,Math.min(100,serialState.x)),y=Math.max(-100,Math.min(100,serialState.y));
  $("joyX").textContent=x;$("joyY").textContent=y;$("joyBtn").textContent=serialState.button;
  $("miniDot").style.left=(50+x*.4)+"%";$("miniDot").style.top=(50-y*.4)+"%";
}
async function sendLine(line){
  if(!writer)return;
  try{await writer.write(new TextEncoder().encode(line+"\n"));log("> "+line);}
  catch(e){log("WRITE ERROR: "+e.message);}
}
function handleSerialLine(line){
  if(!line)return;
  if(line.length<100)log("< "+line);
  if(line==="PONG"){lastPongAt=performance.now();setLink(true);return;}
  if(line.startsWith("JOY,")){
    const p=line.split(",");
    if(p.length>=4){serialState.x=Number(p[1])||0;serialState.y=Number(p[2])||0;serialState.button=Number(p[3])||0;joy.x=serialState.x/100;joy.y=serialState.y/100;updateSerialStick();}
  }
  if(line==="EVENT,BUTTON_DOWN"){keys.a=1;serialState.button=1;updateSerialStick();}
  if(line==="EVENT,BUTTON_UP"){keys.a=0;serialState.button=0;updateSerialStick();}
}
async function readLoop(){
  while(keepReading&&port&&port.readable){
    reader=port.readable.getReader();
    try{
      while(keepReading){
        const {value,done}=await reader.read();
        if(done)break;
        if(value){
          receiveBuffer+=decoder.decode(value,{stream:true});
          receiveBuffer=receiveBuffer.replace(/\0/g,"");
          let n;
          while((n=receiveBuffer.indexOf("\n"))>=0){handleSerialLine(receiveBuffer.slice(0,n).replace(/\r$/,""));receiveBuffer=receiveBuffer.slice(n+1);}
        }
      }
    }catch(e){if(keepReading)log("READ ERROR: "+e.message);break;}
    finally{try{reader.releaseLock();}catch(_){}reader=null;}
  }
}
async function connectSerial(){
  if(!("serial" in navigator)){setLink(false,true);log("Web Serial is not available. Use Chrome or Edge on desktop.");return;}
  try{
    port=await navigator.serial.requestPort();
    await port.open({baudRate:POCKET_ARCADE_BAUD,...POCKET_ARCADE_LINE});
    writer=port.writable.getWriter();keepReading=true;
    const info=port.getInfo();
    $("device").textContent="VID "+(info.usbVendorId??"—")+" / PID "+(info.usbProductId??"—");
    $("connect").disabled=true;$("disconnect").disabled=false;setLink(false);
    log("CONNECTED @ "+POCKET_ARCADE_BAUD+" 8N1");
    readLoop();
    await new Promise(r=>setTimeout(r,900));
    await sendLine("PING");
    await new Promise(r=>setTimeout(r,700));
    if(!lastPongAt){
      log("NO PONG @ "+POCKET_ARCADE_BAUD+" — check that esp32_dev_arcade.ino is flashed.");
      await disconnectSerial(false);
      setLink(false,true);
      return;
    }
    await sendLine("INPUT");
    await sendLine("GAME,"+gameIndex);
  }catch(e){log("CONNECT ERROR: "+e.name+": "+e.message);await disconnectSerial(false);}
}
async function disconnectSerial(show=true){
  keepReading=false;
  try{if(reader)await reader.cancel();}catch(_){}
  try{if(reader)reader.releaseLock();}catch(_){}
  reader=null;
  try{if(writer)writer.releaseLock();}catch(_){}
  writer=null;
  try{if(port)await port.close();}catch(_){}
  port=null;
  receiveBuffer="";
  decoder=new TextDecoder("utf-8");
  lastPongAt=0;
  $("connect").disabled=false;$("disconnect").disabled=true;$("device").textContent="No serial device selected";setLink(false);
  if(show)log("DISCONNECTED");
}
async function chooseGame(i){
  selectGame(i);
  if(writer)await sendLine("GAME,"+gameIndex);
}
async function startPhysicalGame(){
  started=true;
  if(writer)await sendLine("START");
}
async function pulsePhysicalButton(){
  if(writer)await sendLine("BTN,1");
}
async function nextPhysicalGame(){
  selectGame(gameIndex+1);
  if(writer)await sendLine("GAME,"+gameIndex);
}
async function returnToMenu(){
  started=false;
  if(writer)await sendLine("MENU");
}
$("connect").addEventListener("click",connectSerial);
$("disconnect").addEventListener("click",()=>disconnectSerial(true));
$("startBtn").addEventListener("pointerdown",()=>startPhysicalGame());
$("selectBtn").addEventListener("pointerdown",()=>nextPhysicalGame());
$("aBtn").addEventListener("pointerdown",()=>{keys.a=1;pulsePhysicalButton();startPhysicalGame();});
$("bBtn").addEventListener("pointerdown",()=>nextPhysicalGame());

window.addEventListener("keydown",e=>{
  if(e.repeat)return;
  if(e.code==="ArrowLeft"||e.code==="KeyA")keys.left=1;
  if(e.code==="ArrowRight"||e.code==="KeyD")keys.right=1;
  if(e.code==="ArrowUp"||e.code==="KeyW")keys.up=1;
  if(e.code==="ArrowDown"||e.code==="KeyS")keys.down=1;
  if(e.code==="KeyZ"){keys.a=1;pulsePhysicalButton();startPhysicalGame();}
  if(e.code==="KeyX")keys.b=1;
  if(e.code==="Enter"){e.preventDefault();startPhysicalGame();}
  if(e.code==="Space"){e.preventDefault();nextPhysicalGame();}
  if(e.code==="KeyM"||e.code==="Escape")returnToMenu();
});
window.addEventListener("keyup",e=>{
  if(e.code==="ArrowLeft"||e.code==="KeyA")keys.left=0;
  if(e.code==="ArrowRight"||e.code==="KeyD")keys.right=0;
  if(e.code==="ArrowUp"||e.code==="KeyW")keys.up=0;
  if(e.code==="ArrowDown"||e.code==="KeyS")keys.down=0;
  if(e.code==="KeyZ")keys.a=0;if(e.code==="KeyX")keys.b=0;
});

if(!("serial" in navigator)){setLink(false,true);log("WARNING: Web Serial is unavailable.");}


const C=document.getElementById('game'),ctx=C.getContext('2d');const W=C.width,H=C.height;const games=['DINO','BOXES','FREE','PONG','SNAKE','BREAKOUT','INVADERS','ASTEROIDS','FLAPPY','RACING','MEMORY','COINS'];let gameIndex=0,score=0,started=false,last=0,keys={a:0,b:0,start:0,select:0,left:0,right:0,up:0,down:0},joy={x:0,y:0};
const palette=['#4fffe1','#ff4fd8','#ffe66d','#7aa2ff'];
function text(t,x,y,size=20,color='#dff'){ctx.font='800 '+size+'px monospace';ctx.fillStyle=color;ctx.fillText(t,x,y)}function clear(){ctx.fillStyle='#020507';ctx.fillRect(0,0,W,H);for(let y=0;y<H;y+=8){ctx.fillStyle=y%16?'#071116':'#08181c';ctx.fillRect(0,y,W,8)}}function box(x,y,w,h,c,glow=0){if(glow){ctx.shadowBlur=glow;ctx.shadowColor=c}ctx.fillStyle=c;ctx.fillRect(x,y,w,h);ctx.shadowBlur=0}function circle(x,y,r,c){ctx.fillStyle=c;ctx.beginPath();ctx.arc(x,y,r,0,Math.PI*2);ctx.fill()}
let d={x:130,y:390,vy:0,obs:[],t:0};let pong={px:80,py:210,by:270,vx:6,vy:4,ay:270};let snake={body:[[480,270],[460,270],[440,270]],dir:[1,0],food:[700,300],t:0};let ball={x:480,y:390,vx:5,vy:-5};let bricks=[];let inv={ship:480,shots:[],aliens:[],t:0};let ast={x:480,y:270,vx:0,vy:0,a:0,rocks:[]};let flap={y:270,vy:0,pipes:[],t:0};let race={x:480,y:430,road:0,cars:[]};let coins=[];
function reset(){score=0;started=false;d={x:130,y:390,vy:0,obs:[],t:0};pong={px:80,py:210,by:270,vx:6,vy:4,ay:270};snake={body:[[480,270],[460,270],[440,270]],dir:[1,0],food:[700,300],t:0};ball={x:480,y:390,vx:5,vy:-5};bricks=[];for(let y=70;y<180;y+=27)for(let x=90;x<870;x+=62)bricks.push({x,y,w:52,h:17,on:1});inv={ship:480,shots:[],aliens:[],t:0};for(let y=80;y<210;y+=42)for(let x=150;x<850;x+=55)inv.aliens.push({x,y,on:1});ast={x:480,y:270,vx:0,vy:0,a:0,rocks:[]};for(let i=0;i<8;i++)ast.rocks.push({x:Math.random()*W,y:Math.random()*H,vx:(Math.random()-.5)*2,vy:(Math.random()-.5)*2,r:20+Math.random()*22});flap={y:270,vy:0,pipes:[],t:0};race={x:480,y:430,road:0,cars:[]};coins=[];for(let i=0;i<10;i++)coins.push({x:100+Math.random()*760,y:100+Math.random()*300,a:Math.random()*6});}
function inputX(){return joy.x+(keys.right?1:0)-(keys.left?1:0)}function inputY(){return joy.y+(keys.down?1:0)-(keys.up?1:0)}
function dino(dt){d.t+=dt;d.vy+=1500*dt;d.y+=d.vy*dt;if(d.y>390)d.y=390,d.vy=0;if(Math.random()<dt*.75)d.obs.push({x:W+30,w:25+Math.random()*25,h:35+Math.random()*45});d.obs.forEach(o=>o.x-=330*dt);d.obs=d.obs.filter(o=>o.x>-60);if(keys.a||keys.up){if(d.y>=390)d.vy=-610;keys.a=0}for(const o of d.obs)if(o.x<d.x+34&&o.x+o.w>d.x&&390-o.h<d.y+34)started=false;score+=dt*10;clear();box(d.x,d.y,34,34,palette[0],12);for(const o of d.obs)box(o.x,390-o.h,o.w,o.h,palette[1],8);text('DINO RUN',34,48,22,palette[0]);text('JUMP / A',780,48,14,'#9fb');}
function boxes(dt){d.t+=dt;if(Math.random()<dt*1.2)d.obs.push({x:Math.random()*(W-40),y:-40,w:25+Math.random()*35,h:25+Math.random()*35,v:180+score*.2});d.x+=inputX()*350*dt;d.y+=inputY()*350*dt;d.x=Math.max(20,Math.min(W-50,d.x));d.y=Math.max(70,Math.min(H-50,d.y));d.obs.forEach(o=>o.y+=o.v*dt);d.obs=d.obs.filter(o=>o.y<H+50);for(const o of d.obs)if(d.x<o.x+o.w&&d.x+32>o.x&&d.y<o.y+o.h&&d.y+32>o.y)started=false;score+=dt*12;clear();box(d.x,d.y,32,32,palette[0],15);d.obs.forEach(o=>box(o.x,o.y,o.w,o.h,palette[1],10));text('DODGE',34,48,22,palette[0]);}
function free(dt){d.t+=dt;d.x+=inputX()*380*dt;d.y+=inputY()*380*dt;d.x=Math.max(20,Math.min(W-40,d.x));d.y=Math.max(70,Math.min(H-40,d.y));if(Math.hypot(d.x-coins[0]?.x,d.y-coins[0]?.y)<35){score+=100;coins.shift();coins.push({x:40+Math.random()*880,y:90+Math.random()*380})}clear();box(d.x,d.y,30,30,palette[0],14);coins.forEach(c=>circle(c.x,c.y,9,palette[2]));text('FREE MODE',34,48,22,palette[0]);}
function pongGame(dt){pong.py+=inputY()*360*dt;pong.py=Math.max(75,Math.min(H-75,pong.py));pong.by+=pong.vy*dt;pong.px+=pong.vx*dt;if(pong.by<25||pong.by>H-25)pong.vy*=-1;if(pong.vx<0&&pong.px<105&&Math.abs(pong.by-pong.py)<75)pong.vx*=-1;if(pong.vx>0&&pong.px>W-95&&Math.abs(pong.by-pong.ay)<75)pong.vx*=-1;pong.ay+=(pong.by-pong.ay)*.04;if(pong.px<0||pong.px>W)reset();score+=dt*5;clear();box(55,pong.py-55,15,110,palette[0],15);box(W-70,pong.ay-55,15,110,palette[1],12);circle(pong.px,pong.by,10,palette[2]);for(let y=0;y<H;y+=30)box(W/2-2,y,4,15,'#214047');text('PONG',34,48,22,palette[0]);}
function snakeGame(dt){let ix=inputX(),iy=inputY();if(Math.abs(ix)>Math.abs(iy)&&ix){if(ix>0&&snake.dir[0]!==-1)snake.dir=[1,0];else if(ix<0&&snake.dir[0]!==1)snake.dir=[-1,0]}else if(iy){if(iy>0&&snake.dir[1]!==-1)snake.dir=[0,1];else if(iy<0&&snake.dir[1]!==1)snake.dir=[0,-1]}snake.t+=dt;if(snake.t<.1)return;snake.t=0;let [dx,dy]=snake.dir;let h=[snake.body[0][0]+dx*20,snake.body[0][1]+dy*20];if(h[0]<20||h[0]>940||h[1]<70||h[1]>520)started=false;snake.body.unshift(h);if(Math.hypot(h[0]-snake.food[0],h[1]-snake.food[1])<15){score+=50;snake.food=[40+Math.floor(Math.random()*45)*20,80+Math.floor(Math.random()*22)*20]}else snake.body.pop();clear();snake.body.forEach((p,i)=>box(p[0],p[1],18,18,i?palette[0]:'#fff',5));circle(snake.food[0]+9,snake.food[1]+9,8,palette[2]);text('SNAKE',34,48,22,palette[0]);}
function breakout(dt){ball.x+=ball.vx*dt*60;ball.y+=ball.vy*dt*60;if(ball.x<15||ball.x>945)ball.vx*=-1;if(ball.y<55)ball.vy*=-1;let px=ball.x-75+inputX()*8;px=Math.max(20,Math.min(820,px));if(ball.y>480&&ball.y<510&&ball.x>px&&ball.x<px+150)ball.vy=-Math.abs(ball.vy);for(const b of bricks)if(b.on&&ball.x>b.x&&ball.x<b.x+b.w&&ball.y>b.y&&ball.y<b.y+b.h){b.on=0;ball.vy*=-1;score+=25}if(ball.y>560){ball.x=480;ball.y=390;ball.vy=-5}clear();box(px,490,150,13,palette[0],15);circle(ball.x,ball.y,8,palette[2]);bricks.forEach(b=>b.on&&box(b.x,b.y,b.w,b.h,palette[(b.y/27)%3|0],7));text('BREAKOUT',34,38,22,palette[0]);}
function invaders(dt){inv.t+=dt;inv.ship+=inputX()*360*dt;inv.ship=Math.max(40,Math.min(920,inv.ship));if(keys.a){inv.shots.push({x:inv.ship,y:470});keys.a=0}inv.shots.forEach(s=>s.y-=600*dt);for(const s of inv.shots)for(const a of inv.aliens)if(a.on&&Math.abs(s.x-a.x)<24&&Math.abs(s.y-a.y)<18){a.on=0;score+=30}inv.shots=inv.shots.filter(s=>s.y>50);if(inv.t>.5){inv.t=0;inv.aliens.forEach(a=>a.on&&(a.x+=Math.sin(Date.now()/1000)*5));}clear();inv.aliens.forEach(a=>a.on&&box(a.x,a.y,30,20,palette[1],8));box(inv.ship-24,480,48,15,palette[0],12);inv.shots.forEach(s=>box(s.x-2,s.y,4,14,palette[2],8));text('INVADERS',34,38,22,palette[0]);}
function asteroids(dt){ast.a+=inputX()*2.8*dt;ast.vx+=Math.sin(ast.a)*inputY()*50*dt;ast.vy-=Math.cos(ast.a)*inputY()*50*dt;ast.x+=ast.vx*dt;ast.y+=ast.vy*dt;if(ast.x<0)ast.x=W;if(ast.x>W)ast.x=0;if(ast.y<55)ast.y=H;if(ast.y>H)ast.y=55;clear();ctx.save();ctx.translate(ast.x,ast.y);ctx.rotate(ast.a);ctx.strokeStyle=palette[0];ctx.lineWidth=4;ctx.beginPath();ctx.moveTo(0,-25);ctx.lineTo(16,20);ctx.lineTo(0,12);ctx.lineTo(-16,20);ctx.closePath();ctx.stroke();ctx.restore();ast.rocks.forEach(r=>{r.x+=r.vx*60*dt;r.y+=r.vy*60*dt;if(r.x<0)r.x=W;if(r.x>W)r.x=0;if(r.y<55)r.y=H;if(r.y>H)r.y=55;ctx.strokeStyle=palette[1];ctx.lineWidth=3;ctx.beginPath();ctx.arc(r.x,r.y,r.r,0,6.28);ctx.stroke()});score+=dt*8;text('ASTEROIDS',34,38,22,palette[0]);}
function flappyGame(dt){flap.vy+=1450*dt;flap.y+=flap.vy*dt;if(keys.a||keys.up){flap.vy=-520;keys.a=0}if(Math.random()<dt*.65)flap.pipes.push({x:W+40,gap:190+Math.random()*150});flap.pipes.forEach(p=>p.x-=260*dt);flap.pipes=flap.pipes.filter(p=>p.x>-80);for(const p of flap.pipes)if(100<p.x&&p.x<150&& (flap.y<p.gap||flap.y>p.gap+150))started=false;if(flap.y<55||flap.y>H)started=false;score+=dt*10;clear();circle(130,flap.y,18,palette[2]);flap.pipes.forEach(p=>{box(p.x,55,55,p.gap-55,palette[1],7);box(p.x,p.gap+150,55,H-p.gap-150,palette[1],7)});text('FLAPPY',34,38,22,palette[0]);}
function racing(dt){race.x+=inputX()*360*dt;race.x=Math.max(270,Math.min(690,race.x));race.road+=420*dt;clear();ctx.fillStyle='#18201d';ctx.fillRect(220,55,520,H);for(let y=-80+(race.road%90);y<H;y+=90)box(W/2-6,y,12,50,'#ffe66d');box(race.x,420,52,85,palette[0],14);score+=dt*15;text('RACING',34,38,22,palette[0]);}
function memory(){clear();for(let i=0;i<16;i++){let x=270+(i%4)*110,y=100+Math.floor(i/4)*85;box(x,y,80,60,i%2?palette[1]:'#17333a',8)}text('MEMORY',34,38,22,palette[0]);text('A = FLIP',750,38,14,'#9fb');}
function coinsGame(dt){d.x+=inputX()*330*dt;d.y+=inputY()*330*dt;d.x=Math.max(20,Math.min(940,d.x));d.y=Math.max(70,Math.min(500,d.y));coins.forEach(c=>{c.a+=dt*4;if(Math.hypot(d.x-c.x,d.y-c.y)<30){score+=100;c.x=30+Math.random()*900;c.y=80+Math.random()*400}});clear();coins.forEach(c=>circle(c.x,c.y,12+Math.sin(c.a)*3,palette[2]));circle(d.x,d.y,17,palette[0]);text('COIN COLLECTOR',34,38,22,palette[0]);}
function render(dt){if(!started){clear();text('POCKET ARCADE',300,210,34,palette[0]);text(games[gameIndex],390,260,22,palette[2]);text('PRESS A / START',350,315,16,'#9fb');return}switch(gameIndex){case 0:dino(dt);break;case 1:boxes(dt);break;case 2:free(dt);break;case 3:pongGame(dt);break;case 4:snakeGame(dt);break;case 5:breakout(dt);break;case 6:invaders(dt);break;case 7:asteroids(dt);break;case 8:flappyGame(dt);break;case 9:racing(dt);break;case 10:memory();break;case 11:coinsGame(dt);break}document.getElementById('score').textContent=String(Math.floor(score)).padStart(6,'0')}
function loop(t){let dt=Math.min(.033,(t-last)/1000||0);last=t;render(dt);requestAnimationFrame(loop)}
function selectGame(i){gameIndex=(i+games.length)%games.length;document.getElementById('gameName').textContent=games[gameIndex];reset();document.querySelectorAll('.game').forEach((e,n)=>e.classList.toggle('active',n===gameIndex))}
const gc=document.getElementById('games');games.forEach((g,i)=>{let b=document.createElement('button');b.className='game';b.textContent=String(i+1).padStart(2,'0')+' · '+g;b.onclick=()=>selectGame(i);gc.appendChild(b)});selectGame(0);
function press(k,v){keys[k]=v;if(v&&(k==='a'||k==='start'))started=true;if(v&&k==='select')selectGame(gameIndex+1)}document.querySelectorAll('[data-key]').forEach(b=>{let k=b.dataset.key;b.addEventListener('pointerdown',e=>{e.preventDefault();b.setPointerCapture(e.pointerId);press(k,1)});['pointerup','pointercancel','pointerleave'].forEach(ev=>b.addEventListener(ev,e=>{e.preventDefault();press(k,0)}))});window.addEventListener('keydown',e=>{let m={ArrowLeft:'left',ArrowRight:'right',ArrowUp:'up',ArrowDown:'down',KeyA:'a',KeyZ:'a',KeyB:'b',KeyX:'b',Enter:'start',Space:'a'}[e.code];if(m){e.preventDefault();press(m,1)}});window.addEventListener('keyup',e=>{let m={ArrowLeft:'left',ArrowRight:'right',ArrowUp:'up',ArrowDown:'down',KeyA:'a',KeyZ:'a',KeyB:'b',KeyX:'b',Enter:'start',Space:'a'}[e.code];if(m)press(m,0)});
const stick=document.getElementById('stick'),knob=document.getElementById('knob');function joyMove(e){let r=stick.getBoundingClientRect(),x=e.clientX-(r.left+r.width/2),y=e.clientY-(r.top+r.height/2),m=r.width*.36,d=Math.hypot(x,y);if(d>m){x=x/d*m;y=y/d*m}joy.x=x/m;joy.y=y/m;knob.style.transform='translate('+x+'px,'+y+'px)'}function joyEnd(){joy.x=joy.y=0;knob.style.transform='translate(0,0)'}stick.addEventListener('pointerdown',e=>{stick.setPointerCapture(e.pointerId);joyMove(e)});stick.addEventListener('pointermove',e=>{if(e.buttons)joyMove(e)});stick.addEventListener('pointerup',joyEnd);stick.addEventListener('pointercancel',joyEnd);
requestAnimationFrame(loop);

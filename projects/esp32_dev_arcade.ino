/*
 * POCKET ARCADE
 * ESP32 Dev Module
 *
 * Hardware:
 * OLED SDA = GPIO4
 * OLED SCL = GPIO15
 * OLED RESET = GPIO16
 * Joystick VRx = GPIO32
 * Joystick VRy = GPIO33
 * Joystick SW  = GPIO25
 *
 * V3 foundation:
 *   1. Dino Jump
 *   2. Box Climber
 *   3. Free Walk
 *   4. Ping Pong
 *   5. Snake
 *   6. Breakout
 *   7. Space Invaders
 *   8. Asteroids
 *   9. Flappy
 *   10. Racing
 *   11. Memory
 *   12. Coin Collector
 *
 * V3 adds:
 *   - Scrollable game menu
 *   - Explicit function prototypes
 *   - RAM high-score framework
 *   - Non-blocking frame timing foundation
 *   - First additional games: Snake + Breakout
 *
 * Pocket Arcade Wi-Fi AP + web portal foundation.
 * Wi-Fi can be enabled/disabled from the OLED menu.
 */

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <WiFi.h>
#include <WebServer.h>

#define OLED_SDA      4
#define OLED_SCL      15
#define OLED_RESET    16
#define SCREEN_WIDTH  128
#define SCREEN_HEIGHT 64
#define OLED_ADDR     0x3C

#define JOY_X         32
#define JOY_Y         33
#define JOY_BTN       25

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

const char* AP_SSID = "POCKET_ARCADE";
const char* AP_PASSWORD = "arcade123";
WebServer webServer(80);

bool wifiPortalEnabled = false;
bool wifiApStarted = false;
String wifiStatusMessage = "OFF";

const char POCKET_ARCADE_PAGE[] PROGMEM = R"rawliteral(
<!DOCTYPE html><html lang="en"><head>
<meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1,maximum-scale=1,user-scalable=no">
<title>Pocket Arcade</title>
<style>
:root{--bg:#05080b;--panel:#0b1418;--line:#1e3b42;--cyan:#4fffe1;--pink:#ff4fd8;--yellow:#ffe66d;--muted:#7f9da4}*{box-sizing:border-box}html,body{margin:0;min-height:100%;background:radial-gradient(circle at 50% 0,#12313a 0,#071014 38%,#030507 100%);color:#eafafa;font-family:Inter,system-ui,Arial,sans-serif}body{padding:18px}.wrap{width:min(1180px,100%);margin:auto}.top{display:flex;justify-content:space-between;align-items:center;gap:14px;margin-bottom:14px}.brand{letter-spacing:.18em;font-weight:900;color:var(--cyan);font-size:clamp(20px,4vw,34px);text-shadow:0 0 18px #19d9bd66}.sub{color:var(--muted);font-size:12px}.status{border:1px solid var(--line);background:#071116;padding:8px 12px;border-radius:999px;color:var(--cyan);font-size:11px}.arcade{display:grid;grid-template-columns:minmax(0,1fr) 270px;gap:16px}.machine{background:linear-gradient(145deg,#102329,#061014);border:1px solid #28505a;border-radius:28px;padding:18px;box-shadow:0 24px 80px #0009,inset 0 0 35px #2fffd30b}.bezel{position:relative;background:#020405;border:2px solid #20363b;border-radius:20px;padding:14px;box-shadow:inset 0 0 40px #000,0 0 35px #4fffe11a}.screen{position:relative;width:100%;aspect-ratio:16/9;border-radius:12px;overflow:hidden;background:#000;box-shadow:0 0 0 5px #071014,0 0 35px #4fffe133}.screen canvas{display:block;width:100%;height:100%;image-rendering:pixelated}.scan{position:absolute;inset:0;pointer-events:none;background:repeating-linear-gradient(to bottom,#fff0 0 2px,#00000018 3px 4px);mix-blend-mode:screen}.glow{position:absolute;inset:-30%;pointer-events:none;background:radial-gradient(circle,#4fffe110 0 18%,transparent 55%);animation:pulse 3s infinite alternate}@keyframes pulse{to{opacity:.4;transform:scale(1.08)}}.hud{display:flex;justify-content:space-between;gap:8px;margin-top:12px;font:700 12px monospace;color:#9dc1c6}.hud b{color:var(--yellow)}.controls{display:flex;justify-content:space-between;align-items:center;padding:14px 4px 2px}.stick{width:128px;height:128px;border-radius:50%;background:radial-gradient(circle,#1b3439,#081114 65%);border:2px solid #29484e;box-shadow:inset 0 0 25px #000,0 8px 20px #0008;position:relative;touch-action:none}.knob{position:absolute;width:58px;height:58px;left:50%;top:50%;margin:-29px;border-radius:50%;background:radial-gradient(circle at 35% 30%,#6dfff0,#167b70);border:2px solid #9dfff5;box-shadow:0 0 20px #4fffe188;transform:translate(0,0)}.buttons{display:grid;grid-template-columns:repeat(2,64px);gap:12px;transform:rotate(-8deg)}.btn{width:64px;height:64px;border-radius:50%;border:2px solid #70305f;background:radial-gradient(circle at 35% 25%,#ff87e8,#8d236f);color:#fff;font-weight:900;box-shadow:0 8px 14px #0008,0 0 18px #ff4fd844;touch-action:none}.btn:active{transform:translateY(3px) scale(.97)}.btn.b{background:radial-gradient(circle at 35% 25%,#ffe989,#9c7620);border-color:#796329}.aux{display:flex;justify-content:center;gap:12px;margin-top:4px}.smallbtn{border:1px solid #315058;background:#0b171b;color:#b9d4d8;border-radius:999px;padding:7px 16px;font:700 10px monospace}.side{display:flex;flex-direction:column;gap:12px}.card{background:rgba(8,18,22,.9);border:1px solid var(--line);border-radius:16px;padding:14px}.card h3{margin:0 0 10px;font-size:12px;letter-spacing:.1em;color:var(--cyan)}.games{display:grid;grid-template-columns:1fr 1fr;gap:7px}.game{padding:9px;border:1px solid #203d44;background:#091419;color:#a9c5c9;border-radius:9px;text-align:left;font-size:11px;cursor:pointer}.game.active{border-color:var(--cyan);color:#fff;box-shadow:0 0 14px #4fffe11c}.tip{color:var(--muted);font-size:11px;line-height:1.45}.online{color:#63f3bd}.offline{color:#f3b85d}@media(max-width:850px){body{padding:10px}.arcade{grid-template-columns:1fr}.side{order:2}.controls{padding-top:12px}.stick{width:110px;height:110px}.knob{width:52px;height:52px;margin:-26px}.buttons{grid-template-columns:repeat(2,58px)}.btn{width:58px;height:58px}}
</style></head><body><div class="wrap">
<div class="top"><div><div class="brand">POCKET // ARCADE</div><div class="sub">THE SQUARE TABLE · BROWSER EDITION</div></div><div id="status" class="status">● READY</div></div>
<div class="arcade"><section class="machine"><div class="bezel"><div class="screen"><canvas id="game" width="960" height="540"></canvas><div class="glow"></div><div class="scan"></div></div></div><div class="hud"><span id="gameName">DINO</span><span>SCORE <b id="score">000000</b></span><span id="hint">PRESS A</span></div><div class="controls"><div id="stick" class="stick"><div id="knob" class="knob"></div></div><div class="buttons"><button class="btn" data-key="a">A</button><button class="btn b" data-key="b">B</button></div></div><div class="aux"><button class="smallbtn" data-key="start">START</button><button class="smallbtn" data-key="select">SELECT</button></div></section>
<aside class="side"><div class="card"><h3>GAMES</h3><div id="games" class="games"></div></div><div class="card"><h3>INPUT</h3><div class="tip">Touch the stick, use <b>WASD / arrow keys</b>, or connect a gamepad. A/B map to <b>Z / X</b> and <b>Enter / Space</b>.</div></div><div class="card"><h3>DISPLAY</h3><div class="tip"><span class="online">● CANVAS ENGINE</span><br>60 FPS target · CRT scanlines · neon glow · responsive controls</div></div><div class="card"><h3>POCKET LINK</h3><div class="tip">Connected to the Pocket Arcade portal. The canvas is the big-screen presentation layer; the existing ESP32 game engine remains untouched.</div></div></aside></div></div>
<script>
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
function snakeGame(dt){snake.t+=dt;if(snake.t<.1)return;snake.t=0;let [dx,dy]=snake.dir;let h=[snake.body[0][0]+dx*20,snake.body[0][1]+dy*20];if(h[0]<20||h[0]>940||h[1]<70||h[1]>520)started=false;snake.body.unshift(h);if(Math.hypot(h[0]-snake.food[0],h[1]-snake.food[1])<15){score+=50;snake.food=[40+Math.floor(Math.random()*45)*20,80+Math.floor(Math.random()*22)*20]}else snake.body.pop();clear();snake.body.forEach((p,i)=>box(p[0],p[1],18,18,i?palette[0]:'#fff',5));circle(snake.food[0]+9,snake.food[1]+9,8,palette[2]);text('SNAKE',34,48,22,palette[0]);}
function breakout(dt){ball.x+=ball.vx*dt*60;ball.y+=ball.vy*dt*60;if(ball.x<15||ball.x>945)ball.vx*=-1;if(ball.y<55)ball.vy*=-1;let px=ball.x-75+inputX()*8;px=Math.max(20,Math.min(820,px));if(ball.y>480&&ball.y<510&&ball.x>px&&ball.x<px+150)ball.vy=-Math.abs(ball.vy);for(const b of bricks)if(b.on&&ball.x>b.x&&ball.x<b.x+b.w&&ball.y>b.y&&ball.y<b.y+b.h){b.on=0;ball.vy*=-1;score+=25}if(ball.y>560){ball.x=480;ball.y=390;ball.vy=-5}clear();box(px,490,150,13,palette[0],15);circle(ball.x,ball.y,8,palette[2]);bricks.forEach(b=>b.on&&box(b.x,b.y,b.w,b.h,palette[(b.y/27)%3|0],7));text('BREAKOUT',34,38,22,palette[0]);}
function invaders(dt){inv.t+=dt;inv.ship+=inputX()*360*dt;inv.ship=Math.max(40,Math.min(920,inv.ship));if(keys.a){inv.shots.push({x:inv.ship,y:470});keys.a=0}inv.shots.forEach(s=>s.y-=600*dt);for(const s of inv.shots)for(const a of inv.aliens)if(a.on&&Math.abs(s.x-a.x)<24&&Math.abs(s.y-a.y)<18){a.on=0;score+=30}s=inv.shots.filter(s=>s.y>50);inv.shots=s;if(inv.t>.5){inv.t=0;inv.aliens.forEach(a=>a.on&&(a.x+=Math.sin(Date.now()/1000)*5));}clear();inv.aliens.forEach(a=>a.on&&box(a.x,a.y,30,20,palette[1],8));box(inv.ship-24,480,48,15,palette[0],12);inv.shots.forEach(s=>box(s.x-2,s.y,4,14,palette[2],8));text('INVADERS',34,38,22,palette[0]);}
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
</script></body></html>
)rawliteral";

void handleRoot() {
  webServer.send_P(200, "text/html", POCKET_ARCADE_PAGE);
}

void handleNotFound() {
  webServer.sendHeader("Location", "/", true);
  webServer.send(302, "text/plain", "");
}

void startPocketArcadeWiFi() {
  Serial.println();
  Serial.println("=== POCKET ARCADE Wi-Fi START ===");
  Serial.println("Resetting Wi-Fi radio...");

  // Fully reset the radio before starting the AP.
  // This avoids stale AP/STA state from previous starts.
  WiFi.softAPdisconnect(true);
  WiFi.mode(WIFI_OFF);
  delay(200);

  WiFi.mode(WIFI_AP);
  WiFi.setSleep(false);
  WiFi.setTxPower(WIFI_POWER_19_5dBm);

  WiFi.softAPConfig(
    IPAddress(192,168,4,1),
    IPAddress(192,168,4,1),
    IPAddress(255,255,255,0)
  );

  Serial.println("Starting Wi-Fi AP...");
  Serial.println("Channel: 6");
  Serial.println("SSID broadcast: YES");
  Serial.println("TX power: 19.5 dBm");

  // Channel 6 is a standard 2.4 GHz channel and visible SSID.
  bool apStarted = WiFi.softAP(AP_SSID, AP_PASSWORD, 6, false, 4);
  wifiApStarted = apStarted;
  wifiPortalEnabled = apStarted;

  Serial.print("Wi-Fi AP start: ");
  Serial.println(apStarted ? "SUCCESS" : "FAILED");
  Serial.print("Wi-Fi mode: ");
  Serial.println(WiFi.getMode());
  Serial.print("Configured SSID: ");
  Serial.println(AP_SSID);
  Serial.print("Active SSID: ");
  Serial.println(WiFi.softAPSSID());
  Serial.print("Password: ");
  Serial.println(AP_PASSWORD);
  Serial.print("AP IP: ");
  Serial.println(WiFi.softAPIP());
  Serial.print("AP MAC: ");
  Serial.println(WiFi.softAPmacAddress());
  Serial.print("Channel: ");
  Serial.println(WiFi.channel());
  Serial.print("Connected stations: ");
  Serial.println(WiFi.softAPgetStationNum());

  if (apStarted) {
    wifiStatusMessage = "ON";
    webServer.begin();
    Serial.println("Web server: STARTED");
    Serial.print("Open: http://");
    Serial.println(WiFi.softAPIP());
  } else {
    wifiStatusMessage = "FAILED";
    Serial.println("Web server: NOT STARTED");
  }

  Serial.println("==============================");
}

void stopPocketArcadeWiFi() {
  Serial.println();
  Serial.println("=== POCKET ARCADE Wi-Fi STOP ===");

  webServer.stop();
  WiFi.softAPdisconnect(true);
  WiFi.mode(WIFI_OFF);

  wifiPortalEnabled = false;
  wifiApStarted = false;
  wifiStatusMessage = "OFF";

  Serial.println("Wi-Fi AP: OFF");
  Serial.println("==============================");
}

void showWiFiSerialStatus() {
  Serial.println();
  Serial.println("=== POCKET ARCADE Wi-Fi STATUS ===");
  Serial.print("AP: ");
  Serial.println(wifiPortalEnabled ? "ON" : "OFF");
  Serial.print("AP started: ");
  Serial.println(wifiApStarted ? "YES" : "NO");
  Serial.print("SSID: ");
  Serial.println(AP_SSID);
  Serial.print("IP: ");
  Serial.println(WiFi.softAPIP());
  Serial.print("Channel: ");
  Serial.println(WiFi.channel());
  Serial.print("Stations: ");
  Serial.println(WiFi.softAPgetStationNum());
  Serial.println("==================================");
}

// -------------------- States --------------------
enum GameState { STATE_MENU, STATE_PLAYING, STATE_GAMEOVER, STATE_WIFI };
enum GameType  {
  GAME_DINO=0,
  GAME_BOXES=1,
  GAME_FREE=2,
  GAME_PONG=3,
  GAME_SNAKE=4,
  GAME_BREAKOUT=5,
  GAME_INVADERS=6,
  GAME_ASTEROIDS=7,
  GAME_FLAPPY=8,
  GAME_RACING=9,
  GAME_MEMORY=10,
  GAME_COINS=11
};

const int GAME_COUNT = 12;
const int MENU_COUNT = GAME_COUNT + 1;
const int MENU_WIFI_INDEX = GAME_COUNT;
const int MENU_VISIBLE = 4;
const unsigned long FRAME_TIME_MS = 30;

GameState state = STATE_MENU;
GameType  currentGame = GAME_DINO;
int menuSelection = 0;
int gameOverSelection = 0;

// -------------------- Shared player (used by 1-3) --------------------
int manX = 30, manY = 0, manVy = 0;
bool facingRight = true;
int walkFrame = 0, frameCnt = 0;

const int GROUND_Y = 54;
const int JUMP_V   = 7;
const int GRAVITY  = 1;

// -------------------- Game 1 Dino --------------------
struct Spike { int x; bool active; };
Spike spikes[4];
int dinoScore = 0, groundScroll = 0;
int dinoSpeed = 3;
int dinoCloudX = 92;

// -------------------- Game 2 Boxes (with camera) --------------------
struct Box { int x, y; bool active; };
Box boxes[10];
int boxLevel = 0;
int cameraY = 0;
int highestReached = 0;

// -------------------- Game 4 Pong --------------------
int paddleX = 54;
int ballX = 64, ballY = 32;
int ballVX = 2, ballVY = -2;
int pongScore = 0;
int pongLives = 3;

// -------------------- Game 5 Snake --------------------
const int SNAKE_CELL = 4;
const int SNAKE_ORIGIN_Y = 10;
const int SNAKE_COLS = SCREEN_WIDTH / SNAKE_CELL;
const int SNAKE_ROWS = 13;
const int SNAKE_MAX = 120;

int snakeX[SNAKE_MAX];
int snakeY[SNAKE_MAX];
int snakeLength = 4;
int snakeDirX = 1;
int snakeDirY = 0;
int snakeNextDirX = 1;
int snakeNextDirY = 0;
int snakeFoodX = 20;
int snakeFoodY = 6;
int snakeScore = 0;
unsigned long snakeLastMove = 0;

// -------------------- Game 6 Breakout --------------------
const int BRICK_ROWS = 4;
const int BRICK_COLS = 8;
bool bricks[BRICK_ROWS][BRICK_COLS];
int breakoutPaddleX = 50;
int breakoutBallX = 64;
int breakoutBallY = 50;
int breakoutBallVX = 2;
int breakoutBallVY = -2;
int breakoutScore = 0;
int breakoutLives = 3;

// -------------------- Game 7 Space Invaders --------------------
const int INV_ROWS = 3, INV_COLS = 6, INV_MAX_BULLETS = 3;
bool invaders[INV_ROWS][INV_COLS];
int invaderPlayerX = 56, invaderDir = 1, invaderStep = 0;
int invaderOffsetX = 0, invaderDrop = 0;
int invaderBulletX[INV_MAX_BULLETS], invaderBulletY[INV_MAX_BULLETS];
bool invaderBulletActive[INV_MAX_BULLETS];
int invaderScore = 0, invaderLives = 3;

// -------------------- Game 8 Asteroids --------------------
struct Asteroid { int x,y,vx,vy,size; bool active; };
Asteroid asteroids[6];
int asteroidShipX=64, asteroidShipY=48, asteroidVX=0, asteroidVY=0;
int asteroidBulletX[3], asteroidBulletY[3], asteroidBulletVX[3], asteroidBulletVY[3];
bool asteroidBulletActive[3];
int asteroidScore=0, asteroidLives=3;

// -------------------- Game 9 Flappy --------------------
int flappyY=32, flappyV=0, flappyPipeX=128, flappyGapY=32;
int flappyScore=0, flappyLives=1;

// -------------------- Game 10 Racing --------------------
int raceCarX=60, raceRoadOffset=0, raceScore=0, raceSpeed=2;
struct RoadObstacle { int x,y; bool active; };
RoadObstacle raceObstacles[4];

// -------------------- Game 11 Memory --------------------
const int MEMORY_PAIRS=4;
int memoryCards[MEMORY_PAIRS*2];
bool memoryFound[MEMORY_PAIRS*2];
int memoryCursor=0, memoryFirst=-1, memorySecond=-1, memoryScore=0;
unsigned long memoryPauseUntil=0;

// -------------------- Game 12 Coin Collector --------------------
int coinPlayerX=64, coinPlayerY=32, coinScore=0;
int coinsX[8], coinsY[8];
bool coinsActive[8];

// Simple RAM high-score table. Persistent storage comes later.
int highScores[GAME_COUNT] = {0, 0, 0, 0, 0};

// -------------------- Menu --------------------
int menuTop = 0;
const char* gameNames[MENU_COUNT] = {
  "1. Dino Jump",
  "2. Box Climber",
  "3. Free Walk",
  "4. Ping Pong",
  "5. Snake",
  "6. Breakout",
  "7. Space Invaders",
  "8. Asteroids",
  "9. Flappy",
  "10. Racing",
  "11. Memory",
  "12. Coin Collector",
  "13. Wi-Fi Portal"
};
// -------------------- Timing --------------------
unsigned long lastFrame = 0;

// =====================================================
//  FORWARD DECLARATIONS
// =====================================================
void startGame();
void enterGameOver();
int currentScore();
void recordScore();
void updateSnake();
void drawSnake();
void updateBreakout();
void drawBreakout();
void updateInvaders(); void drawInvaders();
void updateAsteroids(); void drawAsteroids();
void updateFlappy(); void drawFlappy();
void updateRacing(); void drawRacing();
void updateMemory(); void drawMemory();
void updateCoins(); void drawCoins();

// =====================================================
//  JOYSTICK helpers (wide dead-zone)
// =====================================================
bool buttonPressed() {
  static bool last = false;
  bool now = digitalRead(JOY_BTN) == LOW;
  bool pressed = now && !last;
  last = now;
  return pressed;
}

int joyXDir() {
  int v = analogRead(JOY_X);
  if (v < 1000) return -1;
  if (v > 3000) return  1;
  return 0;
}

int joyYDir() {
  int v = analogRead(JOY_Y);
  if (v < 1000) return -1;
  if (v > 3000) return  1;
  return 0;
}

// =====================================================
//  DRAW MAN
// =====================================================
void drawMan(int x, int y, int frame, bool right) {
  int by = GROUND_Y - (y - cameraY);

  display.drawCircle(x, by - 14, 4, SSD1306_WHITE);
  display.drawPixel(x + (right ? 1 : -1), by - 15, SSD1306_WHITE);
  display.drawLine(x, by - 10, x, by - 3, SSD1306_WHITE);

  int arm = (frame % 2) ? 3 : -3;
  if (!right) arm = -arm;
  display.drawLine(x, by - 8, x - 5 + arm, by - 4, SSD1306_WHITE);
  display.drawLine(x, by - 8, x + 5 - arm, by - 4, SSD1306_WHITE);

  int lf, lb;
  switch (frame) {
    case 0: lf=-4; lb=4; break;
    case 1: lf=-2; lb=2; break;
    case 2: lf=4;  lb=-4; break;
    default:lf=2;  lb=-2; break;
  }
  if (!right) { lf=-lf; lb=-lb; }
  display.drawLine(x, by-3, x+lf, by, SSD1306_WHITE);
  display.drawLine(x, by-3, x+lb, by, SSD1306_WHITE);
}

// =====================================================
//  MENU
// =====================================================
void drawMenu() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);

  display.setCursor(12, 0);
  display.print("POCKET ARCADE");

  display.setCursor(101, 0);
  display.print(menuSelection + 1);
  display.print("/");
  display.print(MENU_COUNT);

  for (int row = 0; row < MENU_VISIBLE; row++) {
    int index = menuTop + row;
    if (index >= MENU_COUNT) break;

    display.setCursor(5, 13 + row * 12);
    display.print(index == menuSelection ? "> " : "  ");
    display.print(gameNames[index]);
  }

  if (menuTop > 0) {
    display.setCursor(122, 13);
    display.print("^");
  }

  if (menuTop + MENU_VISIBLE < MENU_COUNT) {
    display.setCursor(122, 49);
    display.print("v");
  }

  display.display();
}

void updateMenu() {
  static int lastDir = 0;
  int dir = joyYDir();

  if (dir != 0 && lastDir == 0) {
    menuSelection = constrain(menuSelection + dir, 0, MENU_COUNT - 1);

    if (menuSelection < menuTop)
      menuTop = menuSelection;

    if (menuSelection >= menuTop + MENU_VISIBLE)
      menuTop = menuSelection - MENU_VISIBLE + 1;
  }

  lastDir = dir;

  if (buttonPressed()) {
    if (menuSelection == MENU_WIFI_INDEX) {
      state = STATE_WIFI;
      showWiFiSerialStatus();
      return;
    }

    currentGame = (GameType)menuSelection;
    startGame();
  }
}


void drawWiFiStatus() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);

  display.setCursor(27, 0);
  display.print("WI-FI PORTAL");

  display.setCursor(4, 13);
  display.print("Status: ");
  display.print(wifiPortalEnabled ? "ON" : wifiStatusMessage.c_str());

  display.setCursor(4, 24);
  display.print("SSID: ");
  display.print(AP_SSID);

  display.setCursor(4, 35);
  display.print("IP: ");
  if (wifiPortalEnabled) display.print(WiFi.softAPIP());
  else display.print("OFF");

  display.setCursor(4, 46);
  display.print("Users: ");
  display.print(wifiPortalEnabled ? WiFi.softAPgetStationNum() : 0);

  display.setCursor(4, 57);
  display.print(wifiPortalEnabled ? "BTN: Wi-Fi OFF" : "BTN: Wi-Fi ON");

  display.display();
}

void updateWiFiStatus() {
  static int lastDir = 0;
  int dir = joyYDir();

  // Up/down exits the status screen and returns to the menu.
  if (dir != 0 && lastDir == 0) {
    state = STATE_MENU;
    if (menuSelection < menuTop) menuTop = menuSelection;
    if (menuSelection >= menuTop + MENU_VISIBLE)
      menuTop = menuSelection - MENU_VISIBLE + 1;
    lastDir = dir;
    return;
  }

  lastDir = dir;

  // Button toggles the AP while staying on this screen.
  if (buttonPressed()) {
    if (wifiPortalEnabled) {
      stopPocketArcadeWiFi();
    } else {
      startPocketArcadeWiFi();
    }
    showWiFiSerialStatus();
  }
}

// =====================================================
//  GAME OVER
// =====================================================
int currentScore() {
  if (currentGame == GAME_DINO)  return dinoScore;
  if (currentGame == GAME_BOXES) return boxLevel;
  if (currentGame == GAME_PONG)  return pongScore;
  if (currentGame == GAME_SNAKE) return snakeScore;
  if (currentGame == GAME_BREAKOUT) return breakoutScore;
  if (currentGame == GAME_INVADERS) return invaderScore;
  if (currentGame == GAME_ASTEROIDS) return asteroidScore;
  if (currentGame == GAME_FLAPPY) return flappyScore;
  if (currentGame == GAME_RACING) return raceScore;
  if (currentGame == GAME_MEMORY) return memoryScore;
  if (currentGame == GAME_COINS) return coinScore;
  return 0;
}

void recordScore() {
  int score = currentScore();
  if (score > highScores[currentGame])
    highScores[currentGame] = score;
}

void enterGameOver() {
  recordScore();
  state = STATE_GAMEOVER;
  gameOverSelection = 0;
}

void drawGameOver() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(34, 2);
  display.print("GAME OVER");

  display.setCursor(20, 16);
  display.print("Score: ");
  display.print(currentScore());

  display.setCursor(20, 27);
  display.print("Best:  ");
  display.print(highScores[currentGame]);

  display.setCursor(10, 42);
  display.print(gameOverSelection == 0 ? "> Restart" : "  Restart");
  display.setCursor(10, 54);
  display.print(gameOverSelection == 1 ? "> Menu"    : "  Menu");
  display.display();
}

void updateGameOver() {
  static int lastDir = 0;
  int dir = joyYDir();
  if (dir != 0 && lastDir == 0)
    gameOverSelection = constrain(gameOverSelection + dir, 0, 1);
  lastDir = dir;

  if (buttonPressed()) {
    if (gameOverSelection == 0) startGame();
    else state = STATE_MENU;
  }
}

// =====================================================
//  START / RESET
// =====================================================
void startGame() {
  manX = 30; manY = 0; manVy = 0;
  facingRight = true; walkFrame = 0; frameCnt = 0;
  cameraY = 0;
  state = STATE_PLAYING;

  if (currentGame == GAME_DINO) {
    dinoScore = 0;
    groundScroll = 0;
    dinoSpeed = 3;
    dinoCloudX = 92;
    manX = 28;
    manY = 0;
    manVy = 0;
    walkFrame = 0;
    frameCnt = 0;
    for (int i=0; i<4; i++) spikes[i].active = false;
  }
  else if (currentGame == GAME_BOXES) {
    boxLevel = 0; highestReached = 0; cameraY = 0;
    for (int i=0; i<10; i++) {
      boxes[i].active = true;
      boxes[i].x = 16 + (i%5)*22;
      boxes[i].y = 10 + (i/5)*16;
    }
  }
  else if (currentGame == GAME_PONG) {
    paddleX = 54;
    ballX = 64; ballY = 40;
    ballVX = 2; ballVY = -2;
    pongScore = 0; pongLives = 3;
  }
  else if (currentGame == GAME_SNAKE) {
    snakeLength = 4;
    snakeDirX = 1;
    snakeDirY = 0;
    snakeNextDirX = 1;
    snakeNextDirY = 0;
    snakeScore = 0;

    int startX = SNAKE_COLS / 2;
    int startY = SNAKE_ROWS / 2;

    for (int i = 0; i < snakeLength; i++) {
      snakeX[i] = startX - i;
      snakeY[i] = startY;
    }

    snakeFoodX = 20;
    snakeFoodY = 6;
    snakeLastMove = millis();
  }
  else if (currentGame == GAME_BREAKOUT) {
    breakoutPaddleX = 50; breakoutBallX = 64; breakoutBallY = 50;
    breakoutBallVX = random(0, 2) ? 2 : -2; breakoutBallVY = -2;
    breakoutScore = 0; breakoutLives = 3;
    for (int r=0;r<BRICK_ROWS;r++) for (int col=0;col<BRICK_COLS;col++) bricks[r][col]=true;
  }
  else if (currentGame == GAME_INVADERS) {
    invaderPlayerX=56; invaderDir=1; invaderStep=0; invaderOffsetX=0; invaderDrop=0; invaderScore=0; invaderLives=3;
    for(int r=0;r<INV_ROWS;r++) for(int col=0;col<INV_COLS;col++) invaders[r][col]=true;
    for(int i=0;i<INV_MAX_BULLETS;i++) invaderBulletActive[i]=false;
  }
  else if (currentGame == GAME_ASTEROIDS) {
    asteroidShipX=64; asteroidShipY=48; asteroidVX=0; asteroidVY=0; asteroidScore=0; asteroidLives=3;
    for(int i=0;i<6;i++){ asteroids[i].active=true; asteroids[i].x=random(0,128); asteroids[i].y=random(12,45); asteroids[i].vx=random(-2,3); asteroids[i].vy=random(-1,2); if(!asteroids[i].vx && !asteroids[i].vy) asteroids[i].vx=1; asteroids[i].size=4; }
    for(int i=0;i<3;i++) asteroidBulletActive[i]=false;
  }
  else if (currentGame == GAME_FLAPPY) {
    flappyY=32; flappyV=0; flappyPipeX=128; flappyGapY=random(22,44); flappyScore=0; flappyLives=1;
  }
  else if (currentGame == GAME_RACING) {
    raceCarX=60; raceRoadOffset=0; raceScore=0; raceSpeed=2;
    for(int i=0;i<4;i++){ raceObstacles[i].active=true; raceObstacles[i].x=random(43,86); raceObstacles[i].y=-i*35-random(0,20); }
  }
  else if (currentGame == GAME_MEMORY) {
    for(int i=0;i<MEMORY_PAIRS*2;i++){ memoryCards[i]=i/2; memoryFound[i]=false; }
    for(int i=0;i<20;i++){ int a=random(0,8), b=random(0,8); int t=memoryCards[a]; memoryCards[a]=memoryCards[b]; memoryCards[b]=t; }
    memoryCursor=0; memoryFirst=-1; memorySecond=-1; memoryScore=0; memoryPauseUntil=0;
  }
  else if (currentGame == GAME_COINS) {
    coinPlayerX=64; coinPlayerY=32; coinScore=0;
    for(int i=0;i<8;i++){ coinsActive[i]=true; coinsX[i]=random(8,120); coinsY[i]=random(14,54); }
  }
}

// =====================================================
//  GAME 1 – Dino Jump (WALL·E inspired)
//
//  Controls:
//    Joystick X = move left/right
//    Joystick Y UP = jump
//    Joystick button = jump
// =====================================================
void updateDino() {
  int dir = joyXDir();

  if (dir) {
    manX += dir * 2;
    facingRight = dir > 0;
  }
  manX = constrain(manX, 10, 58);

  if ((buttonPressed() || joyYDir() < 0) && manY == 0)
    manVy = JUMP_V;

  manY += manVy;
  manVy -= GRAVITY;
  if (manY < 0) {
    manY = 0;
    manVy = 0;
  }

  frameCnt++;
  if (manY == 0 && dir && frameCnt % 3 == 0)
    walkFrame = (walkFrame + 1) % 4;

  groundScroll = (groundScroll + dinoSpeed) % 8;

  dinoCloudX--;
  if (dinoCloudX < -35)
    dinoCloudX = SCREEN_WIDTH + random(10, 50);

  static unsigned long lastSpawn = 0;
  unsigned long spawnInterval = max(650UL, 1250UL - (unsigned long)dinoScore * 18UL);

  if (millis() - lastSpawn > spawnInterval) {
    for (int i = 0; i < 4; i++) {
      if (!spikes[i].active) {
        spikes[i].x = SCREEN_WIDTH + 4;
        spikes[i].active = true;
        break;
      }
    }
    lastSpawn = millis();
  }

  for (int i = 0; i < 4; i++) {
    if (!spikes[i].active) continue;

    spikes[i].x -= dinoSpeed;

    if (spikes[i].x < manX + 7 &&
        spikes[i].x + 8 > manX - 5 &&
        manY < 10) {
      enterGameOver();
      return;
    }

    if (spikes[i].x < -12) {
      spikes[i].active = false;
      dinoScore++;

      if (dinoScore % 5 == 0 && dinoSpeed < 6)
        dinoSpeed++;
    }
  }
}

void drawDinoCharacter(int x, int y, int frame) {
  int base = GROUND_Y - y;
  int legA = (frame % 2) ? 2 : -2;
  int legB = -legA;

  // Body and head.
  display.fillRect(x - 5, base - 15, 11, 12, SSD1306_WHITE);
  display.fillRect(x + 2, base - 22, 9, 9, SSD1306_WHITE);
  display.fillRect(x + 9, base - 19, 5, 4, SSD1306_WHITE);

  // Eye and mouth.
  display.drawPixel(x + 8, base - 19, SSD1306_BLACK);
  display.drawPixel(x + 13, base - 16, SSD1306_BLACK);

  // Tail and arm.
  display.drawLine(x - 5, base - 12, x - 10, base - 9, SSD1306_WHITE);
  display.drawLine(x - 10, base - 9, x - 13, base - 9, SSD1306_WHITE);
  display.drawLine(x + 3, base - 11, x + 7, base - 8, SSD1306_WHITE);

  // Legs.
  display.drawLine(x - 2, base - 3, x - 2 + legA, base, SSD1306_WHITE);
  display.drawLine(x + 3, base - 3, x + 3 + legB, base, SSD1306_WHITE);
}

void drawDinoCactus(int x, int variant) {
  int h = (variant == 0) ? 10 : 13;
  int trunk = (variant == 0) ? 4 : 5;

  display.fillRect(x, GROUND_Y - h, trunk, h, SSD1306_WHITE);
  display.fillRect(x - 3, GROUND_Y - h + 4, 3, 4, SSD1306_WHITE);
  display.fillRect(x + trunk, GROUND_Y - h + 2, 3, 5, SSD1306_WHITE);
}

void drawDinoCloud(int x, int y) {
  display.drawCircle(x, y, 4, SSD1306_WHITE);
  display.drawCircle(x + 5, y - 2, 5, SSD1306_WHITE);
  display.drawCircle(x + 11, y, 4, SSD1306_WHITE);
  display.drawLine(x - 2, y + 3, x + 14, y + 3, SSD1306_WHITE);
}

void drawDino() {
  display.clearDisplay();

  drawDinoCloud(dinoCloudX, 17);

  for (int x = -groundScroll; x < SCREEN_WIDTH; x += 8)
    display.drawLine(x, GROUND_Y, x + 4, GROUND_Y, SSD1306_WHITE);

  for (int i = 0; i < 4; i++) {
    if (!spikes[i].active) continue;
    drawDinoCactus(spikes[i].x, i % 2);
  }

  drawDinoCharacter(manX, manY, walkFrame);

  display.setCursor(0, 0);
  display.print("S:");
  display.print(dinoScore);
  display.setCursor(90, 0);
  display.print("x");
  display.print(dinoSpeed - 2);

  display.display();
}

// =====================================================
//  GAME 2 – Box Climber
// =====================================================
void updateBoxes() {
  int dir = joyXDir();
  if (dir) { manX += dir*2; facingRight = dir>0; }
  manX = constrain(manX, 4, SCREEN_WIDTH-4);

  if ((buttonPressed() || joyYDir()<0) && manVy <= 0) {
    bool onSomething = (manY <= 2);
    for (int i=0;i<10;i++) {
      if (boxes[i].active && abs(manX-boxes[i].x)<11 && abs(manY-boxes[i].y)<5)
        onSomething = true;
    }
    if (onSomething) manVy = JUMP_V;
  }

  manY += manVy;
  manVy -= GRAVITY;

  for (int i=0;i<10;i++) {
    if (!boxes[i].active) continue;
    if (manVy <= 0 &&
        abs(manX - boxes[i].x) < 11 &&
        manY >= boxes[i].y && manY <= boxes[i].y + 6) {
      manY = boxes[i].y;
      manVy = 0;

      if (boxes[i].y > highestReached) {
        highestReached = boxes[i].y;
        boxLevel++;

        for (int j=0;j<10;j++)
          if (boxes[j].y < highestReached - 8) boxes[j].active = false;

        for (int k=0;k<3;k++) {
          for (int j=0;j<10;j++) if (!boxes[j].active) {
            boxes[j].active = true;
            boxes[j].x = 12 + random(0, 104);
            boxes[j].y = highestReached + 14 + k*13;
            break;
          }
        }
      }
    }
  }

  if (manY <= 0) {
    manY = 0;
    manVy = 0;
    if (highestReached > 15) {
      enterGameOver();
    }
  }

  int targetCam = manY - 28;
  if (targetCam > cameraY) cameraY = targetCam;
  if (cameraY < 0) cameraY = 0;

  if (manY < cameraY - 20) {
    enterGameOver();
  }

  frameCnt++;
  if (manVy==0 && dir && frameCnt%3==0) walkFrame=(walkFrame+1)%4;
}

void drawBoxes() {
  display.clearDisplay();

  int floorScreenY = GROUND_Y + cameraY;
  if (floorScreenY < SCREEN_HEIGHT)
    display.drawLine(0, floorScreenY, SCREEN_WIDTH-1, floorScreenY, SSD1306_WHITE);

  for (int i=0;i<10;i++) {
    if (!boxes[i].active) continue;
    int by = GROUND_Y - (boxes[i].y - cameraY);
    if (by > -10 && by < SCREEN_HEIGHT+10)
      display.fillRect(boxes[i].x-8, by-4, 16, 5, SSD1306_WHITE);
  }

  drawMan(manX, manY, walkFrame, facingRight);

  display.setCursor(0,0);
  display.print("Lv:");
  display.print(boxLevel);
  display.display();
}

// =====================================================
//  GAME 3 – Free Walk
// =====================================================
void updateFree() {
  int dir = joyXDir();
  if (dir) { manX += dir*2; facingRight = dir>0; }

  if ((buttonPressed() || joyYDir()<0) && manY==0) manVy = JUMP_V;

  manY += manVy; manVy -= GRAVITY;
  if (manY < 0) { manY=0; manVy=0; }

  if (manX > SCREEN_WIDTH+10) { state = STATE_MENU; return; }
  if (manX < 4) manX = 4;

  frameCnt++;
  if (manY==0 && dir && frameCnt%3==0) walkFrame=(walkFrame+1)%4;
}

void drawFree() {
  display.clearDisplay();
  display.drawLine(0, GROUND_Y, SCREEN_WIDTH-1, GROUND_Y, SSD1306_WHITE);
  drawMan(manX, manY, walkFrame, facingRight);
  display.setCursor(0,0);
  display.print("Walk off right = exit");
  display.display();
}

// =====================================================
//  GAME 4 – Ping Pong
// =====================================================
void updatePong() {
  int dir = joyXDir();
  paddleX += dir * 3;
  paddleX = constrain(paddleX, 0, SCREEN_WIDTH-20);

  ballX += ballVX;
  ballY += ballVY;

  if (ballX <= 0 || ballX >= SCREEN_WIDTH-2) ballVX = -ballVX;
  if (ballY <= 0) {
    ballVY = -ballVY;
    pongScore++;
  }

  if (ballY >= 56 && ballY <= 60 &&
      ballX >= paddleX && ballX <= paddleX+20) {
    ballVY = -abs(ballVY);
    ballVX += (ballX - (paddleX+10)) / 4;
    ballVX = constrain(ballVX, -3, 3);
  }

  if (ballY > SCREEN_HEIGHT) {
    pongLives--;
    ballX = 64; ballY = 40;
    ballVX = (random(0,2)?2:-2);
    ballVY = -2;
    if (pongLives <= 0) {
      enterGameOver();
    }
  }
}

void drawPong() {
  display.clearDisplay();

  display.fillRect(paddleX, 58, 20, 3, SSD1306_WHITE);
  display.fillRect(ballX, ballY, 3, 3, SSD1306_WHITE);

  display.setCursor(0,0);
  display.print("S:");
  display.print(pongScore);
  display.print("  L:");
  display.print(pongLives);

  display.display();
}

// =====================================================
//  GAME 5 – Snake
// =====================================================
void placeSnakeFood() {
  for (int attempts = 0; attempts < 100; attempts++) {
    int fx = random(0, SNAKE_COLS);
    int fy = random(0, SNAKE_ROWS);

    bool occupied = false;
    for (int i = 0; i < snakeLength; i++) {
      if (snakeX[i] == fx && snakeY[i] == fy) {
        occupied = true;
        break;
      }
    }

    if (!occupied) {
      snakeFoodX = fx;
      snakeFoodY = fy;
      return;
    }
  }
}

void updateSnake() {
  // Read joystick direction continuously, but only accept 90-degree turns.
  int x = joyXDir();
  int y = joyYDir();

  if (x != 0 && snakeDirX == 0) {
    snakeNextDirX = x;
    snakeNextDirY = 0;
  }
  else if (y != 0 && snakeDirY == 0) {
    snakeNextDirX = 0;
    snakeNextDirY = y;
  }

  unsigned long now = millis();

  int moveInterval = 150 - (snakeScore * 4);
  if (moveInterval < 70) moveInterval = 70;

  if (now - snakeLastMove < (unsigned long)moveInterval)
    return;

  snakeLastMove = now;
  snakeDirX = snakeNextDirX;
  snakeDirY = snakeNextDirY;

  int newX = snakeX[0] + snakeDirX;
  int newY = snakeY[0] + snakeDirY;

  // Wrap-around keeps the first Snake version simple and arcade-like.
  if (newX < 0) newX = SNAKE_COLS - 1;
  if (newX >= SNAKE_COLS) newX = 0;
  if (newY < 0) newY = SNAKE_ROWS - 1;
  if (newY >= SNAKE_ROWS) newY = 0;

  bool ateFood = (newX == snakeFoodX && newY == snakeFoodY);

  // If not eating, the old tail moves away, so it is not a collision target.
  int collisionLength = ateFood ? snakeLength : snakeLength - 1;

  for (int i = 0; i < collisionLength; i++) {
    if (snakeX[i] == newX && snakeY[i] == newY) {
      enterGameOver();
      return;
    }
  }

  int newLength = snakeLength + (ateFood ? 1 : 0);

  if (newLength > SNAKE_MAX)
    newLength = SNAKE_MAX;

  for (int i = newLength - 1; i > 0; i--) {
    snakeX[i] = snakeX[i - 1];
    snakeY[i] = snakeY[i - 1];
  }

  snakeX[0] = newX;
  snakeY[0] = newY;
  snakeLength = newLength;

  if (ateFood) {
    snakeScore++;
    placeSnakeFood();
  }
}

void drawSnake() {
  display.clearDisplay();

  display.setCursor(0, 0);
  display.print("Snake:");
  display.print(snakeScore);

  display.setCursor(78, 0);
  display.print("Best:");
  display.print(highScores[GAME_SNAKE]);

  // Playfield border.
  display.drawRect(0, SNAKE_ORIGIN_Y - 1,
                   SCREEN_WIDTH, SNAKE_ROWS * SNAKE_CELL + 2,
                   SSD1306_WHITE);

  // Food.
  int foodPx = snakeFoodX * SNAKE_CELL;
  int foodPy = SNAKE_ORIGIN_Y + snakeFoodY * SNAKE_CELL;
  display.fillRect(foodPx, foodPy, SNAKE_CELL, SNAKE_CELL, SSD1306_WHITE);

  // Snake.
  for (int i = snakeLength - 1; i >= 0; i--) {
    int px = snakeX[i] * SNAKE_CELL;
    int py = SNAKE_ORIGIN_Y + snakeY[i] * SNAKE_CELL;

    if (i == 0)
      display.fillRect(px, py, SNAKE_CELL, SNAKE_CELL, SSD1306_WHITE);
    else
      display.drawRect(px, py, SNAKE_CELL, SNAKE_CELL, SSD1306_WHITE);
  }

  display.display();
}

// =====================================================
//  GAME 6 – Breakout
// =====================================================
void updateBreakout() {
  int dir = joyXDir();
  breakoutPaddleX += dir * 3;
  breakoutPaddleX = constrain(breakoutPaddleX, 0, SCREEN_WIDTH - 24);

  breakoutBallX += breakoutBallVX;
  breakoutBallY += breakoutBallVY;

  if (breakoutBallX <= 0) {
    breakoutBallX = 0;
    breakoutBallVX = abs(breakoutBallVX);
  }
  if (breakoutBallX >= SCREEN_WIDTH - 3) {
    breakoutBallX = SCREEN_WIDTH - 3;
    breakoutBallVX = -abs(breakoutBallVX);
  }

  if (breakoutBallY <= 12) {
    breakoutBallY = 12;
    breakoutBallVY = abs(breakoutBallVY);
  }

  // Paddle collision.
  if (breakoutBallVY > 0 &&
      breakoutBallY + 3 >= 57 &&
      breakoutBallY <= 61 &&
      breakoutBallX + 3 >= breakoutPaddleX &&
      breakoutBallX <= breakoutPaddleX + 24) {
    breakoutBallY = 54;
    breakoutBallVY = -abs(breakoutBallVY);

    int offset = breakoutBallX - (breakoutPaddleX + 12);
    breakoutBallVX = constrain(2 + (offset / 6), -3, 3);
    if (breakoutBallVX == 0) breakoutBallVX = (offset < 0) ? -1 : 1;
  }

  if (breakoutBallY > SCREEN_HEIGHT) {
    if (--breakoutLives <= 0) { enterGameOver(); return; }
    breakoutBallX=64; breakoutBallY=50; breakoutBallVX=random(0,2)?2:-2; breakoutBallVY=-2;
    return;
  }

  // Brick collision.
  int brickW = 16;
  int brickH = 6;
  int brickTop = 14;

  int col = breakoutBallX / brickW;
  int row = (breakoutBallY - brickTop) / brickH;

  if (col >= 0 && col < BRICK_COLS &&
      row >= 0 && row < BRICK_ROWS &&
      bricks[row][col]) {
    bricks[row][col] = false;
    breakoutScore++;
    breakoutBallVY = -breakoutBallVY;

    bool anyLeft = false;
    for (int r = 0; r < BRICK_ROWS; r++)
      for (int c = 0; c < BRICK_COLS; c++)
        if (bricks[r][c]) anyLeft = true;

    if (!anyLeft) {
      enterGameOver();
      return;
    }
  }
}

// ===== RESTORED MISSING GAME IMPLEMENTATIONS =====
void drawBreakout(){display.clearDisplay();for(int r=0;r<BRICK_ROWS;r++)for(int c=0;c<BRICK_COLS;c++)if(bricks[r][c])display.fillRect(c*16+1,14+r*6,14,5,SSD1306_WHITE);display.fillRect(breakoutPaddleX,58,24,3,SSD1306_WHITE);display.fillRect(breakoutBallX,breakoutBallY,3,3,SSD1306_WHITE);display.setCursor(0,0);display.printf("S:%d L:%d",breakoutScore,breakoutLives);display.display();}

void fireInvader(){for(int i=0;i<INV_MAX_BULLETS;i++)if(!invaderBulletActive[i]){invaderBulletActive[i]=true;invaderBulletX[i]=invaderPlayerX+4;invaderBulletY[i]=53;return;}}
void updateInvaders(){int d=joyXDir();invaderPlayerX=constrain(invaderPlayerX+d*2,2,118);if(buttonPressed())fireInvader();static uint32_t t=0;if(millis()-t>350){t=millis();bool edge=false;for(int r=0;r<INV_ROWS;r++)for(int c=0;c<INV_COLS;c++)if(invaders[r][c]){int x=12+c*16+invaderOffsetX;if((invaderDir>0&&x>112)||(invaderDir<0&&x<2))edge=true;}if(edge){invaderDir=-invaderDir;invaderDrop+=4;}else invaderOffsetX+=invaderDir*3;}for(int i=0;i<INV_MAX_BULLETS;i++)if(invaderBulletActive[i]){invaderBulletY[i]-=4;if(invaderBulletY[i]<8)invaderBulletActive[i]=false;for(int r=0;r<INV_ROWS;r++)for(int c=0;c<INV_COLS;c++)if(invaders[r][c]){int x=12+c*16+invaderOffsetX,y=15+r*9+invaderDrop;if(invaderBulletX[i]>=x&&invaderBulletX[i]<=x+11&&invaderBulletY[i]>=y&&invaderBulletY[i]<=y+6){invaders[r][c]=false;invaderBulletActive[i]=false;invaderScore++;}}}for(int r=0;r<INV_ROWS;r++)for(int c=0;c<INV_COLS;c++)if(invaders[r][c]&&15+r*9+invaderDrop>50){enterGameOver();return;}bool left=false;for(int r=0;r<INV_ROWS;r++)for(int c=0;c<INV_COLS;c++)if(invaders[r][c])left=true;if(!left)enterGameOver();}
void drawInvaders(){display.clearDisplay();for(int r=0;r<INV_ROWS;r++)for(int c=0;c<INV_COLS;c++)if(invaders[r][c]){int x=12+c*16+invaderOffsetX,y=15+r*9+invaderDrop;display.fillRect(x+2,y,8,3,SSD1306_WHITE);display.drawLine(x,y+3,x+11,y+3,SSD1306_WHITE);}display.fillRect(invaderPlayerX,58,9,3,SSD1306_WHITE);for(int i=0;i<INV_MAX_BULLETS;i++)if(invaderBulletActive[i])display.drawFastVLine(invaderBulletX[i],invaderBulletY[i],3,SSD1306_WHITE);display.setCursor(0,0);display.printf("S:%d L:%d",invaderScore,invaderLives);display.display();}

void fireAsteroid(){for(int i=0;i<3;i++)if(!asteroidBulletActive[i]){asteroidBulletActive[i]=true;asteroidBulletX[i]=asteroidShipX;asteroidBulletY[i]=asteroidShipY-5;asteroidBulletVX[i]=0;asteroidBulletVY[i]=-4;return;}}
void updateAsteroids(){asteroidShipX=constrain(asteroidShipX+joyXDir()*2,4,123);asteroidShipY=constrain(asteroidShipY+joyYDir()*2,12,58);if(buttonPressed())fireAsteroid();for(int i=0;i<6;i++)if(asteroids[i].active){asteroids[i].x+=asteroids[i].vx;asteroids[i].y+=asteroids[i].vy;if(asteroids[i].x<-8)asteroids[i].x=136;if(asteroids[i].x>136)asteroids[i].x=-8;if(asteroids[i].y<8)asteroids[i].y=58;if(asteroids[i].y>60)asteroids[i].y=8;if((asteroids[i].x-asteroidShipX)*(asteroids[i].x-asteroidShipX)+(asteroids[i].y-asteroidShipY)*(asteroids[i].y-asteroidShipY)<64){if(--asteroidLives<=0){enterGameOver();return;}asteroids[i].x=random(0,128);asteroids[i].y=random(12,45);}}for(int b=0;b<3;b++)if(asteroidBulletActive[b]){asteroidBulletX[b]+=asteroidBulletVX[b];asteroidBulletY[b]+=asteroidBulletVY[b];if(asteroidBulletY[b]<8){asteroidBulletActive[b]=false;continue;}for(int a=0;a<6;a++)if(asteroids[a].active){int dx=asteroids[a].x-asteroidBulletX[b],dy=asteroids[a].y-asteroidBulletY[b];if(dx*dx+dy*dy<49){asteroids[a].active=false;asteroidBulletActive[b]=false;asteroidScore++;break;}}}bool any=false;for(int i=0;i<6;i++)if(asteroids[i].active)any=true;if(!any)for(int i=0;i<6;i++){asteroids[i].active=true;asteroids[i].x=random(0,128);asteroids[i].y=random(12,45);asteroids[i].vx=random(-2,3);asteroids[i].vy=random(-1,2);if(!asteroids[i].vx&&!asteroids[i].vy)asteroids[i].vx=1;}}
void drawAsteroids(){display.clearDisplay();for(int i=0;i<6;i++)if(asteroids[i].active)display.drawCircle(asteroids[i].x,asteroids[i].y,asteroids[i].size,SSD1306_WHITE);display.drawTriangle(asteroidShipX,asteroidShipY-5,asteroidShipX-4,asteroidShipY+4,asteroidShipX+4,asteroidShipY+4,SSD1306_WHITE);for(int i=0;i<3;i++)if(asteroidBulletActive[i])display.fillRect(asteroidBulletX[i],asteroidBulletY[i],2,3,SSD1306_WHITE);display.setCursor(0,0);display.printf("S:%d L:%d",asteroidScore,asteroidLives);display.display();}

void updateFlappy(){if(buttonPressed()||joyYDir()<0)flappyV=-5;flappyV=constrain(flappyV+1,-6,5);flappyY+=flappyV;flappyPipeX-=2;if(flappyPipeX<-8){flappyPipeX=128;flappyGapY=random(20,44);flappyScore++;}if(flappyY<8||flappyY>60){enterGameOver();return;}if(flappyPipeX<19&&flappyPipeX+8>13&&(flappyY<flappyGapY-10||flappyY>flappyGapY+10))enterGameOver();}
void drawFlappy(){display.clearDisplay();display.fillCircle(16,flappyY,3,SSD1306_WHITE);display.fillRect(flappyPipeX,8,8,max(0,flappyGapY-18),SSD1306_WHITE);display.fillRect(flappyPipeX,flappyGapY+10,8,64-(flappyGapY+10),SSD1306_WHITE);display.setCursor(0,0);display.printf("S:%d",flappyScore);display.display();}

void updateRacing(){raceCarX=constrain(raceCarX+joyXDir()*3,42,82);raceRoadOffset=(raceRoadOffset+raceSpeed)%12;for(int i=0;i<4;i++){raceObstacles[i].y+=raceSpeed;if(raceObstacles[i].y>64){raceObstacles[i].y=-random(20,60);raceObstacles[i].x=random(45,80);raceScore++;if(raceScore%10==0&&raceSpeed<5)raceSpeed++;}if(raceObstacles[i].y>48&&raceObstacles[i].y<62&&abs(raceObstacles[i].x-raceCarX)<8){enterGameOver();return;}}}
void drawRacing(){display.clearDisplay();display.drawLine(38,0,38,64,SSD1306_WHITE);display.drawLine(90,0,90,64,SSD1306_WHITE);for(int y=-12+raceRoadOffset;y<64;y+=12)display.fillRect(63,y,2,7,SSD1306_WHITE);for(int i=0;i<4;i++)display.fillRect(raceObstacles[i].x-4,raceObstacles[i].y,8,10,SSD1306_WHITE);display.fillRect(raceCarX-4,53,8,9,SSD1306_WHITE);display.setCursor(0,0);display.printf("S:%d x%d",raceScore,raceSpeed);display.display();}

void updateMemory(){static int lx=0,ly=0;if(memoryPauseUntil){if(millis()<memoryPauseUntil)return;if(memoryCards[memoryFirst]==memoryCards[memorySecond]){memoryFound[memoryFirst]=true;memoryFound[memorySecond]=true;memoryScore++;}memoryFirst=memorySecond=-1;memoryPauseUntil=0;}int x=joyXDir(),y=joyYDir();if(x&& !lx){int col=constrain(memoryCursor%4+x,0,3);memoryCursor=(memoryCursor/4)*4+col;}if(y&&!ly){int row=constrain(memoryCursor/4+y,0,1);memoryCursor=row*4+memoryCursor%4;}lx=x;ly=y;if(buttonPressed()&&!memoryFound[memoryCursor]){if(memoryFirst<0)memoryFirst=memoryCursor;else if(memorySecond<0&&memoryCursor!=memoryFirst){memorySecond=memoryCursor;memoryPauseUntil=millis()+650;}}bool done=true;for(int i=0;i<8;i++)if(!memoryFound[i])done=false;if(done)enterGameOver();}
void drawMemory(){display.clearDisplay();for(int i=0;i<8;i++){int x=3+(i%4)*31,y=10+(i/4)*25;display.drawRect(x,y,27,22,SSD1306_WHITE);bool show=memoryFound[i]||i==memoryFirst||i==memorySecond;display.setCursor(x+9,y+7);display.print(show?String(memoryCards[i]+1):String("?"));if(i==memoryCursor)display.drawRect(x-1,y-1,29,24,SSD1306_WHITE);}display.setCursor(0,0);display.printf("Pairs:%d",memoryScore);display.display();}

void updateCoins(){coinPlayerX=constrain(coinPlayerX+joyXDir()*2,4,123);coinPlayerY=constrain(coinPlayerY+joyYDir()*2,10,59);for(int i=0;i<8;i++)if(coinsActive[i]){int dx=coinsX[i]-coinPlayerX,dy=coinsY[i]-coinPlayerY;if(dx*dx+dy*dy<36){coinsActive[i]=false;coinScore++;}}bool any=false;for(int i=0;i<8;i++)if(coinsActive[i])any=true;if(!any)for(int i=0;i<8;i++){coinsActive[i]=true;coinsX[i]=random(8,120);coinsY[i]=random(14,54);}}
void drawCoins(){display.clearDisplay();for(int i=0;i<8;i++)if(coinsActive[i])display.drawCircle(coinsX[i],coinsY[i],3,SSD1306_WHITE);display.fillRect(coinPlayerX-3,coinPlayerY-3,7,7,SSD1306_WHITE);display.setCursor(0,0);display.printf("Coins:%d",coinScore);display.display();}

void updateCurrentGame(){switch(currentGame){case GAME_DINO:updateDino();break;case GAME_BOXES:updateBoxes();break;case GAME_FREE:updateFree();break;case GAME_PONG:updatePong();break;case GAME_SNAKE:updateSnake();break;case GAME_BREAKOUT:updateBreakout();break;case GAME_INVADERS:updateInvaders();break;case GAME_ASTEROIDS:updateAsteroids();break;case GAME_FLAPPY:updateFlappy();break;case GAME_RACING:updateRacing();break;case GAME_MEMORY:updateMemory();break;case GAME_COINS:updateCoins();break;}}
void drawCurrentGame(){switch(currentGame){case GAME_DINO:drawDino();break;case GAME_BOXES:drawBoxes();break;case GAME_FREE:drawFree();break;case GAME_PONG:drawPong();break;case GAME_SNAKE:drawSnake();break;case GAME_BREAKOUT:drawBreakout();break;case GAME_INVADERS:drawInvaders();break;case GAME_ASTEROIDS:drawAsteroids();break;case GAME_FLAPPY:drawFlappy();break;case GAME_RACING:drawRacing();break;case GAME_MEMORY:drawMemory();break;case GAME_COINS:drawCoins();break;}}

void setup(){Serial.begin(115200);delay(200);pinMode(JOY_BTN,INPUT_PULLUP);analogReadResolution(12);Wire.begin(OLED_SDA,OLED_SCL);if(!display.begin(SSD1306_SWITCHCAPVCC,OLED_ADDR)){Serial.println("SSD1306 failed");while(true)delay(1000);}WiFi.mode(WIFI_OFF);randomSeed(micros());webServer.on("/",handleRoot);webServer.onNotFound(handleNotFound);display.clearDisplay();display.setTextColor(SSD1306_WHITE);display.setTextSize(1);display.setCursor(22,25);display.print("POCKET ARCADE");display.display();delay(600);lastFrame=millis();}
void loop(){unsigned long now=millis();if(wifiApStarted)webServer.handleClient();if(now-lastFrame<FRAME_TIME_MS)return;lastFrame=now;switch(state){case STATE_MENU:updateMenu();drawMenu();break;case STATE_PLAYING:updateCurrentGame();if(state==STATE_PLAYING)drawCurrentGame();break;case STATE_GAMEOVER:updateGameOver();drawGameOver();break;case STATE_WIFI:updateWiFiStatus();if(state==STATE_WIFI)drawWiFiStatus();break;}}

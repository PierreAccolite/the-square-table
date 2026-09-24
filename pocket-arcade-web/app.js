const POCKET_ARCADE_BAUD=74880;
let port=null,reader=null,writer=null,keepReading=false,receiveBuffer="",calibrationRunning=false;
const $=id=>document.getElementById(id);
const log=message=>{const box=$("console");box.textContent+=message+"\n";box.scrollTop=box.scrollHeight;};

function setConnected(connected){
  $("connect").disabled=connected;$("disconnect").disabled=!connected;$("ping").disabled=!connected;
  $("streamOn").disabled=!connected||calibrationRunning;$("streamOff").disabled=!connected||calibrationRunning;
  $("menu").disabled=!connected||calibrationRunning;$("back").disabled=!connected||calibrationRunning;
  $("select").disabled=!connected||calibrationRunning;$("start").disabled=!connected||calibrationRunning;
  $("calibrate").disabled=!connected||calibrationRunning;
  $("status").textContent="Status: "+(connected?"Connected":"Disconnected");
}
async function connectSerial(){
  if(!("serial" in navigator)){ $("status").textContent="Status: Web Serial is not available in this browser";log("ERROR: navigator.serial is not available.");return; }
  try{
    port=await navigator.serial.requestPort();
    await port.open({baudRate:POCKET_ARCADE_BAUD,dataBits:8,stopBits:1,parity:"none",bufferSize:4096,flowControl:"none"});
    writer=port.writable.getWriter();setConnected(true);
    const info=port.getInfo();$("device").textContent="Device: VID "+(info.usbVendorId??"—")+" / PID "+(info.usbProductId??"—");
    log("CONNECTED");log("Pocket Arcade USB serial link ready.");keepReading=true;readLoop();
    log("Waiting 2 seconds for ESP32 boot...");await new Promise(resolve=>setTimeout(resolve,2000));
    if(keepReading&&writer)await sendLine("PING");
  }catch(err){log("CONNECT ERROR: "+err);setConnected(false);}
}
async function readLoop(){
  if(!port||!port.readable)return;let framingRetries=0;
  while(keepReading&&port&&port.readable){
    reader=port.readable.getReader();
    try{
      while(keepReading){
        const {value,done}=await reader.read();if(done)break;if(!value)continue;
        receiveBuffer+=new TextDecoder().decode(value);let newline;
        while((newline=receiveBuffer.indexOf("\n"))>=0){
          const line=receiveBuffer.slice(0,newline).replace(/\r$/,"");receiveBuffer=receiveBuffer.slice(newline+1);handleLine(line);
        }
      }
      framingRetries=0;break;
    }catch(err){
      const message=String(err);
      if(message.includes("FramingError")||message.toLowerCase().includes("framing error")){
        framingRetries++;log("SERIAL FRAMING ERROR - retry "+framingRetries+"/5");
        try{reader.releaseLock();}catch(_){} reader=null;
        if(framingRetries>=5||!keepReading){log("READ STOPPED: repeated framing errors.");break;}
        await new Promise(resolve=>setTimeout(resolve,100));continue;
      }
      log("READ ERROR: "+message);break;
    }finally{if(reader){try{reader.releaseLock();}catch(_){}reader=null;}}
  }
}
function handleLine(line){
  if(!line)return;log("< "+line);
  if(line.startsWith("JOY,")){const p=line.split(",");if(p.length>=4){$("joyX").textContent=p[1];$("joyY").textContent=p[2];$("joyBtn").textContent=p[3];updateStick(Number(p[1]),Number(p[2]));}}
  if(line.startsWith("JOYRAW,")){const p=line.split(",");if(p.length>=4){$("joyX").textContent=p[1]+" raw";$("joyY").textContent=p[2]+" raw";$("joyBtn").textContent=p[3];updateStick(Number(p[1]),Number(p[2]));}}
  if(line.startsWith("CAL,"))handleCalibrationLine(line);
}
function handleCalibrationLine(line){
  const p=line.split(",");
  if(p[1]==="START"){calibrationRunning=true;setConnected(true);$("calibrationStatus").textContent="Keep the joystick centered...";$("calibrationText").textContent="Step 1/2: do not touch the joystick.";}
  else if(p[1]==="CENTER_CAPTURED"){$("calibrationStatus").textContent="Center captured. Now move the joystick slowly through the full range.";$("calibrationText").textContent="Step 2/2: move X and Y to all extremes for about 6 seconds.";}
  else if(p[1]==="RANGE")$("calibrationStatus").textContent="Move the joystick through all four corners and both axes.";
  else if(p[1]==="DONE"){calibrationRunning=false;$("calibrationStatus").textContent="Calibration saved to the ESP32.";$("calibrationText").textContent="Calibration is stored and will survive a reboot.";setConnected(true);}
  else if(p[1]==="ERROR"){calibrationRunning=false;$("calibrationStatus").textContent="Calibration failed: "+p.slice(2).join(",");$("calibrationText").textContent="Try again and move the joystick through its complete range.";setConnected(true);}
}
function updateStick(x,y){const dot=$("stickDot");if(!dot||!Number.isFinite(x)||!Number.isFinite(y))return;const cx=Math.max(-100,Math.min(100,x)),cy=Math.max(-100,Math.min(100,y));dot.style.left=(50+cx*.42)+"%";dot.style.top=(50-cy*.42)+"%";}
async function sendLine(line){if(!writer)return;await writer.write(new TextEncoder().encode(line+"\n"));log("> "+line);}
async function gameCommand(command){if(!writer||calibrationRunning)return;await sendLine(command);}
async function calibrateJoystick(){if(!writer||calibrationRunning)return;calibrationRunning=true;setConnected(true);$("calibrationStatus").textContent="Starting calibration...";$("calibrationText").textContent="Keep the joystick completely centered when prompted.";await sendLine("CALIBRATE");}
async function disconnectSerial(){
  keepReading=false;calibrationRunning=false;
  try{if(reader){await reader.cancel();reader.releaseLock();reader=null;}}catch(_){}
  try{if(writer){writer.releaseLock();writer=null;}}catch(_){}
  try{if(port)await port.close();}catch(_){}
  port=null;setConnected(false);$("device").textContent="Device: —";$("calibrationText").textContent="The current calibration is stored in the ESP32.";$("calibrationStatus").textContent="";log("DISCONNECTED");
}
$("connect").addEventListener("click",connectSerial);$("disconnect").addEventListener("click",disconnectSerial);
$("ping").addEventListener("click",()=>sendLine("PING"));$("streamOn").addEventListener("click",()=>sendLine("INPUT"));$("streamOff").addEventListener("click",()=>sendLine("INPUT OFF"));
$("start").addEventListener("click",()=>gameCommand("START"));$("select").addEventListener("click",()=>gameCommand("SELECT"));$("menu").addEventListener("click",()=>gameCommand("MENU"));$("back").addEventListener("click",()=>gameCommand("BACK"));$("calibrate").addEventListener("click",calibrateJoystick);
document.addEventListener("keydown",event=>{if(!port||calibrationRunning||event.repeat)return;const key=event.key.toLowerCase();let command=null;if(event.key==="Enter")command="START";else if(event.code==="Space")command="SELECT";else if(event.key==="Escape")command="BACK";else if(key==="m")command="MENU";if(command){event.preventDefault();gameCommand(command);}});
if(!("serial" in navigator))log("WARNING: This browser does not expose Web Serial.");
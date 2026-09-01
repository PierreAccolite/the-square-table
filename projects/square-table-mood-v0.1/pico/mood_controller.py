from machine import Pin, ADC
import neopixel
import time
import sys
import json
import uselect
import dht
import math

MAX_LEDS = 32
LED_PIN = 0
DHT_PIN = 15
AUTO_TEMP_INTERVAL_MS = 10000
AUTO_TEMP_BRIGHTNESS = 70

MAP_4X4 = [0,1,2,3,7,6,5,4,8,9,10,11,15,14,13,12]
MAP_4X8 = [0,1,2,3,4,5,6,7,15,14,13,12,11,10,9,8,16,17,18,19,20,21,22,23,31,30,29,28,27,26,25,24]

np = neopixel.NeoPixel(Pin(LED_PIN), MAX_LEDS)
sensor = dht.DHT11(Pin(DHT_PIN))
pico_adc = ADC(4)
poll = uselect.poll()
poll.register(sys.stdin, uselect.POLLIN)

host_active = False
last_auto_temp_ms = time.ticks_ms()
last_temp = None


def clear():
    for i in range(MAX_LEDS): np[i] = (0,0,0)
    np.write()


def mapping_for(count):
    return MAP_4X8 if count == 32 else MAP_4X4


def clamp(v, lo=0, hi=255):
    return max(lo, min(hi, int(v)))


def rgb(value, default=(255,255,255)):
    if not isinstance(value, (list,tuple)) or len(value) != 3: return default
    return (clamp(value[0]), clamp(value[1]), clamp(value[2]))


def scale(c, brightness):
    f = max(0, min(100, int(brightness))) / 100
    return (int(c[0]*f), int(c[1]*f), int(c[2]*f))


def show_pixels(pixels, brightness=100):
    count = len(pixels)
    mapping = mapping_for(count)
    for i in range(MAX_LEDS): np[i] = (0,0,0)
    for logical in range(count): np[mapping[logical]] = scale(pixels[logical], brightness)
    np.write()


def check_command():
    if poll.poll(0): return sys.stdin.readline().strip()
    return None


def normalize_pixels(pixels):
    if not isinstance(pixels,list) or len(pixels) not in (16,32): return None
    out=[]
    for p in pixels:
        if not isinstance(p,(list,tuple)) or len(p)!=3: return None
        out.append((clamp(p[0]),clamp(p[1]),clamp(p[2])))
    return out


def temperature_color(temp):
    stops=[(-10,(0,70,255)),(0,(0,180,255)),(10,(0,255,180)),(20,(80,255,0)),(25,(255,220,0)),(30,(255,120,0)),(35,(255,35,0)),(40,(150,0,0))]
    if temp <= stops[0][0]: return stops[0][1]
    if temp >= stops[-1][0]: return stops[-1][1]
    for (t1,c1),(t2,c2) in zip(stops,stops[1:]):
        if t1 <= temp <= t2:
            f=(temp-t1)/(t2-t1)
            return tuple(int(c1[i]+(c2[i]-c1[i])*f) for i in range(3))
    return (255,255,255)


def read_dht():
    try:
        sensor.measure()
        return sensor.temperature(),sensor.humidity(),None
    except Exception as e:
        return None,None,str(e)


def read_pico_temperature():
    reading=pico_adc.read_u16(); voltage=reading*3.3/65535
    return 27-(voltage-0.706)/0.001721


def show_temperature(temp, brightness=AUTO_TEMP_BRIGHTNESS, count=16):
    show_pixels([temperature_color(temp)]*count, brightness)


def automatic_temperature_update(force=False):
    global last_auto_temp_ms,last_temp
    if host_active and not force: return
    temp,humidity,error=read_dht(); last_auto_temp_ms=time.ticks_ms()
    if temp is not None:
        last_temp=temp; show_temperature(temp)
        print("AUTO_TEMP %.1f %.1f"%(temp,humidity))
    elif force:
        show_pixels([(0,40,120)]*16,40); print("AUTO_TEMP_ERROR",error)


def blend(a,b,f):
    return (int(a[0]+(b[0]-a[0])*f),int(a[1]+(b[1]-a[1])*f),int(a[2]+(b[2]-a[2])*f))


def transition(current,target,kind,duration,brightness):
    kind=str(kind).upper(); count=len(target); width=8 if count==32 else 4; height=4
    if kind=="CUT" or current is None or len(current)!=count:
        show_pixels(target,brightness); return check_command()
    steps=max(4,min(32,int(duration)//40)); delay=max(0.01,duration/steps/1000)
    if kind=="FADE":
        for s in range(steps+1):
            show_pixels([blend(current[i],target[i],s/steps) for i in range(count)],brightness); time.sleep(delay)
            cmd=check_command()
            if cmd:return cmd
    else:
        show_pixels(target,brightness); time.sleep(max(0.02,duration/1000))
        cmd=check_command()
        if cmd:return cmd
    return None


def run_custom_mood(payload):
    pixels=normalize_pixels(payload.get("pixels"))
    if pixels is None: print("ERROR: mood needs 16 or 32 pixels"); return None
    effect=str(payload.get("effect","STATIC")).upper(); speed=max(20,min(2000,int(payload.get("speed",100)))); brightness=max(1,min(100,int(payload.get("brightness",100))))
    if effect=="STATIC": show_pixels(pixels,brightness); return None
    if effect=="WIPE":
        for n in range(1,len(pixels)+1):
            frame=list(pixels)
            for i in range(n,len(frame)): frame[i]=(0,0,0)
            show_pixels(frame,brightness); time.sleep(speed/1000)
            cmd=check_command()
            if cmd:return cmd
        return None
    if effect in ("SCROLL","PULSE"):
        offset=0
        while True:
            if effect=="SCROLL":
                show_pixels([pixels[(i-offset)%len(pixels)] for i in range(len(pixels))],brightness); offset=(offset+1)%len(pixels); time.sleep(speed/1000)
            else:
                for level in list(range(10,101,5))+list(range(100,9,-5)):
                    show_pixels(pixels,int(brightness*level/100)); time.sleep(speed/5000)
                    cmd=check_command()
                    if cmd:return cmd
                continue
            cmd=check_command()
            if cmd:return cmd
    show_pixels(pixels,brightness); return None


def run_feed(payload):
    frames=payload.get("frames")
    if not isinstance(frames,list) or not frames:return None
    loop=bool(payload.get("loop",True)); current=None
    while True:
        for frame in frames:
            pixels=normalize_pixels(frame.get("pixels") if isinstance(frame,dict) else None)
            if pixels is None: continue
            kind=str(frame.get("transition","FADE")).upper() if isinstance(frame,dict) else "FADE"
            duration=int(frame.get("duration",1200)) if isinstance(frame,dict) else 1200
            brightness=max(1,min(100,int(frame.get("brightness",100)))) if isinstance(frame,dict) else 100
            cmd=transition(current,pixels,kind,duration,brightness); current=pixels
            if cmd:return cmd
            hold=max(20,min(10000,int(frame.get("hold",500)))) if isinstance(frame,dict) else 500
            end=time.ticks_add(time.ticks_ms(),hold)
            while time.ticks_diff(end,time.ticks_ms())>0:
                time.sleep_ms(30); cmd=check_command()
                if cmd:return cmd
        if not loop:return None


def live_rain(payload):
    count=32 if int(payload.get("width",4))==8 else 16
    width=8 if count==32 else 4; height=4
    base=rgb(payload.get("color"),(0,255,80)); accent=rgb(payload.get("accent"),(255,255,255))
    brightness=max(1,min(100,int(payload.get("brightness",70)))); delay=max(0.02,min(1.0,int(payload.get("speed",90))/1000))
    seeds=[(c*17+11)%29 for c in range(width)]; frame=0
    while True:
        pixels=[(0,0,0)]*count
        for c in range(width):
            phase=(frame*(1+(c%3)*0.17)+seeds[c])%(height+7)
            head=phase-2; trail=2+(seeds[c]%3)
            for r in range(height):
                d=head-r
                if 0<=d<=trail:
                    pixels[r*width+c]=accent if d<0.8 else scale(base,max(18,100-int(d*24)))
            if (frame+c*7)%23==0:
                pixels[(seeds[c]+frame)%height*width+c]=scale(accent,55)
        show_pixels(pixels,brightness); frame=(frame+1)%100000; time.sleep(delay)
        cmd=check_command()
        if cmd:return cmd


def palette_color(pos,colors):
    n=len(colors); p=(pos%n+n)%n; i=int(p); f=p-i
    return blend(colors[i],colors[(i+1)%n],f)


def live_clouds(payload):
    count=32 if int(payload.get("width",4))==8 else 16
    width=8 if count==32 else 4; colors=[rgb(c) for c in payload.get("colors",[(158,220,255),(216,180,254),(255,214,231),(255,242,178)])]
    if len(colors)<2: colors=[colors[0],colors[0]]
    brightness=max(1,min(100,int(payload.get("brightness",70)))); delay=max(0.02,min(1.0,int(payload.get("speed",70))/1000)); frame=0
    while True:
        pixels=[]
        for r in range(4):
            for c in range(width):
                wave=math.sin(frame*0.045+c*0.75+r*1.05)*0.8+math.sin(frame*0.021-c*0.42+r*0.55)*0.55+math.sin(frame*0.013+r+c*0.18)*0.3
                pos=(wave+1.65)*len(colors)/3.3
                pixels.append(palette_color(pos,colors))
        show_pixels(pixels,brightness); frame=(frame+1)%100000; time.sleep(delay)
        cmd=check_command()
        if cmd:return cmd


def run_live_effect(payload):
    effect=str(payload.get("effect","")).upper()
    if effect=="CODE_RAIN": return live_rain(payload)
    if effect=="PASTEL_CLOUDS": return live_clouds(payload)
    print("ERROR: unknown live effect",effect); return None


def read_sensor():
    temp,humidity,error=read_dht(); result={"type":"sensor","pico_temperature":round(read_pico_temperature(),1)}
    if temp is None: result["error"]=error
    else: result["temperature"]=temp; result["humidity"]=humidity
    return result


def breathing(r,g,b):
    for brightness in range(2,30,2):
        for i in range(MAX_LEDS): np[i]=(r*brightness//30,g*brightness//30,b*brightness//30)
        np.write();time.sleep(0.04)
    for brightness in range(30,2,-2):
        for i in range(MAX_LEDS): np[i]=(r*brightness//30,g*brightness//30,b*brightness//30)
        np.write();time.sleep(0.04)


def thinking():
    clear()
    for i in range(MAX_LEDS):
        clear();np[i]=(0,0,25)
        if i>0:np[i-1]=(0,0,8)
        np.write();time.sleep(0.08)


def disagreement():
    for c in ((25,0,0),(0,0,25)):
        for i in range(MAX_LEDS):np[i]=c
        np.write();time.sleep(0.3)


def happy():
    clear()
    for i in [0,3,4,7,9,10,13,14]:np[i]=(0,25,0)
    np.write()


def show_mood(mood):
    mood=mood.strip().upper()
    if mood=="HAPPY":happy()
    elif mood=="THINKING":thinking()
    elif mood=="OPTIMISTIC":breathing(0,25,5)
    elif mood=="SKEPTICAL":breathing(25,10,0)
    elif mood=="ERROR":
        for i in range(MAX_LEDS):np[i]=(30,0,0)
        np.write();time.sleep(0.3);clear();time.sleep(0.3)
    elif mood=="AGREEMENT":
        for i in range(MAX_LEDS):np[i]=(0,25,10)
        np.write();time.sleep(1)
    elif mood=="DISAGREEMENT":disagreement()
    elif mood=="BUFFERING":breathing(25,8,0)
    elif mood=="IDLE":breathing(0,5,15)
    else:clear()


automatic_temperature_update(force=True)
print("AI Mood Matrix online")
print("Stand-alone DHT11 temperature mode ready")
print("Pico internal temperature sensor available")
print("4x4 / 4x8 mode supported")
print("READY")

while True:
    if not host_active and time.ticks_diff(time.ticks_ms(),last_auto_temp_ms)>=AUTO_TEMP_INTERVAL_MS: automatic_temperature_update()
    events=poll.poll(100)
    if not events: continue
    command=sys.stdin.readline().strip()
    if not command: continue
    upper=command.upper()
    if upper=="HOST_ON": host_active=True; print("HOST_ON"); continue
    if upper=="HOST_OFF": host_active=False; automatic_temperature_update(force=True); print("HOST_OFF");print("READY");continue
    if upper=="SENSOR": print(json.dumps(read_sensor(),separators=(",",":")));print("READY");continue
    if upper=="STOP_EFFECT": clear();print("EFFECT_STOPPED");print("READY");continue
    returned=None
    if command.startswith("{"):
        try:
            payload=json.loads(command)
            kind=payload.get("type")
            if kind=="mood": returned=run_custom_mood(payload)
            elif kind=="feed": returned=run_feed(payload)
            elif kind=="effect": returned=run_live_effect(payload)
            else: print("ERROR: unknown JSON command")
        except Exception as e:
            print("ERROR:",e); continue
        if returned:
            returned_upper=returned.upper()
            if returned_upper=="STOP_EFFECT": clear();print("EFFECT_STOPPED");print("READY");continue
            if returned_upper=="HOST_OFF":
                host_active=False;automatic_temperature_update(force=True);print("HOST_OFF");print("READY");continue
            if returned.startswith("{"): command=returned
            else: command=returned
        else:
            print("READY");continue
    if command and not command.startswith("{"):
        show_mood(command);print("READY")

from machine import Pin, ADC
import neopixel
import time
import sys
import json
import uselect
import dht
import math
import random

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
active_effect = None
rain_state = None
cloud_state = None


def clear():
    for i in range(MAX_LEDS):
        np[i] = (0, 0, 0)
    np.write()


def clamp(v, lo=0, hi=255):
    return max(lo, min(hi, int(v)))


def rgb(value, default=(255,255,255)):
    if not isinstance(value, (list,tuple)) or len(value) != 3:
        return default
    return (clamp(value[0]), clamp(value[1]), clamp(value[2]))


def scale(c, brightness):
    f = max(0, min(100, int(brightness))) / 100
    return (int(c[0]*f), int(c[1]*f), int(c[2]*f))


def mapping_for(count):
    return MAP_4X8 if count == 32 else MAP_4X4


def show_pixels(pixels, brightness=100):
    count = len(pixels)
    if count not in (16,32):
        return
    mapping = mapping_for(count)
    for i in range(MAX_LEDS):
        np[i] = (0,0,0)
    for logical in range(count):
        np[mapping[logical]] = scale(pixels[logical], brightness)
    np.write()


def check_command():
    if poll.poll(0):
        return sys.stdin.readline().strip()
    return None


def normalize_pixels(pixels):
    if not isinstance(pixels,list) or len(pixels) not in (16,32):
        return None
    out=[]
    for p in pixels:
        if not isinstance(p,(list,tuple)) or len(p)!=3:
            return None
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


def read_dht(retries=2):
    last_error = None
    for attempt in range(retries+1):
        try:
            sensor.measure()
            return sensor.temperature(), sensor.humidity(), None
        except Exception as e:
            last_error = str(e)
            if attempt < retries:
                time.sleep_ms(250)
    return None,None,last_error


def read_pico_temperature():
    reading=pico_adc.read_u16()
    voltage=reading*3.3/65535
    return 27-(voltage-0.706)/0.001721


def show_temperature(temp, brightness=AUTO_TEMP_BRIGHTNESS, count=16):
    show_pixels([temperature_color(temp)]*count, brightness)


def automatic_temperature_update(force=False):
    global last_auto_temp_ms,last_temp
    if host_active and not force:
        return
    temp,humidity,error=read_dht(2)
    last_auto_temp_ms=time.ticks_ms()
    if temp is not None:
        last_temp=temp
        show_temperature(temp)
        print("AUTO_TEMP %.1f %.1f" % (temp,humidity))
    elif force:
        # Always leave a visible boot indication even if the first DHT11 read fails.
        show_pixels([(0,35,120)]*16, 55)
        print("AUTO_TEMP_ERROR",error)


def stop_live_effect():
    global active_effect,rain_state,cloud_state
    active_effect=None
    rain_state=None
    cloud_state=None
    clear()
    print("EFFECT_STOPPED")


def start_live_effect(payload):
    global active_effect,rain_state,cloud_state
    effect=str(payload.get("effect","")).upper()
    if effect not in ("CODE_RAIN","PASTEL_CLOUDS"):
        print("ERROR: unknown live effect",effect)
        return
    active_effect=payload.copy()
    rain_state=None
    cloud_state=None
    print("EFFECT_STARTED",effect)


def effect_interval(payload):
    return max(20,min(1000,int(payload.get("speed",80))))


def live_rain_step(payload):
    global rain_state
    count=32 if int(payload.get("width",4))==8 else 16
    width=8 if count==32 else 4
    base=rgb(payload.get("color"),(0,255,80))
    accent=rgb(payload.get("accent"),(255,255,255))
    brightness=max(1,min(100,int(payload.get("brightness",70))))
    if rain_state is None or rain_state.get("count")!=count:
        rain_state={"count":count,"frame":0,"seeds":[random.randint(0,9999) for _ in range(width)],"lengths":[random.randint(2,5) for _ in range(width)]}
    state=rain_state
    frame_no=state["frame"]
    pixels=[(0,0,0)]*count
    for c in range(width):
        phase=(frame_no*(1+(c%3)*0.17)+state["seeds"][c])%12
        head=phase-2
        trail=state["lengths"][c]
        for r in range(4):
            d=head-r
            if 0 <= d <= trail:
                pixels[r*width+c]=accent if d<0.8 else scale(base,max(18,100-int(d*24)))
        if random.random()<0.08:
            pixels[random.randrange(4)*width+c]=scale(accent,random.randint(35,80))
        if random.random()<0.025:
            state["seeds"][c]=random.randint(0,9999)
            state["lengths"][c]=random.randint(2,5)
    show_pixels(pixels,brightness)
    state["frame"]=(frame_no+1)%100000


def blend(a,b,f):
    return tuple(int(a[i]+(b[i]-a[i])*f) for i in range(3))


def palette_color(pos,colors):
    n=len(colors)
    p=(pos%n+n)%n
    i=int(p)
    return blend(colors[i],colors[(i+1)%n],p-i)


def live_clouds_step(payload):
    global cloud_state
    count=32 if int(payload.get("width",4))==8 else 16
    width=8 if count==32 else 4
    colors=[rgb(c) for c in payload.get("colors",[(158,220,255),(216,180,254),(255,214,231),(255,242,178)])]
    if len(colors)<2: colors=[colors[0],colors[0]]
    brightness=max(1,min(100,int(payload.get("brightness",70))))
    if cloud_state is None or cloud_state.get("count")!=count:
        cloud_state={"count":count,"frame":0,"phase":[random.random()*10 for _ in range(3)]}
    state=cloud_state
    frame=state["frame"]
    p0,p1,p2=state["phase"]
    pixels=[]
    for r in range(4):
        for c in range(width):
            wave=(math.sin(frame*0.045+c*0.75+r*1.05+p0)*0.8+math.sin(frame*0.021-c*0.42+r*0.55+p1)*0.55+math.sin(frame*0.013+r+c*0.18+p2)*0.3)
            pixels.append(palette_color((wave+1.65)*len(colors)/3.3,colors))
    show_pixels(pixels,brightness)
    state["frame"]=(frame+1)%100000


def step_live_effect():
    if not active_effect:
        return
    effect=str(active_effect.get("effect","")).upper()
    if effect=="CODE_RAIN": live_rain_step(active_effect)
    elif effect=="PASTEL_CLOUDS": live_clouds_step(active_effect)
    time.sleep_ms(effect_interval(active_effect))


def read_sensor():
    temp,humidity,error=read_dht(2)
    result={"type":"sensor","pico_temperature":round(read_pico_temperature(),1)}
    if temp is None:
        result["error"]=error
    else:
        result["temperature"]=temp
        result["humidity"]=humidity
    return result


def run_custom_mood(payload):
    pixels=normalize_pixels(payload.get("pixels"))
    if pixels is None:
        print("ERROR: mood needs 16 or 32 pixels")
        return
    brightness=max(1,min(100,int(payload.get("brightness",100))))
    effect=str(payload.get("effect","STATIC")).upper()
    speed=max(20,min(2000,int(payload.get("speed",100))))
    if effect=="STATIC":
        show_pixels(pixels,brightness); return
    if effect=="WIPE":
        for n in range(1,len(pixels)+1):
            frame=list(pixels)
            for i in range(n,len(frame)): frame[i]=(0,0,0)
            show_pixels(frame,brightness); time.sleep_ms(speed)
            cmd=check_command()
            if cmd: handle_command(cmd); return
        return
    if effect=="SCROLL":
        offset=0
        while True:
            show_pixels([pixels[(i-offset)%len(pixels)] for i in range(len(pixels))],brightness)
            offset=(offset+1)%len(pixels); time.sleep_ms(speed)
            cmd=check_command()
            if cmd: handle_command(cmd); return
    if effect=="PULSE":
        while True:
            for level in list(range(10,101,5))+list(range(100,9,-5)):
                show_pixels(pixels,int(brightness*level/100)); time.sleep_ms(max(10,speed//5))
                cmd=check_command()
                if cmd: handle_command(cmd); return
    show_pixels(pixels,brightness)


def show_mood(mood):
    mood=mood.strip().upper()
    if mood=="HAPPY":
        clear()
        for i in [0,3,4,7,9,10,13,14]: np[i]=(0,25,0)
        np.write()
    elif mood=="THINKING":
        clear()
        for i in range(16):
            clear(); np[i]=(0,0,25)
            if i>0: np[i-1]=(0,0,8)
            np.write(); time.sleep_ms(80)
    elif mood=="OPTIMISTIC": breathing(0,25,5)
    elif mood=="SKEPTICAL": breathing(25,10,0)
    elif mood=="ERROR": fill((30,0,0)); time.sleep_ms(300); clear(); time.sleep_ms(300)
    elif mood=="AGREEMENT": fill((0,25,10)); time.sleep_ms(1000)
    elif mood=="DISAGREEMENT": fill((25,0,0)); time.sleep_ms(300); fill((0,0,25)); time.sleep_ms(300)
    elif mood=="BUFFERING": breathing(25,8,0)
    elif mood=="IDLE": breathing(0,5,15)
    else: clear()


def fill(c):
    for i in range(MAX_LEDS): np[i]=c
    np.write()


def breathing(r,g,b):
    for level in list(range(2,30,2))+list(range(30,2,-2)):
        fill((r*level//30,g*level//30,b*level//30))
        time.sleep_ms(40)


def transition(target,kind="FADE",duration=1200,brightness=100):
    # Keep transitions interruptible so the Pico can always receive a command.
    show_pixels(target,brightness)
    end=time.ticks_add(time.ticks_ms(),max(20,duration))
    while time.ticks_diff(end,time.ticks_ms())>0:
        time.sleep_ms(25)
        cmd=check_command()
        if cmd:
            handle_command(cmd)
            return True
    return False


def run_feed(payload):
    frames=payload.get("frames")
    if not isinstance(frames,list) or not frames: return
    loop=bool(payload.get("loop",True))
    while True:
        for frame in frames:
            if not isinstance(frame,dict): continue
            pixels=normalize_pixels(frame.get("pixels"))
            if pixels is None: continue
            if transition(pixels,str(frame.get("transition","FADE")),int(frame.get("duration",1200)),int(frame.get("brightness",100))): return
            hold=max(20,min(10000,int(frame.get("hold",500))))
            end=time.ticks_add(time.ticks_ms(),hold)
            while time.ticks_diff(end,time.ticks_ms())>0:
                time.sleep_ms(25)
                cmd=check_command()
                if cmd:
                    handle_command(cmd); return
        if not loop: return


def handle_command(command):
    global host_active
    if not command: return
    upper=command.upper()
    if upper=="HOST_ON":
        host_active=True
        # Do not blank the matrix on host connection. Keep the last known
        # temperature visible until the PC sends a mood/effect.
        if last_temp is not None:
            show_temperature(last_temp)
        else:
            automatic_temperature_update(force=True)
        print("HOST_ON"); print("READY"); return
    if upper=="HOST_OFF":
        host_active=False
        active_effect=None
        automatic_temperature_update(force=True)
        print("HOST_OFF"); print("READY"); return
    if upper=="SENSOR":
        print(json.dumps(read_sensor(),separators=(",",":")))
        print("READY"); return
    if upper=="STOP_EFFECT":
        stop_live_effect(); print("READY"); return
    if command.startswith("{"):
        try:
            payload=json.loads(command)
            kind=payload.get("type")
            if kind=="effect": start_live_effect(payload); return
            if kind=="mood":
                active_effect=None
                run_custom_mood(payload); print("READY"); return
            if kind=="feed":
                active_effect=None
                run_feed(payload); print("READY"); return
            print("ERROR: unknown JSON command")
        except Exception as e:
            print("ERROR:",e)
        return
    active_effect=None
    show_mood(command)
    print("READY")


# Give the DHT11 a moment after power-up. The first measurement can otherwise
# fail while the sensor settles, leaving a freshly powered table apparently dark.
time.sleep_ms(1200)
automatic_temperature_update(force=True)
print("AI Mood Matrix online")
print("Stand-alone DHT11 temperature mode ready")
print("Pico internal temperature sensor available")
print("4x4 / 4x8 mode supported")
print("Live effects use non-blocking state engine")
print("READY")

while True:
    if not host_active and active_effect is None and time.ticks_diff(time.ticks_ms(),last_auto_temp_ms)>=AUTO_TEMP_INTERVAL_MS:
        automatic_temperature_update()
    command=check_command()
    if command:
        handle_command(command)
    elif active_effect is not None:
        step_live_effect()
    else:
        time.sleep_ms(30)

from machine import Pin, ADC, I2C
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
AUTO_TEMP_BRIGHTNESS = 30

# 16x2 LCD with the common PCF8574T I2C backpack.
# The LCD remains independent of the LED mood/effect engine.
LCD_SDA_PIN = 4
LCD_SCL_PIN = 5
LCD_I2C_ADDRESS = 0x27
LCD_UPDATE_INTERVAL_MS = 10000

MAP_4X4 = [0,1,2,3,7,6,5,4,8,9,10,11,15,14,13,12]
# Confirmed 4x8 hardware mapping: two horizontally chained 4x4 serpentine panels.
MAP_4X8 = [
    0,1,2,3,16,17,18,19,
    7,6,5,4,23,22,21,20,
    8,9,10,11,24,25,26,27,
    15,14,13,12,31,30,29,28
]

np = neopixel.NeoPixel(Pin(LED_PIN), MAX_LEDS)
sensor = dht.DHT11(Pin(DHT_PIN))
pico_adc = ADC(4)
poll = uselect.poll()
poll.register(sys.stdin, uselect.POLLIN)

# -----------------------------------------------------------------------------
# 16x2 I2C LCD driver (PCF8574T, address 0x27)
# -----------------------------------------------------------------------------
class I2CLcd:
    def __init__(self, i2c, address=0x27):
        self.i2c = i2c
        self.address = address
        self.backlight = 0x08
        self._init_lcd()

    def _write(self, value):
        self.i2c.writeto(self.address, bytes([value | self.backlight]))

    def _pulse(self, value):
        self._write(value | 0x04)
        time.sleep_us(1)
        self._write(value & ~0x04)
        time.sleep_us(50)

    def _send_nibble(self, nibble, rs=0):
        value = (nibble & 0xF0) | rs
        self._pulse(value)

    def _send(self, value, rs=0):
        self._send_nibble(value & 0xF0, rs)
        self._send_nibble((value << 4) & 0xF0, rs)

    def command(self, value):
        self._send(value, 0)
        if value in (0x01, 0x02):
            time.sleep_ms(2)

    def putchar(self, value):
        self._send(ord(value), 1)

    def puts(self, text, row=0):
        # Avoid str.ljust(); some MicroPython builds do not implement it.
        text = str(text)[:16]
        if len(text) < 16:
            text += " " * (16 - len(text))
        self.command(0x80 if row == 0 else 0xC0)
        for char in text:
            self.putchar(char)

    def clear(self):
        self.command(0x01)

    def _init_lcd(self):
        time.sleep_ms(50)
        self._write(0x00)
        # Standard HD44780 4-bit initialization sequence.
        self._send_nibble(0x30); time.sleep_ms(5)
        self._send_nibble(0x30); time.sleep_us(150)
        self._send_nibble(0x30); time.sleep_us(150)
        self._send_nibble(0x20); time.sleep_us(150)
        self.command(0x28)  # 4-bit, 2-line, 5x8 font
        self.command(0x08)  # display off
        self.command(0x01)  # clear
        self.command(0x06)  # entry mode
        self.command(0x0C)  # display on, cursor off
        self.clear()

lcd = None
try:
    lcd_i2c = I2C(0, scl=Pin(LCD_SCL_PIN), sda=Pin(LCD_SDA_PIN), freq=100000)
    lcd_devices = lcd_i2c.scan()
    if LCD_I2C_ADDRESS in lcd_devices:
        lcd = I2CLcd(lcd_i2c, LCD_I2C_ADDRESS)
        lcd.puts("SQUARE TABLE", 0)
        lcd.puts("LCD ONLINE", 1)
        print("LCD ONLINE 0x27 GP4/GP5")
    else:
        print("LCD NOT FOUND", lcd_devices)
except Exception as e:
    print("LCD INIT ERROR", e)

host_active = False
last_auto_temp_ms = time.ticks_ms()
last_lcd_update_ms = time.ticks_ms()
last_temp = None
last_humidity = None
active_effect = None
rain_state = None
cloud_state = None


def clear():
    for i in range(MAX_LEDS): np[i] = (0,0,0)
    np.write()


def clamp(v, lo=0, hi=255): return max(lo,min(hi,int(v)))


def rgb(value, default=(255,255,255)):
    if not isinstance(value,(list,tuple)) or len(value)!=3: return default
    return (clamp(value[0]),clamp(value[1]),clamp(value[2]))


def scale(c, brightness):
    f=max(0,min(100,int(brightness)))/100
    return (int(c[0]*f),int(c[1]*f),int(c[2]*f))


def mapping_for(count): return MAP_4X8 if count==32 else MAP_4X4


def show_pixels(pixels, brightness=100):
    count=len(pixels)
    if count not in (16,32): return
    mapping=mapping_for(count)
    for i in range(MAX_LEDS): np[i]=(0,0,0)
    for logical in range(count): np[mapping[logical]]=scale(pixels[logical],brightness)
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
    if temp<=stops[0][0]: return stops[0][1]
    if temp>=stops[-1][0]: return stops[-1][1]
    for (t1,c1),(t2,c2) in zip(stops,stops[1:]):
        if t1<=temp<=t2:
            f=(temp-t1)/(t2-t1)
            return tuple(int(c1[i]+(c2[i]-c1[i])*f) for i in range(3))
    return (255,255,255)


def read_dht(retries=2):
    last_error=None
    for attempt in range(retries+1):
        try:
            sensor.measure(); return sensor.temperature(),sensor.humidity(),None
        except Exception as e:
            last_error=str(e)
            if attempt<retries: time.sleep_ms(250)
    return None,None,last_error


def read_pico_temperature():
    reading=pico_adc.read_u16(); voltage=reading*3.3/65535
    return 27-(voltage-0.706)/0.001721


def show_temperature(temp, brightness=AUTO_TEMP_BRIGHTNESS, count=MAX_LEDS): show_pixels([temperature_color(temp)]*count,brightness)


def update_lcd(temp, humidity, error=None):
    if lcd is None: return
    try:
        if temp is not None and humidity is not None:
            lcd.puts("TEMP:%5.1f C" % temp, 0)
            lcd.puts("HUM :%5.1f %%" % humidity, 1)
        elif error:
            lcd.puts("DHT11 ERROR", 0)
            lcd.puts("Retrying...", 1)
    except Exception as e:
        print("LCD UPDATE ERROR", e)


def update_sensor_state(update_led=False, force=False):
    global last_auto_temp_ms,last_lcd_update_ms,last_temp,last_humidity
    temp,humidity,error=read_dht(2)
    last_auto_temp_ms=time.ticks_ms()
    last_lcd_update_ms=time.ticks_ms()
    if temp is not None:
        last_temp=temp
        last_humidity=humidity
        update_lcd(temp,humidity)
        if update_led:
            show_temperature(temp)
        print("AUTO_TEMP %.1f %.1f"%(temp,humidity))
    else:
        update_lcd(None,None,error)
        if force and update_led:
            show_pixels([(0,35,120)]*MAX_LEDS,55)
            print("AUTO_TEMP_ERROR",error)


def automatic_temperature_update(force=False):
    # Sensor/LCD updates continue even when the web host is connected.
    # The LED temperature mood only runs while the host is idle and no live
    # effect is active, preserving whatever the web UI is currently showing.
    update_sensor_state(update_led=(not host_active and active_effect is None), force=force)


def stop_live_effect():
    global active_effect,rain_state,cloud_state
    active_effect=None; rain_state=None; cloud_state=None; clear(); print("EFFECT_STOPPED")


def start_live_effect(payload):
    global active_effect,rain_state,cloud_state
    effect=str(payload.get("effect","")).upper()
    if effect not in ("CODE_RAIN","PASTEL_CLOUDS"):
        print("ERROR: unknown live effect",effect); return
    active_effect=payload.copy(); rain_state=None; cloud_state=None; print("EFFECT_STARTED",effect)


def effect_interval(payload): return max(20,min(1000,int(payload.get("speed",80))))


def live_rain_step(payload):
    global rain_state
    count=32 if int(payload.get("width",4))==8 else 16; width=8 if count==32 else 4
    base=rgb(payload.get("color"),(0,255,80)); accent=rgb(payload.get("accent"),(255,255,255)); brightness=max(1,min(100,int(payload.get("brightness",70))))
    if rain_state is None or rain_state.get("count")!=count:
        rain_state={"count":count,"frame":0,"seeds":[random.randint(0,9999) for _ in range(width)],"lengths":[random.randint(2,5) for _ in range(width)]}
    state=rain_state; frame_no=state["frame"]; pixels=[(0,0,0)]*count
    for c in range(width):
        phase=(frame_no*(1+(c%3)*0.17)+state["seeds"][c])%12; head=phase-2; trail=state["lengths"][c]
        for r in range(4):
            d=head-r
            if 0<=d<=trail: pixels[r*width+c]=accent if d<0.8 else scale(base,max(18,100-int(d*24)))
        if random.random()<0.08: pixels[random.randrange(4)*width+c]=scale(accent,random.randint(35,80))
        if random.random()<0.025: state["seeds"][c]=random.randint(0,9999); state["lengths"][c]=random.randint(2,5)
    show_pixels(pixels,brightness); state["frame"]=(frame_no+1)%100000


def blend(a,b,f): return tuple(int(a[i]+(b[i]-a[i])*f) for i in range(3))


def palette_color(pos,colors):
    n=len(colors); p=(pos%n+n)%n; i=int(p); return blend(colors[i],colors[(i+1)%n],p-i)


def live_clouds_step(payload):
    global cloud_state
    count=32 if int(payload.get("width",4))==8 else 16; width=8 if count==32 else 4
    colors=[rgb(c) for c in payload.get("colors",[(158,220,255),(216,180,254),(255,214,231),(255,242,178)])]
    if len(colors)<2: colors=[colors[0],colors[0]]
    brightness=max(1,min(100,int(payload.get("brightness",70))))
    if cloud_state is None or cloud_state.get("count")!=count: cloud_state={"count":count,"frame":0,"phase":[random.random()*10 for _ in range(3)]}
    state=cloud_state; frame=state["frame"]; p0,p1,p2=state["phase"]; pixels=[]
    for r in range(4):
        for c in range(width):
            wave=(math.sin(frame*0.045+c*0.75+r*1.05+p0)*0.8+math.sin(frame*0.021-c*0.42+r*0.55+p1)*0.55+math.sin(frame*0.013+r+c*0.18+p2)*0.3)
            pixels.append(palette_color((wave+1.65)*len(colors)/3.3,colors))
    show_pixels(pixels,brightness); state["frame"]=(frame+1)%100000


def step_live_effect():
    if not active_effect: return
    effect=str(active_effect.get("effect","")).upper()
    if effect=="CODE_RAIN": live_rain_step(active_effect)
    elif effect=="PASTEL_CLOUDS": live_clouds_step(active_effect)
    time.sleep_ms(effect_interval(active_effect))


def read_sensor():
    temp,humidity,error=read_dht(2); result={"type":"sensor","pico_temperature":round(read_pico_temperature(),1)}
    if temp is None: result["error"]=error
    else: result["temperature"]=temp; result["humidity"]=humidity
    return result


def fill(c):
    for i in range(MAX_LEDS): np[i]=c
    np.write()


def breathing(r,g,b):
    for level in list(range(2,30,2))+list(range(30,2,-2)):
        fill((r*level//30,g*level//30,b*level//30)); time.sleep_ms(40)


def run_custom_mood(payload):
    pixels=normalize_pixels(payload.get("pixels"))
    if pixels is None: print("ERROR: mood needs 16 or 32 pixels"); return
    brightness=max(1,min(100,int(payload.get("brightness",100)))); effect=str(payload.get("effect","STATIC")).upper(); speed=max(20,min(2000,int(payload.get("speed",100))))
    if effect=="STATIC": show_pixels(pixels,brightness); return
    if effect=="WIPE":
        for n in range(1,len(pixels)+1):
            frame=list(pixels)
            for i in range(n,len(frame)): frame[i]=(0,0,0)
            show_pixels(frame,brightness); time.sleep_ms(speed); cmd=check_command()
            if cmd: handle_command(cmd); return
        return
    if effect=="SCROLL":
        offset=0
        while True:
            show_pixels([pixels[(i-offset)%len(pixels)] for i in range(len(pixels))],brightness); offset=(offset+1)%len(pixels); time.sleep_ms(speed); cmd=check_command()
            if cmd: handle_command(cmd); return
    if effect=="PULSE":
        while True:
            for level in list(range(10,101,5))+list(range(100,9,-5)):
                show_pixels(pixels,int(brightness*level/100)); time.sleep_ms(max(10,speed//5)); cmd=check_command()
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
            if i>1: np[i-2]=(0,0,3)
            np.write(); time.sleep_ms(80)
    elif mood=="SAD": fill((0,0,20))
    elif mood=="ANGRY": fill((35,0,0))
    elif mood=="LOVE": fill((30,0,12))
    elif mood=="CALM": breathing(0,0,25)
    elif mood=="EXCITED": breathing(30,10,0)
    elif mood=="RAINBOW":
        colors=[(30,0,0),(30,15,0),(30,30,0),(0,30,0),(0,0,30),(10,0,30),(30,0,30)]
        for shift in range(14):
            for i in range(16): np[i]=colors[(i+shift)%len(colors)]
            np.write(); time.sleep_ms(60)
    else:
        clear()


def handle_command(line):
    global host_active
    try:
        payload=json.loads(line)
        if payload.get("type")=="effect_stop": stop_live_effect(); host_active=False; return
        if payload.get("type")=="effect": host_active=True; start_live_effect(payload); return
        if payload.get("type")=="mood": host_active=True; run_custom_mood(payload); return
        if payload.get("type")=="sensor": print(json.dumps(read_sensor())); return
        if payload.get("type")=="stop": host_active=False; stop_live_effect(); return
        if payload.get("type")=="temp":
            host_active=True
            show_temperature(float(payload.get("temperature",25)), int(payload.get("brightness",AUTO_TEMP_BRIGHTNESS)), int(payload.get("leds",MAX_LEDS)))
            return
        if payload.get("type")=="auto_temp":
            host_active=False; automatic_temperature_update(force=True); return
        print("ERROR: unknown command")
    except Exception as e:
        print("ERROR",e)


print("AI Mood Matrix online")
print("Stand-alone DHT11 temperature mode ready")
print("Pico internal temperature sensor available")
print("I2C LCD 16x2 support enabled")
print("4x4 / 4x8 mode supported")
print("Live effects use non-blocking state engine")
print("READY")

while True:
    cmd=check_command()
    if cmd:
        handle_command(cmd)
    elif time.ticks_diff(time.ticks_ms(),last_auto_temp_ms)>=AUTO_TEMP_INTERVAL_MS:
        automatic_temperature_update()
    if active_effect:
        step_live_effect()
    else:
        time.sleep_ms(20)

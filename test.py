import asyncio
import time
import json
import math
import asyncio
from PyQt6.QtWidgets import QApplication
from qasync import QEventLoop

from bleak import BleakClient

from scope import Scope

ADDRESS1 = "F6:EE:A5:C8:0F:A0" # MCW-9886
ADDRESS2 = "E7:67:93:87:98:86" # MCW-0FA0

SERVICE_NOTIFY = "57420003-587E-48A0-974C-544D6163C577"
SERVICE_WRITE  = "57420002-587E-48A0-974C-544D6163C577"

BATTERY_UUID = "2A19"



recording = False
display = False
samples = []
gesture_id = 0
gx_sum = 0
gy_sum = 0
gz_sum = 0


ax_filter = 0
ay_filter = 0
az_filter = 0

finger = 0
last = time.time()

ANGLE_STEP = 1
DIR_COUNT = 360 // ANGLE_STEP

MAX_DIR_ERROR = 20      # Grad
MAX_LEN_ERROR = 200     # von 1000

spells = {}

def load_spells():
    global spells
    spells = {}
    for spell in ["bombarda", "calvorio", "colloportus", "confringo", "confundo", "densaugeo", 
                  "entomorphis", "expelliarmus", "expulso", "finestra", "finite", "flagrate", 
                  "immobulus", "impedimenta", "incendio", "lumos", "piertotum_locomotor", 
                  "protego", "reducto", "stupefy", "the_sleeping_charm", "the_streching_jinx", "ventus"]:
        with open(f"gestures/{spell}.json") as f:
            json_data = json.load(f)
            spells[json_data["spell"]] = json_data
            for seg in spells[json_data["spell"]]["segments"]:
                seg["dir"] = round(seg["dir"] / ANGLE_STEP) % DIR_COUNT
            # check that the sum of lengths is 1000
            total_length = sum(seg["l"] for seg in spells[json_data["spell"]]["segments"])
            if total_length != 1000:
                print(f"Warning: spell {json_data['spell']} has total length {total_length} != 1000")
    return spells

def create_segments(points):
    segments = []

    for i in range(1, len(points)):
        x0, y0 = points[i - 1]
        x1, y1 = points[i]

        dx = x1 - x0
        dy = y1 - y0

        length = math.hypot(dx, dy)

        if length < 1e-6:
            continue

        segments.append({
            "dir": round(math.degrees(math.atan2(dx, dy)) / ANGLE_STEP) % DIR_COUNT,
            "l": length,
        })

    return segments


# ------------------------------------------------------------
# Auf Integer normieren
# dx/dy -> -1000...1000
# len   -> Summe = 1000
# ------------------------------------------------------------

def normalize_segments(segments):

    total_length = sum(s["l"] for s in segments)

    for s in segments:
        s["l"] = round(s["l"] / total_length * 1000)

    return segments

def angle_diff(a1, a2):
    """Kleinste Winkeldifferenz in Grad."""
    d = abs(a1 - a2)
    return min(d, 360 - d)


def match_spell(candidate, spell):

    reference = spell["segments"]

    if len(candidate) != len(reference):
        print(f"Candidate length {len(candidate)} != reference length {len(reference)}")
        print(f"reference: {spell['spell']}")
        return None

    score = 0

    for i, (c, r) in enumerate(zip(candidate, reference)):

        dir_error = angle_diff(c["dir"], r["dir"])
        len_error = abs(c["l"] - r["l"])

        print(
            f"Seg {i}: "
            f"dir {c['dir']:3d}°/{r['dir']:3d}° "
            f"err={dir_error:2d}°  "
            f"len {c['l']:3d}/{r['l']:3d} "
            f"err={len_error:3d}"
        )

        if dir_error > MAX_DIR_ERROR:
            print(f"Seg {i}: dir_error={dir_error} > MAX_DIR_ERROR={MAX_DIR_ERROR}")
            return None

        if len_error > MAX_LEN_ERROR:
            print(f"Seg {i}: len_error={len_error} > MAX_LEN_ERROR={MAX_LEN_ERROR}")
            return None

        # kleiner Fehler => hoher Score
        score += (MAX_DIR_ERROR - dir_error) * 100
        score += (MAX_LEN_ERROR - len_error)

    return score

# ------------------------------------------------------------
# Besten Spell suchen
# ------------------------------------------------------------

def recognize(points, spells):

    segments = create_segments(points)
    segments = normalize_segments(segments)

    print("Segments:")

    for s in segments:
        print(
            f"dir={s['dir']:3d} "
            f"len={s['l']:4d}"
        )

    best_spell = None
    best_score = -10**20

    for spell in spells.values():

        score = match_spell(segments, spell)

        if score is None:
            continue

        print(f"{spell['spell']:15s} score={score}")

        if score > best_score:
            best_score = score
            best_spell = spell["spell"]

    return best_spell, best_score

def save_recording():
    global gesture_id


    filename = f"sipmple_{gesture_id:04d}.json"

    with open(filename, "w") as f:
        json.dump({
            "timestamp": time.time(),
            "sample_count": len(scope.simplified),
            "samples": scope.simplified,
        }, f)

    print(f"\nGespeichert: {filename} ({len(scope.simplified)} Samples)")
    print("\ncalculating segments...")
    # segments = create_segments(scope.simplified)
    # segments = normalize_segments(segments)
    recognized_spell, score = recognize(scope.simplified, spells)
    print(f"\nRecognized spell: {recognized_spell} (score={score})\n")

    gesture_id += 1

def notification_handler(sender, data):
    global finger
    global display
    global samples
    global last
    global gx_sum, gy_sum, gz_sum
    global ax_filter, ay_filter, az_filter
    global entrycounter


    now = time.time()
    dt = now - last
    last = now

    if data[0] == 0x10:
        old_finger = finger
        finger = data[1]

        # Aufnahme starten
        if not display and finger == 0x0f:
            print("\n=== RECORD START ===")
            display = True
            samples = []
            scope.clear_xy()
            gx_sum = 0
            gy_sum = 0
            gz_sum = 0
            scope.add_sample(0, 0, 0)
            scope.add_sample(0, 0, 0)
            scope.add_sample(0, 0, 0)
            scope.add_sample(0, 0, 0)
            scope.add_sample(0, 0, 0)
            scope.add_sample(0, 0, 0)
            entrycounter = 0

        # Aufnahme stoppen
        elif display and finger == 0x00:
            # stop recording and print the number of samples
            print("\n=== RECORD STOP with ", entrycounter, "samples ===")
            display = False
            save_recording()
            
            
        return

    if data[0] != 0x2C:
        return

    sample_count = data[3]

    for i in range(sample_count):
        offset = 4 + i * 12

        gx = int.from_bytes(data[offset+0:offset+2], "little", signed=True) + 20
        gy = int.from_bytes(data[offset+2:offset+4], "little", signed=True) + 130
        gz = int.from_bytes(data[offset+4:offset+6], "little", signed=True) - 3

        if True:
        #if gx > 200 or gx < -200:
            gx_sum += gx + 43
        #if gy > 200 or gy < -200:
            gy_sum += gy - 130
        #if gz > 200 or gz < -200:
            gz_sum += gz - 7
        
        gx_sum = gx_sum #- (gx_sum * 0.001)
        gy_sum = gy_sum #- (gy_sum * 0.001)
        gz_sum = gz_sum #- (gz_sum * 0.001)

        if display:
            scope.add_sample(
                gx,
                gx_sum / 1000.0, # gy,
                gz
            )
            entrycounter = entrycounter + 1

        ax = int.from_bytes(data[offset+6:offset+8], "little", signed=True)
        ay = int.from_bytes(data[offset+8:offset+10], "little", signed=True)
        az = int.from_bytes(data[offset+10:offset+12], "little", signed=True)

        ax_filter = ax_filter * 0.99 + ax * 0.01
        ay_filter = ay_filter * 0.99 + ay * 0.01
        az_filter = az_filter * 0.99 + az * 0.01
        # AX=-2000/AZ=0 is the reference direction; AZ positive rotates CCW.
        projection_angle = math.atan2(az_filter, -ax_filter)
        gx_sum_rot = gx_sum * math.cos(projection_angle) - gz_sum * math.sin(projection_angle)
        gz_sum_rot = gx_sum * math.sin(projection_angle) + gz_sum * math.cos(projection_angle)
        if display:
            scope.add_projection_point(
                gx_sum_rot / 100000.0,
                gz_sum_rot / 100000.0,
            )


        if recording:
            samples.append({
                "t": now,
                "gx": gx,
                "gy": gy,
                "gz": gz,
                "ax": ax,
                "ay": ay,
                "az": az,
            })

        print(
            f"\rREC={recording} "
            f"F={finger:01x} "
            f"S={len(samples):04d} "
            f"AX={ax:6d} AY={ay:6d} AZ={az:6d} "
            f"GX={gx:6d} GY={gy:6d} GZ={gz:6d}",
            end="",
            flush=True
        )

async def main():
    
    commands = [
    bytes.fromhex("300080"),
    bytes.fromhex("1001"),
    bytes.fromhex("60"),
]

    

    async with BleakClient(ADDRESS1) as client:
        print("Connected:", client.is_connected)

        await client.start_notify(
            SERVICE_NOTIFY,
            notification_handler
        )
        for cmd in commands:
            print("SEND", cmd.hex())

            await client.write_gatt_char(
                SERVICE_WRITE,
                cmd
            )

            await asyncio.sleep(0.2)

        # for cmd in [
        #     bytes([0x00]),
        #     bytes([0x01]),
        #     bytes([0x04]),
        #     bytes([0x08]),
        #     bytes([0x09]),
        #     bytes([0x6b]),
        #     bytes([0x6c]),
        #     bytes([0xba]),
        #     bytes([0xc8]), # 0xc8 vibrate
        #     bytes([0xdb]),

        # ]:
        #     print("Sending:", cmd.hex())

        #     try:
        #         await client.write_gatt_char(SERVICE_WRITE, cmd)
        #         await asyncio.sleep(1)
        #     except Exception as e:
        #         print(e)

        # send 0x08 100 times with 100ms delay
        # for _ in range(10):
        #     try:
        #         await client.write_gatt_char(SERVICE_WRITE, bytes([0xdb]))
        #         await asyncio.sleep(0.1)
        #     except Exception as e:
        #         print(e)
    
        print("Waiting for notifications...")
        await asyncio.sleep(1000)

app = QApplication([])

scope = Scope()
entrycounter = 0

loop = QEventLoop(app)
asyncio.set_event_loop(loop)

load_spells()

with loop:
    loop.run_until_complete(main())

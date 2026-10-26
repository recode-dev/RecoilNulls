import math
import time

# --- Constants ---
SAFETY_MARGIN = 28.0
T_URGENT      = 0.9
T_FIELD       = 1.8
FIELD_RADIUS  = 275.0
N_DIRECTIONS  = 64
PERP_WEIGHT   = 2.4
AWAY_WEIGHT   = 1.0
INTENT_WEIGHT = 1.2
CHAR_SPEED    = 720.0

class V2:
    def __init__(self, x=0.0, y=0.0):
        self.x = float(x)
        self.y = float(y)
    def __sub__(self, other): return V2(self.x - other.x, self.y - other.y)
    def __add__(self, other): return V2(self.x + other.x, self.y + other.y)
    def __mul__(self, s): return V2(self.x * s, self.y * s)
    def __repr__(self): return f"({self.x:.2f}, {self.y:.2f})"

def normalize(x, y):
    l = math.sqrt(x*x + y*y)
    if l < 1e-6: return V2(1.0, 0.0)
    return V2(x/l, y/l)

def get_direction_samples(n):
    out = []
    for i in range(n):
        angle = (2.0 * math.pi * i) / n
        out.append(V2(math.cos(angle), math.sin(angle)))
    return out

SAMPLES = get_direction_samples(N_DIRECTIONS)

class Projectile:
    def __init__(self, id, x, y, dirX, dirY, speed, radius):
        self.id = id
        self.x = x
        self.y = y
        self.dirX = dirX
        self.dirY = dirY
        self.speed = speed
        self.radius = radius
        self.lastSeen = time.time()

# Global state
g_proj_cache = {}
g_lastSafeDir = V2(0, 0)
g_lastSafeDirTime = 0

def is_urgent_threat(p, myX, myY, myRadius):
    dx = myX - p.x
    dy = myY - p.y
    vx = p.dirX * p.speed
    vy = p.dirY * p.speed

    c1 = dx*vx + dy*vy
    if c1 <= 0: return False
    c2 = vx*vx + vy*vy
    if c2 <= 0: return False
    tHit = c1 / c2
    if tHit > T_URGENT: return False

    closestX = p.x + vx * tHit
    closestY = p.y + vy * tHit

    r = myRadius + p.radius + SAFETY_MARGIN
    ddx = myX - closestX
    ddy = myY - closestY
    return (ddx*ddx + ddy*ddy) <= r*r

def threat_score(dir, myX, myY, myRadius, intentDir, lastSafeDir, lastSafeDirAgeMs):
    score = 0
    for p in g_proj_cache.values():
        r = myRadius + p.radius + SAFETY_MARGIN
        vx = p.dirX * p.speed - dir.x * CHAR_SPEED
        vy = p.dirY * p.speed - dir.y * CHAR_SPEED
        dx = p.x - myX
        dy = p.y - myY

        a = vx*vx + vy*vy
        b = 2 * (dx*vx + dy*vy)
        c = dx*dx + dy*dy

        minD2 = c
        if a > 1e-6:
            tMin = -b / (2*a)
            if 0 < tMin <= T_FIELD:
                minD2 = c + b*tMin + a*tMin*tMin
            elif tMin > T_FIELD:
                minD2 = c + b*T_FIELD + a*T_FIELD*T_FIELD
        
        danger = 2500.0 if minD2 < r*r else (r*r) / max(minD2, 50.0)
        score += danger

    if intentDir.x != 0 or intentDir.y != 0:
        dotIntent = dir.x*intentDir.x + dir.y*intentDir.y
        score -= dotIntent * INTENT_WEIGHT * 15.0
    
    if (lastSafeDir.x != 0 or lastSafeDir.y != 0) and lastSafeDirAgeMs < 400:
        dotInertia = dir.x*lastSafeDir.x + dir.y*lastSafeDir.y
        score -= dotInertia * 45.0
    
    return score

def is_velocity_unsafe(dir, myX, myY, myRadius):
    for p in g_proj_cache.values():
        r = myRadius + p.radius + SAFETY_MARGIN
        vx = p.dirX * p.speed - dir.x * CHAR_SPEED
        vy = p.dirY * p.speed - dir.y * CHAR_SPEED
        dx = p.x - myX
        dy = p.y - myY

        a = vx*vx + vy*vy
        b = 2 * (dx*vx + dy*vy)
        c = dx*dx + dy*dy - r*r

        if c < 0: return True
        if a > 1e-6:
            disc = b*b - 4*a*c
            if disc >= 0:
                t1 = (-b - math.sqrt(disc)) / (2*a)
                if 0 < t1 <= T_FIELD: return True
    return False

def get_urgent_dodge_dir(myX, myY, myRadius, intentDir, now_ms):
    candidates = []
    urgentCount = 0
    for p in g_proj_cache.values():
        if not is_urgent_threat(p, myX, myY, myRadius): continue
        urgentCount += 1
        away = normalize(myX - p.x, myY - p.y)
        perp1 = normalize(-p.dirY, p.dirX)
        perp2 = normalize(p.dirY, -p.dirX)
        intentW = 0.6
        candidates.append(normalize(perp1.x*PERP_WEIGHT + away.x*AWAY_WEIGHT + intentDir.x*intentW,
                                    perp1.y*PERP_WEIGHT + away.y*AWAY_WEIGHT + intentDir.y*intentW))
        candidates.append(normalize(perp2.x*PERP_WEIGHT + away.x*AWAY_WEIGHT + intentDir.x*intentW,
                                    perp2.y*PERP_WEIGHT + away.y*AWAY_WEIGHT + intentDir.y*intentW))
    
    if urgentCount == 0: return None
    candidates.extend(get_direction_samples(16))
    
    best = candidates[0]
    bestScore = 1e18
    age = now_ms - g_lastSafeDirTime
    for c in candidates:
        s = threat_score(c, myX, myY, myRadius, intentDir, g_lastSafeDir, age)
        if s < bestScore:
            bestScore = s
            best = c
    return best

def choose_best_direction(myX, myY, myRadius, intentDir, now_ms):
    best = intentDir if (intentDir.x != 0 or intentDir.y != 0) else SAMPLES[0]
    bestScore = 1e18
    age = now_ms - g_lastSafeDirTime
    for d in SAMPLES:
        s = threat_score(d, myX, myY, myRadius, intentDir, g_lastSafeDir, age)
        if s < bestScore:
            bestScore = s
            best = d
    return best

def apply_VO(dir, myX, myY, myRadius, intentDir, now_ms):
    if not is_velocity_unsafe(dir, myX, myY, myRadius): return dir
    best = dir
    bestScore = 1e18
    foundSafe = False
    age = now_ms - g_lastSafeDirTime
    for d in SAMPLES:
        if is_velocity_unsafe(d, myX, myY, myRadius): continue
        foundSafe = True
        s = threat_score(d, myX, myY, myRadius, intentDir, g_lastSafeDir, age)
        if s < bestScore:
            bestScore = s
            best = d
    return best if foundSafe else dir

def check_danger(myX, myY, myRadius):
    for p in g_proj_cache.values():
        r = myRadius + p.radius + SAFETY_MARGIN * 2.5
        dx = myX - p.x
        dy = myY - p.y
        if dx*dx + dy*dy <= r*r: return True
        
        vx = p.dirX * p.speed
        vy = p.dirY * p.speed
        c1 = dx*vx + dy*vy
        if c1 > 0:
            c2 = vx*vx + vy*vy
            if c2 > 0:
                tHit = c1 / c2
                if tHit <= T_FIELD:
                    cx = p.x + vx * tHit
                    cy = p.y + vy * tHit
                    if (myX-cx)**2 + (myY-cy)**2 <= r*r: return True
    return False

def compute_dodge_direction(myX, myY, myRadius, intentDir, now_ms):
    global g_lastSafeDir, g_lastSafeDirTime
    if not check_danger(myX, myY, myRadius):
        return None
    
    urgentDir = get_urgent_dodge_dir(myX, myY, myRadius, intentDir, now_ms)
    baseDir = urgentDir if urgentDir else choose_best_direction(myX, myY, myRadius, intentDir, now_ms)
    
    safeDir = apply_VO(baseDir, myX, myY, myRadius, intentDir, now_ms)
    
    g_lastSafeDir = safeDir
    g_lastSafeDirTime = now_ms
    
    angle = math.atan2(safeDir.y, safeDir.x)
    # emit logic simplified here
    return angle * 180.0 / math.pi

def run_test(name, myX, myY, myRadius, intentDir, projs):
    global g_proj_cache, g_lastSafeDir, g_lastSafeDirTime
    g_proj_cache = {p.id: p for p in projs}
    now_ms = int(time.time() * 1000)
    print(f"Test: {name}")
    angle = compute_dodge_direction(myX, myY, myRadius, intentDir, now_ms)
    if angle is None:
        print("  Result: No Danger")
    else:
        print(f"  Dodge Angle: {angle:.1f} deg")
    print("-" * 32)

if __name__ == "__main__":
    myRadius = 120.0
    intentNone = V2(0, 0)
    
    # 1. Projectile from Right
    p1 = Projectile(1, 1000, 0, -1, 0, 1500, 100)
    run_test("Projectile from Right (Urgent)", 0, 0, myRadius, intentNone, [p1])
    
    # 2. Projectile from Right (Far)
    p2 = Projectile(2, 2000, 0, -1, 0, 1500, 100)
    run_test("Projectile from Right (Far - T_FIELD)", 0, 0, myRadius, intentNone, [p2])
    
    # 3. Projectile from Top
    p3 = Projectile(3, 0, 1000, 0, -1, 1500, 100)
    run_test("Projectile from Top", 0, 0, myRadius, intentNone, [p3])
    
    # 4. Two Projectiles (Surrounded)
    run_test("Surrounded (Right & Top)", 0, 0, myRadius, intentNone, [p1, p3])

    # 5. Intent vs Dodge
    intentUp = V2(0, 1)
    run_test("Intent Up vs Projectile from Top", 0, 0, myRadius, intentUp, [p3])

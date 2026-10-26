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

class Projectile:
    def __init__(self, x, y, dirX, dirY, speed, radius):
        self.x = x
        self.y = y
        self.dirX = dirX
        self.dirY = dirY
        self.speed = speed
        self.radius = radius

def normalize(x, y):
    l = math.sqrt(x*x + y*y)
    if l < 1e-6:
        return 1.0, 0.0
    return x/l, y/l

CACHED_DIRECTIONS = []
for i in range(N_DIRECTIONS):
    angle = (2.0 * math.pi * i) / N_DIRECTIONS
    CACHED_DIRECTIONS.append((math.cos(angle), math.sin(angle)))

def get_direction_samples(n):
    if n == N_DIRECTIONS:
        return CACHED_DIRECTIONS
    out = []
    for i in range(n):
        angle = (2.0 * math.pi * i) / n
        out.append((math.cos(angle), math.sin(angle)))
    return out

def is_urgent_threat(p, myX, myY, myRadius):
    dx = myX - p.x
    dy = myY - p.y
    vx = p.dirX * p.speed
    vy = p.dirY * p.speed

    c1 = dx * vx + dy * vy
    if c1 <= 0: return False
    c2 = vx * vx + vy * vy
    if c2 <= 0: return False
    tHit = c1 / c2
    if tHit > T_URGENT: return False

    closestX = p.x + vx * tHit
    closestY = p.y + vy * tHit
    r = myRadius + p.radius + SAFETY_MARGIN
    ddx = myX - closestX
    ddy = myY - closestY
    return (ddx*ddx + ddy*ddy) <= r*r

def threat_score(dir_v, myX, myY, myRadius, intentDir, projectiles, lastSafeDir=None, lastSafeDirAgeMs=None):
    score = 0
    for p in projectiles:
        r = myRadius + p.radius + SAFETY_MARGIN
        vx = p.dirX * p.speed - dir_v[0] * CHAR_SPEED
        vy = p.dirY * p.speed - dir_v[1] * CHAR_SPEED
        dx = p.x - myX
        dy = p.y - myY
        a = vx*vx + vy*vy
        b = 2.0 * (dx*vx + dy*vy)
        c = dx*dx + dy*dy

        minD2 = c
        if a > 1e-6:
            tMin = -b / (2.0 * a)
            if 0 < tMin <= T_FIELD:
                minD2 = c + b*tMin + a*tMin*tMin
            elif tMin > T_FIELD:
                minD2 = c + b*T_FIELD + a*T_FIELD*T_FIELD
        
        danger = 2500.0 if minD2 < r*r else (r*r) / max(minD2, 50.0)
        score += danger

    if intentDir and (intentDir[0] != 0 or intentDir[1] != 0):
        dot = dir_v[0]*intentDir[0] + dir_v[1]*intentDir[1]
        score -= dot * INTENT_WEIGHT * 15.0

    if lastSafeDir and lastSafeDirAgeMs is not None and lastSafeDirAgeMs < 400:
        dotInertia = dir_v[0]*lastSafeDir[0] + dir_v[1]*lastSafeDir[1]
        score -= dotInertia * 45.0
    
    return score

def is_velocity_unsafe(dir_v, myX, myY, myRadius, projectiles):
    for p in projectiles:
        r = myRadius + p.radius + SAFETY_MARGIN
        vx = p.dirX * p.speed - dir_v[0] * CHAR_SPEED
        vy = p.dirY * p.speed - dir_v[1] * CHAR_SPEED
        dx = p.x - myX
        dy = p.y - myY
        a = vx*vx + vy*vy
        b = 2.0 * (dx*vx + dy*vy)
        c = dx*dx + dy*dy - r*r
        if c < 0: return True
        if a > 1e-6:
            disc = b*b - 4*a*c
            if disc >= 0:
                t1 = (-b - math.sqrt(disc)) / (2.0 * a)
                if 0 < t1 <= T_FIELD: return True
    return False

def get_urgent_dodge_dir(myX, myY, myRadius, intentDir, projectiles, lastSafeDir, age):
    candidates = []
    urgentCount = 0
    for p in projectiles:
        if not is_urgent_threat(p, myX, myY, myRadius): continue
        urgentCount += 1
        away = normalize(myX - p.x, myY - p.y)
        perp1 = normalize(-p.dirY, p.dirX)
        perp2 = normalize(p.dirY, -p.dirX)
        
        intentX = intentDir[0] * 0.6 if intentDir else 0.0
        intentY = intentDir[1] * 0.6 if intentDir else 0.0

        candidates.append(normalize(perp1[0]*PERP_WEIGHT + away[0]*AWAY_WEIGHT + intentX,
                                    perp1[1]*PERP_WEIGHT + away[1]*AWAY_WEIGHT + intentY))
        candidates.append(normalize(perp2[0]*PERP_WEIGHT + away[0]*AWAY_WEIGHT + intentX,
                                    perp2[1]*PERP_WEIGHT + away[1]*AWAY_WEIGHT + intentY))
    
    if urgentCount == 0: return None
    
    candidates.extend(get_direction_samples(16))
    best = candidates[0]
    bestScore = 1e18
    for c in candidates:
        s = threat_score(c, myX, myY, myRadius, intentDir, projectiles, lastSafeDir, age)
        if s < bestScore:
            bestScore = s
            best = c
    return best

def choose_best_direction(myX, myY, myRadius, intentDir, projectiles, lastSafeDir, age):
    samples = CACHED_DIRECTIONS
    best = intentDir if (intentDir and (intentDir[0] != 0 or intentDir[1] != 0)) else samples[0]
    bestScore = 1e18
    for s_v in samples:
        s = threat_score(s_v, myX, myY, myRadius, intentDir, projectiles, lastSafeDir, age)
        if s < bestScore:
            bestScore = s
            best = s_v
    return best

def apply_vo(dir_v, myX, myY, myRadius, intentDir, projectiles, lastSafeDir, age):
    if not is_velocity_unsafe(dir_v, myX, myY, myRadius, projectiles):
        return dir_v
    samples = CACHED_DIRECTIONS
    best = dir_v
    bestScore = 1e18
    foundSafe = False
    for s_v in samples:
        if is_velocity_unsafe(s_v, myX, myY, myRadius, projectiles): continue
        foundSafe = True
        s = threat_score(s_v, myX, myY, myRadius, intentDir, projectiles, lastSafeDir, age)
        if s < bestScore:
            bestScore = s
            best = s_v
    return best if foundSafe else dir_v

# --- Simulation ---
def test():
    mx, my = 0.0, 0.0
    r = 45.0
    # Projectile coming from right to left
    p1 = Projectile(200.0, 0.0, -1.0, 0.0, 1500.0, 10.0)
    projs = [p1]
    intent = (0.0, 1.0) # Player wants to go Up
    
    # Logic chain
    age = 500 # > 400, no inertia
    urgent = get_urgent_dodge_dir(mx, my, r, intent, projs, None, age)
    if urgent:
        base = urgent
        print(f"Urgent Dodge: {urgent}")
    else:
        base = choose_best_direction(mx, my, r, intent, projs, None, age)
        print(f"APF Choice: {base}")
    
    final = apply_vo(base, mx, my, r, intent, projs, None, age)
    print(f"Final safe dir: {final}")
    
    angle = math.atan2(final[1], final[0])
    print(f"Angle (deg): {math.degrees(angle):.1f}")

if __name__ == "__main__":
    test()

#include "adcore.h"
#include "autododge.h"
#include "../core/scan.h"
#include "../core/scstring.h"
#define PROJ_DATA_BLOCK_SIZE 0x1E8
#include "../utils/walls.h"
#include <cmath>
#include <cstring>
namespace ad
{
Proj* find_proj(int32_t pid)
{
    Proj* spare = nullptr;
    Proj* stale = &projs[0];
    for (int i = 0; i < PROJ_SLOTS; i++)
    {
        if (projs[i].pid == pid) return &projs[i];
        if (projs[i].pid == 0)
        {
            if (!spare) spare = &projs[i];
            continue;
        }
        if (projs[i].seen < stale->seen) stale = &projs[i];
    }
    return spare ? spare : stale;
}

void init_proj(
    Proj* st, int32_t pid, uintptr_t dataPtr, float px, float py, uint64_t now)
{
    uint8_t data[PROJ_DATA_BLOCK_SIZE];
    int32_t speed = 0;
    int32_t radius = 0;
    if (!read_bytes(dataPtr, data, sizeof(data))) return;
    memcpy(&speed, data + PROJECTILEDATA_SPEED_OFF, sizeof(speed));
    memcpy(&radius, data + PROJECTILEDATA_RADIUS_OFF, sizeof(radius));
    if (speed <= 0) return;
    if (radius < 0) radius = 0;
    st->pid = pid;
    st->seen = now;
    st->x = px;
    st->y = py;
    st->speed = (float)speed;
    st->rad = (float)radius;
    st->min_dmin = 1e9f;
    st->vel = false;
    st->ux = 0.0f;
    st->uy = 0.0f;
    st->flags = 0;
    if (data[PROJECTILEDATA_ISBEAM_OFF]) st->flags |= B_BEAM;
    if (data[PROJECTILEDATA_INDIRECT_OFF]) st->flags |= B_IND;
    if (data[PROJECTILEDATA_PIERCES_ENV_OFF] ||
        data[PROJECTILEDATA_PIERCES_ENV_BUTTER_OFF])
        st->flags |= B_PIERCE;
    st->name[0] = '\0';
    uintptr_t str = 0;
    if (addr_dataname)
    {
        using name_fn = uintptr_t (*)(uintptr_t);
        str = reinterpret_cast<name_fn>(addr_dataname)(dataPtr);
    }
    if (str)
        read_scstring(
            str, SCSTRING_DEFAULT_MAX_LENGTH, st->name, sizeof(st->name));
    int near = -1;
    float best = 1e30f;
    for (int i = 0; i < ad_enemy_n; i++)
    {
        float ex = ad_enemy_x[i] - px, ey = ad_enemy_y[i] - py;
        float d2 = ex * ex + ey * ey;
        if (d2 < best)
        {
            best = d2;
            near = i;
        }
    }
    if (near >= 0)
    {
        float a = (float)ad_enemy_aim[near] * 0.0174532925f;
        st->ux = cosf(a);
        st->uy = sinf(a);
        st->vel = true;
    }
}

void collect_chars(int32_t own_gid, float mx, float my)
{
    for (int i = 0; i < obj_count; i++)
    {
        if (!obj_valid[i]) continue;
        const uint8_t* obj = obj_block[i];
        uintptr_t dataPtr = 0;
        memcpy(&dataPtr, obj + ELEM_DEF_OFF, sizeof(dataPtr));
        if (!dataPtr || !char_vt || !pointer_plausible(dataPtr)) continue;
        uintptr_t vt = 0;
        if (!read_ptr(dataPtr, &vt) || vt < base || vt - base != char_vt)
            continue;
        ad_char_n++;
        int32_t team, hp, gid, xi, yi, aim;
        memcpy(&team, obj + OBJ_TEAM_OFF, sizeof(team));
        memcpy(&hp, obj + OBJ_HP_OFF, sizeof(hp));
        memcpy(&gid, obj + OBJ_GLOBALID_OFF, sizeof(gid));
        memcpy(&xi, obj + OBJ_X_OFF, sizeof(xi));
        memcpy(&yi, obj + OBJ_Y_OFF, sizeof(yi));
        memcpy(&aim, obj + OBJ_ATTACK_ANGLE_OFF, sizeof(aim));
        if (team == own_team_b || hp <= 0 || gid <= 0 || gid == own_gid)
            continue;
        if (ad_enemy_n >= AD_ENEMY_MAX) continue;
        int k = ad_enemy_n++;
        float ex = (float)xi - mx, ey = (float)yi - my;
        ad_enemy_pid[k] = gid;
        ad_enemy_x[k] = (float)xi;
        ad_enemy_y[k] = (float)yi;
        ad_enemy_hp[k] = hp;
        ad_enemy_d[k] = sqrtf(ex * ex + ey * ey);
        ad_enemy_aim[k] = aim;
    }
}

void collect_projs(uint64_t now, float mx, float my)
{
    float zone = self_speed * 4.0f, zone2 = zone * zone;
    for (int i = 0; i < obj_count; i++)
    {
        if (!obj_valid[i]) continue;
        const uint8_t* obj = obj_block[i];
        uintptr_t dataPtr = 0;
        memcpy(&dataPtr, obj + ELEM_DEF_OFF, sizeof(dataPtr));
        if (!dataPtr || !pointer_plausible(dataPtr)) continue;
        uintptr_t vt = 0;
        if (!read_ptr(dataPtr, &vt) || vt < base) continue;
        if (vt - base != VTABLE_PROJ_DATA_RVA) continue;
        int32_t pid, team, xi, yi, dead;
        memcpy(&pid, obj + OBJ_GLOBALID_OFF, 4);
        memcpy(&team, obj + OBJ_TEAM_OFF, 4);
        memcpy(&xi, obj + OBJ_X_OFF, 4);
        memcpy(&yi, obj + OBJ_Y_OFF, 4);
        memcpy(&dead, obj + OBJ_DEADFLAG_OFF, 4);
        if (!pid || team == own_team_b || dead) continue;
        ad_proj_n++;
        float px = (float)xi, py = (float)yi;
        Proj* st = find_proj(pid);
        if (st->pid != pid)
        {
            init_proj(st, pid, dataPtr, px, py, now);
            if (st->pid != pid) continue;
        }
        st->seen = now;
        float step_x = px - st->x, step_y = py - st->y;
        float step = sqrtf(step_x * step_x + step_y * step_y);
        if (step > 1.0f)
        {
            st->ux = step_x / step;
            st->uy = step_y / step;
            st->vel = true;
        }
        st->x = px, st->y = py;
        float dx = px - mx, dy = py - my;
        if (!st->vel) continue;
        float vx = st->ux * st->speed, vy = st->uy * st->speed;
        if (vx * dx + vy * dy < 0.0f && !(st->flags & (B_BEAM | B_IND)))
        {
            float perp = fabsf(dx * vy - dy * vx) / st->speed;
            if (perp < st->min_dmin) st->min_dmin = perp;
        }
        if (dx * dx + dy * dy > zone2) continue;
        if (st->flags & B_BEAM)
        {
            ad_beam_n++;
            continue;
        }
        if (st->flags & B_IND) ad_ind_n++;
        if (!(st->flags & (B_IND | B_PIERCE)) &&
            !wall_los(px, py, mx, my, WALL_BLOCKS_PROJECTILES))
            continue;
        float dot = vx * dx + vy * dy;
        if (dot >= 0.0f) continue;
        float rad = st->rad + body_rad();
        float vv = vx * vx + vy * vy, tt = -dot / vv;
        float qx = dx + vx * tt, qy = dy + vy * tt,
              dmin = sqrtf(qx * qx + qy * qy);
        if (dmin > rad + st->speed * (float)TICK_MS * 0.001f) continue;
        if (threat_n >= THREAT_MAX) continue;
        float gap = fmaxf(rad * rad - dmin * dmin, 0.0f);
        Threat& t = threats[threat_n++];
        t.x = px, t.y = py, t.vx = vx, t.vy = vy, t.rad = rad, t.pid = pid;
        t.tt = tt - sqrtf(gap) / st->speed;
        memcpy(t.name, st->name, sizeof(t.name));
        t.name[sizeof(t.name) - 1] = '\0';
    }
}

void evict_stale(uint64_t now)
{
    for (int i = 0; i < PROJ_SLOTS; i++)
        if (projs[i].pid && now - projs[i].seen > PROJ_TTL_MS) projs[i].pid = 0;
}
}

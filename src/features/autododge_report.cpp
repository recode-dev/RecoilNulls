#include "../core/adcore.h"
#include "autododge.h"
#include <cmath>
#include <cstring>
namespace ad
{
int nearest_unused(
    float mx, float my, float dx, float dy, const int* used, float* out_clear)
{
    int best = -1;
    float bc = 1e30f;
    for (int i = 0; i < threat_n; i++)
    {
        bool skip = false;
        for (int k = 0; k < AD_REPORT_MAX; k++)
            if (used[k] == i) skip = true;
        if (skip) continue;
        float c = clear_one(mx, my, dx, dy, threats[i]);
        if (c < bc)
        {
            bc = c;
            best = i;
        }
    }
    *out_clear = bc;
    return best;
}
void hit_report(float mx, float my, float dx, float dy)
{
    ad_hit_dmg = pend_dmg;
    ad_hit_act = active;
    ad_hit_n = threat_n;
    ad_hit_x = (int)mx;
    ad_hit_y = (int)my;
    ad_hit_picked = 0;
    int used[AD_REPORT_MAX] = {-1, -1, -1};
    for (int s = 0; s < AD_REPORT_MAX; s++)
    {
        float bc = 0.0f;
        int best = nearest_unused(mx, my, dx, dy, used, &bc);
        if (best < 0) break;
        used[s] = best;
        ad_hit_ca[s] = bc;
        ad_hit_ttc[s] = threats[best].tt;
        ad_hit_r[s] = threats[best].rad;
        ad_hit_picked++;
    }
    pend_dmg = 0;
}
void fill_report(float mx, float my, float dx, float dy)
{
    ad_threat_n = threat_n < AD_REPORT_MAX ? threat_n : AD_REPORT_MAX;
    int used[AD_REPORT_MAX] = {-1, -1, -1};
    for (int s = 0; s < ad_threat_n; s++)
    {
        float bc = 0.0f;
        int best = nearest_unused(mx, my, dx, dy, used, &bc);
        if (best < 0)
        {
            ad_threat_n = s;
            break;
        }
        used[s] = best;
        const Threat& t = threats[best];
        ad_threat_pid[s] = t.pid;
        ad_threat_x[s] = t.x;
        ad_threat_y[s] = t.y;
        ad_threat_vx[s] = t.vx;
        ad_threat_vy[s] = t.vy;
        ad_threat_r[s] = t.rad;
        ad_threat_ca[s] = bc;
        ad_threat_ttc[s] = t.tt;
        ad_threat_d[s] =
            sqrtf((t.x - mx) * (t.x - mx) + (t.y - my) * (t.y - my));
        memcpy(ad_threat_name[s], t.name, sizeof(ad_threat_name[s]));
        ad_threat_name[s][sizeof(ad_threat_name[s]) - 1] = '\0';
    }
}
}

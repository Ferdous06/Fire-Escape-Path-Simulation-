   #ifdef _WIN32
#include <windows.h>
#endif
#include <GL/glut.h>
#include <cmath>
#include <cstdlib>
#include <cstdio>
#include <cstring>

#define H12 GLUT_BITMAP_HELVETICA_12
#define H18 GLUT_BITMAP_HELVETICA_18
#define H10 GLUT_BITMAP_HELVETICA_10

// ---------------- global state ----------------
bool  fireOn = false;               // agun lagse kina
float fx = 160, fy = 470, fr = 0;   // agun er kendro (fx,fy) ar radius fr
int   fireRoom = 4;                 // kon ghore agun lagbe (1-6)
int   frame = 0;                    // animation frame
int   tFire = 0;                    // agun lagar por koyta step gelo (1 step = 0.05 s)
float alarmAngle = 0;
int   alarmLevel = 0;               // 0=bondho, 1=agun dhora poreche, 2=manush bipode
int   alarmStart = -1;
int   scene = 1;                    // 1..7
bool  paused = false;
int   simSpeed = 1;
bool  soundOn = true;
bool  extinguishing = false, done = false;
int   tAllOut = -1, tOut = -1;
int   leftOut = 0, rightOut = 0;

// fire engine
int   truckMode = 0;                // 0=ashe ni, 1=ashche, 2=pouche gese
float truckX = 900, parkX = 190;
int   arriveStep = 0;

// smoke detector
const int ND = 8;
float detX[ND] = { 160, 400, 640, 160, 400, 640, 250, 560 };
float detY[ND] = { 250, 250, 250, 520, 520, 520, 320, 320 };
const float DET_R = 70;
bool  detOn[ND];

float roomFX[6] = { 150, 400, 640, 160, 400, 640 };
float roomFY[6] = { 210, 215, 210, 470, 480, 470 };

// ---------------- manush (people) ----------------
struct Person { float x, y, speed; int state; };  // 0=opekkha, 1=hatche, 2=beriye gese, 3=atke gese
const int N = 8;
Person people[N];
int escaped = 0, trapped = 0, danger = 0;

void initPeople()
{
    float px[N] = { 150,  90, 390, 630, 700, 230, 400, 620 };
    float py[N] = { 220, 160, 200, 200, 160, 440, 470, 460 };
    float sp[N] = { 2.4f, 2.4f, 2.4f, 2.4f, 2.4f, 2.4f, 1.2f, 2.4f };  // 7 number manush slow
    for (int i = 0; i < N; i++)
    {
        people[i].x = px[i]; people[i].y = py[i];
        people[i].speed = sp[i]; people[i].state = 0;
    }
    escaped = 0; trapped = 0; danger = 0;
}

// ---------------- small helpers ----------------
void drawQuad(float x0, float y0, float x1, float y1)
{
    glBegin(GL_QUADS);
    glVertex2f(x0, y0); glVertex2f(x1, y0);
    glVertex2f(x1, y1); glVertex2f(x0, y1);
    glEnd();
}

void drawText(float x, float y, const char* s, void* font = H12)
{
    glRasterPos2f(x, y);
    for (; *s; s++) glutBitmapCharacter(font, *s);
}

void beep(int freq, int ms)
{
#ifdef _WIN32
    if (soundOn) Beep(freq, ms);
#endif
}

void setFireRoom(int r)
{
    fireRoom = r;
    fx = roomFX[r - 1];
    fy = roomFY[r - 1];
}

// ---------------- algorithms ----------------
bool inFire(int x, int y)
{
    float dx = x - fx, dy = y - fy;
    return fireOn && (dx * dx + dy * dy < fr * fr);   // point ta agun er gol er bhitore?
}

// BRESENHAM LINE: draw=true hole aanke; line er kono point agun e porle true dey
bool bresenham(int x0, int y0, int x1, int y1, bool draw)
{
    bool hit = false;
    int dx = abs(x1 - x0), dy = -abs(y1 - y0);
    int sx = (x0 < x1) ? 1 : -1, sy = (y0 < y1) ? 1 : -1;
    int err = dx + dy;
    while (true)
    {
        if (draw) glVertex2i(x0, y0);
        if (inFire(x0, y0)) hit = true;
        if (x0 == x1 && y0 == y1) break;
        int e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
    return hit;
}

// MIDPOINT CIRCLE
void midpointCircle(int cx, int cy, int r)
{
    int x = 0, y = r, d = 1 - r;
    while (x <= y)
    {
        glVertex2i(cx + x, cy + y); glVertex2i(cx - x, cy + y);
        glVertex2i(cx + x, cy - y); glVertex2i(cx - x, cy - y);
        glVertex2i(cx + y, cy + x); glVertex2i(cx - y, cy + x);
        glVertex2i(cx + y, cy - x); glVertex2i(cx - y, cy - x);
        if (d < 0) d += 2 * x + 3;
        else { d += 2 * (x - y) + 5; y--; }
        x++;
    }
}

// POLYNOMIAL CIRCLE: y = sqrt(r*r - x*x)
void polyCircle(int cx, int cy, int r)
{
    for (int x = -r; x <= r; x++)
    {
        int y = (int)sqrt((double)(r * r - x * x));
        glVertex2i(cx + x, cy + y);
        glVertex2i(cx + x, cy - y);
    }
}

// TRIGONOMETRIC CIRCLE: x = r cos(t), y = r sin(t)
void trigCircle(float cx, float cy, float r)
{
    glBegin(GL_TRIANGLE_FAN);
    glVertex2f(cx, cy);
    for (int a = 0; a <= 360; a += 5)
    {
        float t = a * 3.14159f / 180;
        glVertex2f(cx + r * cos(t), cy + r * sin(t));
    }
    glEnd();
}

// ---------------- building ----------------
void drawFloor()
{
    glColor3f(0.82f, 0.88f, 0.93f); drawQuad(40, 120, 760, 560);
    glColor3f(0.90f, 0.88f, 0.80f); drawQuad(40, 300, 760, 380);
    glColor3f(0.65f, 0.45f, 0.25f);
    drawQuad(120, 296, 180, 304); drawQuad(360, 296, 420, 304); drawQuad(600, 296, 660, 304);
    drawQuad(120, 376, 180, 384); drawQuad(360, 376, 420, 384); drawQuad(600, 376, 660, 384);
}

void drawLabels()
{
    glColor3f(0.1f, 0.1f, 0.2f);
    drawText(60, 280, "Room 1");  drawText(300, 280, "Room 2");  drawText(540, 280, "Room 3");
    drawText(60, 540, "Room 4");  drawText(300, 540, "Room 5");  drawText(540, 540, "Room 6");
    drawText(330, 300, "CORRIDOR");
}

void drawWalls()
{
    glColor3f(0.2f, 0.2f, 0.25f);
    glPointSize(6);
    glBegin(GL_POINTS);
    bresenham(40, 120, 760, 120, true);
    bresenham(40, 560, 760, 560, true);
    bresenham(40, 120, 40, 300, true);    bresenham(40, 380, 40, 560, true);
    bresenham(760, 120, 760, 300, true);  bresenham(760, 380, 760, 560, true);
    bresenham(40, 300, 120, 300, true);   bresenham(180, 300, 360, 300, true);
    bresenham(420, 300, 600, 300, true);  bresenham(660, 300, 760, 300, true);
    bresenham(40, 380, 120, 380, true);   bresenham(180, 380, 360, 380, true);
    bresenham(420, 380, 600, 380, true);  bresenham(660, 380, 760, 380, true);
    bresenham(280, 120, 280, 300, true);  bresenham(520, 120, 520, 300, true);
    bresenham(280, 380, 280, 560, true);  bresenham(520, 380, 520, 560, true);
    glEnd();
}

void drawExits()
{
    glColor3f(0.0f, 0.65f, 0.25f);
    glPointSize(3);
    glBegin(GL_POINTS);
    polyCircle(60, 340, 22);
    polyCircle(740, 340, 22);
    glEnd();
    drawText(46, 336, "EXIT");
    drawText(726, 336, "EXIT");
}

void drawRoad()
{
    glColor3f(0.25f, 0.25f, 0.28f);
    drawQuad(0, 0, 800, 116);
    glColor3f(0.9f, 0.9f, 0.9f);
    glPointSize(2);
    glBegin(GL_POINTS);
    for (int x = 0; x < 800; x += 40) bresenham(x, 14, x + 20, 14, true);   // rasta r dag
    glEnd();
}

// ---------------- route (escape path) ----------------
void buildRoute(float x, float y, int side, int rx[4], int ry[4])
{
    int exitX = (side == 0) ? 80 : 720;
    int doorX = (x < 280) ? 150 : ((x < 520) ? 390 : 630);
    rx[0] = (int)x; ry[0] = (int)y;
    if (y > 300 && y < 380)
    {
        rx[1] = (int)x; ry[1] = 340;
        rx[2] = exitX;  ry[2] = 340;
        rx[3] = exitX;  ry[3] = 340;
    }
    else
    {
        rx[1] = doorX; ry[1] = (int)y;
        rx[2] = doorX; ry[2] = 340;
        rx[3] = exitX; ry[3] = 340;
    }
}

float routeLen(int rx[4], int ry[4])
{
    float s = 0;
    for (int i = 0; i < 3; i++) s += abs(rx[i + 1] - rx[i]) + abs(ry[i + 1] - ry[i]);
    return s;
}

bool routeBlocked(int rx[4], int ry[4])
{
    for (int i = 0; i < 3; i++)
        if (bresenham(rx[i], ry[i], rx[i + 1], ry[i + 1], false)) return true;
    return false;
}

// duto route (bam/dan) theke choto ar agunmukto ta bachhe; dutoi bondho hole -1
int chooseRoute(const Person& p, int rx[4], int ry[4])
{
    int ax[4], ay[4], bx[4], by[4];
    buildRoute(p.x, p.y, 0, ax, ay);
    buildRoute(p.x, p.y, 1, bx, by);
    bool bl0 = routeBlocked(ax, ay), bl1 = routeBlocked(bx, by);
    float l0 = routeLen(ax, ay), l1 = routeLen(bx, by);

    int side;
    if (!bl0 && (bl1 || l0 <= l1)) side = 0;
    else if (!bl1) side = 1;
    else side = -1;

    bool useLeft = (side == 0) || (side == -1 && l0 <= l1);
    for (int i = 0; i < 4; i++)
    {
        rx[i] = useLeft ? ax[i] : bx[i];
        ry[i] = useLeft ? ay[i] : by[i];
    }
    return side;
}

void updatePeople()
{
    escaped = 0; trapped = 0; danger = 0;
    for (int i = 0; i < N; i++)
    {
        Person &p = people[i];
        if (p.state == 2) { escaped++; continue; }

        int rx[4], ry[4];
        int side = chooseRoute(p, rx, ry);
        if (side == -1) { p.state = 3; trapped++; }
        else p.state = 1;

        if (p.state == 1)
        {
            int k = 1;
            while (k < 3 && rx[k] == rx[0] && ry[k] == ry[0]) k++;
            float dx = rx[k] - p.x, dy = ry[k] - p.y;
            float d = sqrt(dx * dx + dy * dy);
            if (d <= p.speed) { p.x = (float)rx[k]; p.y = (float)ry[k]; }
            else { p.x += dx / d * p.speed; p.y += dy / d * p.speed; }

            if (fabs(p.y - 340) < 5 && (p.x <= 90 || p.x >= 710))   // exit e pouchalo
            {
                p.state = 2; escaped++;
                if (p.x <= 90) leftOut++; else rightOut++;
                continue;
            }
        }
        float fdx = p.x - fx, fdy = p.y - fy;
        if (fr > 0 && sqrt(fdx * fdx + fdy * fdy) < fr + 40) danger++;
    }
}

void drawExitStatus()
{
    bool l = bresenham(390, 340, 80, 340, false);
    bool r = bresenham(390, 340, 720, 340, false);
    if (l) { glColor3f(0.85f, 0.1f, 0.1f); drawText(100, 356, "LEFT EXIT: BLOCKED"); }
    else   { glColor3f(0.0f, 0.5f, 0.2f);  drawText(100, 356, "LEFT EXIT: SAFE"); }
    if (r) { glColor3f(0.85f, 0.1f, 0.1f); drawText(570, 356, "RIGHT EXIT: BLOCKED"); }
    else   { glColor3f(0.0f, 0.5f, 0.2f);  drawText(570, 356, "RIGHT EXIT: SAFE"); }
}

void drawRoutes()
{
    glPointSize(2);
    for (int i = 0; i < N; i++)
    {
        if (people[i].state == 2) continue;
        int rx[4], ry[4];
        int side = chooseRoute(people[i], rx, ry);
        if (side == -1) glColor3f(0.85f, 0.1f, 0.1f);
        else            glColor3f(0.0f, 0.65f, 0.2f);
        glBegin(GL_POINTS);
        for (int k = 0; k < 3; k++) bresenham(rx[k], ry[k], rx[k + 1], ry[k + 1], true);
        glEnd();
    }
}

void drawPerson(int x, int y, float r, float g, float b)
{
    glColor3f(0.95f, 0.75f, 0.6f);  trigCircle(x, y + 14, 6);
    glColor3f(r, g, b);             drawQuad(x - 6, y - 6, x + 6, y + 8);
    glColor3f(0.1f, 0.1f, 0.1f);
    glPointSize(3);
    glBegin(GL_POINTS);
    bresenham(x - 3, y - 6, x - 3, y - 16, true);
    bresenham(x + 3, y - 6, x + 3, y - 16, true);
    glEnd();
}

void drawPeople()
{
    float cr[3] = { 0.10f, 0.60f, 0.10f };
    float cg[3] = { 0.30f, 0.20f, 0.60f };
    float cb[3] = { 0.80f, 0.70f, 0.40f };
    for (int i = 0; i < N; i++)
    {
        Person &p = people[i];
        if (p.state == 2) continue;
        if (p.state == 3) drawPerson((int)p.x, (int)p.y, 0.9f, 0.1f, 0.1f);
        else              drawPerson((int)p.x, (int)p.y, cr[i % 3], cg[i % 3], cb[i % 3]);

        float fdx = p.x - fx, fdy = p.y - fy;
        if (fireOn && fr > 0 && sqrt(fdx * fdx + fdy * fdy) < fr + 40)
        {
            glColor3f(1.0f, 0.0f, 0.0f);
            glPointSize(2);
            glBegin(GL_POINTS);
            midpointCircle((int)p.x, (int)p.y, 20);
            glEnd();
        }
    }
}

// ---------------- fire, smoke detector, alarm ----------------
void drawFire()
{
    if (!fireOn || fr <= 0) return;
    float r = fr * (1.0f + 0.04f * sin(frame * 0.5f));
    glColor4f(0.3f, 0.3f, 0.3f, 0.30f);   trigCircle(fx, fy, r * 1.3f);
    glColor4f(1.0f, 0.45f, 0.05f, 0.45f); trigCircle(fx, fy, r);
    glColor4f(1.0f, 0.8f, 0.1f, 0.6f);    trigCircle(fx, fy, r * 0.55f);
    glPointSize(3);
    glColor3f(0.7f, 0.1f, 0.0f);
    glBegin(GL_POINTS); midpointCircle((int)fx, (int)fy, (int)r); glEnd();
    glColor3f(1.0f, 1.0f, 0.6f);
    glBegin(GL_POINTS); polyCircle((int)fx, (int)fy, (int)(r * 0.2f)); glEnd();
}

void drawDetectors()
{
    for (int i = 0; i < ND; i++)
    {
        bool on = fireOn && detOn[i];
        if (on) glColor4f(1.0f, 0.1f, 0.1f, 0.9f);
        else    glColor4f(0.4f, 0.4f, 0.5f, 0.5f);
        glPointSize(1);
        glBegin(GL_POINTS); midpointCircle((int)detX[i], (int)detY[i], (int)DET_R); glEnd();
        if (on && (frame / 4) % 2 == 0) glColor3f(1.0f, 0.1f, 0.1f);
        else                            glColor3f(0.3f, 0.3f, 0.35f);
        trigCircle(detX[i], detY[i], 6);
    }
}

void updateAlarm()
{
    bool any = false;
    for (int i = 0; i < ND; i++)
    {
        float dx = detX[i] - fx, dy = detY[i] - fy;
        detOn[i] = (fr > 0) && (sqrt(dx * dx + dy * dy) - fr < DET_R);   // agun er gol detector er range e dhukse?
        if (detOn[i]) any = true;
    }
    if (any)
    {
        alarmLevel = (danger > 0) ? 2 : 1;
        if (alarmStart < 0) alarmStart = tFire;
    }
    else alarmLevel = 0;
}

void drawAlarm()
{
    float cx = 480, cy = 360;
    glColor3f(0.3f, 0.3f, 0.3f);  trigCircle(cx, cy, 14);
    if (alarmLevel == 0) { glColor3f(0.55f, 0.55f, 0.55f); trigCircle(cx, cy, 9); return; }
    if ((frame / 5) % 2 == 0) glColor3f(1.0f, 0.0f, 0.0f);
    else                      glColor3f(0.5f, 0.0f, 0.0f);
    trigCircle(cx, cy, 9);
    glPushMatrix();
    glTranslatef(cx, cy, 0);
    glRotatef(alarmAngle, 0, 0, 1);
    glColor4f(1.0f, 0.9f, 0.2f, 0.55f);
    glBegin(GL_TRIANGLES);
    glVertex2f(0, 0); glVertex2f(40, -8);  glVertex2f(40, 8);
    glVertex2f(0, 0); glVertex2f(-40, 8);  glVertex2f(-40, -8);
    glEnd();
    glPopMatrix();
}

// ---------------- fire engine ----------------
void drawTruck()
{
    if (truckMode == 0) return;
    glPushMatrix();
    glTranslatef(truckX, 62, 0);                         // truck er local coordinate
    glColor3f(0.85f, 0.10f, 0.10f);  drawQuad(-30, -18, 62, 24);    // body
    glColor3f(0.75f, 0.05f, 0.05f);  drawQuad(-66, -18, -30, 10);   // cabin
    glColor3f(0.70f, 0.85f, 0.95f);  drawQuad(-60, -4, -36, 6);     // janala
    glColor3f(1.0f, 1.0f, 1.0f);     drawQuad(-30, 2, 62, 8);       // safed dag

    glColor3f(0.9f, 0.9f, 0.9f);                                    // sinri (ladder)
    glPointSize(2);
    glBegin(GL_POINTS);
    bresenham(-24, 30, 56, 30, true);
    bresenham(-24, 36, 56, 36, true);
    for (int x = -24; x <= 56; x += 10) bresenham(x, 30, x, 36, true);
    bresenham(-20, 24, -20, 30, true);
    bresenham(50, 24, 50, 30, true);
    glEnd();

    glColor3f(0.05f, 0.05f, 0.05f);                                 // chaka
    trigCircle(-45, -18, 10); trigCircle(35, -18, 10);
    glColor3f(0.8f, 0.8f, 0.8f);
    glPointSize(2);
    glBegin(GL_POINTS); midpointCircle(-45, -18, 6); midpointCircle(35, -18, 6); glEnd();

    glPushMatrix();                                                 // ghurono siren
    glTranslatef(-48, 16, 0);
    glRotatef(frame * 20.0f, 0, 0, 1);
    if ((frame / 5) % 2 == 0) glColor4f(1.0f, 0.1f, 0.1f, 0.7f);
    else                      glColor4f(0.2f, 0.4f, 1.0f, 0.7f);
    glBegin(GL_TRIANGLES);
    glVertex2f(0, 0); glVertex2f(26, -6); glVertex2f(26, 6);
    glVertex2f(0, 0); glVertex2f(-26, 6); glVertex2f(-26, -6);
    glEnd();
    glPopMatrix();
    glPopMatrix();
}

// paner dhara: polynomial parabola  y = y0 + (y1-y0)t + h*4t(1-t)
void drawWater()
{
    if (!extinguishing || fr <= 0) return;
    float x0 = truckX + 56, y0 = 98;
    float x1 = fx, y1 = fy;
    float h = 70;
    glColor4f(0.3f, 0.6f, 1.0f, 0.5f);
    glPointSize(2);
    glBegin(GL_POINTS);
    for (float t = 0; t <= 1.0f; t += 0.01f)
        glVertex2f(x0 + (x1 - x0) * t, y0 + (y1 - y0) * t + h * 4 * t * (1 - t));
    glEnd();
    glColor4f(0.4f, 0.75f, 1.0f, 0.95f);
    for (int k = 0; k < 8; k++)                                    // chalanto phota
    {
        float t = fmod(frame * 0.04f + k / 8.0f, 1.0f);
        trigCircle(x0 + (x1 - x0) * t, y0 + (y1 - y0) * t + h * 4 * t * (1 - t), 4);
    }
}

// ---------------- panel, result ----------------
const char* sceneText[8] = {
    "",
    "",
    "Scene 2: Fire detected - alarm on, nearest exit",
    "Scene 3: Fire spreads toward the exits",
    "Scene 4: Exit blocked - people use the other exit",
    "Scene 5: Fire engine arrived - people still leaving",
    "Scene 6: Everyone is out - fire engine puts out fire",
    "Scene 7: ALL CLEAR - fire is out"
};

void drawPanel()
{
    glColor3f(0.12f, 0.12f, 0.15f);
    drawQuad(0, 560, 800, 650);
    glColor3f(1, 1, 1);
    drawText(10, 632, "Fire Escape Path Simulation");
    if (alarmLevel > 0 && (frame / 8) % 2 == 0)
    {
        glColor3f(1.0f, 0.2f, 0.2f);
        drawText(260, 630, "!!! FIRE ALARM !!!", H18);
    }
    char buf[96];
    if (fireOn)
    {
        sprintf(buf, "Out %d/%d  Risk %d  Trapped %d", escaped, N, danger, trapped);
        glColor3f(0.9f, 0.9f, 0.9f);
        drawText(520, 632, buf);
        sprintf(buf, "%.1fs", tFire * 0.05f);
        drawText(745, 632, buf);
    }

    // scene banner
    if (scene == 1)      { sprintf(buf, "Scene 1: Normal building - SPACE starts fire in Room %d", fireRoom); glColor3f(1, 1, 1); }
    else                 { strcpy(buf, sceneText[scene]);
                           if (scene <= 4) glColor3f(1.0f, 0.7f, 0.2f);
                           else if (scene <= 6) glColor3f(0.4f, 0.85f, 1.0f);
                           else glColor3f(0.3f, 1.0f, 0.4f); }
    drawText(10, 602, buf, H18);

    sprintf(buf, "Speed x%d %s", simSpeed, paused ? "PAUSED" : "");
    glColor3f(0.9f, 0.9f, 0.5f);
    drawText(660, 604, buf);

    if (danger > 0 && fireOn && (frame / 6) % 2 == 0)
    {
        glColor3f(1.0f, 0.3f, 0.3f);
        drawText(10, 574, "WARNING: people are close to the fire zone!", H18);
    }
    else
    {
        glColor3f(0.75f, 0.75f, 0.75f);
        drawText(10, 574, "Green=safe route  Red=no safe way  Red ring=near fire  |  SPACE start  P pause  +/- speed  M sound  1-6 fire room  R reset", H10);
    }
}

void drawResult()
{
    if (scene != 7) return;
    glColor4f(0.0f, 0.0f, 0.0f, 0.78f);
    drawQuad(170, 240, 630, 440);
    glColor3f(0.3f, 1.0f, 0.4f);
    drawText(330, 410, "ALL CLEAR", H18);
    char b[96];
    glColor3f(1, 1, 1);
    sprintf(b, "People safe: %d / %d", escaped, N);
    drawText(200, 375, b);
    if (tAllOut >= 0) sprintf(b, "Everyone out in: %.1f s", tAllOut * 0.05f);
    else              sprintf(b, "Everyone out in: not all escaped");
    drawText(200, 350, b);
    sprintf(b, "Fire put out at: %.1f s", tOut * 0.05f);
    drawText(200, 325, b);
    sprintf(b, "Left exit: %d people     Right exit: %d people", leftOut, rightOut);
    drawText(200, 300, b);
    glColor3f(0.8f, 0.8f, 0.8f);
    drawText(200, 265, "Press R to run again");
}

// ---------------- simulation step ----------------
void startFire()
{
    if (fireOn) return;
    fireOn = true;
    scene = 2;
    tFire = 0;
    parkX = fx + 30;
    if (parkX < 140) parkX = 140;
    if (parkX > 700) parkX = 700;
}

void resetSim()
{
    fireOn = false; fr = 0; frame = 0; tFire = 0;
    alarmAngle = 0; alarmLevel = 0; alarmStart = -1;
    scene = 1; paused = false;
    extinguishing = false; done = false;
    tAllOut = -1; tOut = -1; leftOut = 0; rightOut = 0;
    truckMode = 0; truckX = 900; arriveStep = 0;
    for (int i = 0; i < ND; i++) detOn[i] = false;
    setFireRoom(fireRoom);
    initPeople();
}

void stepSim()
{
    frame++;
    if (!fireOn) return;
    tFire++;

    // 1. agun boro hoy; fire engine ashle komay
    if (!extinguishing)
    {
        if (fr < 220) fr += 0.6f;
    }
    else
    {
        fr -= 1.2f;
        if (fr <= 0) { fr = 0; if (!done) { done = true; tOut = tFire; } }
    }

    // 2. manush hatey
    updatePeople();
    if (escaped == N && tAllOut < 0) tAllOut = tFire;

    // 3. detector ar alarm
    updateAlarm();
    alarmAngle += (alarmLevel == 2) ? 30 : (alarmLevel == 1 ? 12 : 0);

    // 4. fire engine: alarm er 6 second por rowna dey
    if (alarmStart >= 0 && tFire - alarmStart > 120 && truckMode == 0) truckMode = 1;
    if (truckMode == 1)
    {
        truckX -= 5;
        if (truckX <= parkX) { truckX = parkX; truckMode = 2; arriveStep = tFire; }
    }
    if (truckMode == 2 && !extinguishing && !done)
        if (escaped == N || tFire - arriveStep > 300) extinguishing = true;   // sobai ber hole (ba 15s por) nibhano shuru

    // 5. scene
    if (scene < 3 && fr > 70) scene = 3;
    bool lb = bresenham(390, 340, 80, 340, false);
    bool rb = bresenham(390, 340, 720, 340, false);
    if (scene < 4 && (lb || rb)) scene = 4;
    if (scene < 5 && truckMode == 2) scene = 5;
    if (scene < 6 && extinguishing) scene = 6;
    if (scene < 7 && done) scene = 7;
}

// ---------------- display, timer, keyboard, main ----------------
void display()
{
    glClear(GL_COLOR_BUFFER_BIT);
    drawRoad();
    drawFloor();
    drawLabels();
    drawExits();
    drawFire();
    drawWalls();
    drawDetectors();
    drawExitStatus();
    drawRoutes();
    drawPeople();
    drawWater();
    drawTruck();
    drawAlarm();
    drawResult();
    drawPanel();
    glutSwapBuffers();
}

void timer(int v)
{
    static int bc = 0;
    if (!paused)
    {
        for (int s = 0; s < simSpeed; s++) stepSim();
        bc++;
        if (fireOn && alarmLevel == 1 && bc % 10 == 0) beep(800, 60);     // dhire bip
        if (fireOn && alarmLevel == 2 && bc % 5 == 0)  beep(1500, 40);    // druto bip
    }
    glutPostRedisplay();
    glutTimerFunc(50, timer, 0);
}

void keyboard(unsigned char key, int x, int y)
{
    if (key == 27) exit(0);
    if (key == ' ' || key == 'n' || key == 'N') startFire();
    if (key == 'p' || key == 'P') paused = !paused;
    if (key == '+' || key == '=') { if (simSpeed < 4) simSpeed++; }
    if (key == '-')               { if (simSpeed > 1) simSpeed--; }
    if (key == 'm' || key == 'M') soundOn = !soundOn;
    if (key >= '1' && key <= '6' && !fireOn) setFireRoom(key - '0');
    if (key == 'r' || key == 'R') resetSim();
}

int main(int argc, char** argv)
{
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB);
    glutInitWindowSize(800, 650);
    glutCreateWindow("Fire Escape Path Simulation");
    glClearColor(0.1f, 0.1f, 0.12f, 1);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluOrtho2D(0, 800, 0, 650);
    glMatrixMode(GL_MODELVIEW);
    resetSim();
    glutDisplayFunc(display);
    glutKeyboardFunc(keyboard);
    glutTimerFunc(50, timer, 0);
    glutMainLoop();
    return 0;
}

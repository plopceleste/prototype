#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_ttf.h>
#include <SDL2/SDL_mixer.h>
#include <stdio.h>
#include <stdbool.h>
#include <math.h>

#define FIXED_HEIGHT 360

#define GRAVITY 3200.0f
#define JUMP_FORCE -960.0f
#define MAX_SPEED 300.0f
#define ACCEL_GROUND 2500.0f
#define FRICTION_GROUND 1800.0f
#define ACCEL_AIR 1500.0f
#define FRICTION_AIR 500.0f

#define DASH_SPEED 1000.0f
#define DASH_DURATION 0.15f
#define DASH_COOLDOWN 0.6f
#define IDLE_DEATH_TIME 1.5f

#define SPRITE_SIZE 20
#define DRAW_SIZE 80  
#define SPRITE_OFFSET_Y 10
#define ARMED_OFFSET_X 100

#define BTN_SIZE 90
#define DPAD_SIZE 165  

#define UI_MARGIN 20

typedef struct { float x, y, w, h; } RectF;

typedef struct {
    float x, y;
    int frameX, frameY;
    bool facingRight;
    float alpha; 
} Ghost;

typedef struct {
    float x, y;
    float startX, startY;
    float vx, vy;
    bool facingRight;

    float animTimer;
    int currentFrame;
    int state; 

    int lastState;

    bool onGround;
    float coyoteTimer;
    float jumpBufferTimer;

    float dashTimer;
    float dashCooldownTimer;
    bool isDashing;
    bool isAttacking;
    float idleDeathTimer;

    float scaleX, scaleY;
} Player;

typedef struct {
    SDL_Rect area;
    bool left, right, up, down;
    bool active; 
    SDL_FingerID fingerId; 
    float scale; 
} DPad;

typedef struct {
    SDL_FingerID fingerId;
    bool active;
    SDL_Rect area;
    bool justPressed;
    bool justReleased;
    SDL_Texture* tex;
    float currentScale; 
} Button;

SDL_Renderer* renderer = NULL;
SDL_Window* window = NULL;

SDL_Texture* texKnight = NULL;
SDL_Texture* texPadBlank = NULL;
SDL_Texture* texPadLeft = NULL;
SDL_Texture* texPadRight = NULL;
SDL_Texture* texPadUp = NULL;
SDL_Texture* texPadDown = NULL;

TTF_Font* fontBold = NULL;
Mix_Music* bgMusic = NULL;

#define MAX_GHOSTS 20
Ghost ghosts[MAX_GHOSTS];
int ghostHead = 0;

int gameW = 640;
int gameH = 360;
float globalTimer = 0.0f;

float Lerp(float a, float b, float t) {
    return a + (b - a) * t;
}

bool checkCol(RectF p, RectF w) {
    return (p.x < w.x + w.w && p.x + p.w > w.x &&
            p.y < w.y + w.h && p.y + p.h > w.y);
}

SDL_Texture* LoadTex(const char* path, bool colorKey) {
    SDL_Surface* tempSurface = IMG_Load(path);
    if (!tempSurface) {
        SDL_Surface* fallback = SDL_CreateRGBSurface(0, 32, 32, 32, 0,0,0,0);
        SDL_FillRect(fallback, NULL, SDL_MapRGB(fallback->format, 255, 0, 255));
        SDL_Texture* tex = SDL_CreateTextureFromSurface(renderer, fallback);
        SDL_FreeSurface(fallback);
        return tex;
    }
    if (colorKey) {
        SDL_SetColorKey(tempSurface, SDL_TRUE, SDL_MapRGB(tempSurface->format, 0, 0, 0));
    }
    SDL_Texture* tex = SDL_CreateTextureFromSurface(renderer, tempSurface);
    SDL_FreeSurface(tempSurface);
    return tex;
}

void RenderText(SDL_Renderer* r, TTF_Font* font, const char* text, int x, int y, SDL_Color color, bool alignRight) {
    if (!font) return;
    SDL_Surface* surf = TTF_RenderText_Solid(font, text, color);
    if (!surf) return;
    SDL_Texture* tex = SDL_CreateTextureFromSurface(r, surf);
    int drawX = alignRight ? x - surf->w : x;
    SDL_Rect dst = { drawX, y, surf->w, surf->h };
    SDL_RenderCopy(r, tex, NULL, &dst);
    SDL_DestroyTexture(tex);
    SDL_FreeSurface(surf);
}

void SpawnGhost(Player* p, int fx, int fy) {
    ghosts[ghostHead].x = p->x;
    ghosts[ghostHead].y = p->y;
    ghosts[ghostHead].frameX = fx;
    ghosts[ghostHead].frameY = fy;
    ghosts[ghostHead].facingRight = p->facingRight;
    ghosts[ghostHead].alpha = 0.6f; 
    ghostHead = (ghostHead + 1) % MAX_GHOSTS;
}

void Respawn(Player* p) {
    p->x = p->startX;
    p->y = p->startY;
    p->vx = 0; p->vy = 0;
    p->idleDeathTimer = IDLE_DEATH_TIME;
    p->scaleX = 0.1f; p->scaleY = 2.0f;
}

bool IsPointInRect(float x, float y, SDL_Rect r) {
    return (x >= r.x && x <= r.x + r.w && y >= r.y && y <= r.y + r.h);
}

void TouchToGameCoords(float normX, float normY, float* outX, float* outY) {
    int winW, winH;
    SDL_GetWindowSize(window, &winW, &winH);

    float scaleX = (float)winW / gameW;
    float scaleY = (float)winH / gameH;
    float scale = (scaleX < scaleY) ? scaleX : scaleY;

    float viewW = gameW * scale;
    float viewH = gameH * scale;

    float offX = (winW - viewW) / 2.0f;
    float offY = (winH - viewH) / 2.0f;

    float touchWinX = normX * winW;
    float touchWinY = normY * winH;

    *outX = (touchWinX - offX) / scale;
    *outY = (touchWinY - offY) / scale;
}

int main(int argc, char* argv[]) {

    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO);
    IMG_Init(IMG_INIT_PNG);
    TTF_Init();
    Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 2048);

    window = SDL_CreateWindow("Knight Smooth", 0, 0, 0, 0, SDL_WINDOW_FULLSCREEN_DESKTOP | SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);

    texKnight = LoadTex("knight.png", true);
    SDL_Texture* texA = LoadTex("a.png", false);
    SDL_Texture* texB = LoadTex("b.png", false);
    SDL_Texture* texY = LoadTex("y.png", false);

    texPadBlank = LoadTex("blank.png", false);
    texPadLeft  = LoadTex("left.png", false);
    texPadRight = LoadTex("right.png", false);
    texPadUp    = LoadTex("up.png", false);
    texPadDown  = LoadTex("down.png", false);

    fontBold = TTF_OpenFont("PixelAE-Bold.ttf", 24);
    bgMusic = Mix_LoadMUS("japanese_8bit.mp3");
    if (bgMusic) Mix_PlayMusic(bgMusic, -1);

    Player player = { 
        50, 200, 50, 200, 
        0, 0, true, 
        0.0f, 0, 0, 0, 
        false, 0.0f, 0.0f,
        0.0f, 0.0f, false, false,
        IDLE_DEATH_TIME, 1.0f, 1.0f
    };
    RectF platform = { -50, 280, 2000, 80 }; 

    DPad dPad = { {0,0,0,0}, false, false, false, false, false, 0, 1.0f };
    Button btnJump   = { 0, false, {0,0,0,0}, false, false, texA, 1.0f }; 
    Button btnAttack = { 0, false, {0,0,0,0}, false, false, texB, 1.0f };
    Button btnDash   = { 0, false, {0,0,0,0}, false, false, texY, 1.0f };

    bool isRunning = true;
    SDL_Event event;
    Uint64 lastPerf = SDL_GetPerformanceCounter();
    int lastScreenW = 0, lastScreenH = 0;

    for(int i=0; i<MAX_GHOSTS; i++) ghosts[i].alpha = 0.0f;

    while (isRunning) {

        int screenW, screenH;
        SDL_GetRendererOutputSize(renderer, &screenW, &screenH);
        if (screenH <= 0) { SDL_Delay(100); continue; }

        if (screenW != lastScreenW || screenH != lastScreenH) {
            lastScreenW = screenW; lastScreenH = screenH;
            float ratio = (float)screenW / (float)screenH;
            gameW = (int)(FIXED_HEIGHT * ratio);
            gameH = FIXED_HEIGHT;
            SDL_RenderSetLogicalSize(renderer, gameW, gameH);

            dPad.area = (SDL_Rect){ UI_MARGIN, gameH - DPAD_SIZE - UI_MARGIN, DPAD_SIZE, DPAD_SIZE };

            int startX = gameW - UI_MARGIN - BTN_SIZE;
            int startY = gameH - UI_MARGIN - BTN_SIZE;
            btnJump.area = (SDL_Rect){ startX, startY, BTN_SIZE, BTN_SIZE };
            btnAttack.area = (SDL_Rect){ startX - BTN_SIZE - 10, startY, BTN_SIZE, BTN_SIZE };
            int midX = btnAttack.area.x + (btnJump.area.x + btnJump.area.w - btnAttack.area.x)/2 - BTN_SIZE/2;
            btnDash.area = (SDL_Rect){ midX, startY - BTN_SIZE - 10, BTN_SIZE, BTN_SIZE };
        }

        Uint64 nowPerf = SDL_GetPerformanceCounter();
        double dt = (double)((nowPerf - lastPerf) / (double)SDL_GetPerformanceFrequency());
        lastPerf = nowPerf;
        if (dt > 0.05) dt = 0.05;
        globalTimer += dt;

        btnJump.justPressed = false; btnJump.justReleased = false;
        btnDash.justPressed = false; btnAttack.justPressed = false;

        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) isRunning = false;

            if (event.type == SDL_FINGERDOWN || event.type == SDL_FINGERUP || event.type == SDL_FINGERMOTION) {
                float tx, ty;
                TouchToGameCoords(event.tfinger.x, event.tfinger.y, &tx, &ty);
                SDL_FingerID fid = event.tfinger.fingerId;
                bool isDown = (event.type == SDL_FINGERDOWN);
                bool isUp = (event.type == SDL_FINGERUP);

                if (isDown) {
                    if (IsPointInRect(tx, ty, dPad.area) && !dPad.active) {
                        dPad.active = true; dPad.fingerId = fid;
                    }
                }
                if (dPad.active && dPad.fingerId == fid) {
                    if (isUp) {
                        dPad.active = false;
                        dPad.left = false; dPad.right = false; dPad.up = false; dPad.down = false;
                    } else {
                        float cx = dPad.area.x + dPad.area.w / 2.0f;
                        float cy = dPad.area.y + dPad.area.h / 2.0f;
                        float dx = tx - cx;
                        float dy = ty - cy;
                        float deadzone = dPad.area.w * 0.10f;
                        dPad.left  = (dx < -deadzone);
                        dPad.right = (dx > deadzone);
                        dPad.up    = (dy < -deadzone);
                        dPad.down  = (dy > deadzone);
                    }
                }

                #define CHECK_BTN(btn) \
                    if (isDown && IsPointInRect(tx, ty, btn.area) && !btn.active) { \
                        btn.active = true; btn.fingerId = fid; btn.justPressed = true; \
                    } \
                    else if (isUp && btn.active && btn.fingerId == fid) { \
                        btn.active = false; btn.justReleased = true; \
                    }
                CHECK_BTN(btnJump);
                CHECK_BTN(btnAttack);
                CHECK_BTN(btnDash);
            }
        }

        Button* allBtns[] = { &btnJump, &btnAttack, &btnDash };
        for(int i=0; i<3; i++) {
            float target = allBtns[i]->active ? 0.85f : 1.0f; 
            allBtns[i]->currentScale = Lerp(allBtns[i]->currentScale, target, 25.0f * dt);
        }
        float padTarget = dPad.active ? 0.95f : 1.0f;
        dPad.scale = Lerp(dPad.scale, padTarget, 25.0f * dt);

        player.scaleX = Lerp(player.scaleX, 1.0f, 15.0f * dt);
        player.scaleY = Lerp(player.scaleY, 1.0f, 15.0f * dt);

        if (player.coyoteTimer > 0) player.coyoteTimer -= dt;
        if (player.jumpBufferTimer > 0) player.jumpBufferTimer -= dt;
        if (player.dashCooldownTimer > 0) player.dashCooldownTimer -= dt;
        if (player.dashTimer > 0) player.dashTimer -= dt;
        if (player.dashTimer <= 0) player.isDashing = false;

        bool isMoving = (fabs(player.vx) > 10.0f) || player.isDashing || player.isAttacking || !player.onGround;
        if (isMoving) player.idleDeathTimer = IDLE_DEATH_TIME;
        else {
            player.idleDeathTimer -= dt;
            if (player.idleDeathTimer <= 0) Respawn(&player);
        }

        if (btnAttack.justPressed && !player.isDashing && !player.isAttacking) {
            player.isAttacking = true;
            player.state = 4; player.currentFrame = 0; player.animTimer = 0;
            player.vx = 0; 
        }

        if (btnDash.justPressed && player.dashCooldownTimer <= 0 && !player.isAttacking) {
            player.isDashing = true;
            player.dashTimer = DASH_DURATION;
            player.dashCooldownTimer = DASH_COOLDOWN;
            player.vx = (player.facingRight ? 1 : -1) * DASH_SPEED;
            player.vy = 0; 
            player.state = 3;
            player.scaleX = 1.4f; player.scaleY = 0.6f;
        }

        bool jumpRequested = btnJump.justPressed;

        if (dPad.up && player.onGround && player.jumpBufferTimer <= 0) {
            jumpRequested = true;
        }
        if (jumpRequested) player.jumpBufferTimer = 0.1f;

        bool isJumpHeld = btnJump.active || dPad.up;

        if (player.isAttacking) {
            player.vx = Lerp(player.vx, 0, 10.0f * dt);
            player.vy += GRAVITY * dt;
        }
        else if (player.isDashing) {
            player.vx = (player.facingRight ? 1 : -1) * DASH_SPEED;
            player.vy = 0;
            SpawnGhost(&player, 340, 40);
        } 
        else {

            float dir = 0.0f;
            if (dPad.left) dir -= 1.0f;
            if (dPad.right) dir += 1.0f;
            if (dPad.left && dPad.right) dir = 0.0f;

            float targetSpeed = dir * MAX_SPEED;
            float accel = player.onGround ? ACCEL_GROUND : ACCEL_AIR;
            float friction = player.onGround ? FRICTION_GROUND : FRICTION_AIR;

            if (dir != 0) {
                if (player.vx * dir < 0) player.vx = Lerp(player.vx, targetSpeed, 10.0f * dt);
                else {
                    if (dir > 0 && player.vx < targetSpeed) player.vx += accel * dt;
                    else if (dir < 0 && player.vx > targetSpeed) player.vx -= accel * dt;
                }
                player.facingRight = (dir > 0);
            } else {
                if (player.vx > 0) {
                    player.vx -= friction * dt;
                    if (player.vx < 0) player.vx = 0;
                } else if (player.vx < 0) {
                    player.vx += friction * dt;
                    if (player.vx > 0) player.vx = 0;
                }
            }

            if (player.vx > MAX_SPEED) player.vx = MAX_SPEED;
            if (player.vx < -MAX_SPEED) player.vx = -MAX_SPEED;

            if (player.jumpBufferTimer > 0 && player.coyoteTimer > 0) {
                player.vy = JUMP_FORCE;
                player.onGround = false;
                player.coyoteTimer = 0; player.jumpBufferTimer = 0;
                player.scaleX = 0.7f; player.scaleY = 1.3f; 
            }

            if (player.vy < -200.0f && !isJumpHeld) {
                player.vy *= 0.6f; 

            }

            if (!player.onGround) player.state = 2; 
            else if (fabs(player.vx) > 20) player.state = 1; 
            else player.state = 0; 

            player.vy += GRAVITY * dt;
        }

        player.x += player.vx * dt;
        player.y += player.vy * dt;

        float feetY = player.y + DRAW_SIZE - SPRITE_OFFSET_Y;
        RectF pRect = { player.x + DRAW_SIZE/2 - 10, feetY - 40, 20, 40 };
        bool wasOnGround = player.onGround;
        player.onGround = false;

        if (checkCol(pRect, platform)) {
            float penetration = (pRect.y + pRect.h) - platform.y;
            if (player.vy >= 0 && penetration < 50.0f) {
                player.y = platform.y - (DRAW_SIZE - SPRITE_OFFSET_Y);
                player.vy = 0;
                player.onGround = true;
                player.coyoteTimer = 0.1f;
                if (!wasOnGround) { player.scaleX = 1.3f; player.scaleY = 0.7f; }
            }
        } else if (wasOnGround && player.vy >= 0 && !player.isDashing) {
            player.coyoteTimer = 0.1f;
        }

        if (player.y > 600) Respawn(&player);

        if (player.state != player.lastState) {
            if (player.state != 2) { player.currentFrame = 0; player.animTimer = 0; }
            player.lastState = player.state;
        }

        int srcX = 0, srcY = 0;
        if (player.isDashing) { srcX = 340; srcY = 40; } 
        else if (player.isAttacking) {
            player.animTimer += dt;
            if (player.animTimer >= 0.08f) {
                player.animTimer = 0; player.currentFrame++;
                if (player.currentFrame >= 4) { player.isAttacking = false; player.currentFrame = 0; player.state = 0; }
            }
            srcY = 40; srcX = 300 + (player.currentFrame * 20);
        }
        else {
            player.animTimer += dt;
            if (player.state == 0) { 

                if (player.animTimer > 0.3f) { player.animTimer = 0; player.currentFrame = (player.currentFrame + 1) % 2; }
                srcX = ARMED_OFFSET_X + (player.currentFrame * 20); srcY = 40;
            } else if (player.state == 1) { 

                if (player.animTimer > 0.1f) { player.animTimer = 0; player.currentFrame = (player.currentFrame + 1) % 4; }
                srcX = ARMED_OFFSET_X + (player.currentFrame * 20); srcY = 140;
            } else if (player.state == 2) { 

                srcY = 140; 
                if (player.vy < -400) srcX = 200; else if (player.vy > 400) srcX = 240; else srcX = 220; 
            }
        }

        SDL_SetRenderDrawColor(renderer, 20, 20, 30, 255);
        SDL_RenderClear(renderer);

        SDL_SetRenderDrawColor(renderer, 100, 100, 120, 255);
        SDL_Rect rPlatform = { (int)platform.x, (int)platform.y, (int)platform.w, (int)platform.h };
        SDL_RenderFillRect(renderer, &rPlatform);

        SDL_RendererFlip flip = player.facingRight ? SDL_FLIP_HORIZONTAL : SDL_FLIP_NONE;
        if (player.isAttacking) flip = player.facingRight ? SDL_FLIP_NONE : SDL_FLIP_HORIZONTAL;

        for(int i=0; i<MAX_GHOSTS; i++) {
            if (ghosts[i].alpha > 0) {
                SDL_SetTextureAlphaMod(texKnight, (Uint8)(ghosts[i].alpha * 255));
                SDL_Rect gSrc = { ghosts[i].frameX, ghosts[i].frameY, SPRITE_SIZE, SPRITE_SIZE };
                SDL_Rect gDst = { (int)ghosts[i].x, (int)ghosts[i].y, DRAW_SIZE, DRAW_SIZE };
                SDL_RendererFlip gFlip = ghosts[i].facingRight ? SDL_FLIP_HORIZONTAL : SDL_FLIP_NONE;
                SDL_RenderCopyEx(renderer, texKnight, &gSrc, &gDst, 0, NULL, gFlip);
                ghosts[i].alpha -= 3.0f * dt; 
            }
        }
        SDL_SetTextureAlphaMod(texKnight, 255); 

        int finalW = (int)(DRAW_SIZE * player.scaleX);
        int finalH = (int)(DRAW_SIZE * player.scaleY);
        int finalX = (int)(player.x + (DRAW_SIZE - finalW) / 2.0f);
        int finalY = (int)(player.y + (DRAW_SIZE - finalH)); 
        SDL_Rect src = { srcX, srcY, SPRITE_SIZE, SPRITE_SIZE };
        SDL_Rect dst = { finalX, finalY, finalW, finalH };
        SDL_RenderCopyEx(renderer, texKnight, &src, &dst, 0, NULL, flip);

        SDL_Rect rPad = dPad.area;
        int pw = (int)(rPad.w * dPad.scale);
        int ph = (int)(rPad.h * dPad.scale);
        int px = rPad.x + (rPad.w - pw)/2;
        int py = rPad.y + (rPad.h - ph)/2;
        SDL_Rect dstPad = { px, py, pw, ph };

        SDL_RenderCopy(renderer, texPadBlank, NULL, &dstPad);
        if (dPad.left)  SDL_RenderCopy(renderer, texPadLeft, NULL, &dstPad);
        if (dPad.right) SDL_RenderCopy(renderer, texPadRight, NULL, &dstPad);
        if (dPad.up)    SDL_RenderCopy(renderer, texPadUp, NULL, &dstPad);
        if (dPad.down)  SDL_RenderCopy(renderer, texPadDown, NULL, &dstPad);

        Button* btns[] = { &btnDash, &btnAttack, &btnJump };
        for (int i=0; i<3; i++) {
            SDL_Rect r = btns[i]->area;
            int w = (int)(r.w * btns[i]->currentScale);
            int h = (int)(r.h * btns[i]->currentScale);
            int x = r.x + (r.w - w) / 2;
            int y = r.y + (r.h - h) / 2;
            SDL_Rect dstBtn = { x, y, w, h };
            SDL_RenderCopy(renderer, btns[i]->tex, NULL, &dstBtn);
        }

        char timeBuffer[32];
        int min = (int)(globalTimer / 60);
        int sec = (int)(globalTimer) % 60;
        int ms  = (int)((globalTimer - (int)globalTimer) * 100);
        sprintf(timeBuffer, "%02d:%02d:%02d", min, sec, ms);
        SDL_Color cWhite = {255, 255, 255, 255};
        RenderText(renderer, fontBold, timeBuffer, gameW - 10, 10, cWhite, true);

        if (player.idleDeathTimer < IDLE_DEATH_TIME) {
            sprintf(timeBuffer, "%.2f", player.idleDeathTimer);
            SDL_Color cRed = {255, 50, 50, 255};
            RenderText(renderer, fontBold, timeBuffer, gameW - 10, 40, cRed, true);
        }

        SDL_RenderPresent(renderer);
    }

    SDL_DestroyTexture(texKnight);
    SDL_DestroyTexture(texA); SDL_DestroyTexture(texB); SDL_DestroyTexture(texY);
    SDL_DestroyTexture(texPadBlank); SDL_DestroyTexture(texPadLeft); 
    SDL_DestroyTexture(texPadRight); SDL_DestroyTexture(texPadUp); SDL_DestroyTexture(texPadDown);
    Mix_FreeMusic(bgMusic); TTF_CloseFont(fontBold);
    SDL_DestroyRenderer(renderer); SDL_DestroyWindow(window);
    Mix_Quit(); TTF_Quit(); IMG_Quit(); SDL_Quit();
    return 0;
}

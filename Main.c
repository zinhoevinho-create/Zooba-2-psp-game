#include <pspkernel.h>
#include <pspdebug.h>
#include <pspdisplay.h>
#include <pspctrl.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>

PSP_MODULE_INFO("Zooba2PSP", 0, 1, 1);
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER);

/* --- CONSTANTES E DEFINIÇÕES --- */
#define MAP_SIZE 2200
#define MAX_ENTITIES 30
#define MAX_PROJECTILES 50
#define MAX_ITEMS 80
#define SCREEN_WIDTH 480
#define SCREEN_HEIGHT 272

typedef enum { SCREEN_MENU, SCREEN_PLAYING, SCREEN_GAMEOVER } GameState;

typedef struct {
    char name[20];
    char symbol;
    int baseHp;
    int baseDmg;
    int level;
    int unlocked;
    int costTokens;
} Animal;

typedef struct {
    float x, y;
    int hp, maxHp;
    int damage;
    float speed;
    int teamId;
    int isBot;
    int isGuard;
    int knocked;
    float knockTimer;
    float reviveProgress;
    int medkits;
    
    // Nível das armas equipadas (0 = sem arma, 1=Padrao, 2=Bronze, 3=Prata, 4=Ouro, 5=Lendaria)
    int wShotgun;
    int wSpear;
    int wBomb;

    // Cooldowns
    float shotgunCd;
    float spearCd;
    float bombCd;
    
    char symbol;
} Entity;

typedef struct {
    float x, y;
    float vx, vy;
    int damage;
    int life;
    int ownerTeam;
} Projectile;

typedef struct {
    float x, y;
    int itemType; // 0 = Medkit, 1 = Shotgun, 2 = Spear, 3 = Bomb
    int rarity;   // 1 a 5
    int active;
} GroundItem;

typedef struct {
    int trophies;
    int tokens;
    int coins;
    int selectedAnimalIndex;
} PlayerStats;

/* --- ESTADO GLOBAL --- */
GameState currentState = SCREEN_MENU;
PlayerStats stats = { .trophies = 0, .tokens = 0, .coins = 500, .selectedAnimalIndex = 0 };

Animal animals[10] = {
    {"Larry", 'L', 160, 20, 1, 1, 0},
    {"Nix",   'N', 150, 18, 1, 1, 0},
    {"Buck",  'B', 220, 16, 1, 0, 100},
    {"Bruce", 'G', 250, 24, 1, 0, 100},
    {"Duke",  'D', 200, 22, 1, 0, 100},
    {"Shelly",'S', 280, 14, 1, 0, 100},
    {"Pepper",'P', 140, 21, 1, 0, 100},
    {"Ollie", 'O', 210, 19, 1, 0, 100},
    {"Jade",  'J', 180, 25, 1, 0, 100},
    {"Fuzzy", 'F', 170, 17, 1, 0, 100}
};

Entity entities[MAX_ENTITIES];
int entityCount = 0;

Projectile projectiles[MAX_PROJECTILES];
int projectileCount = 0;

GroundItem groundItems[MAX_ITEMS];
int itemCount = 0;

int aliveCount = 0;
int lastVictoryState = 0;

/* --- CALLBACKS DO PSP --- */
int exit_callback(int arg1, int arg2, void *common) {
    sceKernelExitGame();
    return 0;
}

int CallbackThread(SceSize args, void *argp) {
    int cbid = sceKernelCreateCallback("Exit Callback", exit_callback, NULL);
    sceKernelRegisterExitCallback(cbid);
    sceKernelSleepThreadCB();
    return 0;
}

int SetupCallbacks(void) {
    int thid = sceKernelCreateThread("update_thread", CallbackThread, 0x11, 0xFA0, 0, 0);
    if (thid >= 0) sceKernelStartThread(thid, 0, 0);
    return thid;
}

/* --- LÓGICA DO JOGO --- */
int getStatHp(Animal a) { return a.baseHp + (a.level - 1) * 20; }
int getStatDmg(Animal a) { return a.baseDmg + (a.level - 1) * 3; }

void spawnItems() {
    itemCount = 0;
    for (int i = 0; i < MAX_ITEMS; i++) {
        groundItems[i].x = (rand() % (MAP_SIZE * 2)) - MAP_SIZE;
        groundItems[i].y = (rand() % (MAP_SIZE * 2)) - MAP_SIZE;
        groundItems[i].active = 1;
        groundItems[i].itemType = rand() % 4;
        groundItems[i].rarity = (rand() % 5) + 1;
    }
}

void initGame() {
    entityCount = 0;
    projectileCount = 0;

    // Player Principal (Time 1)
    Animal sel = animals[stats.selectedAnimalIndex];
    entities[0].x = 0; entities[0].y = 0;
    entities[0].hp = getStatHp(sel); entities[0].maxHp = entities[0].hp;
    entities[0].damage = getStatDmg(sel);
    entities[0].speed = 4.0f;
    entities[0].teamId = 1;
    entities[0].isBot = 0;
    entities[0].isGuard = 0;
    entities[0].knocked = 0;
    entities[0].medkits = 1;
    entities[0].wShotgun = 1; entities[0].wSpear = 0; entities[0].wBomb = 0;
    entities[0].symbol = sel.symbol;
    entityCount++;

    // Inimigos Bots
    for (int i = 1; i < 15; i++) {
        int aIdx = rand() % 10;
        Animal botA = animals[aIdx];
        entities[i].x = (rand() % 1600) - 800;
        entities[i].y = (rand() % 1600) - 800;
        entities[i].hp = getStatHp(botA); entities[i].maxHp = entities[i].hp;
        entities[i].damage = getStatDmg(botA);
        entities[i].speed = 3.2f;
        entities[i].teamId = i + 1;
        entities[i].isBot = 1;
        entities[i].isGuard = 0;
        entities[i].knocked = 0;
        entities[i].medkits = 1;
        entities[i].wShotgun = (rand() % 3) + 1;
        entities[i].symbol = botA.symbol;
        entityCount++;
    }

    // Guarda Lendário (Boss)
    entities[entityCount].x = 0; entities[entityCount].y = -500;
    entities[entityCount].hp = 500; entities[entityCount].maxHp = 500;
    entities[entityCount].damage = 30;
    entities[entityCount].speed = 2.5f;
    entities[entityCount].teamId = 99;
    entities[entityCount].isBot = 1;
    entities[entityCount].isGuard = 1;
    entities[entityCount].knocked = 0;
    entities[entityCount].wShotgun = 5;
    entities[entityCount].symbol = 'G';
    entityCount++;

    spawnItems();
    currentState = SCREEN_PLAYING;
}

void shootWeapon(Entity *ent, int type) {
    if (projectileCount >= MAX_PROJECTILES) return;

    float dirX = 1.0f, dirY = 0.0f; // Direção padrão
    float speed = 8.0f;
    int dmg = ent->damage;

    if (type == 1 && ent->wShotgun > 0 && ent->shotgunCd <= 0) {
        ent->shotgunCd = 1.5f;
        dmg *= ent->wShotgun;
    } else if (type == 2 && ent->wSpear > 0 && ent->spearCd <= 0) {
        ent->spearCd = 2.0f;
        speed = 12.0f;
        dmg *= (ent->wSpear * 1.3f);
    } else if (type == 3 && ent->wBomb > 0 && ent->bombCd <= 0) {
        ent->bombCd = 2.5f;
        speed = 6.0f;
        dmg *= (ent->wBomb * 1.6f);
    } else {
        return;
    }

    projectiles[projectileCount].x = ent->x;
    projectiles[projectileCount].y = ent->y;
    projectiles[projectileCount].vx = dirX * speed;
    projectiles[projectileCount].vy = dirY * speed;
    projectiles[projectileCount].damage = dmg;
    projectiles[projectileCount].life = 30;
    projectiles[projectileCount].ownerTeam = ent->teamId;
    projectileCount++;
}

void updateGame(SceCtrlData pad) {
    Entity *player = &entities[0];

    // Controles do Player (Analógico + D-Pad)
    float moveX = 0, moveY = 0;
    if (pad.Lx < 80 || (pad.Buttons & PSP_CTRL_LEFT)) moveX = -1;
    if (pad.Lx > 170 || (pad.Buttons & PSP_CTRL_RIGHT)) moveX = 1;
    if (pad.Ly < 80 || (pad.Buttons & PSP_CTRL_UP)) moveY = -1;
    if (pad.Ly > 170 || (pad.Buttons & PSP_CTRL_DOWN)) moveY = 1;

    if (!player->knocked && player->hp > 0) {
        player->x += moveX * player->speed;
        player->y += moveY * player->speed;
    }

    // Botões de Ação
    if (pad.Buttons & PSP_CTRL_SQUARE) shootWeapon(player, 1);   // Shotgun
    if (pad.Buttons & PSP_CTRL_TRIANGLE) shootWeapon(player, 2); // Lança
    if (pad.Buttons & PSP_CTRL_CROSS) shootWeapon(player, 3);    // Bomba
    if (pad.Buttons & PSP_CTRL_CIRCLE) {                         // Cura
        if (player->medkits > 0 && player->hp < player->maxHp) {
            player->medkits--;
            player->hp = (player->hp + 60 > player->maxHp) ? player->maxHp : player->hp + 60;
        }
    }

    // Cooldowns & Projéteis
    for (int i = 0; i < entityCount; i++) {
        if (entities[i].shotgunCd > 0) entities[i].shotgunCd -= 0.016f;
        if (entities[i].spearCd > 0) entities[i].spearCd -= 0.016f;
        if (entities[i].bombCd > 0) entities[i].bombCd -= 0.016f;
    }

    for (int i = 0; i < projectileCount; i++) {
        projectiles[i].x += projectiles[i].vx;
        projectiles[i].y += projectiles[i].vy;
        projectiles[i].life--;

        // Colisão com entidades
        for (int j = 0; j < entityCount; j++) {
            if (entities[j].hp > 0 && entities[j].teamId != projectiles[i].ownerTeam) {
                float dist = sqrtf(powf(projectiles[i].x - entities[j].x, 2) + powf(projectiles[i].y - entities[j].y, 2));
                if (dist < 20.0f) {
                    entities[j].hp -= projectiles[i].damage;
                    projectiles[i].life = 0;
                    if (entities[j].hp <= 0) entities[j].hp = 0;
                }
            }
        }
    }

    // Coleta de Itens
    for (int i = 0; i < itemCount; i++) {
        if (!groundItems[i].active) continue;
        float dist = sqrtf(powf(player->x - groundItems[i].x, 2) + powf(player->y - groundItems[i].y, 2));
        if (dist < 25.0f) {
            if (groundItems[i].itemType == 0) player->medkits++;
            else if (groundItems[i].itemType == 1) player->wShotgun = groundItems[i].rarity;
            else if (groundItems[i].itemType == 2) player->wSpear = groundItems[i].rarity;
            else if (groundItems[i].itemType == 3) player->wBomb = groundItems[i].rarity;
            groundItems[i].active = 0;
        }
    }

    // Checar Fim de Jogo
    aliveCount = 0;
    for (int i = 0; i < entityCount; i++) {
        if (entities[i].hp > 0 && !entities[i].isGuard) aliveCount++;
    }

    if (player->hp <= 0) {
        lastVictoryState = 0;
        stats.trophies = (stats.trophies - 5 < 0) ? 0 : stats.trophies - 5;
        currentState = SCREEN_GAMEOVER;
    } else if (aliveCount <= 1) {
        lastVictoryState = 1;
        stats.trophies += 50;
        stats.tokens += 25;
        stats.coins += 150;
        currentState = SCREEN_GAMEOVER;
    }
}

/* --- RENDERIZAÇÃO --- */
void render() {
    pspDebugScreenSetXY(0, 0);

    if (currentState == SCREEN_MENU) {
        pspDebugScreenPrintf("==================================================\n");
        pspDebugScreenPrintf("          ZOOBA 2 — ARENA BATTLE (PSP)            \n");
        pspDebugScreenPrintf("==================================================\n\n");
        pspDebugScreenPrintf("  [STATUS CONTA]:\n");
        pspDebugScreenPrintf("  Trofeus: %d | Fichas: %d | Moedas: %d\n\n", stats.trophies, stats.tokens, stats.coins);
        pspDebugScreenPrintf("  Animal Selecionado: %s (Nivel %d)\n\n", animals[stats.selectedAnimalIndex].name, animals[stats.selectedAnimalIndex].level);
        pspDebugScreenPrintf("  ------------------------------------------------\n");
        pspDebugScreenPrintf("  Pressione [START] para Iniciar Partida Solo\n");
        pspDebugScreenPrintf("  Pressione [SELECT] para Sair do Jogo\n");
    } 
    else if (currentState == SCREEN_PLAYING) {
        Entity player = entities[0];
        pspDebugScreenPrintf("HP: %d/%d | Vivos: %d | Kits: %d | Pos: (%.0f, %.0f)\n", 
                             player.hp, player.maxHp, aliveCount, player.medkits, player.x, player.y);
        pspDebugScreenPrintf("Armas -> Shotgun: Lvl %d | Lanca: Lvl %d | Bomba: Lvl %d\n", 
                             player.wShotgun, player.wSpear, player.wBomb);
        pspDebugScreenPrintf("--------------------------------------------------\n");

        pspDebugScreenPrintf("RADAR ARENA (Sua Posicao [P]):\n\n");
        for (int y = -5; y <= 5; y++) {
            for (int x = -15; x <= 15; x++) {
                int drew = 0;
                if (x == 0 && y == 0) { pspDebugScreenPrintf("P"); drew = 1; }
                else {
                    for (int i = 1; i < entityCount; i++) {
                        if (entities[i].hp > 0) {
                            int relX = (int)((entities[i].x - player.x) / 50.0f);
                            int relY = (int)((entities[i].y - player.y) / 50.0f);
                            if (relX == x && relY == y) {
                                pspDebugScreenPrintf("%c", entities[i].symbol);
                                drew = 1;
                                break;
                            }
                        }
                    }
                }
                if (!drew) pspDebugScreenPrintf(".");
            }
            pspDebugScreenPrintf("\n");
        }
        pspDebugScreenPrintf("\n[Q] Shotgun | [Tri] Lanca | [X] Bomba | [O] Kit Cura\n");
    } 
    else if (currentState == SCREEN_GAMEOVER) {
        pspDebugScreenPrintf("==================================================\n");
        if (lastVictoryState) {
            pspDebugScreenPrintf("                 VITORIA ROYAL!                   \n");
            pspDebugScreenPrintf("          +50 Trofeus | +25 Fichas | +150 Moedas  \n");
        } else {
            pspDebugScreenPrintf("                   VOCE MORREU!                   \n");
            pspDebugScreenPrintf("                    -5 Trofeus                    \n");
        }
        pspDebugScreenPrintf("==================================================\n\n");
        pspDebugScreenPrintf("Pressione [START] para Voltar ao Menu Principal\n");
    }
}

/* --- LOOP PRINCIPAL --- */
int main(void) {
    pspDebugScreenInit();
    SetupCallbacks();

    sceCtrlSetSamplingCycle(0);
    sceCtrlSetSamplingMode(PSP_CTRL_MODE_ANALOG);

    SceCtrlData pad;

    while (1) {
        sceCtrlReadBufferPositive(&pad, 1);

        if (currentState == SCREEN_MENU) {
            if (pad.Buttons & PSP_CTRL_START) initGame();
        } else if (currentState == SCREEN_PLAYING) {
            updateGame(pad);
        } else if (currentState == SCREEN_GAMEOVER) {
            if (pad.Buttons & PSP_CTRL_START) currentState = SCREEN_MENU;
        }

        render();
        sceDisplayWaitVblankStart();
    }

    return 0;
}
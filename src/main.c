/* The Lost World: Jurassic Park -- statically recompiled.
 *
 * The board comes from ext/model3recomp; the game comes from src/recomp/,
 * which you generate from your own dump (see the README). This file is only
 * the glue: load the ROM images, register the lifted functions, and enter at
 * the PowerPC reset vector.
 *
 * Model 3 games never return from their main loop, so model3recomp_run() does
 * not come back while the game is running. Anything a host wants to do per
 * frame goes in the frame hook.
 */
#include "model3recomp/model3recomp.h"
#include "model3recomp/bus.h"
#include "model3recomp/input.h"

#include "lwrom_funcs.h"
#include "lwram_funcs.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#define strdup _strdup
#define putenv _putenv
#endif

/* Built by tools/build_roms.py from the romset. */
/* Built by tools/build_roms.py from the romset, in roms/ unless --roms
 * says otherwise. */
static const char *g_roms_dir = "roms";
static uint8_t *slurp(const char *path, size_t *len, int required);

static uint8_t *slurp_rom(const char *name, size_t *len, int required)
{
    char path[512];
    snprintf(path, sizeof path, "%s/%s", g_roms_dir, name);
    return slurp(path, len, required);
}

/* --env FILE: KEY=VALUE lines put into the environment, for the harness
 * variables (M3_COIN_AT, M3_NETPLAY, ...) where a command line is all a
 * launcher can pass, such as a test harness's run line. */
static void load_env(const char *path)
{
    char line[512];
    FILE *f = fopen(path, "r");
    if (!f) { fprintf(stderr, "%s: cannot read\n", path); return; }
    while (fgets(line, sizeof line, f)) {
        char *nl = strpbrk(line, "\r\n");
        if (nl) *nl = 0;
        if (line[0] && line[0] != '#' && strchr(line, '='))
            putenv(strdup(line));
    }
    fclose(f);
}

static uint8_t *slurp(const char *path, size_t *len, int required)
{
    FILE *f = fopen(path, "rb");
    long n;
    uint8_t *p;

    if (!f) {
        if (required) perror(path);
        return NULL;
    }
    fseek(f, 0, SEEK_END);
    n = ftell(f);
    fseek(f, 0, SEEK_SET);
    p = malloc((size_t)n);
    if (!p || fread(p, 1, (size_t)n, f) != (size_t)n) {
        fprintf(stderr, "%s: short read\n", path);
        fclose(f);
        free(p);
        return NULL;
    }
    fclose(f);
    *len = (size_t)n;
    return p;
}

/* Optional: stop after N fields and write a screenshot. Useful for the
 * conformance harness and for bug reports; harmless otherwise. */
static uint64_t g_stop_after;

/* M3_SHOT_EVERY=N[,DIR]: write shot_<field>.ppm every N fields. One run of
 * attract mode is some 4000 fields and its 3D runs for 800 of them, so
 * sampling it a field at a time means forty runs to see one sequence. */
static void shots(void)
{
    static long every = -1;
    static char dir[256];
    uint64_t f = model3recomp_frame_count();
    char path[512];

    if (every < 0) {
        const char *e = getenv("M3_SHOT_EVERY");
        const char *c;
        every = e ? strtol(e, NULL, 0) : 0;
        c = e ? strchr(e, ',') : NULL;
        snprintf(dir, sizeof dir, "%s", c ? c + 1 : ".");
    }
    if (every <= 0 || (f % (uint64_t)every) != 0)
        return;
    snprintf(path, sizeof path, "%s/shot_%06llu.ppm", dir,
             (unsigned long long)f);
    model3recomp_screenshot(path);
}

/* M3_RAM_EVERY=N[,DIR]: write the low 2 MB of work RAM -- where the game
 * keeps its variables -- every N fields, for finding them by search. */
static void ram_dumps(void)
{
    static long every = -1;
    static char dir[256];
    uint64_t f = model3recomp_frame_count();
    char path[512];
    FILE *fp;

    if (every < 0) {
        const char *e = getenv("M3_RAM_EVERY");
        const char *c;
        every = e ? strtol(e, NULL, 0) : 0;
        c = e ? strchr(e, ',') : NULL;
        snprintf(dir, sizeof dir, "%s", c ? c + 1 : ".");
    }
    if (every <= 0 || (f % (uint64_t)every) != 0)
        return;
    snprintf(path, sizeof path, "%s/ram_%06llu.bin", dir, (unsigned long long)f);
    if ((fp = fopen(path, "wb")) != NULL) {
        fwrite(bus_ram(), 1, 0x200000u, fp);
        fclose(fp);
    }
}

/* The game's own variables, found by searching RAM dumps (M3_RAM_EVERY)
 * for values that moved exactly when the HUD did, and then confirmed by
 * pinning each candidate:
 *
 *   health   a word per player, 0x5C apart, counting medkits -- three to
 *            start, zero on the continue screen. A HUD copy at 0x1C1260
 *            tracks it too, but pinning that one does not keep you alive.
 *   ammo     a word per player, 4 apart, rounds left in the gun; 0x3C on
 *            is the same player's capacity, so it serves specials too.
 *   credits  one byte for the cabinet. Touching it during the boot wedges
 *            the machine, so it is only ever changed once in the game.
 *   country  a byte in the RAM copy of the EEPROM settings: 0 Japan,
 *            1 USA, 2 export, 3 Australia. The game reads it whenever it
 *            picks text -- subtitles, attract screens, the join prompt --
 *            and rewrites the EEPROM's copy from its region at boot (field
 *            86), so it is set from field 90 on rather than in the EEPROM. */
#define HEALTH_P1   0x001A3720u
#define HEALTH_P2   0x001A377Cu
#define AMMO_P1     0x001A3680u
#define AMMO_P2     0x001A3684u
#define AMMO_CAP    0x3Cu       /* capacity, after the rounds */
#define CREDITS     0x000012D4u
#define COUNTRY     0x00001226u

enum { CHEAT_HEALTH_P1, CHEAT_HEALTH_P2, CHEAT_AMMO_P1, CHEAT_AMMO_P2, CHEAT_CREDITS };
enum { OPT_REGION };
static const char *const k_regions[] = { "Japan (Japanese text)", "USA (English)",
                                         "Export (English)", "Australia (English)" };

static uint32_t peek32(uint32_t a)
{
    const uint8_t *r = bus_ram();
    return ((uint32_t)r[a] << 24) | ((uint32_t)r[a + 1] << 16) |
           ((uint32_t)r[a + 2] << 8) | r[a + 3];
}

static void poke32(uint32_t a, uint32_t v)
{
    uint8_t *r = bus_ram();
    r[a] = (uint8_t)(v >> 24); r[a + 1] = (uint8_t)(v >> 16);
    r[a + 2] = (uint8_t)(v >> 8); r[a + 3] = (uint8_t)v;
}

/* Topped up only while the player is in the game: zero is the continue
 * screen, and writing over that would be a different cheat. */
static void top_up_health(uint32_t addr)
{
    uint32_t v = peek32(addr);
    if (v && v < 3u)
        poke32(addr, 3u);
}

static void refill_ammo(uint32_t addr)
{
    uint32_t cap = peek32(addr + AMMO_CAP);
    if (cap && cap <= 99u && peek32(addr) < cap)
        poke32(addr, cap);
}

/* Cheats and options, from the Debug and Game menus. They poke the game's
 * own RAM once a field, and they come from the netplay host's record, so
 * both machines do the same thing on the same field. */
static void cheats(void)
{
    uint32_t on = m3_input()->cheats;
    uint8_t *r = bus_ram();

    if (model3recomp_frame_count() >= 90u)
        r[COUNTRY] = (uint8_t)m3_option(OPT_REGION);
    if (on & (1u << CHEAT_HEALTH_P1)) top_up_health(HEALTH_P1);
    if (on & (1u << CHEAT_HEALTH_P2)) top_up_health(HEALTH_P2);
    if (on & (1u << CHEAT_AMMO_P1))   refill_ammo(AMMO_P1);
    if (on & (1u << CHEAT_AMMO_P2))   refill_ammo(AMMO_P2);
    /* The counter shows two digits, so "a hundred credits" is 99. */
    if ((on & (1u << CHEAT_CREDITS)) && model3recomp_frame_count() > 1000u)
        r[CREDITS] = 99;
}

static void on_field(void)
{
    shots();
    ram_dumps();
    cheats();
    if (!g_stop_after || model3recomp_frame_count() < g_stop_after)
        return;
    model3recomp_screenshot("shot.ppm");
#ifdef M3_LOOP_GUARD
    m3_fn_report(200);
#endif
    if (getenv("M3_DROPPED")) {
        bus_report_dropped_writes();
        bus_report_device_reads();
    }
    if (getenv("M3_DUMP_SCENE"))
        model3recomp_dump_scene(getenv("M3_DUMP_SCENE"));
    fprintf(stderr, "captured shot.ppm after %llu fields\n",
            (unsigned long long)model3recomp_frame_count());
    exit(0);
}

int main(int argc, char **argv)
{
    m3_config_t cfg;
    size_t n = 0;

    memset(&cfg, 0, sizeof cfg);
    cfg.step  = M3_STEP_1_5;
    cfg.title = "The Lost World: Jurassic Park";

    /*   lostworld [FIELDS] [--host PORT | --join ADDR:PORT] [--delay N]
     *             [--roms DIR] [--env FILE]
     *
     * FIELDS stops after that many and writes shot.ppm. The netplay flags
     * are the same as M3_NETPLAY / M3_NET_DELAY. A relaunch from the menu
     * carries the command line along but has already set those itself. */
    {
        int i, relaunched = getenv("M3_RELAUNCHED") != NULL;
        for (i = 1; i < argc; i++) {
            char spec[256];
            if (!strcmp(argv[i], "--host") && i + 1 < argc) {
                snprintf(spec, sizeof spec, "M3_NETPLAY=host:%s", argv[++i]);
                if (!relaunched) putenv(strdup(spec));
            } else if (!strcmp(argv[i], "--join") && i + 1 < argc) {
                snprintf(spec, sizeof spec, "M3_NETPLAY=join:%s", argv[++i]);
                if (!relaunched) putenv(strdup(spec));
            } else if (!strcmp(argv[i], "--delay") && i + 1 < argc) {
                snprintf(spec, sizeof spec, "M3_NET_DELAY=%s", argv[++i]);
                if (!relaunched) putenv(strdup(spec));
            } else if (!strcmp(argv[i], "--roms") && i + 1 < argc) {
                g_roms_dir = argv[++i];
            } else if (!strcmp(argv[i], "--env") && i + 1 < argc) {
                load_env(argv[++i]);
            } else {
                g_stop_after = strtoull(argv[i], NULL, 0);
            }
        }
    }

    cfg.roms.crom = slurp_rom("lw_prog.bin", &n, 1);
    if (!cfg.roms.crom) {
        fprintf(stderr,
                "No program ROM. Build the images from your own romset:\n"
                "    python tools/build_roms.py /path/to/lostwsga.zip roms/\n");
        return 2;
    }
    cfg.roms.crom_size = n;

    /* The banked CROM carries the bulk of the game and the VROM its models
     * and textures. Without them the game boots and has nothing to show. */
    cfg.roms.crom_bank = slurp_rom("lw_bank.bin", &n, 0);
    if (cfg.roms.crom_bank) cfg.roms.crom_bank_size = n;
    cfg.roms.vrom = slurp_rom("lw_vrom.bin", &n, 0);
    if (cfg.roms.vrom) cfg.roms.vrom_size = n;

    /* The board's own settings: where it keeps them, and the guest address
     * save states are taken at: 0x2100, the branch back to the top of the
     * loop in 0x1EF4 that runs the game's states. It follows the per-field
     * wait (0x118340), the function never returns, so the guest passes it
     * once a field with the same host call stack every time. (The outer loop
     * at 0x1A0C looks like the main loop and is never reached again.) */
    cfg.ini_path     = "lostworld.ini";
    cfg.nvram_path   = "lostworld.nv";
    cfg.state_prefix = "lostworld";
    cfg.safepoint_pc = 0x00002100u;

    if (!model3recomp_init(&cfg)) {
        fprintf(stderr, "model3recomp_init failed\n");
        return 1;
    }
    model3recomp_set_frame_hook(on_field);
    m3_cheat_add(CHEAT_HEALTH_P1, "Infinite health, player 1");
    m3_cheat_add(CHEAT_HEALTH_P2, "Infinite health, player 2");
    m3_cheat_add(CHEAT_AMMO_P1, "Endless ammo, player 1");
    m3_cheat_add(CHEAT_AMMO_P2, "Endless ammo, player 2");
    m3_cheat_add_action(CHEAT_CREDITS, "Add credits (to 99)");
    /* English by default: the only dump is the Japanese board, and the
     * game carries every region's text in it. */
    m3_option_add(OPT_REGION, "&Region", k_regions, 4, 1);

    /* Two halves: the boot code that runs from ROM, and the game the boot
     * copies into RAM. Both are lifted, and both register here. */
    lwrom_register_all();
    lwram_register_all();

    model3recomp_run();
    model3recomp_shutdown();
    return 0;
}

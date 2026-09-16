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

#include "lwrom_funcs.h"
#include "lwram_funcs.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Built by tools/build_roms.py from the romset. */
#define ROM_PROG "roms/lw_prog.bin"
#define ROM_BANK "roms/lw_bank.bin"
#define ROM_VROM "roms/lw_vrom.bin"

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

static void on_field(void)
{
    if (!g_stop_after || model3recomp_frame_count() < g_stop_after)
        return;
    model3recomp_screenshot("shot.ppm");
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

    cfg.roms.crom = slurp(ROM_PROG, &n, 1);
    if (!cfg.roms.crom) {
        fprintf(stderr,
                "No program ROM. Build the images from your own romset:\n"
                "    python tools/build_roms.py /path/to/lostwsga.zip roms/\n");
        return 2;
    }
    cfg.roms.crom_size = n;

    /* The banked CROM carries the bulk of the game and the VROM its models
     * and textures. Without them the game boots and has nothing to show. */
    cfg.roms.crom_bank = slurp(ROM_BANK, &n, 0);
    if (cfg.roms.crom_bank) cfg.roms.crom_bank_size = n;
    cfg.roms.vrom = slurp(ROM_VROM, &n, 0);
    if (cfg.roms.vrom) cfg.roms.vrom_size = n;

    if (argc > 1)
        g_stop_after = strtoull(argv[1], NULL, 0);

    if (!model3recomp_init(&cfg)) {
        fprintf(stderr, "model3recomp_init failed\n");
        return 1;
    }
    if (g_stop_after)
        model3recomp_set_frame_hook(on_field);

    /* Two halves: the boot code that runs from ROM, and the game the boot
     * copies into RAM. Both are lifted, and both register here. */
    lwrom_register_all();
    lwram_register_all();

    model3recomp_run();
    model3recomp_shutdown();
    return 0;
}

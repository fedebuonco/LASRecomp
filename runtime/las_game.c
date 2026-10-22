// LAS Recomp on the shared runtime (nxrt): who the game is, and its hooks into the runtime (runtime/nxgame.h).
// Everything else LAS-specific is in this folder: HLE features (hle/), the randomizer (rando.c), texture restyling
// (nvn/texpatch.c), the menu's pages (ui/).
#include <dirent.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "hle.h"
#include "las_settings.h"
#include "nvn/texpatch.h"
#include "nxgame.h"
#include "rando.h"

// The release tag this build is (also names the release files; Makefile / tools/bundle_macos.sh / make_dmg.sh read
// it): beta_<n> until 1.0, then v<major>.<minor>.<patch>.
#define LAS_VERSION "beta_4"

extern uint64_t renderer_present_count(void);

// ------------------------------------------------------------------ the game's own functions the hooks call
int game_in_play(void);  // hle/warp.c
const char *game_level_name(void);
void warp_frame(uint64_t frame);
void warp_game_tick(Cpu *c);
void give_item_tick(Cpu *c, uint64_t frame);
void chamber_game_tick(Cpu *c, uint64_t frame);
void digs_game_tick(Cpu *c, uint64_t frame);
void qol_tick(void);
void pets_game_tick(Cpu *c);
bool pets_hide_item_cursor(void);
uint64_t pets_menu_input(uint64_t b, int32_t st[4]);
void input_note_rstick(int32_t ry);  // ui/tracker_overlay.mm
bool mirror_input(void);
bool mirror_draw(bool composite);
void save_editor_frame(uint64_t frame);  // ui/save_editor.mm
void save_editor_file_read(const char *path, uint64_t addr, int64_t off, int64_t size);
void save_editor_on_open(void);
void flags_ext_save_read(const char *path, uint64_t addr, int64_t off, int64_t size);
void flags_ext_save_written(const char *path, uint64_t addr, int64_t off, int64_t size);
bool widescreen_allowed(void);
void widescreen_set_factor(float f);
void widescreen_frame_end(uint64_t frame);
float widescreen_effect_stretch(uint64_t fs_hash);
void widescreen_backdrop_seen(uint64_t hash);
void widescreen_scene_ubo(const float *u, uint64_t size);
void widescreen_present_crop(uint64_t frame, double shape, float crop[4], double *vx, double *vy, double *vw, double *vh);
bool pot_level_active(void);
bool pot_glow_active(void);
bool pot_tint(const float *m, float tint[8], uint64_t mesh);
bool pot_instance_gold(const float *m);
void pot_gold_tint(float tint[8]);
void pot_item_draw(float *m);
void pot_probe(const float *m, bool tintable, uint64_t mesh, uint32_t inst);
void escape_bottle_tint(int kind, const float *ui, uint32_t ui_size, float tint[8]);

// ui/las_menu.mm
bool las_tab_save_editor(float scale);
bool las_tab_rando(float scale);
bool las_tab_warp(float scale);
bool las_tab_cheats(float scale);
bool las_tab_gameplay(float scale);
bool las_tab_cosmetics(float scale);
bool las_overlay_wanted(void);
void las_overlay_draw(float w, float h, void *font);

// ------------------------------------------------------------------ per frame
static void game_tick(Cpu *c, uint64_t frame) {
    warp_game_tick(c);  // apply a queued warp
    give_item_tick(c, frame);
    chamber_game_tick(c, frame);
    digs_game_tick(c, frame);
    qol_tick();
    pets_game_tick(c);
}

static void present_tick(uint64_t frame) {
    warp_frame(frame);
    save_editor_frame(frame);
}

static bool in_play(void) { return game_in_play() != 0; }
static bool movie_skippable(const char *path) { return strstr(path, "movie/opening.webm") != NULL; }

// levels that never draw their surroundings (the phone booth): render targets start black there
static bool scene_fresh_targets(const char *scene) { return !strncmp(scene, "TelephoneBox", 12); }

// ------------------------------------------------------------------ input
// test hooks steering the pad (hle/doorjump.c NXRT_DOORJUMP_TEST, NXRT_PLACE action scripts)
extern int32_t g_test_lx, g_test_ly;
extern uint64_t g_test_stick_until, g_test_buttons, g_test_buttons_until;
static void pad_script(uint64_t frame, NxPad *p) {
    if (frame < g_test_stick_until) p->lx = g_test_lx, p->ly = g_test_ly;
    if (frame < g_test_buttons_until) p->buttons |= g_test_buttons;
}

// door jumps stop Link for a moment before moving him (hle/doorjump.c): stick + d-pad released
extern int32_t g_stick_lx, g_stick_ly;  // (what the player holds, before that: doorjump.c reads it)
extern uint64_t g_stick_freeze_until;
static void pad_filter(NxPad *p) {
    g_stick_lx = p->lx, g_stick_ly = p->ly;
    if (renderer_present_count() < g_stick_freeze_until) {
        p->lx = p->ly = 0;
        p->buttons &= ~(0xfull << 12);
    }
}

// the pause menu's companion slot (hle/pets.c) takes the d-pad / sticks / A while the cursor is on it
static void pad_final(NxPad *p) {
    int32_t st[4] = {p->lx, p->ly, p->rx, p->ry};
    p->buttons = pets_menu_input(p->buttons, st);
    p->lx = st[0], p->ly = st[1], p->rx = st[2], p->ry = st[3];
    input_note_rstick(p->ry);
}

// ------------------------------------------------------------------ files
static void save_read(const char *path, uint64_t addr, int64_t off, int64_t size) {
    save_editor_file_read(path, addr, off, size);
    flags_ext_save_read(path, addr, off, size);
}

// ------------------------------------------------------------------ draws
static bool draw_shader(NxDraw *d) {
    if (d->fs_hash == 0xa2177dbb72197bd9ull && pot_level_active()) return false;  // item twinkle (hle/pot_glow.c)
    // interiors / dungeons draw a 16:9 backdrop quad over the still-rendered overworld: frames that draw it are
    // presented cropped to 16:9 (hle/widescreen.c)
    if (d->fs_hash) widescreen_backdrop_seen(d->fs_hash);
    if (d->ubo[5]) widescreen_scene_ubo(d->ubo[5], d->ubo_size[5]);  // (the camera's view-projection)
    return true;
}

static bool draw_textures(NxDraw *d) {
    bool cursor = false;
    for (int i = 0; i < d->ntex; i++) cursor |= texpatch_is_item_cursor(d->tex_ids[i]);
    if (cursor && pets_hide_item_cursor()) return false;  // (the cursor is on the companion card: hle/pets.c)
    const float *m = d->ubo[2];
    if (m && d->ubo_size[2] >= 48 && !((uintptr_t)m & 3)) pot_item_draw((float *)m);  // cut bushes' items
    if (m && d->ubo_size[2] >= 48 && ENV("NXRT_POT_PROBE")) pot_probe(m, d->tintable, d->mesh, d->instances);
    return true;
}

static void draw_tint(const NxDraw *d, float tint[8]) {
    if (d->ubo[2] && d->ubo_size[2] >= 48 && pot_glow_active()) pot_tint(d->ubo[2], tint, d->mesh);  // golden pots
    int bottle = 0, heart = 0;
    for (int i = 0; i < d->ntex; i++) {
        if (!bottle) bottle = texpatch_bottle_kind(d->tex_ids[i]);
        if (!heart) heart = texpatch_heart_kind(d->tex_ids[i]);
    }
    if (bottle) {  // the gold Escape Bottle (hle/escape.c): its item icon / held-up model
        int u = bottle == 3 ? 3 : 0;  // (the fairy model: its bone matrices, for where it stands - souls.c)
        escape_bottle_tint(bottle, d->ubo[u], d->ubo_size[u], tint);
    }
    if (heart) texpatch_heart_tint(heart, tint);  // heart colour (Cosmetics tab)
}

// bushes are drawn in batches: the ones that are still checks are gold (bushsanity)
static bool instance_tint(const float *m, float tint[8]) {
    if (!pot_instance_gold(m)) return false;
    pot_gold_tint(tint);
    return true;
}

// ------------------------------------------------------------------ boot profiles: randomizer seeds (rando/<name>/)
static int profiles_list(char names[][128], int max) {
    int n = 0;
    DIR *d = opendir("rando");
    if (d) {
        struct dirent *e;
        while ((e = readdir(d)) && n < max) {
            if (e->d_name[0] == '.') continue;
            char p[600];
            struct stat st;
            snprintf(p, sizeof p, "rando/%s/settings.json", e->d_name);
            if (stat(p, &st) != 0) continue;
            snprintf(p, sizeof p, "rando/%s/romfs", e->d_name);
            if (stat(p, &st) != 0) continue;
            strlcpy(names[n++], e->d_name, 128);
        }
        closedir(d);
    }
    qsort(names, n, 128, (int (*)(const void *, const void *))strcmp);
    return n;
}
static const char *profile_get(void) { return g_las.rando_seed; }
static void profile_set(const char *seed) {
    strlcpy(g_las.rando_seed, seed, sizeof g_las.rando_seed);
    unsetenv("NXRT_RANDO_SEED");  // (a test's seed was for the first boot only)
}

// ------------------------------------------------------------------ the game
// The pages of the game's tab ("Link's Awakening"), in order (ids as before the split: test hooks select them by id)
static const NxMenuTab menu_pages[] = {
    {"Gameplay", 6, false, false, las_tab_gameplay},
    {"Cosmetics", 7, false, false, las_tab_cosmetics},
    {"Randomizer", 3, false, false, las_tab_rando},
    {"Cheats", 5, true, false, las_tab_cheats},
    {"Warp", 4, true, true, las_tab_warp},
    {"Save Editor", 2, true, false, las_tab_save_editor},
};

const NxGame nx_game = {
    .info = {
        .id = "las",
        .exe = "lasrecomp",
        .title = "LAS Recomp",
        .game_name = "The Legend of Zelda: Link's Awakening",
        .game_version = "1.0.0",
        .title_id = 0x01006BB00C6F0000ull,
        .build_id = {0xae, 0x16, 0xf7, 0x1e, 0x00, 0x2a, 0xf8, 0xcb},
        .bundle_id = "com.peiikai.lasrecomp",
        .data_dir = "lasrecomp",
        .ini = "larecomp.ini",
        .version = LAS_VERSION,
        .releases_api = "https://api.github.com/repos/peiikai/LASRecomp/releases?per_page=100",
        .release_page = "https://github.com/peiikai/LASRecomp/releases/tag/",
        .headline = "LAS Recompiled",
        .romfs_probe = "region_common",
        .title_movie = "region_common/movie/title.webm",
        .nickname = "Link",
        .menu_name = "Link's Awakening",
    },
    .hooks = {
        .modules_loaded = rando_boot,
        .code_variant = rando_word,
        .game_tick = game_tick,
        .present_tick = present_tick,
        .in_play = in_play,
        .movie_skippable = movie_skippable,
        .scene_name = game_level_name,
        .scene_fresh_targets = scene_fresh_targets,
        .pad_script = pad_script,
        .pad_mirrored = mirror_input,
        .pad_filter = pad_filter,
        .pad_final = pad_final,
        .content_dir = rando_seed_dir,
        .romfs_override = rando_romfs_override,
        .save_read = save_read,
        .save_written = flags_ext_save_written,
        .settings_defaults = las_settings_defaults,
        .settings_key = las_settings_key,
        .settings_write = las_settings_write,
        .tex_identify = texpatch_identify,
        .tex_generation = texpatch_generation,
        .tex_apply = texpatch_apply,
        .draw_shader = draw_shader,
        .draw_textures = draw_textures,
        .draw_effect_stretch = widescreen_effect_stretch,
        .draw_mirror = mirror_draw,
        .draw_tint = draw_tint,
        .box_depth_draws = true,  // (LAS's HUD draws depth-tested into the window image)
        .instances_tinted = pot_glow_active,
        .instance_tint = instance_tint,
        .present_wide = widescreen_allowed,
        .present_crop = widescreen_present_crop,
        .present_shape = widescreen_set_factor,
        .present_frame_end = widescreen_frame_end,
        .menu_pages = menu_pages,
        .menu_npages = sizeof menu_pages / sizeof *menu_pages,
        .menu_opened = save_editor_on_open,
        .overlay_wanted = las_overlay_wanted,
        .overlay_draw = las_overlay_draw,
        .profiles_list = profiles_list,
        .profile_get = profile_get,
        .profile_set = profile_set,
        .profile_label = "Rando",
    },
};

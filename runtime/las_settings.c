#include "las_settings.h"

#include <stdlib.h>
#include <string.h>

LasSettings g_las;

void las_settings_defaults(void) {
    memset(&g_las, 0, sizeof g_las);
    g_las.inv_keys = true;
    g_las.tracker = true;
    g_las.chaos_letters = CHAOS_LETTERS_MAP;
    g_las.diff_damage = 1.0f;
}

bool las_settings_key(const char *key, const char *val) {
    if (!strcmp(key, "rando_seed")) strlcpy(g_las.rando_seed, val, sizeof g_las.rando_seed);
    else if (!strcmp(key, "inv_keys")) g_las.inv_keys = atoi(val) != 0;
    else if (!strcmp(key, "move_360")) g_las.move_360 = atoi(val) != 0;
    else if (!strcmp(key, "blur_removal")) g_las.blur_removal = atoi(val) != 0;
    else if (!strcmp(key, "hide_hero_icon")) g_las.hide_hero_icon = atoi(val) != 0;
    else if (!strcmp(key, "mirror_mode")) g_las.mirror_mode = atoi(val) != 0;
    else if (!strcmp(key, "tracker")) g_las.tracker = atoi(val) != 0;
    else if (!strcmp(key, "chaos_letters"))
        g_las.chaos_letters = !strcmp(val, "off") ? CHAOS_LETTERS_OFF : !strcmp(val, "explored") ? CHAOS_LETTERS_EXPLORED : CHAOS_LETTERS_MAP;
    else if (!strcmp(key, "cheat_hearts")) g_las.cheat_hearts = atoi(val) != 0;
    else if (!strcmp(key, "cheat_ammo")) g_las.cheat_ammo = atoi(val) != 0;
    else if (!strcmp(key, "cheat_rupees")) g_las.cheat_rupees = atoi(val) != 0;
    else if (!strcmp(key, "cheat_hookshot")) g_las.cheat_hookshot = atoi(val) != 0;
    else if (!strcmp(key, "no_buff_drops")) g_las.no_buff_drops = atoi(val) != 0;
    else if (!strcmp(key, "diff_damage")) g_las.diff_damage = (float)atof(val);
    else if (!strcmp(key, "diff_ohko")) g_las.diff_ohko = atoi(val) != 0;
    else if (!strcmp(key, "diff_hero")) g_las.diff_hero = atoi(val) != 0;
    else if (!strcmp(key, "tunic")) { for (int i = 0; i < N_TUNIC_COLOURS; i++) if (!strcmp(val, g_tunic_colours[i])) g_las.tunic = i; }
    else if (!strcmp(key, "hearts")) { for (int i = 0; i < N_HEART_COLOURS; i++) if (!strcmp(val, g_heart_colours[i])) g_las.hearts = i; }
    else return false;
    return true;
}

void las_settings_write(FILE *f) {
    fprintf(f, "inv_keys = %d\n", g_las.inv_keys);
    fprintf(f, "move_360 = %d\nblur_removal = %d\nhide_hero_icon = %d\n", g_las.move_360, g_las.blur_removal,
            g_las.hide_hero_icon);
    fprintf(f, "mirror_mode = %d\n", g_las.mirror_mode);
    fprintf(f, "tracker = %d\n", g_las.tracker);
    static const char *const letters[] = {"off", "explored", "map"};
    fprintf(f, "chaos_letters = %s\n", letters[g_las.chaos_letters]);
    fprintf(f, "cheat_hearts = %d\ncheat_ammo = %d\ncheat_rupees = %d\n", g_las.cheat_hearts, g_las.cheat_ammo,
            g_las.cheat_rupees);
    fprintf(f, "cheat_hookshot = %d\n", g_las.cheat_hookshot);
    fprintf(f, "no_buff_drops = %d\n", g_las.no_buff_drops);
    fprintf(f, "diff_damage = %.2f\ndiff_ohko = %d\ndiff_hero = %d\n", g_las.diff_damage, g_las.diff_ohko, g_las.diff_hero);
    fprintf(f, "tunic = %s\nhearts = %s\n", g_tunic_colours[g_las.tunic], g_heart_colours[g_las.hearts]);
    fprintf(f, "rando_seed = %s\n", g_las.rando_seed);
}

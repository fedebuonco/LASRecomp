// LAS Recomp's pages in the menu's game tab ("Link's Awakening": nx-ui menu.mm) and its HUD overlays: the nx_game
// hooks menu_pages / overlay_* (runtime/las_game.c). All of the game's options are on these pages.
#include <stdlib.h>

#include "imgui.h"
#include "option_rows.h"

extern "C" {
#include "../las_settings.h"
int game_in_play(void);  // hle/warp.c
void save_editor_draw(float scale);  // save_editor.mm
void rando_tab_draw(float scale);    // rando_tab.mm
bool warp_tab_draw(float scale);     // warp_tab.mm: true when a warp started
void cheats_draw(float scale);       // cheats.mm
void difficulty_draw(float scale);   // difficulty.mm
bool inventory_overlay_wanted(void);
void inventory_overlay_draw(ImVec2 display, ImFont *font);
bool tracker_overlay_wanted(void);
void tracker_overlay_draw(ImVec2 display, ImFont *font);
bool chaos_overlay_wanted(void);
void chaos_overlay_draw(ImVec2 display, ImFont *font);
}

// ------------------------------------------------------------------ pages (true: close the menu)
extern "C" bool las_tab_save_editor(float scale) {
    if (game_in_play()) save_editor_draw(scale);
    else ImGui::TextDisabled("Load a save file and get into the game to edit it.");
    return false;
}
extern "C" bool las_tab_rando(float scale) { rando_tab_draw(scale); return false; }
extern "C" bool las_tab_warp(float scale) { return warp_tab_draw(scale); }
extern "C" bool las_tab_cheats(float scale) { cheats_draw(scale); return false; }
extern "C" bool las_tab_gameplay(float scale) { difficulty_draw(scale); return false; }

// Cosmetics: tunic / heart colours, live (runtime/nvn/texpatch.c); never part of a seed
extern "C" bool las_tab_cosmetics(float) {
    opt_begin("##cosmetics", ImGui::GetFontSize() / 34);
    opt_section("Colors");
    opt_choice("Tunic color", &g_las.tunic, g_tunic_colours, N_TUNIC_COLOURS, 0,
               "Link's tunic color. Default: the game's own (or the randomizer seed's).");
    opt_choice("Heart color", &g_las.hearts, g_heart_colours, N_HEART_COLOURS, 0,
               "The color of the hearts on the HUD.");
    if (opt_action("Random colors", "", "Picks a random tunic and heart color (any other color, Default included).")) {
        g_las.tunic = (g_las.tunic + 1 + rand() % (N_TUNIC_COLOURS - 1)) % N_TUNIC_COLOURS;
        g_las.hearts = (g_las.hearts + 1 + rand() % (N_HEART_COLOURS - 1)) % N_HEART_COLOURS;
    }
    opt_end();
    return false;
}

// ------------------------------------------------------------------ HUD overlays (menu closed)
extern "C" bool las_overlay_wanted(void) {
    return inventory_overlay_wanted() || tracker_overlay_wanted() || chaos_overlay_wanted();
}
extern "C" void las_overlay_draw(float w, float h, void *font) {
    ImVec2 display(w, h);
    ImFont *f = (ImFont *)font;
    if (tracker_overlay_wanted()) tracker_overlay_draw(display, f);
    if (chaos_overlay_wanted()) chaos_overlay_draw(display, f);
    if (inventory_overlay_wanted()) inventory_overlay_draw(display, f);
}

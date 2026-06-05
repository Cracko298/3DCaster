#include "common.h"

int main(int argc, char **argv) {
    (void)argc;
    (void)argv;

    gfxInitDefault();
    gfxSetDoubleBuffering(GFX_TOP, true);
    gfxSetDoubleBuffering(GFX_BOTTOM, true);

    Result fs_rc = fsInit();
    g_fs_ready = R_SUCCEEDED(fs_rc);

    g_is_new3ds = false;
    APT_CheckNew3DS(&g_is_new3ds);

    bool settings_loaded = load_app_settings();
    snprintf(g_status, sizeof(g_status), "%s %s%s",
             g_fs_ready ? "READY" : "FS INIT FAIL",
             g_is_new3ds ? "N3DS" : "O3DS",
             settings_loaded ? " CFG" : "");

    new_open_level(&g_level);
    refresh_all_slots();
    refresh_preview_slot();

    u64 last_tick = svcGetSystemTick();
    const float tick_rate = (float)SYSCLOCK_ARM11;

    while (aptMainLoop()) {
        hidScanInput();
        u32 kDown = hidKeysDown();
        u32 kHeld = hidKeysHeld();

        if (g_in_menu && (kDown & KEY_START)) break;

        u64 now = svcGetSystemTick();
        float dt = (float)(now - last_tick) / tick_rate;
        last_tick = now;
        if (dt <= 0.0f || dt > 0.05f) dt = 1.0f / 60.0f;
        g_frame_dt = dt;
        float inst_fps = dt > 0.0001f ? (1.0f / dt) : 60.0f;
        g_fps_smooth += (inst_fps - g_fps_smooth) * 0.08f;

        if (g_in_menu) {
            g_preview_spin += dt * 0.75f;
            if (g_preview_spin >= TWO_PI_F) g_preview_spin -= TWO_PI_F;
            handle_world_menu_input(kDown);
            render_world_menu();
        } else {
            handle_global_input(kDown, kHeld);

            if (!g_in_menu) {
                if (g_edit_mode) {
                    handle_editor_input(dt, kDown, kHeld);
                } else {
                    update_physics_and_movement(dt, kDown, kHeld);
                }

                render_raycast(&g_level);
                render_bottom_map();
            } else {
                render_world_menu();
            }
        }

        gfxFlushBuffers();
        gfxSwapBuffers();
        gspWaitForVBlank();
    }

    if (g_fs_ready) fsExit();
    gfxExit();
    return 0;
}

#include <stdlib.h>

#ifdef TARGET_WEB
#include <emscripten.h>
#include <emscripten/html5.h>
#endif

#include "sm64.h"

#include "game/memory.h"
#include "audio/external.h"

#include "gfx/gfx_pc.h"
#include "gfx/gfx_opengl.h"
#include "gfx/gfx_direct3d11.h"
#include "gfx/gfx_direct3d12.h"
#include "gfx/gfx_dxgi.h"
#include "gfx/gfx_gx2.h"
#include "gfx/gfx_glx.h"
#include "gfx/gfx_sdl.h"
#include "gfx/gfx_dummy.h"

#include "audio/audio_api.h"
#include "audio/audio_wasapi.h"
#include "audio/audio_pulse.h"
#include "audio/audio_alsa.h"
#include "audio/audio_sdl.h"
#include "audio/audio_null.h"

#include "controller/controller_keyboard.h"
#include "djui/djui.h"
#include "djui/djui_ctx_display.h"
#include "djui/djui_fps_display.h"
#include "djui/djui_lua_profiler.h"
#include "lua/smlua.h"
#include "mods/mods.h"

#include "configfile.h"
#include "platform.h"
#include "fs/fs.h"
#include "pc_diag.h"

#include "compat.h"
#include "pc_main.h"

#ifdef TARGET_WII_U
#include <whb/log.h>
#endif

OSMesg gMainReceivedMesg;
OSMesgQueue gSIEventMesgQueue;

s8 gResetTimer;
s8 gNmiResetBarsTimer;
s8 gDebugLevelSelect;
s8 gShowProfiler;
s8 gShowDebugText;
u8 gRenderingInterpolated = 0;
f32 gMasterVolume = 1.0f;
u8 gLuaVolumeMaster = 127;
u8 gLuaVolumeLevel = 127;
u8 gLuaVolumeSfx = 127;
u8 gLuaVolumeEnv = 127;

static struct AudioAPI *audio_api;
struct GfxWindowManagerAPI *wm_api;
static struct GfxRenderingAPI *rendering_api;
static uint32_t sFrameMarkerCount = 0;
static double sFpsWindowStart = 0.0;
static u32 sFpsFrameCount = 0;

extern void gfx_run(Gfx *commands);
extern void thread5_game_loop(void *arg);
extern void create_next_audio_buffer(s16 *samples, u32 num_samples);
void game_loop_one_iteration(void);

void dispatch_audio_sptask(UNUSED struct SPTask *spTask) {
}

void set_vblank_handler(UNUSED s32 index, UNUSED struct VblankHandler *handler, UNUSED OSMesgQueue *queue, UNUSED OSMesg *msg) {
}

static uint8_t inited = 0;

#include "game/game_init.h" // for gGlobalTimer
void exec_display_list(struct SPTask *spTask) {
    if (!inited) {
        return;
    }
    gfx_run((Gfx *)spTask->task.t.data_ptr);
}

#define printf

#ifdef VERSION_EU
#define SAMPLES_HIGH 656
#define SAMPLES_LOW 640
#else
#define SAMPLES_HIGH 544
#define SAMPLES_LOW 528
#endif

void produce_one_frame(void) {
    pc_diag_mark_stage("produce_one_frame:begin");
    gfx_start_frame();
    if (configWindow.settings_changed) {
        configWindow.settings_changed = false;
        if (wm_api != NULL && wm_api->set_fullscreen != NULL) {
            wm_api->set_fullscreen(configWindow.fullscreen);
        }
        configfile_save();
    }
#ifdef TARGET_WII_U
    if (sFrameMarkerCount == 0) {
        WHBLogPrint("pc: first frame");
    }
    sFrameMarkerCount++;
#endif
    pc_diag_mark_frame(sFrameMarkerCount);
#ifdef TARGET_WII_U
    if (sFrameMarkerCount == 1) {
        WHBLogPrint("pc: frame1 pre game_loop_one_iteration");
    }
#endif
    pc_diag_mark_stage("produce_one_frame:before_game_loop");
    game_loop_one_iteration();
    pc_diag_mark_stage("produce_one_frame:after_game_loop");
#ifdef TARGET_WII_U
    if (sFrameMarkerCount == 1) {
        WHBLogPrint("pc: frame1 post game_loop_one_iteration");
    }
#endif
    smlua_update();
    pc_diag_mark_stage("produce_one_frame:after_smlua_update");
#ifdef TARGET_WII_U
    if (sFrameMarkerCount == 1) {
        WHBLogPrint("pc: frame1 post smlua_update");
    }
#endif

    bool has_focus = true;
    bool should_mute = false;
    int samples_left = audio_api->buffered();
    u32 num_audio_samples = samples_left < audio_api->get_desired_buffered() ? SAMPLES_HIGH : SAMPLES_LOW;
    //printf("Audio samples: %d %u\n", samples_left, num_audio_samples);
    s16 audio_buffer[SAMPLES_HIGH * 2 * 2];
    for (int i = 0; i < 2; i++) {
        /*if (audio_cnt-- == 0) {
            audio_cnt = 2;
        }
        u32 num_audio_samples = audio_cnt < 2 ? 528 : 544;*/
        create_next_audio_buffer(audio_buffer + i * (num_audio_samples * 2), num_audio_samples);
    }
    if (wm_api != NULL && wm_api->has_focus != NULL) {
        has_focus = wm_api->has_focus();
    }

    gMasterVolume = ((f32)configMasterVolume / 127.0f) * ((f32)gLuaVolumeMaster / 127.0f);
    should_mute = (configMuteFocusLoss && !has_focus) || (gMasterVolume <= 0.0f);

    if (!should_mute) {
        set_sequence_player_volume(SEQ_PLAYER_LEVEL, ((f32)configMusicVolume / 127.0f) * ((f32)gLuaVolumeLevel / 127.0f));
        set_sequence_player_volume(SEQ_PLAYER_SFX,   ((f32)configSfxVolume   / 127.0f) * ((f32)gLuaVolumeSfx   / 127.0f));
        set_sequence_player_volume(SEQ_PLAYER_ENV,   ((f32)configEnvVolume   / 127.0f) * ((f32)gLuaVolumeEnv   / 127.0f));

        // Apply master gain at the final mixed buffer stage.
        for (u32 i = 0; i < (4 * num_audio_samples); i++) {
            audio_buffer[i] = (s16)((f32)audio_buffer[i] * gMasterVolume);
        }

        //printf("Audio samples before submitting: %d\n", audio_api->buffered());
        audio_api->play((u8 *)audio_buffer, 2 * num_audio_samples * 4);
    }
    pc_diag_mark_stage("produce_one_frame:after_audio_play");
#ifdef TARGET_WII_U
    if (sFrameMarkerCount == 1) {
        WHBLogPrint("pc: frame1 post audio play");
    }
#endif

    gfx_end_frame();
    pc_diag_mark_stage("produce_one_frame:after_gfx_end_frame");
#ifdef TARGET_WII_U
    if (sFrameMarkerCount == 1) {
        WHBLogPrint("pc: frame1 post gfx_end_frame");
    }
#endif

    if (wm_api != NULL && wm_api->get_time != NULL) {
        double now = wm_api->get_time();
        if (sFpsWindowStart <= 0.0) {
            sFpsWindowStart = now;
            sFpsFrameCount = 0;
        }
        sFpsFrameCount++;
        if (now >= sFpsWindowStart + 1.0) {
            double elapsed = now - sFpsWindowStart;
            if (elapsed > 0.0) {
                u32 fps = (u32)(((double)sFpsFrameCount / elapsed) + 0.5);
                djui_fps_display_update(fps);
            }
            sFpsWindowStart = now;
            sFpsFrameCount = 0;
        }
    }
}

#ifdef TARGET_WEB
static void em_main_loop(void) {
}

static void request_anim_frame(void (*func)(double time)) {
    EM_ASM(requestAnimationFrame(function(time) {
        dynCall("vd", $0, [time]);
    }), func);
}

static void on_anim_frame(double time) {
    static double target_time;

    time *= 0.03; // milliseconds to frame count (33.333 ms -> 1)

    if (time >= target_time + 10.0) {
        // We are lagging 10 frames behind, probably due to coming back after inactivity,
        // so reset, with a small margin to avoid potential jitter later.
        target_time = time - 0.010;
    }

    for (int i = 0; i < 2; i++) {
        // If refresh rate is 15 Hz or something we might need to generate two frames
        if (time >= target_time) {
            produce_one_frame();
            target_time = target_time + 1.0;
        }
    }

    request_anim_frame(on_anim_frame);
}
#endif

static void save_config(void) {
    configfile_save();
}

// Ensures Lua/mod state is torn down during normal process shutdown.
static void shutdown_mod_runtime(void) {
    djui_shutdown();
    smlua_shutdown();
    mods_shutdown();
}

static void on_fullscreen_changed(bool is_now_fullscreen) {
    configFullscreen = is_now_fullscreen;
}

void main_func(void) {
#ifdef USE_SYSTEM_MALLOC
    main_pool_init();
    gGfxAllocOnlyPool = alloc_only_pool_init();
#else
    static u8 pool[DOUBLE_SIZE_ON_64_BIT(0x165000)] __attribute__ ((aligned(64)));
    main_pool_init(pool, pool + sizeof(pool));
#endif
    gEffectsMemoryPool = mem_pool_init(0x4000, MEMORY_POOL_LEFT);

    // Initialize write path before config/save I/O so Wii U writes stay on SD.
    fs_init(sys_user_path());
    configfile_load();
    gMasterVolume = (f32)configMasterVolume / 127.0f;
    atexit(save_config);
    atexit(shutdown_mod_runtime);

#ifdef TARGET_WEB
    emscripten_set_main_loop(em_main_loop, 0, 0);
    request_anim_frame(on_anim_frame);
#endif

#if defined(TARGET_WII_U)
    save_config(); // Mount SD write now

    rendering_api = &gfx_gx2_api;
    wm_api = &gfx_gx2_window;
    configFullscreen = true;
#elif defined(ENABLE_DX12)
    rendering_api = &gfx_direct3d12_api;
    wm_api = &gfx_dxgi_api;
#elif defined(ENABLE_DX11)
    rendering_api = &gfx_direct3d11_api;
    wm_api = &gfx_dxgi_api;
#elif defined(ENABLE_OPENGL)
    rendering_api = &gfx_opengl_api;
    #if defined(__linux__) || defined(__BSD__)
        wm_api = &gfx_glx;
    #else
        wm_api = &gfx_sdl;
    #endif
#elif defined(ENABLE_GFX_DUMMY)
    rendering_api = &gfx_dummy_renderer_api;
    wm_api = &gfx_dummy_wm_api;
#endif

    gfx_init(wm_api, rendering_api, "Super Mario 64 PC-Port", configFullscreen);

    wm_api->set_fullscreen_changed_callback(on_fullscreen_changed);
    wm_api->set_keyboard_callbacks(keyboard_on_key_down, keyboard_on_key_up, keyboard_on_all_keys_up, NULL, NULL);

    pc_diag_watchdog_init();
    pc_diag_mark_stage("main:after_watchdog_init");


#if HAVE_WASAPI
    if (audio_api == NULL && audio_wasapi.init()) {
        audio_api = &audio_wasapi;
    }
#endif
#if HAVE_PULSE_AUDIO
    if (audio_api == NULL && audio_pulse.init()) {
        audio_api = &audio_pulse;
    }
#endif
#if HAVE_ALSA
    if (audio_api == NULL && audio_alsa.init()) {
        audio_api = &audio_alsa;
    }
#endif
#if defined(TARGET_WEB) || defined(TARGET_WII_U)
    if (audio_api == NULL && audio_sdl.init()) {
        audio_api = &audio_sdl;
    }
#endif
    if (audio_api == NULL) {
        audio_api = &audio_null;
    }

    audio_init();
    sound_init();

    thread5_game_loop(NULL);
    // Load Lua scripts only after core game/audio systems are initialized.
#ifdef TARGET_WII_U
    WHBLogPrint("pc: mods init begin");
#endif
    mods_init();
#ifdef TARGET_WII_U
    WHBLogPrint("pc: lua init begin");
#endif
    smlua_init();
    // Restore persisted host-mod selections after Lua startup so boot remains modless;
    // selected mods are applied when the user starts hosting.
    enable_queued_mods();
#ifdef TARGET_WII_U
    WHBLogPrint("pc: djui init begin");
#endif
    djui_init();
    djui_init_late();
#ifdef TARGET_WII_U
    WHBLogPrint("pc: entering main loop");
#endif
#ifdef TARGET_WEB
    /*for (int i = 0; i < atoi(argv[1]); i++) {
        game_loop_one_iteration();
    }*/
    inited = 1;
#else
    inited = 1;
    while (1) {
        wm_api->main_loop(produce_one_frame);
#ifdef DEVELOPMENT
        djui_ctx_display_update();
#endif
        djui_lua_profiler_update();
    }
#endif
}

#if defined(_WIN32) || defined(_WIN64)
#include <windows.h>
int WINAPI WinMain(UNUSED HINSTANCE hInstance, UNUSED HINSTANCE hPrevInstance, UNUSED LPSTR pCmdLine, UNUSED int nCmdShow) {
    main_func();
    return 0;
}
#else
int main(UNUSED int argc, UNUSED char *argv[]) {
    main_func();
    return 0;
}
#endif

void produce_one_dummy_frame(void (*callback)(), UNUSED u8 clearColorR, UNUSED u8 clearColorG, UNUSED u8 clearColorB) {
    if (callback != NULL) {
        callback();
    }
}

void game_deinit(void) {
}

void game_exit(void) {
#ifdef TARGET_WII_U
    WHBLogPrint("pc: game_exit requested");
#endif
}

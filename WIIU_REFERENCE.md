# Wii U Platform API Reference

Low-level reference for Wii U homebrew APIs. Consult when writing new platform-level code (graphics, audio, input, networking). Not needed for Lua/mod compatibility work.

---

## Hardware constraints that affect code

### Big-endian architecture (Espresso CPU)

PowerPC 750CL, tri-core, 1.24 GHz. **Big-endian byte order.** MSB at lowest address.

Standard file formats and network protocols are often little-endian. Byte-swap anything multi-byte crossing that boundary:

| Type | Swap? | Function |
|------|-------|----------|
| `uint16_t` | Yes | `__builtin_bswap16(x)` |
| `uint32_t` | Yes | `__builtin_bswap32(x)` |
| `float` | Yes | Manual via reinterpret cast or union |
| `char` / `uint8_t` | No | Direct |

### Memory layout

| Pool | Size | Use |
|------|------|-----|
| MEM1 | 32 MB | High-speed, backward compat, system-critical |
| MEM2 | 1 GB usable | Primary app memory (other 1 GB is OS-reserved) |
| eDRAM | 32 MB | ~140 GB/s bandwidth, ideal for render targets |

### CPU–GPU cache coherency

CPU and GPU do **not** share coherent cache. CPU writes sit in L2; GPU reads from physical RAM. Without explicit management, GPU sees stale data.

```c
// Flush CPU cache to RAM (before GPU reads CPU-written data)
DCFlushRange(void* addr, size_t size);

// Invalidate CPU cache (before CPU reads GPU-written data)
DCInvalidateRange(void* addr, size_t size);
```

This is the single most common source of rendering bugs on this platform.

### GPU (Latte)

AMD R700/TeraScale 2 derivative. 320 stream processors at 550 MHz (~352 GFLOPS). Similar to PC Radeon R7xx.

---

## Build system and packaging

### CMake with WUT

Use the toolchain file at `/opt/devkitpro/cmake/WiiU.cmake`:

```cmake
cmake_minimum_required(VERSION 3.7)
project(myapp CXX C)

include("${DEVKITPRO}/wut/share/wut.cmake" REQUIRED)

add_executable(myapp source/main.cpp)

target_link_libraries(myapp
    wut::coreinit
    wut::vpad
    wut::gx2
    wut::proc_ui
    wut::sysapp
)

wut_create_rpx(myapp)
wut_create_wuhb(myapp
    NAME "My Application"
    SHORTNAME "MyApp"
    AUTHOR "Developer"
    ICON "${CMAKE_CURRENT_SOURCE_DIR}/icon.png"
)
```

Build:
```bash
mkdir build && cd build
/opt/devkitpro/portlibs/wiiu/bin/powerpc-eabi-cmake ../
make
```

### wuhbtool arguments

| CMake arg | wuhbtool flag | Description | Constraint |
|-----------|---------------|-------------|------------|
| NAME | --name | Display title on Wii U Menu | UTF-8 |
| AUTHOR | --author | Developer attribution | UTF-8 |
| ICON | --icon | Menu icon | 128×128 PNG |
| TVSPLASH | --tv-image | Boot splash for TV | 1280×720 PNG |
| DRCSPLASH | --drc-image | Boot splash for GamePad | 854×480 PNG |
| CONTENT | --content | Directory to virtualize | Valid path |
| SHORTNAME | --short-name | Abbreviated title | < 64 chars |

Command-line packaging:
```bash
wuhbtool myapp.rpx myapp.wuhb \
  --name="My Application" \
  --short-name="MyApp" \
  --author="Developer" \
  --icon=icon.png \
  --tv-image=splash_tv.png \
  --drc-image=splash_drc.png \
  --content=./content
```

### The CONTENT directory

The `--content` argument maps a local directory to `/vol/content` inside the running app. Standard filesystem calls (`fopen`, `fread`) access bundled assets without hardcoding SD paths. Up to 4 GB of bundled data.

Deploy: place `.wuhb` in `sd:/wiiu/apps/`.

---

## Core WUT libraries

| Library | Header | What it covers |
|---------|--------|----------------|
| coreinit | `<coreinit/*.h>` | Threads, heaps, filesystem, time, atomics |
| gx2 | `<gx2/*.h>` | Graphics (Latte GPU) |
| vpad | `<vpad/input.h>` | GamePad input |
| padscore | `<padscore/*.h>` | Wii Remote, Classic, Pro Controller |
| proc_ui | `<proc_ui/procui.h>` | App lifecycle (foreground/background) |
| sndcore2 | `<sndcore2/*.h>` | AX audio DSP |
| nsysnet | `<nsysnet/*.h>` | BSD sockets |
| nn_ac/nn_act | `<nn/*.h>` | Network auto-connect, accounts |

### Memory allocation

```c
#include <coreinit/memdefaultheap.h>

void* ptr = MEMAllocFromDefaultHeap(size);
MEMFreeToDefaultHeap(ptr);

// Aligned (required for GPU resources — typically 0x100 / 256 bytes)
void* aligned = MEMAllocFromDefaultHeapEx(size, GX2_VERTEX_BUFFER_ALIGNMENT);
```

### libwhb helpers

| Function | Purpose |
|----------|---------|
| `WHBProcInit()` | Init OS process management (required) |
| `WHBProcIsRunning()` | Main loop condition |
| `WHBProcShutdown()` | Clean shutdown |
| `WHBLogConsoleInit()` / `Free()` | Debug console |
| `WHBGfxInit()` | Init graphics subsystem |

### ProcUI lifecycle (mandatory)

Every Aroma app must handle foreground/background transitions. Skipping this causes crashes on HOME press.

```c
#include <proc_ui/procui.h>

int main(int argc, char **argv) {
    ProcUIInit(&OSSavesDone_ReadyToRelease);
    while (ProcUIProcessMessages(TRUE) != PROCUI_STATUS_EXITING) {
        // main loop
    }
    ProcUIShutdown();
    return 0;
}
```

Or via WHB:
```c
WHBProcInit();
while (WHBProcIsRunning()) { /* main loop */ }
WHBProcShutdown();
```

---

## Aroma ecosystem

Aroma is the modern CFW environment. Apps launch directly from the home menu as native channels (not through Homebrew Launcher).

| Aspect | Homebrew Launcher | Aroma |
|--------|-------------------|-------|
| Launch | Load into HBL, select app | Direct from home menu |
| Format | .elf or .rpx | .rpx or .wuhb |
| Background services | Not possible | Plugins run alongside games |
| Memory | Shared heaps, crash-prone | Separate heaps per plugin |
| Exploits | Apps can launch own | Must not launch exploits |

### Components

- **Modules** — persistent background libraries:
  - KernelModule (privileged access)
  - FunctionPatcherModule (OS hooking)
  - CURLWrapperModule (HTTPS with bundled CA certs)
- **Plugins** — WUPS-based extensions with user config
- **homebrew_on_menu** — scans `sd:/wiiu/apps/` for `.wuhb` files, displays them as channels

---

## Minimal Aroma application

```c
#include <whb/proc.h>
#include <whb/log.h>
#include <whb/log_console.h>
#include <coreinit/screen.h>
#include <vpad/input.h>

int main(int argc, char **argv) {
    WHBProcInit();
    WHBLogConsoleInit();
    OSScreenInit();
    VPADInit();

    uint32_t tvBufSize = OSScreenGetBufferSizeEx(SCREEN_TV);
    uint32_t drcBufSize = OSScreenGetBufferSizeEx(SCREEN_DRC);
    void* tvBuf = MEMAllocFromDefaultHeapEx(tvBufSize, 0x100);
    void* drcBuf = MEMAllocFromDefaultHeapEx(drcBufSize, 0x100);
    OSScreenSetBufferEx(SCREEN_TV, tvBuf);
    OSScreenSetBufferEx(SCREEN_DRC, drcBuf);
    OSScreenEnableEx(SCREEN_TV, TRUE);
    OSScreenEnableEx(SCREEN_DRC, TRUE);

    while (WHBProcIsRunning()) {
        VPADStatus vpad;
        VPADReadError err;
        VPADRead(VPAD_CHAN_0, &vpad, 1, &err);

        if (vpad.trigger & VPAD_BUTTON_HOME) break;

        OSScreenClearBufferEx(SCREEN_TV, 0x000000FF);
        OSScreenClearBufferEx(SCREEN_DRC, 0x000000FF);
        OSScreenPutFontEx(SCREEN_TV, 0, 0, "Hello Wii U! (TV)");
        OSScreenPutFontEx(SCREEN_DRC, 0, 0, "Hello Wii U! (GamePad)");
        OSScreenPutFontEx(SCREEN_DRC, 0, 2, "Press HOME to exit");
        OSScreenFlipBuffersEx(SCREEN_TV);
        OSScreenFlipBuffersEx(SCREEN_DRC);
    }

    MEMFreeToDefaultHeap(tvBuf);
    MEMFreeToDefaultHeap(drcBuf);
    VPADShutdown();
    OSScreenShutdown();
    WHBLogConsoleFree();
    WHBProcShutdown();
    return 0;
}
```

---

## Graphics (GX2)

### Design model

GX2 has no state retention. Unlike OpenGL, nothing is implicit:
- Commands queue directly to GPU
- State only persists through explicit `GX2ContextState` objects
- All memory management is manual
- Alignment is strict (typically 0x100)

### Simple init via WHB

```c
#include <whb/proc.h>
#include <whb/gfx.h>

WHBProcInit();
WHBGfxInit();  // handles GX2Init, context, scan buffers
```

### Manual scan buffer setup

```c
GX2TVRenderMode tvMode = GX2_TV_RENDER_MODE_WIDE_720P;
GX2SurfaceFormat format = GX2_SURFACE_FORMAT_UNORM_R8_G8_B8_A8;

uint32_t tvSize, unk;
GX2CalcTVSize(tvMode, format, GX2_BUFFERING_MODE_DOUBLE, &tvSize, &unk);
void* tvScanBuffer = MEMAllocFromDefaultHeapEx(tvSize, GX2_SCAN_BUFFER_ALIGNMENT);
GX2SetTVBuffer(tvScanBuffer, tvSize, tvMode, format, GX2_BUFFERING_MODE_DOUBLE);
GX2SetTVEnable(TRUE);
```

### OSScreen (simple 2D text output)

```c
#include <coreinit/screen.h>

OSScreenInit();
OSScreenClearBufferEx(SCREEN_TV, 0xFF000000);
OSScreenClearBufferEx(SCREEN_DRC, 0xFF000000);
OSScreenPutFontEx(SCREEN_TV, 0, 0, "Hello TV!");
OSScreenPutFontEx(SCREEN_DRC, 0, 0, "Hello GamePad!");
OSScreenFlipBuffersEx(SCREEN_TV);
OSScreenFlipBuffersEx(SCREEN_DRC);
```

### Dual-screen rendering

TV is 1280×720, GamePad is 854×480. They can show different content:

```c
void renderFrame() {
    // TV
    GX2SetColorBuffer(&tvColorBuffer, GX2_RENDER_TARGET_0);
    GX2SetViewport(0, 0, 1280, 720, 0, 1);
    GX2ClearColor(&tvColorBuffer, 0.0f, 0.0f, 0.2f, 1.0f);
    // draw...
    GX2CopyColorBufferToScanBuffer(&tvColorBuffer, GX2_SCAN_TARGET_TV);

    // GamePad
    GX2SetColorBuffer(&drcColorBuffer, GX2_RENDER_TARGET_0);
    GX2SetViewport(0, 0, 854, 480, 0, 1);
    GX2ClearColor(&drcColorBuffer, 0.0f, 0.2f, 0.0f, 1.0f);
    // draw...
    GX2CopyColorBufferToScanBuffer(&drcColorBuffer, GX2_SCAN_TARGET_DRC);

    GX2SwapScanBuffers();
    GX2Flush();
    GX2WaitForVsync();
}
```

### Render loop pattern

1. `GX2WaitForVsync()` — sync with display
2. `GX2SetContextState()` — set context
3. `GX2SetColorBuffer()`, `GX2SetDepthBuffer()` — bind targets
4. `GX2ClearColor()`, `GX2ClearDepthStencil()` — clear
5. `GX2DrawEx()` — draw
6. `GX2CopyColorBufferToScanBuffer()` — copy to scan buffer
7. `GX2SwapScanBuffers()` — flip

### Cache invalidation (mandatory for CPU→GPU data)

```c
GX2Invalidate(GX2_INVALIDATE_MODE_CPU_ATTRIBUTE_BUFFER, vertexBuffer, size);
GX2Invalidate(GX2_INVALIDATE_MODE_CPU_TEXTURE, textureData, textureSize);
GX2Invalidate(GX2_INVALIDATE_MODE_CPU_SHADER, shaderProgram, shaderSize);
```

Forgetting this causes rendering glitches or crashes. No exceptions.

### Textures

Must be aligned to 0x100+ and typically **swizzled** (tiled) for GPU cache performance. Use `GX2Swizzle_` functions to convert linear bitmaps. `GX2Surface` defines dimensions, format, and tiling mode.

### CafeGLSL (runtime GLSL compilation)

```c
#include "CafeGLSLCompiler.h"

GLSL_Init();
char infoLog[1024];
GX2VertexShader* vs = GLSL_CompileVertexShader(vertexSource, infoLog,
                                                sizeof(infoLog),
                                                GLSL_COMPILER_FLAG_NONE);
```

Requires `glslcompiler.rpl`. Only separable shaders with explicit binding locations.

### SDL2 on Wii U

Maps `SDL_Window` to GX2 context, `SDL_Joystick` to VPAD/KPAD. Supports hardware-accelerated 2D. Does **not** support full OpenGL contexts — use GX2 directly for 3D.

---

## Audio (sndcore2 / AX)

96 concurrent voices with hardware mixing.

### Init

```c
#include <sndcore2/core.h>
#include <sndcore2/voice.h>
#include <sndcore2/device.h>

AXInitParams params = {
    .renderer = AX_INIT_RENDERER_48KHZ,
    .pipeline = AX_INIT_PIPELINE_SINGLE
};
AXInitWithParams(&params);
```

### Voice setup and playback

```c
AXVoice *voice = AXAcquireVoice(31, NULL, NULL);  // priority 0-31

AXVoiceOffsets offsets = {
    .dataType = AX_VOICE_FORMAT_LPCM16,
    .loopingEnabled = AX_VOICE_LOOP_DISABLED,
    .endOffset = sampleCount - 1,
    .currentOffset = 0,
    .data = audioBuffer  // MUST be big-endian samples
};
AXSetVoiceOffsets(voice, &offsets);

AXVoiceVeData ve = { .volume = 0x8000, .delta = 0 };  // 0x8000 = max
AXSetVoiceVe(voice, &ve);

// Route to TV speakers
AXVoiceDeviceMixData tvMix[6] = {0};
tvMix[0].bus[0].volume = 0x8000;  // left
tvMix[1].bus[0].volume = 0x8000;  // right
AXSetVoiceDeviceMix(voice, AX_DEVICE_TYPE_TV, 0, tvMix);

AXSetVoiceState(voice, AX_VOICE_STATE_PLAYING);
```

### Audio samples must be big-endian

WAV files are little-endian. Swap every 16-bit sample or you get loud noise:

```c
for (int i = 0; i < sampleCount; i++) {
    samples[i] = __builtin_bswap16(samples[i]);
}
```

Always `DCFlushRange()` audio buffers before playback.

### Supported formats

| Format | Description |
|--------|-------------|
| `AX_VOICE_FORMAT_LPCM16` | 16-bit signed PCM |
| `AX_VOICE_FORMAT_LPCM8` | 8-bit signed PCM |
| `AX_VOICE_FORMAT_ADPCM` | Nintendo DSP ADPCM (compressed) |

### Streaming via double-buffer

While buffer A plays, CPU fills buffer B. Use `AXRegisterAppFrameCallback` for DSP-done notification, then swap.

---

## Input

### GamePad (VPAD)

```c
#include <vpad/input.h>

VPADStatus status;
VPADReadError error;
VPADRead(VPAD_CHAN_0, &status, 1, &error);

if (error == VPAD_READ_SUCCESS) {
    if (status.trigger & VPAD_BUTTON_A) { /* just pressed */ }
    if (status.hold & VPAD_BUTTON_B)    { /* held */ }
    if (status.release & VPAD_BUTTON_X) { /* just released */ }

    // Sticks: -1.0 to 1.0
    float lx = status.leftStick.x, ly = status.leftStick.y;
    float rx = status.rightStick.x, ry = status.rightStick.y;

    // Gyro / accelerometer
    float gyroX = status.gyro.x;
    float accX = status.acc.x;

    // Touch (854×480 resolution)
    if (status.tpNormal.validity == VPAD_VALID) {
        VPADTouchData cal;
        VPADGetTPCalibratedPoint(VPAD_CHAN_0, &cal, &status.tpNormal);
        int tx = cal.x, ty = cal.y;
    }
}
```

### Button constants

| Button | Constant |
|--------|----------|
| A / B / X / Y | `VPAD_BUTTON_A`, `_B`, `_X`, `_Y` |
| D-pad | `VPAD_BUTTON_UP`, `_DOWN`, `_LEFT`, `_RIGHT` |
| Shoulders | `VPAD_BUTTON_L`, `_R`, `_ZL`, `_ZR` |
| System | `VPAD_BUTTON_PLUS`, `_MINUS`, `_HOME` |
| Stick click | `VPAD_BUTTON_STICK_L`, `_STICK_R` |

Touch is 854×480. Scale coordinates when mapping to TV resolution.

### Wii Remote + Pro Controller (KPAD)

```c
#include <padscore/kpad.h>
#include <padscore/wpad.h>

KPADInit();
WPADEnableURCC(TRUE);  // enable Pro Controller

KPADStatus kpadData;
int32_t error;

for (int chan = 0; chan < 4; chan++) {
    if (KPADReadEx((KPADChan)chan, &kpadData, 1, &error) > 0) {
        switch (kpadData.extensionType) {
            case WPAD_EXT_CORE:
                if (kpadData.trigger & WPAD_BUTTON_A) { /* ... */ }
                break;
            case WPAD_EXT_NUNCHUK:
                float nx = kpadData.nunchuk.stick.x;
                break;
            case WPAD_EXT_PRO_CONTROLLER:
                float lx = kpadData.pro.leftStick.x;
                float rx = kpadData.pro.rightStick.x;
                break;
        }
    }
}
```

---

## Networking

### BSD sockets

```c
#include <nsysnet/_socket.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

socket_lib_init();

int sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
struct sockaddr_in server = {0};
server.sin_family = AF_INET;
server.sin_port = htons(8080);
inet_aton("192.168.1.100", &server.sin_addr);

connect(sock, (struct sockaddr*)&server, sizeof(server));
send(sock, data, len, 0);
recv(sock, buffer, maxLen, 0);
close(sock);
```

### HTTPS via libcurl

Requires `CURLWrapperModule` (bundles CA certs). Link with `-lcurlwrapper`. Module must be at `sd:/wiiu/environments/aroma/modules/CURLWrapperModule.wms`.

```c
#include <curl/curl.h>

curl_global_init(CURL_GLOBAL_DEFAULT);
CURL *curl = curl_easy_init();
curl_easy_setopt(curl, CURLOPT_URL, "https://example.com/api");
curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCallback);
curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
curl_easy_perform(curl);
curl_easy_cleanup(curl);
curl_global_cleanup();
```

---

## Debugging

### Wireless deploy

```bash
export WIILOAD=tcp:192.168.1.50
wiiload myapp.wuhb
```

### Logging

Logs go to UDP port 4405. Listen with `nc -u -l 4405`.

### GDB (via gdbstub_plugin)

```gdb
set arch powerpc:750
set endian big
target remote tcp:192.168.1.50:3000
```

Up to 512 software breakpoints, hardware watchpoints, single-stepping, memory inspection.

### Crash address lookup

```bash
powerpc-eabi-addr2line -e myapp.elf 800084ac
```

Build with `-g -save-temps` for assembly listings.

---

## Common pitfalls

| Problem | Cause | Fix |
|---------|-------|-----|
| HOME menu crash | Missing ProcUI loop | Use `WHBProcIsRunning()` or `ProcUIProcessMessages()` |
| Rendering glitches | No cache flush/invalidate | `GX2Invalidate()` + `DCFlushRange()` after writing GPU data |
| Audio static/noise | Little-endian samples | `__builtin_bswap16()` every 16-bit sample |
| App exits immediately | No main loop | Implement proper event loop |
| Alignment crash | GPU resource misaligned | `MEMAllocFromDefaultHeapEx(size, GX2_VERTEX_BUFFER_ALIGNMENT)` |
| Network data corruption | Endianness mismatch | `htonl()` / `ntohl()` for network byte order |
| Texture won't load | Linear bitmap not swizzled | Use `GX2Swizzle_` or set tiling mode in `GX2Surface` |

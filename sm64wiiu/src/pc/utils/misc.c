#include <ctype.h>
#include <math.h>
#include <string.h>
#include <time.h>

#include "engine/math_util.h"
#include "misc.h"

float smooth_step(float edge0, float edge1, float x) {
    if (edge0 == edge1) { return 0.0f; }
    float t = (x - edge0) / (edge1 - edge0);
    if (t < 0.0f) { t = 0.0f; }
    if (t > 1.0f) { t = 1.0f; }
    return t * t * (3.0f - 2.0f * t);
}

void update_all_mario_stars(void) {
}

static u64 sClockStartNs = 0;

static u64 clock_now_ns(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ((u64)ts.tv_sec * 1000000000ULL) + (u64)ts.tv_nsec;
}

f32 clock_elapsed(void) {
    if (sClockStartNs == 0) { sClockStartNs = clock_now_ns(); }
    return (f32)((clock_now_ns() - sClockStartNs) / 1000000000.0);
}

f64 clock_elapsed_f64(void) {
    if (sClockStartNs == 0) { sClockStartNs = clock_now_ns(); }
    return (f64)((clock_now_ns() - sClockStartNs) / 1000000000.0);
}

u32 clock_elapsed_ticks(void) {
    return (u32)(clock_elapsed_f64() * 30.0);
}

bool clock_is_date(u8 month, u8 day) {
    time_t t = time(NULL);
    struct tm *ti = localtime(&t);
    if (ti == NULL) { return false; }
    return (ti->tm_mon == (int)month - 1) && (ti->tm_mday == (int)day);
}

void precise_delay_f64(f64 delaySec) {
    f64 start = clock_elapsed_f64();
    while ((clock_elapsed_f64() - start) < delaySec) {
    }
}

void file_get_line(char *buffer, size_t maxLength, FILE *fp) {
    if (buffer == NULL || maxLength == 0 || fp == NULL) { return; }
    if (fgets(buffer, (int)maxLength, fp) == NULL) {
        buffer[0] = '\0';
        return;
    }
    size_t n = strlen(buffer);
    while (n > 0 && (buffer[n - 1] == '\n' || buffer[n - 1] == '\r')) {
        buffer[--n] = '\0';
    }
}

f32 delta_interpolate_f32(f32 a, f32 b, f32 delta) {
    return a * (1.0f - delta) + b * delta;
}

s32 delta_interpolate_s32(s32 a, s32 b, f32 delta) {
    return (s32)(a * (1.0f - delta) + b * delta);
}

void delta_interpolate_vec3f(VEC_OUT Vec3f res, Vec3f a, Vec3f b, f32 delta) {
    for (int i = 0; i < 3; i++) {
        res[i] = delta_interpolate_f32(a[i], b[i], delta);
    }
}

void delta_interpolate_vec3s(VEC_OUT Vec3s res, Vec3s a, Vec3s b, f32 delta) {
    for (int i = 0; i < 3; i++) {
        res[i] = (s16)delta_interpolate_s32(a[i], b[i], delta);
    }
}

void delta_interpolate_normal(s8 *res, s8 *a, s8 *b, f32 delta) {
    for (int i = 0; i < 3; i++) {
        res[i] = (s8)delta_interpolate_s32(a[i], b[i], delta);
    }
}

void delta_interpolate_rgba(u8 *res, u8 *a, u8 *b, f32 delta) {
    for (int i = 0; i < 4; i++) {
        res[i] = (u8)delta_interpolate_s32(a[i], b[i], delta);
    }
}

void delta_interpolate_mtx(Mtx *out, Mtx *a, Mtx *b, f32 delta) {
    if (out == NULL || a == NULL || b == NULL) { return; }
#ifdef GBI_FLOATS
    f32 antiDelta = 1.0f - delta;
    for (s32 i = 0; i < 4; i++) {
        for (s32 j = 0; j < 4; j++) {
            out->m[i][j] = (a->m[i][j] * antiDelta) + (b->m[i][j] * delta);
        }
    }
#else
    for (int r = 0; r < 4; r++) {
        for (int c = 0; c < 4; c++) {
            out->m[r][c] = (s16)delta_interpolate_s32(a->m[r][c], b->m[r][c], delta);
        }
    }
#endif
}

void detect_and_skip_mtx_interpolation(Mtx **mtxPrev, Mtx **mtx) {
#ifdef GBI_FLOATS
    if (mtxPrev == NULL || mtx == NULL || *mtxPrev == NULL || *mtx == NULL) { return; }

    // If the basis vectors have flipped far enough, interpolation can take the long arc
    // and cause a visible pop. In that case, skip by clamping "current" to "previous".
    const f32 minDot = sqrtf(2.0f) / -3.0f;

    Vec3f prevX; vec3f_copy(prevX, (f32*)(*mtxPrev)->m[0]); vec3f_normalize(prevX);
    Vec3f prevY; vec3f_copy(prevY, (f32*)(*mtxPrev)->m[1]); vec3f_normalize(prevY);
    Vec3f prevZ; vec3f_copy(prevZ, (f32*)(*mtxPrev)->m[2]); vec3f_normalize(prevZ);

    Vec3f nextX; vec3f_copy(nextX, (f32*)(*mtx)->m[0]); vec3f_normalize(nextX);
    Vec3f nextY; vec3f_copy(nextY, (f32*)(*mtx)->m[1]); vec3f_normalize(nextY);
    Vec3f nextZ; vec3f_copy(nextZ, (f32*)(*mtx)->m[2]); vec3f_normalize(nextZ);

    f32 dotX = (prevX[0] * nextX[0]) + (prevX[1] * nextX[1]) + (prevX[2] * nextX[2]);
    f32 dotY = (prevY[0] * nextY[0]) + (prevY[1] * nextY[1]) + (prevY[2] * nextY[2]);
    f32 dotZ = (prevZ[0] * nextZ[0]) + (prevZ[1] * nextZ[1]) + (prevZ[2] * nextZ[2]);

    if ((dotX < minDot) || (dotY < minDot) || (dotZ < minDot)) {
        *mtx = *mtxPrev;
    }
#else
    (void)mtxPrev;
    (void)mtx;
#endif
}

void str_seperator_concat(char *output_buffer, int buffer_size, char **strings, int num_strings, char *seperator) {
    if (output_buffer == NULL || buffer_size <= 0) { return; }
    output_buffer[0] = '\0';
    for (int i = 0; i < num_strings; i++) {
        const char *s = (strings != NULL && strings[i] != NULL) ? strings[i] : "";
        if (i > 0 && seperator != NULL) {
            strncat(output_buffer, seperator, (size_t)buffer_size - strlen(output_buffer) - 1);
        }
        strncat(output_buffer, s, (size_t)buffer_size - strlen(output_buffer) - 1);
    }
}

char *str_remove_color_codes(const char *str) {
    static char sBuffer[1024];
    size_t out = 0;
    if (str == NULL) {
        sBuffer[0] = '\0';
        return sBuffer;
    }

    for (size_t i = 0; str[i] != '\0' && out < sizeof(sBuffer) - 1; i++) {
        if (str[i] == '\\' && str[i + 1] == '#') {
            i += 7;
            continue;
        }
        sBuffer[out++] = str[i];
    }
    sBuffer[out] = '\0';
    return sBuffer;
}

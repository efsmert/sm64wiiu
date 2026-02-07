#ifdef TARGET_WII_U

#include <malloc.h>
#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>

#include <vector>

#ifndef _LANGUAGE_C
#define _LANGUAGE_C
#endif
#include <PR/gbi.h>

#include <gx2/draw.h>
#include <gx2/event.h>
#include <gx2/mem.h>
#include <gx2/registers.h>
#include <gx2/shaders.h>
#include <gx2/state.h>
#include <gx2/texture.h>

#include <whb/log.h>
#include <whb/gfx.h>

#include "shaders_wiiu/shaders_wiiu.h"
#include "gx2_shader_gen.h"
#include "gfx_gx2.h"

#include "gfx_cc.h"
#include "gfx_rendering_api.h"

struct ShaderProgram
{
    uint32_t shader_id;
    union
    {
        WHBGfxShaderGroup whb_group;
        struct ShaderGroup gen_group;
    };
    uint8_t num_inputs;
    bool used_textures[2];
    uint8_t num_floats;
    bool used_noise;
    bool is_precompiled;
    uint32_t window_params_offset;
    uint32_t samplers_location[2];
};

typedef struct _Texture
{
    GX2Texture texture;
    GX2Sampler sampler;
    bool texture_uploaded;
    bool sampler_set;
}
Texture;

static struct ShaderProgram shader_program_pool[256];
static size_t shader_program_pool_size = 0;
static bool sShaderPoolOverflowLogged = false;
static uint32_t sInvalidSamplerStateCount = 0;
static uint32_t sNullShaderSamplerSetCount = 0;
static uint32_t sTexturePoolOverflowCount = 0;

static struct ShaderProgram* current_shader_program = nullptr;
static std::vector<float*> vbo_array;

#define GX2_MAX_TEXTURES 2048
static Texture gx2_textures[GX2_MAX_TEXTURES];
static size_t gx2_texture_count = 0;
static uint8_t current_tile = 0;
static uint32_t current_texture_ids[2];

static uint32_t frame_count = 0;
static uint32_t current_height = 0;
static bool sLoggedFirstDrawCall = false;

static BOOL current_depth_test = FALSE;
static BOOL current_depth_write = FALSE;
static GX2CompareFunction current_depth_compare = GX2_COMPARE_FUNC_LEQUAL;

// Validates tile index used by N64 combiner samplers.
static bool gfx_gx2_is_valid_tile(int tile)
{
    return tile >= 0 && tile < 2;
}

// Validates texture ids before indexing into gx2_textures.
static bool gfx_gx2_is_valid_texture_id(uint32_t texture_id)
{
    return texture_id < gx2_texture_count;
}

// Logs invalid sampler/texture state with bounded spam.
static void gfx_gx2_log_invalid_sampler_state(const char *where, int tile, uint32_t texture_id, uint32_t sampler_location)
{
    if (sInvalidSamplerStateCount < 20) {
        WHBLogPrintf("gfx: invalid sampler state at %s tile=%d tex=%u loc=%u pool=%u",
                     where,
                     tile,
                     (unsigned)texture_id,
                     (unsigned)sampler_location,
                     (unsigned)shader_program_pool_size);
        sInvalidSamplerStateCount++;
    }
}

GX2SamplerVar* GX2GetPixelSamplerVar(const GX2PixelShader* shader, const char* name)
{
    for (uint32_t i = 0; i < shader->samplerVarCount; i++)
    {
       if (strcmp(shader->samplerVars[i].name, name) == 0)
           return &(shader->samplerVars[i]);
    }

    return nullptr;
}

s32 GX2GetPixelSamplerVarLocation(const GX2PixelShader* shader, const char* name)
{
    GX2SamplerVar* sampler = GX2GetPixelSamplerVar(shader, name);
    if (!sampler)
        return -1;

    return sampler->location;
}

// Looks up sampler location with defensive bounds against malformed shader metadata.
static s32 GX2GetPixelSamplerVarLocationSafe(const GX2PixelShader* shader, const char* name)
{
    if (shader == nullptr || shader->samplerVars == nullptr || name == nullptr) {
        return -1;
    }

    // Expected sampler counts are small (<=2 for this renderer); clamp hard.
    uint32_t count = shader->samplerVarCount;
    if (count > 64) {
        count = 64;
    }

    for (uint32_t i = 0; i < count; i++) {
        if (shader->samplerVars[i].name != nullptr && strcmp(shader->samplerVars[i].name, name) == 0) {
            return shader->samplerVars[i].location;
        }
    }
    return -1;
}

// Looks up uniform offset with defensive bounds against malformed shader metadata.
static s32 GX2GetPixelUniformVarOffsetSafe(const GX2PixelShader* shader, const char* name)
{
    if (shader == nullptr || shader->uniformVars == nullptr || name == nullptr) {
        return -1;
    }

    // Expected uniform counts are small in these shaders; clamp to avoid stalls.
    uint32_t count = shader->uniformVarCount;
    if (count > 128) {
        count = 128;
    }

    for (uint32_t i = 0; i < count; i++) {
        if (shader->uniformVars[i].name != nullptr && strcmp(shader->uniformVars[i].name, name) == 0) {
            return shader->uniformVars[i].offset;
        }
    }
    return -1;
}

static bool gfx_gx2_z_is_from_0_to_1(void)
{
    return false;
}

static void gfx_gx2_unload_shader(struct ShaderProgram* old_prg)
{
    if (current_shader_program == old_prg)
        current_shader_program = nullptr;

    else
    {
        // ??????????
    }
}

static void gfx_gx2_set_uniforms(struct ShaderProgram* prg)
{
    if (prg->used_noise && prg->window_params_offset != UINT32_MAX)
    {
        float window_params_array[4] = { (float)current_height, (float)frame_count };
        GX2SetPixelUniformReg(prg->window_params_offset, 4, window_params_array);
    }
}

static void gfx_gx2_load_shader(struct ShaderProgram* new_prg)
{
    current_shader_program = new_prg;
    if (!new_prg)
        return;

    if (new_prg->is_precompiled)
    {
        GX2SetFetchShader(&new_prg->whb_group.fetchShader);
        GX2SetVertexShader(new_prg->whb_group.vertexShader);
        GX2SetPixelShader(new_prg->whb_group.pixelShader);
    }
    else
    {
        GX2SetFetchShader(&new_prg->gen_group.fetchShader);
        GX2SetVertexShader(&new_prg->gen_group.vertexShader);
        GX2SetPixelShader(&new_prg->gen_group.pixelShader);
    }

    gfx_gx2_set_uniforms(new_prg);
}

static struct ShaderProgram* gfx_gx2_create_and_load_new_shader(struct ColorCombiner *cc)
{
    uint32_t shader_id = cc->shader_id;
    struct CCFeatures cc_features;
    gfx_cc_get_features(shader_id, &cc_features);

    if (shader_program_pool_size >= (sizeof(shader_program_pool) / sizeof(shader_program_pool[0]))) {
        if (!sShaderPoolOverflowLogged) {
            sShaderPoolOverflowLogged = true;
            WHBLogPrintf("gfx: shader pool full (%u), cannot create shader_id=0x%08x",
                         (unsigned)shader_program_pool_size, (unsigned)shader_id);
        }
        return nullptr;
    }

    struct ShaderProgram* prg = &shader_program_pool[shader_program_pool_size++];
    prg->is_precompiled = true;

    const uint8_t *shader_wiiu;
    switch (shader_id)
    {
    case 0x01200200: shader_wiiu = shader_wiiu_01200200; break;
    case 0x00000045: shader_wiiu = shader_wiiu_00000045; break;
    case 0x00000200: shader_wiiu = shader_wiiu_00000200; break;
    case 0x01200a00: shader_wiiu = shader_wiiu_01200a00; break;
    case 0x00000a00: shader_wiiu = shader_wiiu_00000a00; break;
    case 0x01a00045: shader_wiiu = shader_wiiu_01a00045; break;
    case 0x00000551: shader_wiiu = shader_wiiu_00000551; break;
    case 0x01045045: shader_wiiu = shader_wiiu_01045045; break;
    case 0x05a00a00: shader_wiiu = shader_wiiu_05a00a00; break;
    case 0x01200045: shader_wiiu = shader_wiiu_01200045; break;
    case 0x05045045: shader_wiiu = shader_wiiu_05045045; break;
    case 0x01045a00: shader_wiiu = shader_wiiu_01045a00; break;
    case 0x01a00a00: shader_wiiu = shader_wiiu_01a00a00; break;
    case 0x0000038d: shader_wiiu = shader_wiiu_0000038d; break;
    case 0x01081081: shader_wiiu = shader_wiiu_01081081; break;
    case 0x0120038d: shader_wiiu = shader_wiiu_0120038d; break;
    case 0x03200045: shader_wiiu = shader_wiiu_03200045; break;
    case 0x03200a00: shader_wiiu = shader_wiiu_03200a00; break;
    case 0x01a00a6f: shader_wiiu = shader_wiiu_01a00a6f; break;
    case 0x01141045: shader_wiiu = shader_wiiu_01141045; break;
    case 0x07a00a00: shader_wiiu = shader_wiiu_07a00a00; break;
    case 0x05200200: shader_wiiu = shader_wiiu_05200200; break;
    case 0x03200200: shader_wiiu = shader_wiiu_03200200; break;
    case 0x09200200: shader_wiiu = shader_wiiu_09200200; break;
    case 0x0920038d: shader_wiiu = shader_wiiu_0920038d; break;
    case 0x09200045: shader_wiiu = shader_wiiu_09200045; break;
    case 0x09200a00: shader_wiiu = shader_wiiu_09200a00; break;
    default:
        if (gx2GenerateShaderGroup(&prg->gen_group, &cc_features) != 0)
        {
            WHBLogPrintf("Failed to generate shader. shader_id: 0x%08x", shader_id);
error_gen:
            shader_program_pool_size--;
            return (current_shader_program = nullptr);
        }
        prg->is_precompiled = false;
    }

    if (prg->is_precompiled)
    {
        if (!WHBGfxLoadGFDShaderGroup(&prg->whb_group, 0, shader_wiiu))
        {
            WHBLogPrintf("Failed to load gsh. shader_id: 0x%08x", shader_id);
            goto error_gen;
        }

        uint32_t pos = 0;
        prg->num_floats = 0;

        if (!WHBGfxInitShaderAttribute(&prg->whb_group, "aVtxPos", 0, pos, GX2_ATTRIB_FORMAT_FLOAT_32_32_32_32))
        {
error_attr:
            WHBLogPrintf("Failed to initialize attribute. shader_id: 0x%08x", shader_id);
            goto error_gen;
        }

        pos += 4 * sizeof(float);
        prg->num_floats += 4;

        if (cc_features.used_textures[0] || cc_features.used_textures[1])
        {
            if (!WHBGfxInitShaderAttribute(&prg->whb_group, "aTexCoord", 0, pos, GX2_ATTRIB_FORMAT_FLOAT_32_32))
                goto error_attr;

            pos += (2+2) * sizeof(float);
            prg->num_floats += (2+2);// 2 floats for the texcoord + 2 floats (8 bytes) as padding, for faster GPU reading
        }

        if (cc_features.opt_fog)
        {
            if (!WHBGfxInitShaderAttribute(&prg->whb_group, "aFog", 0, pos, GX2_ATTRIB_FORMAT_FLOAT_32_32_32_32))
                goto error_attr;

            pos += 4 * sizeof(float);
            prg->num_floats += 4;
        }

        for (int i = 0; i < cc_features.num_inputs; i++)
        {
            char name[16];
            sprintf(name, "aInput%d", i + 1);
            if (!WHBGfxInitShaderAttribute(&prg->whb_group, name, 0, pos, GX2_ATTRIB_FORMAT_FLOAT_32_32_32_32))
                goto error_attr;

            pos += 4 * sizeof(float);
            prg->num_floats += 4;
        }

        if (!WHBGfxInitFetchShader(&prg->whb_group))
        {
            WHBLogPrintf("Failed to initialize fetch shader. shader_id: 0x%08x", shader_id);
            goto error_gen;
        }
    }
    else
    {
        // In our case, each attribute has 4 floats
        prg->num_floats = prg->gen_group.numAttributes * 4;
    }

    prg->shader_id = shader_id;
    prg->num_inputs = cc_features.num_inputs;
    prg->used_textures[0] = cc_features.used_textures[0];
    prg->used_textures[1] = cc_features.used_textures[1];

    gfx_gx2_load_shader(prg);

    // Default sampler bindings expected by generated/precompiled shader groups.
    // Keep defaults for precompiled shaders to avoid startup stalls caused by
    // malformed sampler/uniform metadata seen on some Cemu runs.
    prg->window_params_offset = UINT32_MAX;
    prg->samplers_location[0] = 0;
    prg->samplers_location[1] = 1;

    if (!prg->is_precompiled) {
        GX2PixelShader* pixel_shader = &prg->gen_group.pixelShader;
        s32 window_params = GX2GetPixelUniformVarOffsetSafe(pixel_shader, "window_params");
        s32 sampler0 = GX2GetPixelSamplerVarLocationSafe(pixel_shader, "uTex0");
        s32 sampler1 = GX2GetPixelSamplerVarLocationSafe(pixel_shader, "uTex1");

        if (window_params >= 0) {
            prg->window_params_offset = (uint32_t)window_params;
        }
        if (sampler0 >= 0) {
            prg->samplers_location[0] = (uint32_t)sampler0;
        }
        if (sampler1 >= 0) {
            prg->samplers_location[1] = (uint32_t)sampler1;
        }
    }

    prg->used_noise = cc_features.opt_alpha && cc_features.opt_noise &&
                      prg->window_params_offset != UINT32_MAX;

    return prg;
}

static struct ShaderProgram* gfx_gx2_lookup_shader(struct ColorCombiner *cc)
{
    uint32_t shader_id = cc->shader_id;
    for (size_t i = 0; i < shader_program_pool_size; i++)
        if (shader_program_pool[i].shader_id == shader_id)
            return &shader_program_pool[i];

    return nullptr;
}

static void gfx_gx2_shader_get_info(struct ShaderProgram* prg, uint8_t* num_inputs, bool used_textures[2])
{
    if (prg)
    {
        *num_inputs = prg->num_inputs;
        used_textures[0] = prg->used_textures[0];
        used_textures[1] = prg->used_textures[1];
    }
    else
    {
        *num_inputs = 0;
        used_textures[0] = false;
        used_textures[1] = false;
    }
}

static uint32_t gfx_gx2_new_texture(void)
{
    size_t texture_id = gx2_texture_count;
    if (texture_id >= GX2_MAX_TEXTURES) {
        if (sTexturePoolOverflowCount < 8) {
            WHBLogPrintf("gfx: texture pool full (%u)", (unsigned)gx2_texture_count);
            sTexturePoolOverflowCount++;
        }
        return 0;
    }
    gx2_texture_count++;

    Texture& texture = gx2_textures[texture_id];
    memset(&texture, 0, sizeof(texture));
    texture.texture_uploaded = false;
    texture.sampler_set = false;

    return (uint32_t)texture_id;
}

static void gfx_gx2_select_texture(int tile, uint32_t texture_id)
{
    if (!gfx_gx2_is_valid_tile(tile) || !gfx_gx2_is_valid_texture_id(texture_id)) {
        gfx_gx2_log_invalid_sampler_state("select_texture", tile, texture_id, 0);
        return;
    }

    current_tile = tile;
    current_texture_ids[tile] = texture_id;

    if (current_shader_program)
    {
        Texture& texture = gx2_textures[texture_id];
        if (texture.texture_uploaded)
        {
            uint32_t sampler_loc = current_shader_program->samplers_location[tile];
            if (sampler_loc < 32) {
                GX2SetPixelTexture(&texture.texture, sampler_loc);
            } else {
                gfx_gx2_log_invalid_sampler_state("set_pixel_texture", tile, texture_id, sampler_loc);
            }
        }
        if (texture.sampler_set) {
            uint32_t sampler_loc = current_shader_program->samplers_location[tile];
            if (sampler_loc < 32) {
                GX2SetPixelSampler(&texture.sampler, sampler_loc);
            } else {
                gfx_gx2_log_invalid_sampler_state("set_pixel_sampler", tile, texture_id, sampler_loc);
            }
        }
    }
}

static void gfx_gx2_upload_texture(const uint8_t* rgba32_buf, int width, int height)
{
    int tile = current_tile;
    if (!gfx_gx2_is_valid_tile(tile) || !gfx_gx2_is_valid_texture_id(current_texture_ids[tile])) {
        gfx_gx2_log_invalid_sampler_state("upload_texture", tile, current_texture_ids[tile], 0);
        return;
    }
    GX2Texture& texture = gx2_textures[current_texture_ids[tile]].texture;

    texture.surface.use       = GX2_SURFACE_USE_TEXTURE;
    texture.surface.dim       = GX2_SURFACE_DIM_TEXTURE_2D;
    texture.surface.width     = width;
    texture.surface.height    = height;
    texture.surface.depth     = 1;
    texture.surface.mipLevels = 1;
    texture.surface.format    = GX2_SURFACE_FORMAT_UNORM_R8_G8_B8_A8;
    texture.surface.aa        = GX2_AA_MODE1X;
    texture.surface.tileMode  = GX2_TILE_MODE_LINEAR_ALIGNED;
    texture.viewFirstMip      = 0;
    texture.viewNumMips       = 1;
    texture.viewFirstSlice    = 0;
    texture.viewNumSlices     = 1;
    texture.surface.swizzle   = 0;
    texture.surface.alignment = 0;
    texture.surface.pitch     = 0;

    for (uint32_t i = 0; i < 13; i++)
        texture.surface.mipLevelOffset[i] = 0;

    texture.viewFirstMip   = 0;
    texture.viewNumMips    = 1;
    texture.viewFirstSlice = 0;
    texture.viewNumSlices  = 1;
    texture.compMap        = 0x00010203;

    for (uint32_t i = 0; i < 5; i++)
        texture.regs[i] = 0;

    GX2CalcSurfaceSizeAndAlignment(&texture.surface);
    GX2InitTextureRegs(&texture);

    texture.surface.image = memalign(texture.surface.alignment, texture.surface.imageSize);
    if (texture.surface.image == nullptr) {
        WHBLogPrintf("gfx: texture allocation failed, size=%u align=%u",
                     (unsigned)texture.surface.imageSize,
                     (unsigned)texture.surface.alignment);
        return;
    }
    GX2Invalidate(GX2_INVALIDATE_MODE_CPU_TEXTURE, texture.surface.image, texture.surface.imageSize);

    GX2Surface surf;
    surf = texture.surface;
    surf.tileMode = GX2_TILE_MODE_LINEAR_SPECIAL;
    surf.image = (void*)rgba32_buf;
    GX2CalcSurfaceSizeAndAlignment(&surf);
    GX2Invalidate(GX2_INVALIDATE_MODE_CPU_TEXTURE, (void*)rgba32_buf, surf.imageSize);

    GX2CopySurface(&surf, 0, 0, &texture.surface, 0, 0);

    if (current_shader_program) {
        uint32_t sampler_loc = current_shader_program->samplers_location[tile];
        if (sampler_loc < 32) {
            GX2SetPixelTexture(&texture, sampler_loc);
        } else {
            gfx_gx2_log_invalid_sampler_state("upload_set_pixel_texture", tile, current_texture_ids[tile], sampler_loc);
        }
    }

    gx2_textures[current_texture_ids[tile]].texture_uploaded = true;

    //WHBLogPrint("Texture set.");
}

static GX2TexClampMode gfx_cm_to_gx2(uint32_t val)
{
    if (val & G_TX_CLAMP)
        return GX2_TEX_CLAMP_MODE_CLAMP;
    else if (val & G_TX_MIRROR)
        return GX2_TEX_CLAMP_MODE_MIRROR;
    else
        return GX2_TEX_CLAMP_MODE_WRAP;
}

static void gfx_gx2_set_sampler_parameters(int tile, bool linear_filter, uint32_t cms, uint32_t cmt)
{
    if (!gfx_gx2_is_valid_tile(tile) || !gfx_gx2_is_valid_texture_id(current_texture_ids[tile])) {
        gfx_gx2_log_invalid_sampler_state("set_sampler_parameters", tile, current_texture_ids[tile], 0);
        return;
    }

    current_tile = tile;

    uint32_t texture_id = current_texture_ids[tile];
    GX2Sampler* sampler = &gx2_textures[texture_id].sampler;
    gx2_textures[texture_id].sampler_set = true;
    GX2InitSampler(sampler, GX2_TEX_CLAMP_MODE_CLAMP, linear_filter ? GX2_TEX_XY_FILTER_MODE_LINEAR : GX2_TEX_XY_FILTER_MODE_POINT);
    GX2InitSamplerClamping(sampler, gfx_cm_to_gx2(cms), gfx_cm_to_gx2(cmt), GX2_TEX_CLAMP_MODE_WRAP);

    if (current_shader_program) {
        uint32_t sampler_loc = current_shader_program->samplers_location[tile];
        if (sampler_loc < 32) {
            GX2SetPixelSampler(sampler, sampler_loc);
        } else {
            gfx_gx2_log_invalid_sampler_state("set_sampler", tile, current_texture_ids[tile], sampler_loc);
        }
    } else if (sNullShaderSamplerSetCount < 10) {
        WHBLogPrintf("gfx: set_sampler_parameters without active shader tile=%d tex=%u", tile, texture_id);
        sNullShaderSamplerSetCount++;
    }
}

static void gfx_gx2_set_depth_test(bool depth_test)
{
    current_depth_test = depth_test;
    GX2SetDepthOnlyControl(current_depth_test, current_depth_write, current_depth_compare);
}

static void gfx_gx2_set_depth_mask(bool z_upd)
{
    current_depth_write = z_upd;
    GX2SetDepthOnlyControl(current_depth_test, current_depth_write, current_depth_compare);
}

static void gfx_gx2_set_zmode_decal(bool zmode_decal)
{
    if (zmode_decal)
    {
        GX2SetPolygonControl(GX2_FRONT_FACE_CCW, FALSE, FALSE, TRUE,
                             GX2_POLYGON_MODE_TRIANGLE, GX2_POLYGON_MODE_TRIANGLE,
                             TRUE, TRUE, FALSE);

        GX2SetPolygonOffset(-2.0f, -2.0f, -2.0f, -2.0f, 0.0f );
    }
    else
    {
        GX2SetPolygonControl(GX2_FRONT_FACE_CCW, FALSE, FALSE, FALSE,
                             GX2_POLYGON_MODE_TRIANGLE, GX2_POLYGON_MODE_TRIANGLE,
                             FALSE, FALSE, FALSE);

        GX2SetPolygonOffset( 0.0f,  0.0f,  0.0f,  0.0f, 0.0f );
    }
}

static void gfx_gx2_set_viewport(int x, int y, int width, int height)
{
    GX2SetViewport(x, g_window_height - y - height, width, height, 0.0f, 1.0f);
    current_height = height;
}

static void gfx_gx2_set_scissor(int x, int y, int width, int height)
{
    GX2SetScissor(x, g_window_height - y - height, width, height);
}

static void gfx_gx2_set_use_alpha(bool use_alpha)
{
    if (use_alpha)
    {
        GX2SetBlendControl(GX2_RENDER_TARGET_0,
                           GX2_BLEND_MODE_SRC_ALPHA, GX2_BLEND_MODE_INV_SRC_ALPHA,
                           GX2_BLEND_COMBINE_MODE_ADD, FALSE,
                           GX2_BLEND_MODE_ZERO, GX2_BLEND_MODE_ZERO,
                           GX2_BLEND_COMBINE_MODE_ADD);

        GX2SetColorControl(GX2_LOGIC_OP_COPY, 1, FALSE, TRUE);
    }
    else
    {
        GX2SetBlendControl(GX2_RENDER_TARGET_0,
                           GX2_BLEND_MODE_ONE, GX2_BLEND_MODE_ZERO,
                           GX2_BLEND_COMBINE_MODE_ADD, FALSE,
                           GX2_BLEND_MODE_ZERO, GX2_BLEND_MODE_ZERO,
                           GX2_BLEND_COMBINE_MODE_ADD);

        GX2SetColorControl(GX2_LOGIC_OP_COPY, 0, FALSE, TRUE);
    }
}

static void gfx_gx2_draw_triangles(float buf_vbo[], size_t buf_vbo_len, size_t buf_vbo_num_tris)
{
    if (!current_shader_program)
        return;

    if (!sLoggedFirstDrawCall) {
        sLoggedFirstDrawCall = true;
        WHBLogPrintf("gfx: first draw shader=0x%08x tris=%u stride_f=%u",
                     (unsigned)current_shader_program->shader_id,
                     (unsigned)buf_vbo_num_tris,
                     (unsigned)current_shader_program->num_floats);
    }

    size_t idx = vbo_array.size();
    vbo_array.resize(idx + 1);

    size_t vbo_len = sizeof(float) * buf_vbo_len;
    vbo_array[idx] = static_cast<float*>(memalign(0x40, vbo_len));

    float* new_vbo = vbo_array[idx];
    memcpy(new_vbo, buf_vbo, vbo_len);

    GX2Invalidate(GX2_INVALIDATE_MODE_CPU_ATTRIBUTE_BUFFER, new_vbo, vbo_len);

    GX2SetAttribBuffer(0, vbo_len, sizeof(float) * current_shader_program->num_floats, new_vbo);
    GX2DrawEx(GX2_PRIMITIVE_MODE_TRIANGLES, 3 * buf_vbo_num_tris, 0, 1);
}

static void gfx_gx2_init(void)
{
}

static void gfx_gx2_on_resize(void)
{
}

static void gfx_gx2_start_frame(void)
{
    frame_count++;
}

static void gfx_gx2_end_frame(void)
{
}

static void gfx_gx2_finish_render(void)
{
    // Wait until drawing is done
    GX2DrawDone();
    // Free resources
    gfx_gx2_free_vbo();
}

extern "C" void gfx_gx2_free_vbo(void)
{
    for (uint32_t i = 0; i < vbo_array.size(); i++)
        free(vbo_array[i]);

    vbo_array.clear();
}

extern "C" void gfx_gx2_free(void)
{
    // Free our textures and shaders
    for (uint32_t i = 0; i < gx2_texture_count; i++)
    {
        Texture& texture = gx2_textures[i];
        if (texture.texture_uploaded)
            free(texture.texture.surface.image);
    }

    for (uint32_t i = 0; i < shader_program_pool_size; i++)
    {
        if (shader_program_pool[i].is_precompiled)
            WHBGfxFreeShaderGroup(&shader_program_pool[i].whb_group);

        else
            gx2FreeShaderGroup(&shader_program_pool[i].gen_group);
    }

    gx2_texture_count = 0;
    shader_program_pool_size = 0;
}

struct GfxRenderingAPI gfx_gx2_api = {
    gfx_gx2_z_is_from_0_to_1,
    gfx_gx2_unload_shader,
    gfx_gx2_load_shader,
    gfx_gx2_create_and_load_new_shader,
    gfx_gx2_lookup_shader,
    gfx_gx2_shader_get_info,
    gfx_gx2_new_texture,
    gfx_gx2_select_texture,
    gfx_gx2_upload_texture,
    gfx_gx2_set_sampler_parameters,
    gfx_gx2_set_depth_test,
    gfx_gx2_set_depth_mask,
    gfx_gx2_set_zmode_decal,
    gfx_gx2_set_viewport,
    gfx_gx2_set_scissor,
    gfx_gx2_set_use_alpha,
    gfx_gx2_draw_triangles,
    gfx_gx2_init,
    gfx_gx2_on_resize,
    gfx_gx2_start_frame,
    gfx_gx2_end_frame,
    gfx_gx2_finish_render
};

#endif

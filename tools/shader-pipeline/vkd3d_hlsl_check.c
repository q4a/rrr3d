/*
 * Compile one .fx entry point to Direct3D 9 shader model 3 with vkd3d-shader.
 *
 * This is the native replacement for the D3DCompile-under-wine step the rest
 * of this directory uses. That step proved the shaders survive the toolchain;
 * it could never ship, because it runs Microsoft's d3dcompiler under Wine.
 * vkd3d-shader does the same job in-process, which is what the runtime effects
 * framework is built on.
 *
 * Build and run through vkd3d_hlsl_check.py, which drives it over every entry
 * point in every .fx file.
 *
 *   usage: vkd3d_hlsl_check <shader_dir> <file.fx> <profile> <entry_point>
 *
 * Exit status is 0 when the entry point compiled.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <vkd3d_shader.h>

static char shader_dir[1024];

/*
 * The .fx files #include shared fragments -- consts.fx, model.fx, lighting.fx
 * -- so the preprocessor needs to resolve them. Everything is in one directory,
 * and `local` does not matter here because there is nowhere else to look.
 */
static int open_include(const char *filename, bool local, const char *parent_data,
        void *context, struct vkd3d_shader_code *out)
{
    char path[2048];
    FILE *file;
    long size;
    void *buffer;

    snprintf(path, sizeof(path), "%s/%s", shader_dir, filename);

    if (!(file = fopen(path, "rb")))
    {
        fprintf(stderr, "  include not found: %s\n", filename);
        return VKD3D_ERROR;
    }

    fseek(file, 0, SEEK_END);
    size = ftell(file);
    fseek(file, 0, SEEK_SET);

    if (!(buffer = malloc(size)) || fread(buffer, 1, size, file) != (size_t)size)
    {
        fclose(file);
        free(buffer);
        return VKD3D_ERROR;
    }
    fclose(file);

    out->code = buffer;
    out->size = size;
    return VKD3D_OK;
}

static void close_include(const struct vkd3d_shader_code *code, void *context)
{
    free((void *)code->code);
}

int main(int argc, char **argv)
{
    struct vkd3d_shader_code out = {0};
    char *messages = NULL;
    char path[2048];
    FILE *file;
    long size;
    void *source;
    int ret;

    if (argc != 5)
    {
        fprintf(stderr, "usage: %s <shader_dir> <file.fx> <profile> <entry_point>\n", argv[0]);
        return 2;
    }

    snprintf(shader_dir, sizeof(shader_dir), "%s", argv[1]);
    snprintf(path, sizeof(path), "%s/%s", shader_dir, argv[2]);

    if (!(file = fopen(path, "rb")))
    {
        fprintf(stderr, "cannot open %s\n", path);
        return 2;
    }

    fseek(file, 0, SEEK_END);
    size = ftell(file);
    fseek(file, 0, SEEK_SET);

    if (!(source = malloc(size)) || fread(source, 1, size, file) != (size_t)size)
    {
        fprintf(stderr, "cannot read %s\n", path);
        return 2;
    }
    fclose(file);

    struct vkd3d_shader_preprocess_info preprocess =
    {
        .type = VKD3D_SHADER_STRUCTURE_TYPE_PREPROCESS_INFO,
        .pfn_open_include = open_include,
        .pfn_close_include = close_include,
    };
    struct vkd3d_shader_hlsl_source_info hlsl =
    {
        .type = VKD3D_SHADER_STRUCTURE_TYPE_HLSL_SOURCE_INFO,
        .next = &preprocess,
        .entry_point = argv[4],
        .profile = argv[3],
    };
    struct vkd3d_shader_compile_info info =
    {
        .type = VKD3D_SHADER_STRUCTURE_TYPE_COMPILE_INFO,
        .next = &hlsl,
        .source = {.code = source, .size = size},
        .source_type = VKD3D_SHADER_SOURCE_HLSL,
        .target_type = VKD3D_SHADER_TARGET_D3D_BYTECODE,
        .log_level = VKD3D_SHADER_LOG_ERROR,
        .source_name = argv[2],
    };

    ret = vkd3d_shader_compile(&info, &out, &messages);

    if (ret == VKD3D_OK)
        printf("OK   %-20s %-7s %-24s %zu bytes\n", argv[2], argv[3], argv[4], out.size);
    else
        printf("FAIL %-20s %-7s %-24s ret=%d\n%s\n", argv[2], argv[3], argv[4], ret,
                messages ? messages : "(no compiler output)");

    return ret == VKD3D_OK ? 0 : 1;
}

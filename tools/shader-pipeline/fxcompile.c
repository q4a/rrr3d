/* Compiles HLSL to D3D9 SM3 bytecode with a named entry point.
 * Like d9mt's hlsl2dxso but takes the entry point, since the game's .fx
 * files name their shaders (ClearSurf, ModelVS, ...) rather than "main".
 * Runs under wine.
 *   usage: fxcompile.exe <in.hlsl> <entry> <profile> <out.bin>
 */
#define COBJMACROS
#include <windows.h>
#include <d3dcompiler.h>
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv) {
  if (argc != 5) {
    fprintf(stderr, "usage: fxcompile <in.hlsl> <entry> <profile> <out.bin>\n");
    return 2;
  }

  FILE *f = fopen(argv[1], "rb");
  if (!f) { fprintf(stderr, "cannot open %s\n", argv[1]); return 1; }
  fseek(f, 0, SEEK_END);
  long len = ftell(f);
  fseek(f, 0, SEEK_SET);
  char *src = malloc(len + 1);
  fread(src, 1, len, f);
  src[len] = 0;
  fclose(f);

  ID3DBlob *code = NULL, *errors = NULL;
  HRESULT hr = D3DCompile(src, len, argv[1], NULL, NULL,
                          argv[2], argv[3], 0, 0, &code, &errors);

  if (FAILED(hr)) {
    if (errors)
      fprintf(stderr, "%s", (const char *)ID3D10Blob_GetBufferPointer(errors));
    else
      fprintf(stderr, "D3DCompile failed: 0x%08lx\n", (unsigned long)hr);
    return 1;
  }

  FILE *out = fopen(argv[4], "wb");
  if (!out) { fprintf(stderr, "cannot write %s\n", argv[4]); return 1; }
  fwrite(ID3D10Blob_GetBufferPointer(code), 1,
         ID3D10Blob_GetBufferSize(code), out);
  fclose(out);
  printf("%lu\n", (unsigned long)ID3D10Blob_GetBufferSize(code));
  return 0;
}

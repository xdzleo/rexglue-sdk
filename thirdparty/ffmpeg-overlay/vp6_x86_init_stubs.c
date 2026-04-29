// Empty x86 SIMD init stubs for the VP6 decode path. The C code in
// libavcodec/{vp6dsp.c,vp3dsp.c,h264chroma.c,hpeldsp.c,videodsp.c} calls
// these to register x86 SIMD kernels, but our build doesn't compile the
// .asm files (no NASM/yasm in the rex SDK build pipeline). Providing
// empty stubs satisfies the linker; the resulting decoder uses the
// portable C kernels which is fine for low-throughput VP6 intro videos.
#include <stdint.h>

struct VP6DSPContext;
struct VP3DSPContext;
struct H264ChromaContext;
struct HpelDSPContext;
struct VideoDSPContext;

void ff_vp6dsp_init_x86(struct VP6DSPContext *s)             { (void)s; }
void ff_vp3dsp_init_x86(struct VP3DSPContext *c, int flags)  { (void)c; (void)flags; }
void ff_h264chroma_init_x86(struct H264ChromaContext *c, int bit_depth) { (void)c; (void)bit_depth; }
void ff_hpeldsp_init_x86(struct HpelDSPContext *c, int flags) { (void)c; (void)flags; }
void ff_videodsp_init_x86(struct VideoDSPContext *ctx, int bpc) { (void)ctx; (void)bpc; }

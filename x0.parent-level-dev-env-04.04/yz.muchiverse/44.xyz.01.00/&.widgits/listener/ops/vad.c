/* vad - find speech spans in a WAV: vad <in.wav> [hangover_ms=700] [min_ms=150] [--split <outdir>]       (BOOK 19.audio/LISTENER-STT-DESIGN.md, slice L0)
 * Prints one line per span:  SEG | start_ms | end_ms   and a final  SEGMENTS | n.  With --split writes each span as <outdir>/seg_<k>.wav. Energy VAD (adsp_vad in _shared-lib/audio_dsp.c): it finds LOUD stretches, not speech as such.
 * Exit 0 = at least one span, 3 = none (silence / steady noise), 1 = unreadable, 2 = usage. Pure and deterministic. Text-includes ../../_shared-lib/audio_dsp.c; link -lm. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdint.h>
#include "../../_shared-lib/audio_dsp.c"
int main(int argc, char **argv) {
    if (argc < 2) { fprintf(stderr, "usage: vad in.wav [hangover_ms] [min_ms] [--split outdir]\n"); return 2; } int hang = 700, minms = 150; const char *split = NULL; int pos = 0;
    for (int i = 2; i < argc; i++) { if (!strcmp(argv[i], "--split") && i + 1 < argc) split = argv[++i]; else if (pos == 0) { hang = atoi(argv[i]); pos++; } else { minms = atoi(argv[i]); pos++; } }
    if (hang < 100 || hang > 5000 || minms < 20 || minms > 5000) { fprintf(stderr, "hangover 100..5000 ms, min 20..5000 ms\n"); return 2; }
    short *s; long n; int sr; if (adsp_read_wav(argv[1], &s, &n, &sr)) { fprintf(stderr, "cannot read %s as 16-bit PCM wav\n", argv[1]); return 1; }
    AdspSeg seg[64]; int k = adsp_vad(s, n, sr, hang, minms, seg, 64);
    for (int i = 0; i < k; i++) { printf("SEG | %ld | %ld\n", seg[i].start * 1000 / sr, seg[i].end * 1000 / sr); if (split) { char p[4096]; snprintf(p, sizeof p, "%s/seg_%d.wav", split, i + 1); adsp_write_wav(p, s + seg[i].start, seg[i].end - seg[i].start, sr); } }
    printf("SEGMENTS | %d\n", k); free(s); return k ? 0 : 3;
}

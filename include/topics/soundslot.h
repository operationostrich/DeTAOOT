/* SoundChannelSlot, 104 = 0x68 bytes. Header #included mid-file by sound_channels.c and movie_player.c only.
 * Cost: 2 C1 symbols (tag + typedef name). Replaces the SIX function-local
 * `typedef struct { int d[0x1a]; } SoundChannelSlot;` in src/sound/sound_channels.c (-12 symbols there,
 * compensated by a phase row on the NEXT function of each, match/symbol_phase.json).
 * Instances: gSoundSlots[4] at 0x00489ef8 (ends on gSoundStereo 0x0048a098, so slot 3 = 0x0048a030) and
 * gMovieVoiceRow at 0x0048cd40 (ends on 0x0048cda8). Offsets: GetActiveTrackDescriptor family symmetry
 * (+8 / +0x24 / +0x28 per slot) and the tsv aliases; +0x59..+0x67 is one GainTriple.
 * Storage is NOT retyped (the globals table keeps int[104] / byte[104]): the views below are
 * macros, which cost no symbol. */
#pragma pack(push, 1)
typedef struct SoundChannelSlot {
    int        key;          /* +0x00 priority key (0 = idle)                        gSoundSlots[0] */
    int        unk04;        /* +0x04 */
    byte       name[16];     /* +0x08 Pascal name  (DAT_00489f00 / f68 / fd0 / a038 / 0048cd48) */
    int        flag;         /* +0x18 DAT_00489f10 (loop-gate node, SetNodeFlagValue) */
    byte       unk1c[8];     /* +0x1c */
    int        aux;          /* +0x24 DAT_00489f1c / f84 / fec / a054 */
    int        handle;       /* +0x28 DAT_00489f20 / f88 / ff0 / a058 */
    byte       unk2c[10];    /* +0x2c */
    int        templateKey;  /* +0x36 DAT_00489f2e: unaligned (pack 1 is REQUIRED) */
    byte       unk3a[31];    /* +0x3a */
    GainTriple gain;         /* +0x59 DAT_00489f51/55/59/5d/5f are its fields; 0x59 + 15 = 0x68 */
} SoundChannelSlot;          /* 0x68 */
#pragma pack(pop)
/* typed views over the unchanged storage (no symbol) */
#define gSoundSlotTbl (*(SoundChannelSlot (*)[4])gSoundSlots)
#define gMovieVoice   (*(SoundChannelSlot *)gMovieVoiceRow)

/* link_dead.c -- linked by match/build.py after the game units; never part of the port. It stands
 * for code TI.EXE's source had and /OPT:REF discarded, whose references still shaped the link,
 * where that code is not source of its own:
 *  - a use of the double 1.0: the unreferenced 1.0 TI.EXE has at 0x0045a090, before testfdiv's
 *    constants (a non-COMDAT .rdata constant survives /OPT:REF);
 *  - dead_user: the anchor of the three USER32 import stand-ins (import_order.DEAD_REFS).
 * Which original functions held them is unknown. */
double dead_one(double x) { return x + 1.0; }
int dead_user(void) { return 0; }

/* .reloc: LINK 4.20 makes the section 2 bytes per DIR32 relocation in every input it loaded,
 * discarded code included, plus 10 per page of the other sections. TI.EXE's .reloc size implies
 * more discarded DIR32s than this tree loads; this function supplies the difference: 140 (the
 * discarded C keepers of the two naked functions bring 149 DIR32s of their own, and the display-
 * attach loader thread in container_store.c, compiled uncalled, more). Each store is one DIR32.
 * When dead code is reconstructed as source, or a function's relocation count changes, adjust the
 * count here. */
#define R1(i)  *(volatile int *)((char *)dead_one + (i)) = 0;
#define R4(i)  R1(i) R1(i + 4) R1(i + 8) R1(i + 12)
#define R16(i) R4(i) R4(i + 16) R4(i + 32) R4(i + 48)
#define R64(i) R16(i) R16(i + 64) R16(i + 128) R16(i + 192)
void dead_relocs(void)
{
    R64(0) R64(256)                                 /* 128 */
    R4(512) R4(528) R4(544)                         /* 140 */
}

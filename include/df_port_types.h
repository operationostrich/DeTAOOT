/* df_port_types.h -- types that used to be private to one source file and are
 * now needed across files (the translation units, the prototypes in
 * df_protos.h). Deliberately NOT force-included: the byte-match harness gives
 * each test file only the declarations its source file had, and a typedef
 * added to every file moved register allocation in 70 functions. */
#ifndef DF_PORT_TYPES_H
#define DF_PORT_TYPES_H

/* -----------------------------------------------------------------------
 * ContainerFileHandle -- 36-byte file-handle struct (9 ints) created by
 * OpenContainerFile and stored inline at node+0x000 in each container node.
 * --------------------------------------------------------------------- */
typedef struct {
    HANDLE           hFile;    /* +0x00  Win32 file handle                   */
    int              filePos;  /* +0x04  current read position (lazy seek)   */
    CRITICAL_SECTION critsec;  /* +0x08  24-byte Win32 CRITICAL_SECTION      */
    volatile int     state;    /* +0x20  busy flag another thread reads; volatile: the exe keeps the 1-then-0 stores */
} ContainerFileHandle;         /* total: 36 bytes                            */

/* Node descriptor structure (22 bytes) used in local_94.
 * Packed: natural 4-byte alignment would pad pNodeObj to offset 0x14, but
 * ground truth (0x0041f860, QdSetPort called on `param_1 + 0x12`) reads
 * it at offset 0x12. The unpacked layout silently read 2 bytes of padding +
 * 2 bytes of whatever follows the struct in memory as the node handle,
 * which is what produced a garbage hNode into QdSetPortWindow on click. */
#pragma pack(push, 1)
typedef struct
{
    short  nodeType;    /* [0]   type code (1, 2, ...) */
    short  data[8];     /* [1-8] payload */
    void  *pNodeObj;    /* [+0x12] pointer to the underlying scene-node object;
                         *         first field of *pNodeObj is the node's HWND */
} NodeDesc;
#pragma pack(pop)

#pragma pack(push, 1)
typedef struct {
    int           value;    /* gain = value / 2^shift */
    unsigned char shift;
} FixedGain;                /* 5 bytes */
typedef struct {
    FixedGain a;            /* +0x00 */
    FixedGain b;            /* +0x05 */
    FixedGain c;            /* +0x0a */
} GainTriple;               /* 15 bytes */
#pragma pack(pop)

/* A hotspot plane-row decoder: three stack arguments (hotspot_raster.c). */
typedef void (*HotspotPlaneFn)(byte *pRle, int deltaPx, int planeIdx);

/* Buffer allocation return pair -- MSVC 32-bit returns two pointers in EAX / EDX
 * (AllocateHotspotPixelBuffer, scene.c). */
typedef struct {
    void *handle;   /* EAX -- HBITMAP DIB section */
    void *pPixels;  /* EDX -- device-independent bitmap bits */
} HotspotAllocPair;


/* The heap's per-thread state, one TLS struct as TI.EXE lays it out (heap_resources.c):
 * +8 the active descriptor, +0xc a 33-entry stack used [1..32], +0x90 the depth. */
typedef struct {
    int *active;
    int  stack[33];
    int  depth;
} HeapTls;
extern __thread HeapTls g_heapTls;
#define g_pActiveHeapDesc g_heapTls.active
#define g_heapCtxStack    g_heapTls.stack
#define g_heapCtxDepth    g_heapTls.depth

#endif /* DF_PORT_TYPES_H */

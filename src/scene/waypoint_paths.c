/* waypoint_paths.c -- TI.EXE 0x0040a290..0x0040add0: waypoint paths and the scene / view row
 * lookups. */

#include <windows.h>
#include <mmsystem.h>
#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include "df_protos.h"
#include "topics/sound.h"
#include "topics/scriptcmd.h"
#include "topics/scriptprop.h"



#include "topics/savestate.h"

/* Finds the waypoint entry closest to the 3D position at pPos+2/+4/+6
 * (as packed shorts) by IntSqrt distance.  Writes pPos's position into
 * that entry, then memmoves the tail (from that entry onward) to the front
 * of the list, truncating entries before it.  Recalculates cumulative
 * distances for the rotated list.  Releases hPath via UnlockResObject. */
void __cdecl RotateWaypointPathToNearestEntry(int pPos, int *hPath)  /* 0x0040A290 */
{
    typedef struct { short x, y, z, dist; } WayPt;
    typedef struct { int total; int hdr1; int count; int hdr3; int hdr4; WayPt e[1]; } WayPath;
    int      nearest;
    WayPath *path;
    WayPt    cur;
    WayPt    prev;
    WayPt   *pe;
    int      d;
    int      best;
    int      total;
    int      i;
    int      count;
    int      dx;
    int      dy;
    int      dz;

    nearest = -1;
    best    = 0x7fffffff;
    path    = (WayPath *)GetResDataPtr(hPath);
    count   = path->count;
    for (i = 0; i < count; i++) {
        cur = path->e[i];
        dx = *(short *)(pPos + 2) - cur.x;
        dy = *(short *)(pPos + 4) - cur.y;
        dz = *(short *)(pPos + 6) - cur.z;
        d = (unsigned short)IntSqrt(dx * dx + dy * dy + dz * dz);
        if (d < best) {
            best    = d;
            nearest = i;
        }
    }
    if (nearest < 0) {
        UnlockResObject(hPath);
        return;
    }
    if (count - nearest == 1)
        nearest = count - 2;
    path->e[nearest].x = ((WayPt *)(pPos + 2))->x;
    path->e[nearest].y = ((WayPt *)(pPos + 2))->y;
    path->e[nearest].z = ((WayPt *)(pPos + 2))->z;
    pe = path->e;
    memmove(pe, pe + nearest, (count - nearest) * 8);
    count -= nearest;
    path->count = count;
    total = 0;
    for (i = 0; i < count; i++) {
        cur = *pe;
        if (i == 0) {
            cur.dist = 0;
        } else {
            prev = pe[-1];
            dx = cur.x - prev.x;
            dy = cur.y - prev.y;
            dz = cur.z - prev.z;
            cur.dist = IntSqrt(dx * dx + dy * dy + dz * dz);
        }
        total += cur.dist;
        *pe = cur;
        pe++;
    }
    path->total = total;
    UnlockResObject(hPath);
}


/* Attaches child render node at index `selector` from the trigger directory
 * (DAT_0048a780), clones the CLUT resource via CloneResource, then detaches
 * the child.  Returns the cloned CLUT handle. */
uint32_t __cdecl CloneRenderChildResource(uint32_t selector)  /* 0x0040a470 */
{
    short    attachStatus;
    short detachStatus;
    uint32_t hClone;

    attachStatus = AttachRenderChild((int *)DAT_0048a780, selector, (int **)&hClone);
    if (attachStatus != 0) {
        FatalError(attachStatus, 0x1011);
    }
    CloneResource((void **)&hClone);
    detachStatus = DetachRenderChild((int)DAT_0048a780, selector);
    if (detachStatus != 0) {
        FatalError(detachStatus, 0x1012);
    }
    return hClone;
}


/* Reverses a waypoint path buffer in-place.  Phase 1: re-stamps each
 * entry's distance field by borrowing the next entry's upper short.
 * Phase 2: swaps entry pairs from the two ends toward the middle.
 * Releases hPath via UnlockResObject. */
void __cdecl ReverseWaypointPathDirection(int *hPath)  /* 0x0040A4E0 */
{
    /* One 8-byte path entry: position, adjusted Y, cumulative distance. The exe
     * copies whole entries through two 8-byte stack temporaries. */
    typedef struct { int pos; short y; short dist; } WayPt;
    typedef struct { int hdr0; int hdr1; int count; int hdr3; int hdr4; WayPt e[1]; } WayPath;
    WayPt  t;
    WayPt  n;
    int    path, count, i;
    WayPt *p, *q;

    path = GetResDataPtr(hPath);
    count = *(int *)(path + 8);
    for (i = 1; i < count; i++) {
        t = ((WayPath *)path)->e[i - 1];
        n = ((WayPath *)path)->e[i];
        t.dist = n.dist;
        ((WayPath *)path)->e[i - 1] = t;
    }
    q = &((WayPath *)path)->e[count - 1];
    t = *q;
    count /= 2;
    t.dist = 0;
    *q = t;
    if (0 < count) {
        p = (WayPt *)(path + 0x14);
        do {
            t  = *p;
            *p = *q;
            *q = t;
            q--;
            p++;
        } while (--count != 0);
    }
    UnlockResObject(hPath);
}


/* Attaches the render node for selector from DAT_0048a780, reads its entry count and per-entry
 * positions (stride 0x3c), allocates a path buffer (count * 8 + 0x14 bytes), computes cumulative
 * IntSqrt distances between consecutive entries, detaches the node and returns the path handle
 * through *phPath. The first entry has distance 0; each later entry stores its distance and its
 * adjusted Y in word 1. */
void __cdecl BuildAnimWaypointPath(uint32_t selector, int **phPath)  /* 0x0040A5B0 */
{
    /* 8-byte path entry {x, y, z, cumulative distance}; header: total length
     * at +0, 0 at +4, count at +8, entries at +0x14 (see ReverseWaypointPathDirection).
     * The exe builds each entry in an 8-byte stack temporary and keeps a
     * whole-entry copy of the previous one. */
    typedef struct { short x, y, z, dist; } WayPt;
    typedef struct { int total; int hdr1; int count; int hdr3; int hdr4; WayPt e[1]; } WayPath;
    WayPt    cur;
    int     *hPath;
    WayPt   *pe;
    WayPt    prev;
    int      count;
    char    *src;
    int      i;
    WayPath *path;
    int     *hChild;  /* local_c */
    short    status;
    int      dx, dy;
    int      total;

    status = AttachRenderChild((int *)DAT_0048a780, selector, (int **)&hChild);
    if (status != 0)
        FatalError(status, 0x1013);
    count = GetResDataPtr(hChild);
    src   = (char *)count + 0xc;
    count = *(int *)(count + 4);
    if (count <= 1)
        FatalError(0, 0x1014);
    hPath = (int *)AllocResObject((unsigned int)(count * 8 + 0x14));
    path  = (WayPath *)GetResDataPtr(hPath);
    pe    = path->e;
    total = 0;
    for (i = 0; i < count; i++) {
        cur.x = *(short *)(src + 0x20);
        cur.y = *(short *)(src + 0x22);
        cur.z = *(short *)(src + 0x24) - DAT_0048a794;
        if (i <= 0) {
            cur.dist = 0;
        } else {
            dx = cur.x - prev.x;
            dy = cur.y - prev.y;
            cur.dist = IntSqrt(dy * dy + dx * dx + (cur.z - prev.z) * (cur.z - prev.z));
        }
        total += cur.dist;
        *pe = cur;
        prev = cur;
        src += 0x3c;
        pe++;
    }
    path->total = total;
    path->hdr1  = 0;
    path->count = count;
    UnlockResObject(hPath);
    UnlockResObject(hChild);
    status = (short)DetachRenderChild((int)DAT_0048a780, selector);
    if (status != 0)
        FatalError(status, 0x1015);
    *phPath = hPath;
}


/* Resolves an animation sound entry by two names:
 *   pName1 - scene/stage descriptor (parsed via FindSceneRowByName to get
 *            child selectors viewListSel and pathListSel)
 *   pName2 - animation/target name searched in first child entries
 *            (stride 0x2e, name at entry+0x1e)
 *
 * Searches the second child entries (stride 0x3c) for the index matched
 * above and returns entry[+0x34].  Returns 0xffffffff on failure. */
uint __cdecl ResolveWalkPathSelectorForView(byte *pName1, byte *pName2)  /* 0x0040a760 */
{
    short      status;
    uint       pathSelector;
    undefined4 pathCount;
    int        pRow;
    int        data;
    int        i;
    int        viewIdx;
    int       *hChild;  /* local_30 */
    /* ONE 44-byte record in the original frame (ebp-0x2c..): FindSceneRowByName copies the
     * 42-byte scene row through the pointer it is handed, the child selectors are fields of it
     * (offsets 0x2c minus each Ghidra name) and the name is at +0x1a. Declared as three separate
     * locals the port left viewListSel / pathListSel unwritten (class 3, stack adjacency), and a
     * buffer shorter than 42 bytes smashes the frame. */
    byte       rec2c[44];
#define sceneRow  ((undefined4 *)&rec2c[0])
#define viewListSel  (*(uint *)&rec2c[10])
#define pathListSel  (*(uint *)&rec2c[14])

    if (gSet.active == 0)
        return 0xffffffff;

    status = FindSceneRowByName(pName1, sceneRow);
    if (status != 0)
        return 0xffffffff;

    status = AttachRenderChild((int *)DAT_0048a780, viewListSel, &hChild);
    if (status != 0)
        FatalError(status, 0x1016);

    data = GetResDataPtr(hChild);
    viewIdx = -1;
    for (i = 0, pRow = data + 0x34; i < *(int *)(data + 0x30);
         pRow += 0x2e, i++) {
        if (PascalStringsEqualNoCase((byte *)(pRow + 0x1e), pName2) != 0) {
            viewIdx = i;
            break;
        }
    }
    UnlockResObject(hChild);
    status = DetachRenderChild((int)DAT_0048a780, viewListSel);
    if (status != 0)
        FatalError(status, 0x1017);

    if (viewIdx < 0)
        return 0xffffffff;

    status = AttachRenderChild((int *)DAT_0048a780, pathListSel, &hChild);
    if (status != 0)
        FatalError(status, 0x1018);

    data = GetResDataPtr(hChild);
    for (pRow = data + 0xc, i = 0, pathCount = *(int *)(data + 4); i < (int)pathCount;
         pRow += 0x3c, i++) {
        if (*(int *)(pRow + 0x38) == viewIdx) {
            viewIdx = -1;
            pathSelector = *(uint *)(pRow + 0x34);
            break;
        }
    }
    UnlockResObject(hChild);
    status = DetachRenderChild((int)DAT_0048a780, pathListSel);
    if (status != 0)
        FatalError(status, 0x1019);

    /* Not found: FatalError exits, so pathSelector is never read unset (the exe has
     * no default either). */
    if (viewIdx >= 0)
        FatalError(0, 0x101a);

    return pathSelector;
}


/* Resolves the scene's (gSceneName) view list through FindSceneRowByName. First pass: an entry
 * whose Pascal name (at +0x1e) equals gViewName (PascalStringsEqualNoCase) wins. Second pass, if
 * none matches: the entry whose direction angle (at +8) is closest to gCameraAngle, by
 * ComputeAngularDistance. Writes the winner's name to gViewName and commits both descriptors with
 * ShowSetView. FatalError 0x1020 if there is no entry. */
void __stdcall SelectViewByNameOrNearestAngle(void)  /* 0x0040a910 */
{
    /* The 42-byte playfield row (selector at +10, name at +0x1a), copied
     * whole (rep movsd 0xa + movsw). Declared in this order for the exe's
     * register allocation (byte-exact). */
    typedef struct { byte b[42]; } PfRow;
    short  st;
    short  noMatch;     /* 1 = nothing matched yet, 0 = match found */
    int    i;
    PfRow  row;
    byte  *ent;
    byte  *base;
    short  best;
    short  d;
    int   *hChild;
    byte  *p;

    if (gSet.active == 0)
        FatalError(0, 0x101b);
    if (FindSceneRowByName(&gSceneName, &row) != 0) {
        if (gSceneCount <= 0)
            FatalError(0, 0x101c);
        row = *(PfRow *)GetResDataPtr(gSceneTable);
        UnlockResObject(gSceneTable);
        CopyPascalString(row.b + 0x1a, &gSceneName);
    }
    st = AttachRenderChild(DAT_0048a780, *(uint32_t *)(row.b + 10), &hChild);
    if (st != 0)
        FatalError(st, 0x101d);
    base = (byte *)GetResDataPtr(hChild);
    ent  = base + 0x34;
    noMatch = 1;
    p = ent;
    for (i = 0; i < *(int *)(base + 0x30); i++) {
        if (PascalStringsEqualNoCase(p + 0x1e, &gViewName) != 0) {
            noMatch = 0;
            break;
        }
        p += 0x2e;
    }
    if (noMatch != 0) {
        best = 1000;
        if (*(int *)(base + 0x30) < 0)
            FatalError(0, 0x101e);
        for (i = 0; i < *(int *)(base + 0x30); i++) {
            d = ComputeAngularDistance(*(short *)(ent + 8), gCameraAngle);
            if (d < best) {
                best = d;
                noMatch = 0;
                CopyPascalString(ent + 0x1e, &gViewName);
            }
            ent += 0x2e;
        }
    }
    UnlockResObject(hChild);
    st = DetachRenderChild((int)DAT_0048a780, *(uint32_t *)(row.b + 10));
    if (st != 0)
        FatalError(st, 0x101f);
    if (noMatch != 0)
        FatalError(0, 0x1020);
    ShowSetView(&gSceneName, &gViewName);
}


/* Returns 1 if the scene named by pA has a view named pB (a PascalStringsEqualNoCase scan of its
 * view rows), 0 if not, and -1 (0xffff) when FindSceneRowByName fails to parse pA. */
short __cdecl SceneHasViewNamed(byte *pA, byte *pB) /* 0x0040aae0 */
{
    short  status;
    int    i, pRow, data;
    short  found;  /* local_32 */
    int   *hChild;  /* local_30 */
    /* the 42-byte scene row FindSceneRowByName fills (it was uint[5], class 3) */
    uint   parseOut[11];
    /* the child selector is the uint at BYTE +10, not index 2; read in place: the exe has no
     * local for it (frame 0x34) */
#define sel22 (*(uint *)((byte *)parseOut + 10))

    status = FindSceneRowByName(pA, parseOut);
    if (status != 0)
        return (short)0xffff;
    status = AttachRenderChild((int *)DAT_0048a780, sel22, &hChild);
    if (status != 0)
        FatalError(status, 0x1021);
    data = GetResDataPtr(hChild);
    found = 0;
    for (i = 0, pRow = data + 0x34; i < *(int *)(data + 0x30); pRow += 0x2e, i++) {
        if (PascalStringsEqualNoCase((byte *)(pRow + 0x1e), pB) != 0) {
            found = 1;
            break;
        }
    }
#if DF_TRACE_ENABLED
    /* `currentview` answers "No such object" (status 10) when this returns 0.
     * Print what was actually searched: the set descriptor, the wanted view,
     * the row count and the first few row names (+0x1e, stride 0x2e). */
    {
        int k, n = *(int *)(data + 0x30);
        DF_TRACE("viewLookup: set='%.*s' want='%.*s' rows=%d found=%d sel=%u",
                 (int)pA[0], (const char *)pA + 1,
                 (int)pB[0], (const char *)pB + 1, n, (int)found,
                 (unsigned)sel22);
        for (k = 0; k < n && k < 6; k++) {
            const byte *r = (const byte *)(data + 0x34 + k * 0x2e);
            DF_TRACE("  viewRow[%d] '%.*s'", k, (int)r[0x1e],
                     (const char *)r + 0x1f);
        }
    }
#endif
    UnlockResObject(hChild);
    status = DetachRenderChild((int)DAT_0048a780, sel22);
    if (status != 0)
        FatalError(status, 0x1022);
    return found;
#undef sel22
}


/* Attaches the current scene's (gSceneName) view list and copies into pOutBuf the name of the view
 * whose angle (at +8) is nearest `kind` by ComputeAngularDistance, if it is within 1000. Returns
 * 0, 10 when nothing was closer than 1000, 0x28 when no set is active, or the
 * FindSceneRowByName error. */
short __cdecl FindViewNameNearestAngle(short kind, byte *pOutBuf) /* 0x0040abc0 */
{
    /* The 42-byte scene row; the child selector is at BYTE +10. Declaration order is the exe's
     * register allocation. */
    typedef struct { byte b[42]; } PfRow;
    byte  *p;
    byte  *data;
    int    i;
    short  d;
    short  best;
    PfRow  row;
    short  st;
    int   *hChild;

    if (gSet.active == 0)
        return 0x28;
    st = FindSceneRowByName(&gSceneName, &row);
    if (st != 0)
        return st;
    st = AttachRenderChild((int *)DAT_0048a780, *(uint *)(row.b + 10), &hChild);
    if (st != 0)
        FatalError(st, 0x1023);
    best = 1000;
    data = (byte *)GetResDataPtr(hChild);
    p    = data + 0x34;
    for (i = 0; i < *(int *)(data + 0x30); i++) {
        d = ComputeAngularDistance(*(short *)(p + 8), kind);
        if (d < best) {
            best = d;
            CopyPascalString(p + 0x1e, pOutBuf);
        }
        p += 0x2e;
    }
    UnlockResObject(hChild);
    st = DetachRenderChild((int)DAT_0048a780, *(uint *)(row.b + 10));
    if (st != 0)
        FatalError(st, 0x1024);
    return (best == 1000) ? 10 : 0;
}


/* With a set active, parses the first descriptor as a scene (FindSceneRowByName), attaches its
 * view child under DAT_0048a780 and scans the 0x2e-byte rows for a Pascal name equal to the
 * second descriptor; on a hit the 46-byte row is copied to pOutRow46. Returns 0, 10 when no row
 * matches, or the FindSceneRowByName status. Fatal 0x1025 / 0x1026 on attach / detach failure. */
short __cdecl LookupViewRow(byte *pDescA, byte *pDescB,  /* 0x0040acc0 */
                                                         void *pOutRow46)
{
    /* the parsed 42-byte playfield row (its child selector is the unaligned
     * uint at byte +10) and the 46-byte child rows, each copied as one object */
    typedef struct { short w[21]; } PlayfieldRow;
    typedef struct { short w[23]; } ChildRow46;
    short         st;
    short         notFound;
    int          *hChild;
    ChildRow46   *pWalk;
    int           rowIdx;
    int           base;
    PlayfieldRow  parsed;

    if (gSet.active == 0)
        return 0x0b;

    st = FindSceneRowByName(pDescA, &parsed);
    if (st != 0)
        return st;
    st = AttachRenderChild(DAT_0048a780, *(uint *)((byte *)&parsed + 10), (int **)&hChild);
    if (st != 0)
        FatalError(st, 0x1025);

    base     = GetResDataPtr(hChild);
    pWalk    = (ChildRow46 *)(base + 0x34);
    notFound = 1;
    for (rowIdx = 0; rowIdx < *(int *)(base + 0x30); rowIdx++) {
        if (PascalStringsEqualNoCase((byte *)pWalk + 0x1e, pDescB) != 0) {
            *(ChildRow46 *)pOutRow46 = *pWalk;
            notFound = 0;
            break;
        }
        pWalk++;
    }

    UnlockResObject(hChild);
    st = DetachRenderChild((int)(intptr_t)DAT_0048a780, *(uint *)((byte *)&parsed + 10));
    if (st != 0)
        FatalError(st, 0x1026);
    return notFound ? 10 : 0;
}


/* Attaches render child selector under DAT_0048a780 and scans its 9-int rows for a Pascal name
 * equal to pOut (PascalStringsEqualNoCase); on a match copies the row's 9 ints to pRowOut and
 * returns 0. Returns 10 if nothing matches or selector is 0. */
short __cdecl LookupNamedRowInChild(uint32_t selector, byte *pOut, int *pRowOut)  /* 0x0040ADD0 */
{
    /* The twin of LookupViewRow: 36-byte rows copied
     * as one object. The exe returns short (`mov ax,0xa`, `and ax,0xa`); every
     * caller stores the status in a short, so the low word is all that is read. */
    typedef struct { int d[9]; } SlotRow36;
    SlotRow36 *pWalk;
    short      st;
    short      notFound;
    int       *hChild;
    int        rowIdx;
    int       *base;

    if (selector == 0)
        return 10;
    st = AttachRenderChild((int *)DAT_0048a780, selector, (int **)&hChild);
    if (st != 0)
        FatalError(st, 0x1027);

    base     = (int *)GetResDataPtr(hChild);
    pWalk    = (SlotRow36 *)(base + 2);
    notFound = 1;
    for (rowIdx = 0; rowIdx < *base; rowIdx++) {
        if (PascalStringsEqualNoCase((byte *)pWalk + 0x14, pOut) != 0) {
            *(SlotRow36 *)pRowOut = *pWalk;
            notFound = 0;
            break;
        }
        pWalk++;
    }

    UnlockResObject(hChild);
    st = DetachRenderChild((int)DAT_0048a780, selector);
    if (st != 0)
        FatalError(st, 0x1028);
    return notFound ? 10 : 0;
}


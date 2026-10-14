# DreamFactory Script Format

Combined reference for the DreamFactory scripting language: binary wire format, opcode tables, and keyword list.
For opcode-to-handler mappings see [opcodes.md](opcodes.md). For container file structure see [container-format.md](container-format.md).

---

## Binary Script Format

Scripts are stored inside DreamFactory container files as a sequence of **8-byte segments**, terminated by a segment where `cmd == 0`.

```c
struct ScriptSegment {      /* 8 bytes */
    uint16_t cmd;           /* opcode -- 0 = end of script */
    uint32_t info;          /* operand (meaning depends on cmd) */
    uint16_t unknown;       /* always 0 in practice */
};
```

Scripts are stored in *binary* form (already parsed); the binary is actually larger than the equivalent text source.

---

## Literal / Value Opcodes (cmd 3–6)

| cmd | Name | `info` meaning |
|---|---|---|
| 3 | STRING | byte offset from current segment to Pascal string |
| 4 | INTEGER | literal integer value |
| 5 | VARIABLE | byte offset from current segment to Pascal variable name |
| 6 | BREAK | tab depth of the following line (indentation) |

---

## Operator Opcodes (8001–8013)

These are the same opcodes processed by `ProcessScriptBlock`'s 40-entry work queue in `multimedia.c`.

| cmd | Symbol | cmd | Symbol |
|---|---|---|---|
| 8001 | `+` | 8002 | `-` |
| 8003 | `*` | 8004 | `/` |
| 8005 | `&` | 8006 | `\|` |
| 8007 | `@` | 8008 | `=` |
| 8009 | `!=` | 8010 | `>` |
| 8011 | `<` | 8012 | `>=` |
| 8013 | `<=` | | |

---

## Punctuation (4018–4020)

| cmd | Token |
|---|---|
| 4018 | `(` |
| 4019 | `)` |
| 4020 | `,` |

---

## Control Flow Opcodes (4xxx)

| cmd | Keyword | cmd | Keyword |
|---|---|---|---|
| 4001 | `code` | 4004 | `endcode` |
| 4002 | `global` | 4003 | `local` |
| 4005 | `exitcode` | 4006 | `if` |
| 4007 | `endif` | 4008 | `else` |
| 4010 | `endswitch` | 4011 | `case` |
| 4012 | `for` | 4015 | `endfor` |
| 4017 | `endwhile` | 4022 | `false` |
| 4023 | `not` | 4024 | `return` |
| 4025 | `passcode` | 4026 | `me` |
| 4028 | `dumplocal` | 4029 | `dumpglobal` |

---

## Engine Command Ranges

| Range | Category | Direction |
|---|---|---|
| 12xxx | Action commands (void) | Script → engine |
| 16xxx | Query commands (single value) | Script → engine |
| 20xxx | Computed-value commands | Script → engine |
| 24xxx | Transition/effect commands | Script → engine |

Selected important opcodes (full table in [opcodes.md](opcodes.md)):

| cmd | Keyword | cmd | Keyword |
|---|---|---|---|
| 12001 | `bootscript` | 12017 | `playmovie` |
| 12013 | `opencastfile` | 12018 | `openset` / `beginscene` |
| 12019 | `opentrackfile` | 12021 | `playtheme` |
| 12025 | `bothsound` | 12034 | `sendtoscene` |
| 12038 | `clut` | 12039 | `cursor` |
| 12047 | `castscript` | 12049 | `blacktoscreen` |
| 12050 | `screentoblack` | 12051 | `blackscreen` |
| 12071 | `quit` | | |
| 16029 | `currentscene` | 16011 | `currentview` |
| 20001 | `random` | 20027 | `freemem` |
| 20019 | `countscenes` | 20121 | `heapsize` |
| 24001 | `barndoorclose` | 24002 | `barndooropen` |
| 24003 | `irisclose` | 24004 | `irisopen` |
| 24005–8 | `scroll*` | 24019 | `nodraw` |

The full command map is in DFET's `DFscript.h`.

---

## Keyword Table (`0x0045b129`–`0x0045ccbc`)

The block starting at `0x0045b129` contains the DreamFactory scripting language keyword table — hundreds of zero-terminated ASCII strings used by the script parser/interpreter. See `src/data/keyword_tables.c` for the full table.

**Navigation / movement:**
`right`, `strait`, `walkonframes`, `walkonroad`, `walktoxyz`, `walkonpath`, `walkdest`, `walktostar`, `pausewalk`, `stopwalk`, `turnleft`, `turnright`, `turndown`, `turntodeg`, `turnhalfright`, `turnhalfleft`

**Scene / set lifecycle:**
`openscene()`, `closescene()`, `openset()`, `closeset()`, `openstage()`, `closestage()`, `openflat()`, `closeflat()`, `opencast()`, `closecast()`, `openactor()`, `closeactor()`, `openpuppet()`, `closepuppet()`, `openprop()`, `closeprop()`, `openshop()`, `closeshop()`

**Networking:**
`network`, `connect`, `disconnect`, `sendmessage`, `joingroup`, `leavegroup`, `addplayer(`, `delplayer(`, `getmemberid`, `getmembercount`, `getourid`, `everyone`, `netconnect`, `netdisconnect`, `netbroadcast`, `netjoingroup`, `netleavegroup`

**Audio:**
`voicesound`, `voicedone`, `haltvoice`, `singlesound`, `dualsound`, `multiplesound`, `bothsound`, `wavevolume`, `soundvol`, `soundpan`, `soundrate`, `soundloop`, `sounddone`, `haltsound`, `playtheme`, `halttheme`, `themevol`, `themepan`, `makecricket`, `stopcricket`, `pausecricket`

**Flow control:**
`while`, `endwhile`, `switch`, `endswitch`, `if`, `endif`, `for`, `endfor`, `return`, `local`, `global`, `variable`, `delay`, `defer`

**Screen effects:**
`screentoblack`, `blacktoscreen`, `blackscreen`, `irisopen`, `irisclose`, `visualeffect`, `venetian`, `wiperight`, `wipeleft`, `wipedown`, `barndooropen`, `barndoorclose`

**Save / game state:**
`savegame`, `opengame`, `passcode`, `reboot`, `exitcode`

**String / number ops:**
`substring`, `stringlength`, `stringtonum`, `numtostring`, `stringwidth`, `findword`, `putword`

**Query / enumeration:**
`countactors`, `countscenes`, `countprops`, `currentscene`, `currentstage`, `cameraxyz`, `playerxyz`, `freemem`, `heapsize`, `machinespeed`, `machinetype`, `framerate`

---

## Notes

- `%p` is a DreamFactory format specifier (not standard C) that expands to the project/game name (`"Titanic"`).
- Binary scripts are evaluated by `InterpretStatements` / `ProcessScriptBlock` in `multimedia.c`.
- The operator opcodes 8001–8013 are the same opcodes pushed onto `ProcessScriptBlock`'s 40-entry work queue.
- Script variables and strings are stored as Pascal strings (length-prefixed byte arrays).

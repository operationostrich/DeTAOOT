/* script_dispatch.c -- TI.EXE 0x00425400..0x004290a0: DispatchScriptCommand and the value-returning
 * ScriptCmd_* script commands, with the Eval*ScriptArgs helpers. */

#include <windows.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include "df_protos.h"
#include "topics/scriptcmd.h"
#include "topics/scriptquery.h"
#include "topics/scene.h"
#include "topics/multimedia.h"
#include "topics/anim.h"
#include "topics/hotspot_raster.h"

#include "topics/savestate.h"

/* Runs one value-returning script command. The opcode is the first short of the 8-byte instruction
 * entry; 16001-16056 are the basic commands, 20001 is random, 20002-20121 the extended ones. The
 * arguments sit in a block that opens with a 0xfb2 entry and closes with 0xfb3.
 *
 * pResult receives an 8-byte typed value: word 0 the type (2 logic/short, 3 string, 4 int), words 1-2
 * the 32-bit value, word 3 zero. *pSkipCount receives the number of 8-byte entries consumed.
 * Returns 0, or 1 for an unknown opcode, 2 for a missing 0xfb2 opener or 0xfb3 closer, otherwise
 * the handler's status (0x0e wrong argument type, 10 value out of range, 0x1c missing 0xfb4
 * argument separator).
 *
 * Port note: same shape as DispatchActionCommand -- one switch over the absolute opcodes, the
 * pointer walking with its start kept, no pre-set skip count. Every handler gets the arg-block
 * pointer (the port had handed puppetvisible, freemem and sysmem the opcode entry; none reads it). */
short __cdecl DispatchScriptCommand(int *pCtx, short *pArgBlock, int *pTable,  /* 0x00425400 */
                                   short *pResult, int *pSkipCount)
{
    short  op;
    short *start;
    short  status;
    int    subSkip;        /* instructions consumed within the arg block */

    op = *pArgBlock;
    start = pArgBlock;
    pArgBlock += 4;
    if (*pArgBlock != 0xfb2)
    {
        DF_TRACE("!! err2 DispatchScriptCommand: opcode %d has no arg-block opener (got %d)",
                 (int)op, (int)*pArgBlock);
        return 2;
    }
    pArgBlock += 4;   /* past the opcode entry and the block opener */

    DF_TRACE("query %d", (int)op);

    switch (op)
    {
        /* Basic commands 16001-16056 (0x3E81-0x3EB8) */
        case 0x3E81: status = ScriptCmd_ActorVisible(pCtx, pArgBlock, pTable,
                                                      pResult, &subSkip); break;         /* 16001 actorvisible */
        case 0x3E82: status = ScriptCmd_ActorDeg(pCtx, pArgBlock, pTable,
                                                     pResult, &subSkip); break;          /* 16002 actordeg */
        case 0x3E83: status = ScriptCmd_ActorXyz(pCtx, (int)pArgBlock, pTable, pResult, &subSkip); break;  /* 16003 actorxyz */
        case 0x3E84: status = ScriptCmd_ActorXy(pCtx, (int)pArgBlock, pTable, pResult, &subSkip); break;  /* 16004 actorxy */
        case 0x3E85: status = ScriptCmd_ActorIs3d(pCtx, pArgBlock, pTable, pResult, &subSkip); break;                                       /* 16005 actoris3d */
        case 0x3E86: status = ScriptCmd_ActorStar(pCtx, pArgBlock, pTable, pResult, &subSkip); break;                                    /* 16006 actorstar */
        case 0x3E87: status = ScriptCmd_SetVisible(pCtx, pArgBlock, pTable, pResult, &subSkip); break;                                          /* 16007 setvisible */
        case 0x3E88: status = ScriptCmd_StageVisible(pCtx, pArgBlock, pTable, pResult, &subSkip); break;                                          /* 16008 stagevisible */
        case 0x3E89: status = ScriptCmd_Path(pCtx, pArgBlock, pTable, pResult, &subSkip); break;                                /* 16009 path */
        case 0x3E8A: status = ScriptCmd_Result(pCtx, pArgBlock, pTable, pResult, &subSkip); break;                                     /* 16010 result */
        case 0x3E8B: status = ScriptCmd_CurrentView(pCtx, pArgBlock, pTable, pResult, &subSkip); break;                                          /* 16011 currentview */
        case 0x3E8C: status = ScriptCmd_ActorDist(pCtx, pArgBlock, pTable, pResult, &subSkip); break;                                   /* 16012 actordist */
        case 0x3E8D: status = ScriptCmd_PropDist(pCtx, pArgBlock, pTable, pResult, &subSkip); break;                                      /* 16013 propdist */
        case 0x3E8E: status = ScriptCmd_ActorPose(pCtx, pArgBlock, pTable, pResult, &subSkip); break;                                    /* 16014 actorpose */
        case 0x3E8F: status = ScriptCmd_PropVisible(pCtx, pArgBlock, pTable, pResult, &subSkip); break;                                         /* 16015 propvisible */
        case 0x3E90: status = ScriptCmd_PropDeg(pCtx, pArgBlock, pTable, pResult, &subSkip); break;                                           /* 16016 propdeg */
        case 0x3E91: status = ScriptCmd_PropXyz(pCtx, (int)pArgBlock, pTable, pResult, &subSkip); break;    /* 16017 propxyz */
        case 0x3E92: status = ScriptCmd_PropXy(pCtx, (int)pArgBlock, pTable, pResult, &subSkip); break;    /* 16018 propxy */
        case 0x3E93: status = ScriptCmd_PropIs3d(pCtx, pArgBlock, pTable, pResult, &subSkip); break;                                         /* 16019 propis3d */
        case 0x3E94: status = ScriptCmd_PropStar(pCtx, pArgBlock, pTable, pResult, &subSkip); break;                                       /* 16020 propstar */
        case 0x3E95: status = ScriptCmd_ActorSet(pCtx, pArgBlock, pTable, pResult, &subSkip); break;                                    /* 16021 actorset */
        case 0x3E96: status = ScriptCmd_Framerate(pCtx, pArgBlock, pTable, pResult, &subSkip); break;                                          /* 16022 framerate */
        case 0x3E97: status = ScriptCmd_ActorSpeed(pCtx, pArgBlock, pTable, pResult, &subSkip); break;                                       /* 16023 actorspeed */
        case 0x3E98: status = ScriptCmd_ActorScale(pCtx, pArgBlock, pTable, pResult, &subSkip); break;                                      /* 16024 actorscale */
        case 0x3E99: status = ScriptCmd_PropView(pCtx, pArgBlock, pTable, pResult, &subSkip); break;                                       /* 16025 propview */
        case 0x3E9A: status = ScriptCmd_PropSpeed(pCtx, pArgBlock, pTable, pResult, &subSkip); break;                                          /* 16026 propspeed */
        case 0x3E9B: status = ScriptCmd_PropSet(pCtx, pArgBlock, pTable, pResult, &subSkip); break;                                       /* 16027 propset */
        case 0x3E9C: status = ScriptCmd_PropScale(pCtx, pArgBlock, pTable, pResult, &subSkip); break;                                          /* 16028 propscale */
        case 0x3E9D: status = ScriptCmd_CurrentScene(pCtx, pArgBlock, pTable, pResult, &subSkip); break;                                         /* 16029 currentscene */
        case 0x3E9E: status = ScriptCmd_Variable(pCtx, (int)pArgBlock, pTable, pResult, &subSkip); break; /* 16030 variable */
        case 0x3E9F: status = ScriptCmd_CurrentDeg(pCtx, pArgBlock, pTable, pResult, &subSkip); break;                                           /* 16031 currentdeg */
        case 0x3EA0: status = ScriptCmd_PropOwner(pCtx, pArgBlock, pTable, pResult, &subSkip); break;                                       /* 16032 propowner */
        case 0x3EA1: status = ScriptCmd_WaveVolume(pCtx, pArgBlock, pTable, pResult, &subSkip); break;                                    /* 16033 wavevolume */
        case 0x3EA2: status = ScriptCmd_CurrentCd(pCtx, pArgBlock, pTable, pResult, &subSkip); break;                                       /* 16034 currentcd */
        case 0x3EA3: status = ScriptCmd_CameraHi(pCtx, pArgBlock, pTable, pResult, &subSkip); break;                                  /* 16035 camerahi */
        case 0x3EA4: status = ScriptCmd_ActorTurn(pCtx, pArgBlock, pTable, pResult, &subSkip); break;                                       /* 16036 actorturn */
        case 0x3EA5: status = ScriptCmd_MenuVisible(pCtx, pArgBlock, pTable, pResult, &subSkip); break;                                        /* 16037 menuvisible */
        case 0x3EA6: status = ScriptCmd_SoundPan(pCtx, pArgBlock, pTable, pResult, &subSkip); break;                                            /* 16038 soundpan */
        case 0x3EA7: status = ScriptCmd_SoundLoop(pCtx, pArgBlock, pTable, pResult, &subSkip); break;                                           /* 16039 soundloop */
        case 0x3EA8: status = ScriptCmd_SoundVol(pCtx, pArgBlock, pTable, pResult, &subSkip); break;                                           /* 16040 soundvol */
        case 0x3EA9: status = ScriptCmd_ThemeVol(pCtx, pArgBlock, pTable, pResult, &subSkip); break;                                       /* 16041 themevol */
        case 0x3EAA: status = ScriptCmd_PropValue(pCtx, pArgBlock, pTable, pResult, &subSkip); break;                                          /* 16042 propvalue */
        case 0x3EAB: status = ScriptCmd_ActorOwner(pCtx, pArgBlock, pTable, pResult, &subSkip); break;                                    /* 16043 actorowner */
        case 0x3EAC: status = ScriptCmd_ActorValue(pCtx, pArgBlock, pTable, pResult, &subSkip); break;                                     /* 16044 actorvalue */
        case 0x3EAD: status = ScriptCmd_KeyAborts(pCtx, pArgBlock, pTable, pResult, &subSkip); break;                                       /* 16045 keyaborts */
        case 0x3EAE: status = ScriptCmd_PauseLoop(pCtx, (int)pArgBlock, pTable, pResult, &subSkip); break; /* 16046 pauseloop */
        case 0x3EAF: status = ScriptCmd_PauseCricket(pCtx, pArgBlock, pTable, pResult, &subSkip); break;                                         /* 16047 pausecricket */
        case 0x3EB0: status = ScriptCmd_PauseWalk(pCtx, pArgBlock, pTable, pResult, &subSkip); break;                                         /* 16048 pausewalk */
        case 0x3EB1: status = (uint)(unsigned short)ScriptCmd_PuppetParam(pCtx, pArgBlock, pTable, pResult, &subSkip); break;                   /* 16049 puppetparam */
        case 0x3EB2: status = ScriptCmd_PuppetVisible(pCtx, pArgBlock, pTable, pResult, &subSkip); break;            /* 16050 puppetvisible */
        case 0x3EB3: status = ScriptCmd_ActorZclip(pCtx, pArgBlock, pTable, pResult, &subSkip); break;                                       /* 16051 actorzclip */
        case 0x3EB4: status = ScriptCmd_PropZclip(pCtx, pArgBlock, pTable, pResult, &subSkip); break;                                          /* 16052 propzclip */
        case 0x3EB5: status = ScriptCmd_PuppetBase(pCtx, pArgBlock, pTable, pResult, &subSkip); break;                                      /* 16053 puppetbase */
        case 0x3EB6: status = ScriptCmd_ThemePan(pCtx, pArgBlock, pTable, pResult, &subSkip); break;                                      /* 16054 themepan */
        case 0x3EB7: status = (uint)(unsigned short)ScriptCmd_SetParam(pCtx, pArgBlock, pTable, pResult, &subSkip); break;                   /* 16055 setparam */
        case 0x3EB8: status = (uint)(unsigned short)ScriptCmd_StageParam(pCtx, pArgBlock, pTable, pResult, &subSkip); break;                   /* 16056 stageparam */

        /* 20001: special command */
        case 0x4E21: status = (uint)(unsigned short)ScriptCmd_Random(pCtx, pArgBlock, pTable, pResult, &subSkip); break;

        /* Extended commands 20002-20121 (0x4E22-0x4E99) */
        case 0x4E22: status = (uint)(unsigned short)ScriptCmd_PointX(pCtx, pArgBlock,
                                                      pTable, pResult, &subSkip); break;             /* 20002 pointx */
        case 0x4E23: status = (uint)(unsigned short)ScriptCmd_PointY(pCtx, pArgBlock,
                                                      pTable, pResult, &subSkip); break;             /* 20003 pointy */
        case 0x4E24: status = (uint)(unsigned short)ScriptCmd_MakePoint(pCtx, (int)pArgBlock, pTable, (int *)pResult, &subSkip); break;                /* 20004 makepoint */
        case 0x4E25: status = ScriptCmd_Button(pCtx, pArgBlock, pTable,
                                              pResult, &subSkip); break;                             /* 20005 button */
        case 0x4E26: status = ScriptCmd_Mouse(pCtx, pArgBlock, pTable, pResult, &subSkip); break;                                         /* 20006 mouse */
        case 0x4E27: status = ScriptCmd_StillDown(pCtx, pArgBlock, pTable,
                                              pResult, &subSkip); break;                             /* 20007 stilldown */
        case 0x4E28: status = ScriptCmd_Tick(pCtx, pArgBlock, pTable,
                                               pResult, &subSkip); break;                            /* 20008 tick */
        case 0x4E29: status = ScriptCmd_IsWalk(pCtx, pArgBlock, pTable, pResult, &subSkip); break;                                      /* 20009 iswalk */
        case 0x4E2A: status = ScriptCmd_IsLoop(pCtx, (int)pArgBlock, pTable, pResult, &subSkip); break; /* 20010 isloop */
        case 0x4E2B: status = ScriptCmd_IsCricket(pCtx, pArgBlock, pTable, pResult, &subSkip); break;                                       /* 20011 iscricket */
        case 0x4E2C: status = ScriptCmd_CountActors(pCtx, pArgBlock, pTable, pResult, &subSkip); break;                                    /* 20012 countactors */
        case 0x4E2D: status = ScriptCmd_IndexToActor(pCtx, pArgBlock, pTable, pResult, &subSkip); break;                                        /* 20013 indextoactor */
        case 0x4E2E: status = ScriptCmd_CountSounds(pCtx, pArgBlock, pTable, pResult, &subSkip); break;                                      /* 20014 countsounds */
        case 0x4E2F: status = ScriptCmd_IndexToSound(pCtx, (int)pArgBlock, pTable, pResult, &subSkip); break; /* 20015 indextosound */
        case 0x4E30: status = ScriptCmd_SoundDone(pCtx, pArgBlock, pTable, pResult, &subSkip); break;                                          /* 20016 sounddone */
        case 0x4E31: status = ScriptCmd_PointInPainting(pCtx, (int)pArgBlock, pTable, pResult, &subSkip); break; /* 20017 pointinpainting */
        case 0x4E32: status = ScriptCmd_CountPaintings(pCtx, (int)pArgBlock, pTable,
                                                                     pResult, &subSkip); break;        /* 20018 countpaintings */
        case 0x4E33: status = ScriptCmd_CountScenes(pCtx, pArgBlock, pTable, pResult, &subSkip); break;       /* 20019 countscenes */
        case 0x4E34: status = ScriptCmd_IndexToScene(pCtx, pArgBlock, pTable,
                                                                   pResult, &subSkip); break;          /* 20020 indextoscene */
        case 0x4E35: status = (uint)(unsigned short)ScriptCmd_SendToPostFx(
                                    pCtx, pArgBlock, pTable, pResult, &subSkip); break;   /* 20021 sendtopostfx */
        case 0x4E36: status = ScriptCmd_IndexToPainting(pCtx, (int)pArgBlock, pTable,
                                                                    pResult, &subSkip); break;          /* 20022 indextopainting */
        case 0x4E37: status = ScriptCmd_ActorExists(pCtx, pArgBlock, pTable, pResult, &subSkip); break; /* 20023 actorexists */
        case 0x4E38: status = ScriptCmd_PropExists(pCtx, pArgBlock, pTable, pResult, &subSkip); break; /* 20024 propexists */
        case 0x4E39: status = ScriptCmd_StringToNum(pCtx, pArgBlock, pTable,
                                                                      pResult, &subSkip); break;        /* 20025 stringtonum */
        case 0x4E3A: status = ScriptCmd_NumToString(pCtx, pArgBlock, pTable,
                                                                     pResult, &subSkip); break;          /* 20026 numtostring */
        case 0x4E3B: status = ScriptCmd_FreeMem(pCtx, pArgBlock, pTable, pResult, &subSkip); break;   /* 20027 freemem */
        case 0x4E3C: status = (uint)(unsigned short)ScriptCmd_PuppetEvent(
                                           pCtx, pArgBlock, pTable, pResult, &subSkip); break;          /* 20028 puppetevent */
        case 0x4E3D: status = ScriptCmd_CountCasts(pCtx, pArgBlock, pTable, pResult, &subSkip); break;               /* 20029 countcasts */
        case 0x4E3E: status = ScriptCmd_IndexToCast(pCtx, pArgBlock, pTable,
                                                               pResult, &subSkip); break;              /* 20030 indextocast */
        case 0x4E3F: status = ScriptCmd_CountProps(pCtx, pArgBlock, pTable, pResult, &subSkip); break;               /* 20031 countprops */
        case 0x4E40: status = ScriptCmd_IndexToProp(pCtx, pArgBlock, pTable,
                                                                      pResult, &subSkip); break;       /* 20032 indextoprop */
        case 0x4E41: status = ScriptCmd_CountShops(pCtx, pArgBlock, pTable, pResult, &subSkip); break;              /* 20033 countshops */
        case 0x4E42: status = ScriptCmd_IndexToShop(pCtx, pArgBlock, pTable,
                                                                     pResult, &subSkip); break;         /* 20034 indextoshop */
        case 0x4E43: status = ScriptCmd_CountFlats(pCtx, pArgBlock, pTable, pResult, &subSkip); break;                  /* 20035 countflats */
        case 0x4E44: status = ScriptCmd_IndexToFlat(pCtx, pArgBlock, pTable,
                                                                          pResult, &subSkip); break;   /* 20036 indextoflat */
        case 0x4E45: status = ScriptCmd_FlatToIndex(pCtx, pArgBlock,
                                             pTable, pResult, &subSkip); break;                       /* 20037 flattoindex */
        case 0x4E46: status = ScriptCmd_CurrentFlat(pCtx, pArgBlock, pTable,
                                                                       pResult, &subSkip); break;      /* 20038 currentflat */
        case 0x4E47: status = ScriptCmd_PointInActor(pCtx, (int)pArgBlock,
                                                                pTable, pResult, &subSkip); break;     /* 20039 pointinactor */
        case 0x4E48: status = ScriptCmd_PointInProp(pCtx, (int)pArgBlock,
                                                                pTable, pResult, &subSkip); break;    /* 20040 pointinprop */
        case 0x4E49: status = (uint)(unsigned short)ScriptCmd_PointInSet(pCtx,
                                              pArgBlock, pTable, pResult, &subSkip); break;           /* 20041 pointinset */
        case 0x4E4A: status = (uint)(unsigned short)ScriptCmd_PointInStage(pCtx,
                                              pArgBlock, pTable, pResult, &subSkip); break;           /* 20042 pointinstage */
        case 0x4E4B: status = ScriptCmd_PointInButton(pCtx, (int)pArgBlock, pTable,
                                                                       pResult, &subSkip); break;      /* 20043 pointinbutton */
        case 0x4E4C: status = ScriptCmd_CountButtons(pCtx, (int)pArgBlock, pTable,
                                                                    pResult, &subSkip); break;        /* 20044 countbuttons */
        case 0x4E4D: status = ScriptCmd_IndexToButton(pCtx, (int)pArgBlock, pTable,
                                                                   pResult, &subSkip); break;         /* 20045 indextobutton */
        case 0x4E4E: status = ScriptCmd_CountPuppets(pCtx, pArgBlock, pTable, pResult, &subSkip); break;               /* 20046 countpuppets */
        case 0x4E4F: status = ScriptCmd_IndexToPuppet(pCtx, (int)pArgBlock, pTable,
                                                                       pResult, &subSkip); break;      /* 20047 indextopuppet */
        case 0x4E50: status = ScriptCmd_CurrentStage(pCtx, pArgBlock, pTable, pResult, &subSkip); break;        /* 20048 currentstage */
        case 0x4E51: status = ScriptCmd_CurrentPuppet(pCtx, pArgBlock, pTable, pResult, &subSkip); break;  /* 20049 currentpuppet */
        case 0x4E52: status = ScriptCmd_Type(pCtx, pArgBlock, pTable, pResult, &subSkip); break;                                               /* 20050 type */
        case 0x4E53: status = ScriptCmd_CountGlobals(pCtx, pArgBlock, pTable, pResult, &subSkip); break;                                          /* 20051 countglobals */
        case 0x4E54: status = ScriptCmd_IndexToGlobal(pCtx, pArgBlock, pTable, pResult, &subSkip); break;                                       /* 20052 indextoglobal */
        case 0x4E55: status = ScriptCmd_CurrentSet(pCtx, pArgBlock, pTable, pResult, &subSkip); break;      /* 20053 currentset */
        case 0x4E56: status = ScriptCmd_FindWord(pCtx, (int)pArgBlock,
                                                                              pTable, pResult,
                                                                              &subSkip); break;      /* 20054 findword */
        case 0x4E57: status = ScriptCmd_SubString(pCtx, (int)pArgBlock, pTable,
                                                                    pResult, &subSkip); break;        /* 20055 substring */
        case 0x4E58: status = ScriptCmd_StringLength(pCtx, (int)pArgBlock, pTable,
                                                                       pResult, &subSkip); break;     /* 20056 stringlength */
        case 0x4E59: status = ScriptCmd_PutWord(pCtx, (int)pArgBlock, pTable,
                                                                pResult, &subSkip); break;            /* 20057 putword */
        case 0x4E5A: status = ScriptCmd_OptionKey(pCtx, pArgBlock, pTable, pResult, &subSkip); break;       /* 20058 optionkey */
        case 0x4E5B: status = ScriptCmd_ShiftKey(pCtx, pArgBlock, pTable, pResult, &subSkip); break;    /* 20059 shiftkey */
        case 0x4E5C: status = ScriptCmd_CommandKey(pCtx, pArgBlock, pTable, pResult, &subSkip); break;    /* 20060 commandkey */
        case 0x4E5D: status = (uint)(unsigned short)ScriptCmd_CalcVectX(pCtx,
                                                                    (int)pArgBlock, pTable, pResult,
                                                                    &subSkip); break;               /* 20061 calcvectx */
        case 0x4E5E: status = (uint)(unsigned short)ScriptCmd_CalcVectY(pCtx,
                                                                    (int)pArgBlock, pTable, pResult,
                                                                    &subSkip); break;               /* 20062 calcvecty */
        case 0x4E5F: status = ScriptCmd_CameraXyz(pCtx, pArgBlock,
                                                                              pTable, pResult,
                                                                              &subSkip); break;     /* 20063 cameraxyz */
        case 0x4E60: status = ScriptCmd_PlayerXyz(pCtx, pArgBlock,
                                                                              pTable, pResult,
                                                                              &subSkip); break;     /* 20064 playerxyz */
        case 0x4E61: status = ScriptCmd_MachineType(pCtx, pArgBlock, pTable, pResult, &subSkip); break;                                          /* 20065 machinetype */
        case 0x4E62: status = ScriptCmd_MachineSpeed(pCtx, pArgBlock, pTable, pResult, &subSkip); break;                                         /* 20066 machinespeed */
        case 0x4E63: status = ScriptCmd_FileExists(pCtx, pArgBlock, pTable, pResult, &subSkip); break;                                         /* 20067 fileexists */
        case 0x4E64: status = ScriptCmd_QuestionDialog(pCtx, pArgBlock, pTable, pResult, &subSkip); break;                                      /* 20068 questiondialog */
        case 0x4E65: status = ScriptCmd_TextDialog(pCtx, (int)pArgBlock, pTable, pResult, &subSkip); break;  /* 20069 textdialog */
        case 0x4E66: status = ScriptCmd_HitTest(pCtx, pArgBlock, pTable,
                                                                           pResult, &subSkip); break; /* 20070 hittest */
        case 0x4E67: status = (uint)(unsigned short)ScriptCmd_CalcDeg(
                        pCtx, (int)pArgBlock, pTable, pResult, &subSkip); break;                         /* 20071 calcdeg */
        case 0x4E68: status = (uint)(unsigned short)ScriptCmd_CalcTurn(
                        pCtx, (int)pArgBlock, pTable, pResult, &subSkip); break;                         /* 20072 calcturn */
        case 0x4E69: status = ScriptCmd_StarXyz(pCtx, (int)pArgBlock,
                                                                     pTable, pResult, &subSkip); break; /* 20073 starxyz */
        case 0x4E6A: status = ScriptCmd_Frame(pCtx, pArgBlock, pTable,
                                                                pResult, &subSkip); break;     /* 20074 frame */
        case 0x4E6B: status = ScriptCmd_CountTracks(pCtx, pArgBlock, pTable,
                                                                pResult, &subSkip); break;     /* 20075 counttracks */
        case 0x4E6C: status = ScriptCmd_IndexToTrack(pCtx, pArgBlock,
                                                                          pTable, pResult,
                                                                          &subSkip); break;    /* 20076 indextotrack */
        case 0x4E6D: status = ScriptCmd_CurrentSound(pCtx, pArgBlock,
                                                                        pTable, pResult,
                                                                        &subSkip); break;      /* 20077 currentsound */
        case 0x4E6E: status = ScriptCmd_CurrentVoice(pCtx, pArgBlock,
                                                                             pTable, pResult,
                                                                             &subSkip); break; /* 20078 currentvoice */
        case 0x4E6F: status = ScriptCmd_CurrentTheme(pCtx, pArgBlock,
                                                                      pTable, pResult,
                                                                      &subSkip); break;       /* 20079 currenttheme */
        case 0x4E70: status = ScriptCmd_SoundRate(pCtx, pArgBlock,
                                                                          pTable, pResult,
                                                                          &subSkip); break;    /* 20080 soundrate */
        case 0x4E71: status = (uint)(unsigned short)ScriptCmd_CalcDist(
                        pCtx, (int)pArgBlock, pTable, pResult, &subSkip); break;                         /* 20081 calcdist */
        case 0x4E72: status = (uint)(unsigned short)ScriptCmd_CalcMod(pCtx, (int)pArgBlock,
                                                                               pTable, pResult, &subSkip); break; /* 20082 calcmod */
        case 0x4E73: status = (uint)(unsigned short)ScriptCmd_ActionFrame(pCtx, pArgBlock, pTable, pResult, &subSkip); break;                     /* 20083 actionframe */
        case 0x4E74: status = ScriptCmd_SendToActorFx(pCtx, (int)pArgBlock, pTable,
                                                                        pResult, &subSkip); break; /* 20084 sendtoactorfx */
        case 0x4E75: status = ScriptCmd_SendToSceneFx((int)pCtx, (int)pArgBlock, pTable,
                                                                  pResult, &subSkip); break;    /* 20085 sendtoscenefx */
        case 0x4E76: status = ScriptCmd_SendToPuppetFx((int *)pCtx, (int)pArgBlock, pTable,
                                                   pResult, &subSkip); break;                      /* 20086 sendtopuppetfx */
        case 0x4E77: status = ScriptCmd_SendToCastFx((int *)pCtx, (int)pArgBlock, pTable,
                                                 pResult, &subSkip); break;                        /* 20087 sendtocastfx */
        case 0x4E78: status = ScriptCmd_SendToPropFx(pCtx, (int)pArgBlock, pTable, pResult, &subSkip); break; /* 20088 sendtopropfx */
        case 0x4E79: status = ScriptCmd_SendToShopFx(pCtx, (int)pArgBlock, pTable, pResult, &subSkip); break; /* 20089 sendtoshopfx */
        case 0x4E7A: status = ScriptCmd_SendToPaintingFx(pCtx, (int)pArgBlock, pTable, pResult, &subSkip); break; /* 20090 sendtopaintingfx */
        case 0x4E7B: status = (uint)(unsigned short)ScriptCmd_SendToSetFx(
                                    pCtx, pArgBlock, pTable, pResult, &subSkip); break;   /* 20091 sendtosetfx */
        case 0x4E7C: status = ScriptCmd_SendToButtonFx(pCtx, (int)pArgBlock, pTable, pResult, &subSkip); break; /* 20092 sendtobuttonfx */
        case 0x4E7D: status = ScriptCmd_SendToFlatFx(pCtx, (int)pArgBlock, pTable, pResult, &subSkip); break;   /* 20093 sendtoflatfx */
        case 0x4E7E: status = (uint)(unsigned short)ScriptCmd_SendToStageFx(
                                    pCtx, (short *)pArgBlock, pTable, pResult, &subSkip); break; /* 20094 sendtostagefx */
        case 0x4E7F: status = (uint)(unsigned short)ScriptCmd_SendToBootFx(
                                    pCtx, pArgBlock, pTable, pResult, &subSkip); break;              /* 20095 sendtobootfx */
        case 0x4E80: status = ScriptCmd_SceneXyz(pCtx, (int)(intptr_t)pArgBlock, pTable, pResult, &subSkip); break;                   /* 20096 scenexyz */
        case 0x4E81: status = ScriptCmd_VoiceDone(pCtx, pArgBlock, pTable, pResult, &subSkip); break; /* 20097 voicedone */
        case 0x4E82: status = ScriptCmd_PluginFx(pCtx, (int)pArgBlock, pTable, pResult, &subSkip); break; /* 20098 pluginfx */
        case 0x4E83: status = ScriptCmd_WalkDest(
                        pCtx, pArgBlock, pTable, pResult, &subSkip); break;               /* 20099 walkdest */
        case 0x4E84: status = (uint)(unsigned short)ScriptCmd_SendToServerFx(
                                    pCtx, pArgBlock, pTable, pResult, &subSkip); break;   /* 20100 sendtoserverfx */
        case 0x4E85: status = ScriptCmd_IndexToCricket(pCtx, pArgBlock, pTable, pResult, &subSkip); break;                          /* 20101 indextocricket */
        case 0x4E86: status = ScriptCmd_IndexToLoop(pCtx, (int)pArgBlock, pTable, pResult, &subSkip); break; /* 20102 indextoloop */
        case 0x4E87: status = ScriptCmd_IndexToWalk(
                        pCtx, pArgBlock, pTable, pResult, &subSkip); break;               /* 20103 indextowalk */
        case 0x4E88: status = ScriptCmd_CountCrickets(pCtx, pArgBlock, pTable, pResult, &subSkip); break;                             /* 20104 countcrickets */
        case 0x4E89: status = ScriptCmd_CountLoops(pCtx, pArgBlock, pTable, pResult, &subSkip); break;                           /* 20105 countloops */
        case 0x4E8A: status = ScriptCmd_CountWalks(pCtx, pArgBlock, pTable, pResult, &subSkip); break;                             /* 20106 countwalks */
        case 0x4E8B: status = ScriptCmd_CountBevels(pCtx, pArgBlock, pTable, pResult, &subSkip); break; /* 20107 countbevels */
        case 0x4E8C: status = ScriptCmd_Sqrt(pCtx, pArgBlock, pTable,
                                                            pResult, &subSkip); break;    /* 20108 sqrt */
        case 0x4E8D: status = ScriptCmd_PluginExists(
                        pCtx, pArgBlock, pTable, pResult, &subSkip); break;               /* 20109 pluginexists */
        case 0x4E8E: status = ScriptCmd_NetworkOn(pCtx, pArgBlock, pTable, pResult, &subSkip); break; /* 20110 networkon */
        case 0x4E8F: status = ScriptCmd_NetCountMembers(
                        pCtx, pArgBlock, pTable, pResult, &subSkip); break;                /* 20111 netcountmembers */
        case 0x4E90: status = ScriptCmd_NetIndexToMember(
                        pCtx, pArgBlock, pTable, pResult, &subSkip); break;             /* 20112 netindextomember */
        case 0x4E91: status = ScriptCmd_NetOurId(pCtx, pArgBlock, pTable, pResult, &subSkip); break; /* 20113 netourid */
        case 0x4E92: status = ScriptCmd_NetMessageSender(pCtx, pArgBlock, pTable, pResult, &subSkip); break; /* 20114 netmessagesender */
        case 0x4E93: status = ScriptCmd_NetMessageGroup(pCtx, pArgBlock, pTable, pResult, &subSkip); break; /* 20115 netmessagegroup */
        case 0x4E94: status = ScriptCmd_RoadAhead(
                        pCtx, (int)pArgBlock, pTable, pResult, &subSkip); break;         /* 20116 roadahead */
        case 0x4E95: status = ScriptCmd_StringWidth(
                        pCtx, (int)pArgBlock, pTable, pResult, &subSkip); break;                  /* 20117 stringwidth */
        case 0x4E96: status = ScriptCmd_CountViews(pCtx, pArgBlock, pTable,
                                                 pResult, &subSkip); break;      /* 20118 countviews */
        case 0x4E97: status = ScriptCmd_IndexToView(
                        pCtx, (int)pArgBlock, pTable, pResult, &subSkip); break;                   /* 20119 indextoview */
        case 0x4E98: status = ScriptCmd_SysMem(pCtx, pArgBlock, pTable, pResult, &subSkip); break; /* 20120 sysmem */
        case 0x4E99: status = ScriptCmd_HeapSize(pCtx, pArgBlock, pTable, pResult, &subSkip); break; /* 20121 heapsize */
        default:
            DF_TRACE("!! DispatchScriptCommand: NO HANDLER for opcode %d", (int)op);
            status = 1;
            break;
    }

    if (status != 0)
        return status;

#if DF_TRACE_ENABLED
    if (pResult != NULL)
    {
        int v = *(int *)(pResult + 1);
        if (pResult[0] == 3 && gStringSlots != NULL)
        {
            /* type 3 = string: the value is a slot index into the 20-entry
             * descriptor table, each slot a 256-byte Pascal string. */
            byte *s = (byte *)gStringSlots + v * 0x100;
            DF_TRACE("  query %d -> string[%d] '%.*s'", (int)op, v,
                     (int)s[0], (char *)(s + 1));
        }
        else
        {
            DF_TRACE("  query %d -> type=%d value=%d", (int)op, (int)pResult[0], v);
        }
    }
#endif

    pArgBlock += subSkip * 4;
    if (*pArgBlock != 0xfb3)
    {
        DF_TRACE("!! DispatchScriptCommand: opcode %d missing block closer "
                 "(subSkip=%d, got %d)", (int)op, subSkip, (int)*pArgBlock);
        return 2;
    }
    pArgBlock += 4;
    *pSkipCount = ((int)pArgBlock - (int)start) >> 3;
    return 0;
}


/* stringwidth (string, font, size) -> int, opcode 20117: the pixel width of the string in that font and
 * size, measured on channel 1's port. Arguments 2 and 3 are not type-checked (the port had added one).
 *
 * Port note: five parameters. The exe's first instruction here is MOV EAX,[ESP+0x14] (param 5), pushed
 * as the Eval helper's skip-count out-pointer; with it dropped the dispatcher's subSkip stayed 0 and
 * every call failed with error 2 "Expected paren" (port-bug-classes.md #2). Every handler that calls
 * an Eval*ScriptArgs helper takes it. */
short __cdecl ScriptCmd_StringWidth(int *pCtx, int pArgBlock,
                                                                            int *pTable, short *pResult, int *pSkipCount) /* 0x00426CC0 */
{
    short  st;
    short  o1[4];
    short  o2[4];
    short  o3[4];

    st = EvalThreeScriptArgs((int)pCtx, pArgBlock, pTable, (int *)o1, (int *)o2, (int *)o3, pSkipCount);
    if (st != 0)
        return st;
    st = TakeScriptString(o1, gScratchStr);
    if (st != 0)
        return st;
    /* no type check on arguments 2 and 3 in the exe (the port had added one) */
    QdSetPort((int *)&gChannels[1].hResource);
    QdTextFont((short)*(int *)(o2 + 1));
    QdTextSize((short)*(int *)(o3 + 1));
    *pResult   = 4;
    pResult[3] = 0;
    *(int *)(pResult + 1) = QdStringWidth(gScratchStr);
    return 0;
}


/* sqrt (n) -> int, opcode 20108: IntSqrt of the argument, a 16-bit root. Error 0x0e if it is not an int. */
short __cdecl ScriptCmd_Sqrt(int *pCtx, short *pArgBlock, int *pTable,
                                              short *pResult, int *pSkipCount) /* 0x00426D60 */
{
    short   status;
    short  typed[4];
    uint   uArg;

    status = ProcessScriptBlock((int)pCtx, pArgBlock, pTable, (int *)typed, pSkipCount);
    if (status != 0)
        return status;

    if (typed[0] != 4)
        return 0xe;

    uArg                   = *(uint *)((char *)typed + 2);
    *pResult               = 4;
    pResult[3]             = 0;         /* stored before the call, as the exe does */
    *(uint *)(pResult + 1) = IntSqrt(uArg);   /* a 16-bit root: movzx eax,ax */
    return 0;
}


/* calcmod (a, b) -> int, opcode 20082: a % b. Error 0x0e if either argument is not an int.
 *
 * Port note: five parameters, see ScriptCmd_StringWidth. */
short __cdecl ScriptCmd_CalcMod(int *pCtx, int pArgBlock, int *pTable, short *pResult, int *pSkipCount) /* 0x00426DD0 */
{
    short st;
    short argA[4];
    short argB[4];

    st = EvalTwoScriptArgs((int)pCtx, pArgBlock, pTable, (int *)argA, (int *)argB, pSkipCount);
    if (st != 0)
        return st;

    if (argA[0] != 4)
        return 0x0e;
    if (argB[0] != 4)
        return 0x0e;

    *pResult               = 4;
    *(int *)(pResult + 1) = *(int *)((char *)argA + 2) % *(int *)((char *)argB + 2);
    pResult[3]             = 0;
    return 0;
}


/* walkdest (name) -> string, opcode 20099: the destination name of the named walk, via CopyWalkDestName. */
short __cdecl ScriptCmd_WalkDest(int *pCtx, short *pArgBlock, int *pTable,
                                 short *pResult, int *pSkipCount) /* 0x00426E40 */
{
    short  status;
    short  nameDesc[4];  /* local_8 */

    status = ProcessScriptBlock((int)pCtx, pArgBlock, pTable, (int *)nameDesc, pSkipCount);
    if (status != 0)
        return status;

    status = TakeScriptString(nameDesc, gScratchStr);
    if (status != 0)
        return status;

    CopyWalkDestName(gScratchStr, gScratchStr2);
    return AllocScriptString(gScratchStr2, pResult);
}


/* menuvisible -> short, opcode 16037: gIsMenuVisible. Takes no arguments. */
short ScriptCmd_MenuVisible(int *pCtx, short *pArgBlock, int *pTable,
                              short *pResult, int *pSkipCount) /* 0x00426EB0 */
{
    *pSkipCount = 0;
    pResult[3] = 0;
    *pResult = 2;
    *(int *)(pResult + 1) = (int)gIsMenuVisible;
    return 0;
}


/* keyaborts -> short, opcode 16045: gKeyAborts. Takes no arguments. */
short ScriptCmd_KeyAborts(int *pCtx, short *pArgBlock, int *pTable,
                               short *pResult, int *pSkipCount) /* 0x00426EE0 */
{
    *pSkipCount = 0;
    pResult[3] = 0;
    *pResult = 2;
    *(int *)(pResult + 1) = (int)gKeyAborts;
    return 0;
}


/* frame -> int, opcode 20074: gSceneFrame. Takes no arguments. */
short ScriptCmd_Frame(int *pCtx, short *pArgBlock, int *pTable,
                                         short *pResult, int *pSkipCount) /* 0x00426F10 */
{
    (void)pCtx;
    (void)pArgBlock;
    (void)pTable;

    *pSkipCount          = 0;
    pResult[3]           = 0;           /* tag, type, value: the exe's store order; the frame is an int */
    *pResult             = 4;
    *(int *)(pResult + 1) = gSceneFrame;
    return 0;
}


/* starxyz (name, sel) -> int, opcode 20073: a coordinate of the named waypoint node
 * (FindWaypointNodePosition;
 * error 10 if there is none). sel 1, 2, 3 picks X, Y or Z; 4 returns X and Y packed, X in the high word.
 * Error 0x0e if sel is not an int or is outside 1-4.
 *
 * Port note: five parameters, see ScriptCmd_StringWidth. */
short __cdecl ScriptCmd_StarXyz(int *pCtx, int pArgBlock, int *pTable,
                                                      short *pResult, int *pSkipCount) /* 0x00426F40 */
{
    short  st;
    short  argName[4];
    short  argSel[4];
    short  posTmp[4];
    short  pt[2];

    st = EvalTwoScriptArgs((int)pCtx, pArgBlock, pTable, (int *)argName, (int *)argSel, pSkipCount);
    if (st != 0)
        return st;
    st = TakeScriptString(argName, gScratchStr);
    if (st != 0)
        return st;
    if (FindWaypointNodePosition(gScratchStr, posTmp) == 0)
        return 10;
    if (argSel[0] != 4)
        return 0xE;

    *pResult   = 4;
    pResult[3] = 0;
    switch (*(int *)(argSel + 1)) {
    case 1:
        *(int *)(pResult + 1) = posTmp[1];
        return 0;
    case 2:
        *(int *)(pResult + 1) = posTmp[2];
        return 0;
    case 3:
        *(int *)(pResult + 1) = posTmp[3];
        return 0;
    case 4:
        /* packed: x in the high word, y in the low word */
        pt[1] = posTmp[1];
        pt[0] = posTmp[2];
        *(int *)(pResult + 1) = *(int *)pt;
        return 0;
    default:
        return 0xE;
    }
}


/* scenexyz (name, sel) -> int, opcode 20096: a coordinate of the named scene's playfield row
 * (FindSceneRowByName). sel 1, 2, 3 picks X, Y or Z; 4 returns X and Y packed, X in the high word.
 * Error 0x0e if sel is not an int or is outside 1-4.
 *
 * Port note: five parameters, see ScriptCmd_StringWidth. */
short __cdecl ScriptCmd_SceneXyz(int *pCtx, int pArgBlock,  /* 0x00427050 */
                                                                   int *pTable, short *pResult, int *pSkipCount)
{
    typedef struct { short w[21]; } PlayfieldRow42;
    short  st;
    short  argDesc[4];
    short  argSel[4];
    PlayfieldRow42 row;
    short  pt[2];

    st = EvalTwoScriptArgs((int)pCtx, pArgBlock, pTable, (int *)argDesc, (int *)argSel, pSkipCount);
    if (st != 0)
        return st;
    st = TakeScriptString(argDesc, gScratchStr);
    if (st != 0)
        return st;
    st = FindSceneRowByName(gScratchStr, &row);
    if (st != 0)
        return st;
    if (argSel[0] != 4)
        return 0xE;

    /* the playfield row's x/y/z are the shorts at +4/+6/+8 */
    *pResult   = 4;
    pResult[3] = 0;
    switch (*(int *)(argSel + 1)) {
    case 1:
        *(int *)(pResult + 1) = row.w[2];
        return 0;
    case 2:
        *(int *)(pResult + 1) = row.w[3];
        return 0;
    case 3:
        *(int *)(pResult + 1) = row.w[4];
        return 0;
    case 4:
        /* packed: x in the high word, y in the low word */
        pt[1] = row.w[2];
        pt[0] = row.w[3];
        *(int *)(pResult + 1) = *(int *)pt;
        return 0;
    default:
        return 0xE;
    }
}


/* calcdeg (pointA, pointB) -> int, opcode 20071: ComputeAngleBetweenPoints from A to B, each argument a
 * packed script point (X in the high word, Y in the low). Error 0x0e if either is not an int.
 *
 * Port note: five parameters, see ScriptCmd_StringWidth. */
short __cdecl ScriptCmd_CalcDeg(int *pCtx, int pArgBlock, int *pTable,
                                                         short *pResult, int *pSkipCount) /* 0x00427150 */
{
    short st;
    short argPairA[4];
    short argPairB[4];

    st = EvalTwoScriptArgs((int)pCtx, pArgBlock, pTable,
                     (int *)argPairA, (int *)argPairB, pSkipCount);
    if (st != 0)
        return st;

    if (argPairA[0] != 4)
        return 0x0e;
    if (argPairB[0] != 4)
        return 0x0e;

    {
        /* ComputeAngleBetweenPoints takes two point structs with X at +2 and Y at +4. A script point
         * packs X in the high word and Y in the low, so the binary unpacks both into that order
         * (0x0042719e-0x004271c6) and pushes A last (0x004271cf), making A the first argument.
         * Port note: reversing either mirrored the angle and dials ran backwards. */
        short ptA[3], ptB[3];
        short ang;

        ptA[1] = argPairA[2];   /* +2 = X = value's high word */
        ptA[2] = argPairA[1];   /* +4 = Y = value's low word  */
        ptB[1] = argPairB[2];
        ptB[2] = argPairB[1];

        ang = ComputeAngleBetweenPoints((int)(intptr_t)ptA, (int)(intptr_t)ptB);

        DF_TRACE("dial: calcdeg A(x=%d,y=%d) -> B(x=%d,y=%d) = %d",
                 (int)ptA[1], (int)ptA[2], (int)ptB[1], (int)ptB[2], (int)ang);
        *(int *)(pResult + 1) = (int)ang;
        *pResult = 4;
        pResult[3] = 0;
        return 0;
    }
}


/* calcdist (pointA, pointB) -> int, opcode 20081: the distance between two packed script points, a 16-bit
 * IntSqrt of dx*dx + dy*dy. Error 0x0e if either is not an int.
 *
 * Port note: five parameters, see ScriptCmd_StringWidth. */
short __cdecl ScriptCmd_CalcDist(int *pCtx, int pArgBlock,
                                                                  int *pTable, short *pResult, int *pSkipCount) /* 0x00427200 */
{
    short st;
    short argA[4];
    short argB[4];
    int   dx;
    int   dy;
    uint  distSq;

    st = EvalTwoScriptArgs((int)pCtx, pArgBlock, pTable, (int *)argA, (int *)argB, pSkipCount);
    if (st != 0)
        return st;

    if (argA[0] != 4)
        return 0x0e;
    if (argB[0] != 4)
        return 0x0e;

    dy = (int)*(short *)((char *)argA + 4) - (int)*(short *)((char *)argB + 4);
    dx = (int)*(short *)((char *)argA + 2) - (int)*(short *)((char *)argB + 2);

    distSq                 = (uint)(dx * dx + dy * dy);
    *(uint *)(pResult + 1) = IntSqrt((int)distSq) & 0xffffu;
    *pResult               = 4;
    pResult[3]             = 0;
    return 0;
}


/* calcturn (a, b, step) -> int, opcode 20072: StepValueTowardTarget moving a toward b by at most
 * step (floored
 * at 1) on a 0-255 scale, a and b masked to a byte first. Error 0x0e if an argument is not an int.
 *
 * Port note: five parameters, see ScriptCmd_StringWidth. */
short __cdecl ScriptCmd_CalcTurn(int *pCtx, int pArgBlock, int *pTable,
                                                short *pResult, int *pSkipCount) /* 0x004272A0 */
{
    short  st;
    short  out1[4];
    short  out2[4];
    short  out3[4];
    short  a;
    short  b;
    short  delta;
    short  stepped;

    st = EvalThreeScriptArgs((int)pCtx, pArgBlock, pTable,
                             (int *)out1, (int *)out2, (int *)out3, pSkipCount);
    if (st != 0)
        return st;

    if (out1[0] != 4)
        return 0x0e;
    if (out2[0] != 4)
        return 0x0e;
    if (out3[0] != 4)
        return 0x0e;

    /* The masks are computed before the delta clamp (byte-exact). */
    a     = (short)*(int *)((char *)out1 + 2) & 0xff;
    b     = (short)*(int *)((char *)out2 + 2) & 0xff;
    delta = (short)*(int *)((char *)out3 + 2);
    if (delta <= 0)
        delta = 1;
    stepped = StepValueTowardTarget(a, b, delta, 0xff);

    *(int *)(pResult + 1) = (int)stepped;
    *pResult           = 4;
    pResult[3]         = 0;
    return 0;
}


/* hittest (point) -> string, opcode 20070: names what is under a packed script point, and leaves the kind
 * ("actor", "prop", "painting", "scene", "button", "flat") in result() (gScriptResult). Order: a
 * painting binding key (actor or prop name; fatal 0x104f if neither resolves), then the set viewport
 * (painting name, else the scene name), then the stage viewport (button name, else the flat page's
 * name); otherwise "none" and the empty string. Error 0x0e if the argument is not an int. */
short __cdecl ScriptCmd_HitTest(int *pCtx, short *pArgBlock, int *pTable,  /* 0x00427350 */
                                                            short *pResult, int *pSkipCount)
{
    /* Every AllocScriptString status is the handler's result (a tail call in the exe); the port
     * returned `status & 0xFFFF0000` through a short, i.e. always 0. */
    typedef struct { byte b[0x2e]; } PageRow;   /* flat-page row, copied whole */
    short    uVar;
    short   typed[4];
    int     scriptSlotArg;
    int     rowCtx;
    int     work[30];
    short   rectW[4];
    PageRow row;

    uVar = ProcessScriptBlock((int)pCtx, pArgBlock, pTable, (int *)typed, pSkipCount);
    if (uVar != 0)
        return uVar;

    if (typed[0] != 4)
        return 0xE;

    scriptSlotArg = *(int *)((char *)typed + 2);

    if ((short)FindPaintingBindingKeyAtPoint(scriptSlotArg, &rowCtx) != 0) {
        if ((short)GetActorNameByBindingKey(rowCtx, gScratchStr) == 0) {
            CopyPascalString((byte *)UpdatePascalLength(kStrActor), gScriptResult);
            return AllocScriptString(gScratchStr, pResult);
        }
        if ((short)GetPropNameByBindingKey(rowCtx, gScratchStr) == 0) {
            CopyPascalString((byte *)UpdatePascalLength(kStrProp), gScriptResult);
            return AllocScriptString(gScratchStr, pResult);
        }
        FatalError(0, 0x104f);
    }

    if ((gSet.active != 0) && (DAT_0048a77a != 0)) {
        CopyTrigOverlayBufferAndViewportPair((short *)work, (int *)rectW);
        if (PointInSceneRect(*(ScenePointYX *)&scriptSlotArg, rectW)) {
            if (FindPaintingAtPoint(scriptSlotArg, gScratchStr) != 0) {
                CopyPascalString((byte *)UpdatePascalLength(kStrPainting), gScriptResult);
                return AllocScriptString(gScratchStr, pResult);
            }
            CopyPascalString((byte *)UpdatePascalLength(kStrScene), gScriptResult);
            return AllocScriptString(&gSceneName, pResult);
        }
    }

    if ((gStage.active != 0) && (DAT_0048a74e != 0)) {
        CopyTrigSceneBufferAndViewportPair((short *)work, (int *)rectW);
        if (PointInSceneRect(*(ScenePointYX *)&scriptSlotArg, rectW)) {
            if (FindButtonNameAtPoint(scriptSlotArg, gScratchStr) != 0) {
                CopyPascalString((byte *)UpdatePascalLength(kStrButton), gScriptResult);
                return AllocScriptString(gScratchStr, pResult);
            }
            /* "\0flat" (0x0045d8c4), not the empty string at 0x0045d8c0 --
             * 0x004275d4: PUSH 0x45d8c4. This is the flat-hit arm, so
             * result() has to read "flat". */
            CopyPascalString((byte *)UpdatePascalLength(kStrFlat), gScriptResult);
            row = *(PageRow *)((char *)GetResDataPtr(DAT_0048a760) + 0x84c + DAT_0048a758 * 0x2e);
            UnlockResObject(DAT_0048a760);
            return AllocScriptString(row.b + 0x1e, pResult);
        }
    }

    CopyPascalString((byte *)UpdatePascalLength(&kStrNone), gScriptResult);
    return AllocScriptString((byte *)UpdatePascalLength(kStrEmptyPascal), pResult);
}


/* camerahi -> int, opcode 16035: gCameraHeight (set record +0x1a). Takes no arguments. */
short ScriptCmd_CameraHi(int *pCtx, short *pArgBlock, int *pTable,
                          short *pResult, int *pSkipCount) /* 0x00427690 */
{
    int v;

    *pSkipCount = 0;
    v = gCameraHeight;                   /* read before the stores, as the exe does */
    pResult[3] = 0;
    *pResult = 4;
    *(int *)(pResult + 1) = v;
    return 0;
}


/* machinetype -> string, opcode 20065: the machine type name from CopyMachineTypeName. Takes no arguments. */
short ScriptCmd_MachineType(int *pCtx, short *pArgBlock, int *pTable,
                            short *pResult, int *pSkipCount) /* 0x004276C0 */
{
    *pSkipCount = 0;
    CopyMachineTypeName(&gScratchStr);
    return AllocScriptString(&gScratchStr, pResult);  /* the exe returns the callee's status */
}


/* machinespeed -> int, opcode 20066: the DWORD from QueryProcessorType. Takes no arguments. */
short ScriptCmd_MachineSpeed(int *pCtx, short *pArgBlock, int *pTable,
                             short *pResult, int *pSkipCount) /* 0x004276F0 */
{
    DWORD val;

    *pSkipCount = 0;
    val = QueryProcessorType();
    *pResult = 4;
    *(DWORD *)(pResult + 1) = val;
    pResult[3] = 0;
    return 0;
}


/* fileexists (name) -> short, opcode 20067: 1 if ProbeContainerFile opens the name under any of the
 * search path
 * slots 1-8 (GetSearchPathSlot + the name), else 0. */
short ScriptCmd_FileExists(int *pCtx, short *pArgBlock, int *pTable,
                             short *pResult, int *pSkipCount) /* 0x00427720 */
{
    short status;
    short sVar;
    unsigned short probeOutC;  /* local_e */
    undefined2 probeOutA;  /* local_10 */
    short argResult[4];
    undefined4 probeOutB;  /* local_4 */

    status = ProcessScriptBlock(pCtx, pArgBlock, pTable, (int *)argResult, pSkipCount);
    if (status != 0)
        return status;

    status = TakeScriptString(argResult, (byte *)&gScratchStr);
    if (status != 0)
        return status;

    sVar = 1;
    *(int *)(pResult + 1) = 0;
    do {
        GetSearchPathSlot(sVar, &gScratchStr2);
        AppendPascalString(&gScratchStr, &gScratchStr2);
        status = ProbeContainerFile(&gScratchStr2, &probeOutA, &probeOutB, &probeOutC);
        if (status == 0) {
            *(int *)(pResult + 1) = 1;
            break;
        }
        sVar = sVar + 1;
    } while (sVar <= 8);

    pResult[3] = 0;
    *pResult = 2;
    return 0;
}


/* actionframe (n) -> short, opcode 20083: whether bit n of gActionFrameFlags is set, n being 1 or 2.
 * Error 0x0e if the argument is not an int or n is not 1 or 2. */
short ScriptCmd_ActionFrame(int *pCtx, short *pArgBlock, int *pTable,
                             short *pResult, int *pSkipCount) /* 0x004277E0 */
{
    short status;
    short argResult[4];

    status = (short)ProcessScriptBlock(pCtx, pArgBlock, pTable, (int *)argResult, pSkipCount);
    if (status != 0)
        return status;

    if (argResult[0] != 4)
        return 0x0e;

    *(int *)(pResult + 1) = 0;
    switch (*(int *)(argResult + 1)) {
    case 1:
        if (gActionFrameFlags & 1)
            *(int *)(pResult + 1) = 1;
        break;
    case 2:
        if (gActionFrameFlags & 2)
            *(int *)(pResult + 1) = 1;
        break;
    default:
        return 0x0e;
    }

    pResult[3] = 0;
    *pResult = 2;
    return 0;
}


/* textdialog (prompt, default) -> string, opcode 20069: RunTextEntryDialog with the prompt,
 * returning what the
 * player typed, or the empty Pascal string on Cancel or an empty reply.
 *
 * Port note: returns each failing status and AllocScriptString's own (every path leaves the callee's
 * ax). The port was void, so DispatchScriptCommand forced 0 and an argument error was swallowed.
 * Five parameters, see ScriptCmd_StringWidth. */
short ScriptCmd_TextDialog(int *pCtx, int pArgBlock, int *pTable,
                          short *pResult, int *pSkipCount) /* 0x00427870 */
{
    BOOL bVar;
    short sVar;
    short argResult1[4];
    short argResult2[4];

    sVar = EvalTwoScriptArgs(pCtx, pArgBlock, pTable,
                       (undefined4 *)argResult1, (undefined4 *)argResult2, pSkipCount);
    if (sVar != 0)
        return sVar;

    sVar = TakeScriptString(argResult1, (byte *)&gScratchStr);
    if (sVar != 0)
        return sVar;

    sVar = TakeScriptString(argResult2, (byte *)&gScratchStr2);
    if (sVar != 0)
        return sVar;

    bVar = RunTextEntryDialog(&gScratchStr, &gScratchStr2);
    if ((short)bVar != 0)
        return AllocScriptString(&gScratchStr2, pResult);
    return AllocScriptString((byte *)UpdatePascalLength(kStrEmptyPascal), pResult);   /* the lookup written inline: the other arguments are pushed before it is called */
}


/* questiondialog (prompt) -> short, opcode 20068: RunDescriptorYesNoDialog on the prompt string,
 * its answer as the result. */
short ScriptCmd_QuestionDialog(int *pCtx, short *pArgBlock, int *pTable,
                                  short *pResult, int *pSkipCount) /* 0x00427910 */
{
    short  status;
    short valid;
    short argResult[4];

    status = ProcessScriptBlock(pCtx, pArgBlock, pTable, (int *)argResult, pSkipCount);
    if (status != 0)
        return status;

    status = TakeScriptString(argResult, (byte *)&gScratchStr);
    if (status != 0)
        return status;

    valid = RunDescriptorYesNoDialog(&gScratchStr);
    *(int *)(pResult + 1) = valid;
    *pResult = 2;
    pResult[3] = 0;
    return 0;
}


/* indextoglobal (n) -> string, opcode 20052: the name of entry n (1-based) of the globals node, via
 * ReadPropertyNameByTag. Error 0x0e if n is not an int, 10 if it is outside 1..count. */
short ScriptCmd_IndexToGlobal(int *pCtx, short *pArgBlock, int *pTable,
                                short *pResult, int *pSkipCount) /* 0x00427980 */
{
    short status;
    int   count;
    short argResult[4];

    status = ProcessScriptBlock(pCtx, pArgBlock, pTable, (int *)argResult, pSkipCount);
    if (status != 0)
        return status;
    if (argResult[0] != 4)
        return 0x0e;
    count = *(short *)GetResDataPtr(gGlobalsNode);
    UnlockResObject(gGlobalsNode);
    if (*(int *)(argResult + 1) < 1 || *(int *)(argResult + 1) > count)
        return 10;
    /* the property read's own status is the result (the port had returned 0) */
    return ReadPropertyNameByTag(gGlobalsNode, (short)(*(int *)(argResult + 1) - 1), pResult);
}


/* countglobals -> int, opcode 20051: the number of entries in the globals node (gGlobalsNode).
 * Takes no arguments. */
short ScriptCmd_CountGlobals(int *pCtx, short *pArgBlock, int *pTable,
                             short *pResult, int *pSkipCount) /* 0x00427A10 */
{
    short *psVar;
    uint status;

    *pSkipCount = 0;
    psVar = (short *)GetResDataPtr(gGlobalsNode);
    *(int *)(pResult + 1) = (int)*psVar;
    UnlockResObject(gGlobalsNode);
    pResult[3] = 0;
    *pResult = 4;
    return 0;
}


/* type (value) -> string, opcode 20050: the script type name of the argument: "logic" (type 2),
 * "text" (3),
 * "number" (4), otherwise "unknown". */
short ScriptCmd_Type(int *pCtx, short *pArgBlock, int *pTable,
                           short *pResult, int *pSkipCount) /* 0x00427A60 */
{
    short status;
    short argResult[4];

    status = ProcessScriptBlock(pCtx, pArgBlock, pTable, (int *)argResult, pSkipCount);
    if (status != 0)
        return status;
    /* the exe returns AllocScriptString's own status (no xor ax,ax after it) */
    /* arms in this order: a file's literals land in reverse order of first mention, and
     * TI.EXE has "logic" < "number" < "text"; the code is the same in any order */
    switch (argResult[0]) {
    case 3:
        return AllocScriptString((byte *)UpdatePascalLength(&kStrText), pResult);
    case 4:
        return AllocScriptString((byte *)UpdatePascalLength(&kStrNumber), pResult);
    case 2:
        return AllocScriptString((byte *)UpdatePascalLength(&kStrLogic), pResult);
    default:
        return AllocScriptString((byte *)UpdatePascalLength(&kStrUnknown), pResult);
    }
}


/* path (n) -> string, opcode 16009: the search path slot n (0-8), via GetSearchPathSlot.
 * Error 0x0e if n is not an int, 10 if it is outside 0-8. */
short ScriptCmd_Path(int *pCtx, short *pArgBlock, int *pTable,
                                 short *pResult, int *pSkipCount) /* 0x00427B10 */
{
    short status;
    short argResult[4];

    status = ProcessScriptBlock(pCtx, pArgBlock, pTable, (int *)argResult, pSkipCount);
    if (status != 0)
        return status;

    if (argResult[0] != 4)
        return (status & 0xFFFF0000) | 0x0e;

    if (*(int *)(argResult + 1) >= 0 && *(int *)(argResult + 1) <= 8)   /* cmp 8 / jg */ {
        GetSearchPathSlot((short)*(int *)(argResult + 1), &gScratchStr);
        status = AllocScriptString(&gScratchStr, pResult);
        return status;
    }
    return (status & 0xFFFF0000) | 10;
}


/* wavevolume -> int, opcode 16033: PollWaveDeviceIfSoundActive(). The command name is older than what the
 * callee does. Takes no arguments. */
short ScriptCmd_WaveVolume(int *pCtx, short *pArgBlock, int *pTable,
                            short *pResult, int *pSkipCount) /* 0x00427B90 */
{
    int   v;

    *pSkipCount = 0;
    v = PollWaveDeviceIfSoundActive();  /* 0x004244E0 */
    DF_TRACE("vol: wavevolume GET -> %d", v);
    *(int *)(pResult + 1) = v;
    *pResult = 4;
    pResult[3] = 0;
    return 0;
}


/* result -> string, opcode 16010: the shared result() slot (gScriptResult, a 256-byte Pascal string).
 * Commands with a second thing to report -- hittest, indextoprop, indextoactor -- leave it there, so a
 * script reads result() straight after such a call. Takes no arguments. */
short ScriptCmd_Result(int *pCtx, short *pArgBlock, int *pTable,
                          short *pResult, int *pSkipCount) /* 0x00427BC0 */
{
    *pSkipCount = 0;
    return AllocScriptString(gScriptResult, pResult);   /* the exe returns the callee's status */
}


/* random (n) -> int, opcode 20001: RandomUpTo(n). Error 0x0e if n is not an int, 10 if n < 1. */
short ScriptCmd_Random(int *pCtx, short *pArgBlock, int *pTable,
                               short *pResult, int *pSkipCount) /* 0x00427BE0 */
{
    typedef struct { int w[2]; } TypedVal8;
    short status;
    short argResult[4];

    status = (short)ProcessScriptBlock(pCtx, pArgBlock, pTable, (int *)argResult, pSkipCount);
    if (status != 0)
        return status;

    if (argResult[0] != 4)
        return 0x0e;

    if (*(int *)(argResult + 1) <= 0)
        return 10;

    /* the argument block becomes the result, copied out whole */
    *(int *)(argResult + 1) = RandomUpTo(*(int *)(argResult + 1));
    *(TypedVal8 *)pResult = *(TypedVal8 *)argResult;
    return 0;
}


/* pointx (point) -> int, opcode 20002: the high word of a packed script point, sign-extended.
 * Error 0x0e if the argument is not an int.
 *
 * Port note: the original takes no declared arguments and reads them from its caller's frame
 * (port-bug-classes.md #2); they are explicit here. */
short ScriptCmd_PointX(int *pCtx, short *pArgBlock, int *pTable,  /* 0x00427c60 */
                       short *pResult, int *pSkipCount)
{
    short status;
    short argResult[4];

    status = (short)ProcessScriptBlock(pCtx, pArgBlock, pTable,
                                       (int *)argResult, pSkipCount);
    if (status != 0)
        return status;

    if (argResult[0] != 4)
        return 0x0e;

    /* As ScriptCmd_PointY, with the high word sign-extended into place. */
    *(int *)(argResult + 1) = (int)argResult[2];
    *(ScriptTypedValue *)pResult = *(ScriptTypedValue *)argResult;   /* two dword moves */
    return 0;
}


/* pointy (point) -> int, opcode 20003: the low word of a packed script point, sign-extended. Error
 * 0x0e if the argument is not an int. */
short ScriptCmd_PointY(int *pCtx, short *pArgBlock, int *pTable,  /* 0x00427cc0 */
                       short *pResult, int *pSkipCount)
{
    short status;
    short argResult[4];

    status = (short)ProcessScriptBlock(pCtx, pArgBlock, pTable,
                                       (int *)argResult, pSkipCount);
    if (status != 0)
        return status;

    if (argResult[0] != 4)
        return 0x0e;

    /* The exe sign-extends the low word in place (movsx / mov [esp+2],eax) and
     * copies the whole 8-byte value; word 0 is already 4 and word 3 is not cleared. */
    *(int *)(argResult + 1) = (int)argResult[1];
    *(ScriptTypedValue *)pResult = *(ScriptTypedValue *)argResult;   /* two dword moves */
    return 0;
}


/* makepoint (x, y) -> int, opcode 20004: packs the low 16 bits of x into the high word and of y into the
 * low word. Error 0x0e if either is not an int.
 *
 * Port note: five parameters, see ScriptCmd_StringWidth. */
short ScriptCmd_MakePoint(int *pCtx, int pArgBlock, int *pTable, int *pResult, int *pSkipCount)  /* 0x00427d20 */
{
    typedef struct { int w[2]; } TypedVal8;
    short status;
    short arg1Result[4];
    short arg2Result[4];
    short pt[2];

    status = EvalTwoScriptArgs((int)pCtx, pArgBlock, pTable,
                         (int *)arg1Result, (int *)arg2Result, pSkipCount);
    if (status != 0)
        return status;
    if (arg1Result[0] != 4)
        return 0x0e;
    if (arg2Result[0] != 4)
        return 0x0e;
    /* packed: arg1's low word high, arg2's low word low, built in a point
     * and stored back into arg1, which is copied out whole (its fourth word
     * included, as the exe does) */
    pt[1] = (short)*(int *)(arg1Result + 1);
    pt[0] = (short)*(int *)(arg2Result + 1);
    *(int *)(arg1Result + 1) = *(int *)pt;
    *(TypedVal8 *)pResult = *(TypedVal8 *)arg1Result;
    return 0;
}


/* button -> short, opcode 20005: IsMouseDown(). Takes no arguments. */
short ScriptCmd_Button(int *pCtx, short *pArgBlock, int *pTable,  /* 0x00427da0 */
                       short *pResult, int *pSkipCount)
{
    int flag;

    *pSkipCount = 0;
    flag = IsMouseDown();               /* movsx right after the call, before pResult is loaded */

    *(int *)(pResult + 1) = flag;
    pResult[0] = 2;
    pResult[3] = 0;
    return 0;
}


/* mouse -> int, opcode 20006: the cursor position packed as a script point, from GetSceneCursorPos
 * on gChannelB. Takes no arguments. */
short ScriptCmd_Mouse(int *pCtx, short *pArgBlock, int *pTable, short *pResult, int *pSkipCount) /* 0x00427DD0 */
{
    uint status;
    int  cursorPos;

    *pSkipCount = 0;
    GetSceneCursorPos((int)&gChannelB, (short *)&cursorPos);
    *(int *)(pResult + 1) = cursorPos;
    *pResult = 4;
    pResult[3] = 0;
    return 0;
}


/* tick -> int, opcode 20008: GetTick60Hz(). Takes no arguments. */
short ScriptCmd_Tick(int *pCtx, short *pArgBlock, int *pTable,  /* 0x00427e10 */
                        short *pResult, int *pSkipCount)
{
    uint val;

    *pSkipCount = 0;
    val = GetTick60Hz();

    pResult[0] = 4;
    *(uint *)(pResult + 1) = val;
    pResult[3] = 0;
    return 0;
}


/* stilldown -> short, opcode 20007: IsMouseStillDown(). Takes no arguments. */
short ScriptCmd_StillDown(int *pCtx, short *pArgBlock, int *pTable,  /* 0x00427e40 */
                       short *pResult, int *pSkipCount)
{
    int val;

    *pSkipCount = 0;
    val = IsMouseStillDown();            /* short -> int right after the call */

    *(int *)(pResult + 1) = val;
    pResult[0] = 2;
    pResult[3] = 0;
    return 0;
}


/* countloops (name) -> int, opcode 20105: CountLoopsByType of the name. */
short ScriptCmd_CountLoops(int *pCtx, short *pArgBlock, int *pTable,
                                short *pResult, int *pSkipCount) /* 0x00427E70 */
{
    int   v;
    short status;
    short argResult[4];

    status = ProcessScriptBlock(pCtx, pArgBlock, pTable, (int *)argResult, pSkipCount);
    if (status != 0)
        return status;

    status = TakeScriptString(argResult, (byte *)&gScratchStr);
    if (status != 0)
        return status;

    v = CountLoopsByType(&gScratchStr);
    *(int *)(pResult + 1) = v;
    *pResult = 4;
    pResult[3] = 0;
    return 0;
}


/* countwalks -> int, opcode 20106: CountWalks(). Takes no arguments. */
short ScriptCmd_CountWalks(int *pCtx, short *pArgBlock, int *pTable,
                              short *pResult, int *pSkipCount) /* 0x00427EE0 */
{
    int   v;

    *pSkipCount = 0;
    v = CountWalks();
    *(int *)(pResult + 1) = v;
    *pResult = 4;
    pResult[3] = 0;
    return 0;
}


/* countcrickets -> int, opcode 20104: CountCrickets(). Takes no arguments. */
short ScriptCmd_CountCrickets(int *pCtx, short *pArgBlock, int *pTable,
                              short *pResult, int *pSkipCount) /* 0x00427F10 */
{
    int   v;

    *pSkipCount = 0;
    v = CountCrickets();
    *(int *)(pResult + 1) = v;
    *pResult = 4;
    pResult[3] = 0;
    return 0;
}


/* indextoloop (name, n) -> string, opcode 20102: the name of loop n (1..CountLoopsByType(name)), via
 * GetLoopNameByIndex. Error 0x0e if n is not an int, 10 if it is out of range.
 *
 * Port note: five parameters, see ScriptCmd_StringWidth. */
short ScriptCmd_IndexToLoop(int *pCtx, int pArgBlock, int *pTable,
                           short *pResult, int *pSkipCount) /* 0x00427F40 */
{
    short sVar;
    short status;
    short argResult1[4];
    short argResult2[4];

    sVar = EvalTwoScriptArgs(pCtx, pArgBlock, pTable, (undefined4 *)argResult2, (undefined4 *)argResult1, pSkipCount);
    if (sVar != 0)
        return sVar;

    status = TakeScriptString(argResult2, (byte *)&gScratchStr);
    if (status != 0)
        return status;

    if (argResult1[0] != 4)
        return 0x0e;

    if (*(int *)(argResult1 + 1) >= 1) {    /* cmp 1 / jl, as 0x00427f72 has it */
        sVar = CountLoopsByType(&gScratchStr);
        if (*(int *)(argResult1 + 1) <= (int)sVar) {
            GetLoopNameByIndex((short)*(int *)(argResult1 + 1), &gScratchStr, &gScratchStr2);
            status = AllocScriptString(&gScratchStr2, pResult);
            return status;
        }
    }
    return 10;
}


/* indextocricket (n) -> string, opcode 20101: the name of cricket n (1..CountCrickets()), via
 * GetCricketNameByIndex. Error 0x0e if n is not an int, 10 if it is out of range. */
short ScriptCmd_IndexToCricket(int *pCtx, short *pArgBlock, int *pTable,
                                short *pResult, int *pSkipCount) /* 0x00427FF0 */
{
    short sVar;
    short status;
    short argResult[4];

    status = ProcessScriptBlock(pCtx, pArgBlock, pTable, (int *)argResult, pSkipCount);
    if (status != 0)
        return status;

    if (argResult[0] != 4)
        return (status & 0xFFFF0000) | 0x0e;

    if (*(int *)(argResult + 1) >= 1) {   /* cmp 1 / jl */
        sVar = CountCrickets();
        if (*(int *)(argResult + 1) <= (int)sVar) {
            GetCricketNameByIndex((short)*(int *)(argResult + 1), &gScratchStr);
            status = AllocScriptString(&gScratchStr, pResult);
            return status;
        }
    }
    return (status & 0xFFFF0000) | 10;
}


/* indextowalk (n) -> string, opcode 20103: the name of walk n (1..CountWalks()), via GetWalkNameByIndex.
 * Error 0x0e if n is not an int, 10 if it is out of range. */
short __cdecl ScriptCmd_IndexToWalk(int *pCtx, short *pArgBlock,
                                                                           int *pTable, short *pResult,
                                                                           int *pSkipCount) /* 0x00428070 */
{
    short  u;
    short  typed[4];

    u = ProcessScriptBlock((int)pCtx, pArgBlock, pTable, (int *)typed, pSkipCount);
    if (u != 0)
        return u;
    if (typed[0] != 4)
        return 0xE;
    if (*(int *)(typed + 1) < 1 || CountWalks() < *(int *)(typed + 1))
        return 10;
    GetWalkNameByIndex((short)*(int *)(typed + 1), gScratchStr);  /* 0x00443B60 */
    return AllocScriptString(gScratchStr, pResult);
}


/* pauseloop (a, b) -> short, opcode 16046: 1 if GetLoopPauseCount on the two names is non-zero, else 0.
 *
 * Port note: five parameters, see ScriptCmd_StringWidth. */
short ScriptCmd_PauseLoop(int *pCtx, int pArgBlock, int *pTable,
                         short *pResult, int *pSkipCount) /* 0x004280F0 */
{
    short sVar;
    short  status;
    short argResult1[4];
    short argResult2[4];
    

    sVar = EvalTwoScriptArgs(pCtx, pArgBlock, pTable,
                       (undefined4 *)argResult1, (undefined4 *)argResult2, pSkipCount);
    status = CONCAT22((short)(sVar >> 16), sVar);
    if (sVar != 0)
        return status;

    status = TakeScriptString(argResult1, &gScratchStr);
    if (status != 0)
        return status;

    status = TakeScriptString(argResult2, &gScratchStr2);
    if (status != 0)
        return status;

    {
        short cmp = GetLoopPauseCount(&gScratchStr, &gScratchStr2);
        if (cmp != 0) {                 /* test / je: the found case first */
            *(int *)(pResult + 1) = 1;
        } else {
            *(int *)(pResult + 1) = 0;
        }
        pResult[3] = 0;
        *pResult = 2;
        return 0;
    }
}


/* pausewalk (name) -> short, opcode 16048: 1 if GetWalkPauseCount of the name is non-zero, else 0. */
short ScriptCmd_PauseWalk(int *pCtx, short *pArgBlock, int *pTable,
                             short *pResult, int *pSkipCount) /* 0x00428190 */
{
    short sVar;
    short  status;
    short argResult[4];

    status = ProcessScriptBlock(pCtx, pArgBlock, pTable,
                                (int *)argResult, pSkipCount);
    if (status != 0)
        return status;

    status = TakeScriptString(argResult, &gScratchStr);
    if (status != 0)
        return status;

    sVar = GetWalkPauseCount(&gScratchStr);
    if (sVar != 0) {                    /* test / je, as the exe branches */
        *(int *)(pResult + 1) = 1;
    } else {
        *(int *)(pResult + 1) = 0;
    }
    pResult[3] = 0;
    *pResult = 2;
    return 0;
}


/* pausecricket (name) -> short, opcode 16047: 1 if GetCricketPauseCount of the name is non-zero, else 0. */
short ScriptCmd_PauseCricket(int *pCtx, short *pArgBlock, int *pTable,
                             short *pResult, int *pSkipCount) /* 0x00428210 */
{
    short sVar;
    short  status;
    short argResult[4];

    status = ProcessScriptBlock(pCtx, pArgBlock, pTable,
                                (int *)argResult, pSkipCount);
    if (status != 0)
        return status;

    status = TakeScriptString(argResult, &gScratchStr);
    if (status != 0)
        return status;

    sVar = GetCricketPauseCount(&gScratchStr);
    if (sVar != 0) {                    /* test / je, as the exe branches */
        *(int *)(pResult + 1) = 1;
    } else {
        *(int *)(pResult + 1) = 0;
    }
    pResult[3] = 0;
    *pResult = 2;
    return 0;
}


/* iswalk (name) -> short, opcode 20009: IsWalkActive of the name. */
short ScriptCmd_IsWalk(int *pCtx, short *pArgBlock, int *pTable,
                               short *pResult, int *pSkipCount) /* 0x00428290 */
{
    short  status;
    uint  typeVal;
    short argResult[4];

    status = ProcessScriptBlock(pCtx, pArgBlock, pTable,
                                (int *)argResult, pSkipCount);
    if (status != 0)
        return status;

    status = TakeScriptString(argResult, &gScratchStr);
    if (status != 0)
        return status;

    typeVal = IsWalkActive(&gScratchStr);
    *(int *)(pResult + 1) = (int)(short)typeVal;
    *pResult = 2;
    pResult[3] = 0;
    return 0;
}


/* isloop (a, b) -> short, opcode 20010: IsLoopActive of the two names.
 *
 * Port note: five parameters, see ScriptCmd_StringWidth. */
short ScriptCmd_IsLoop(int *pCtx, int pArgBlock, int *pTable,
                                short *pResult, int *pSkipCount) /* 0x00428300 */
{
    short sVar;
    short  status;
    short v;
    short argResult1[4];
    short argResult2[4];

    sVar = EvalTwoScriptArgs((int)pCtx, pArgBlock, pTable, (int *)argResult1, (int *)argResult2, pSkipCount);
    status = CONCAT22(0, sVar);
    if (sVar != 0)
        return status;

    status = TakeScriptString(argResult1, &gScratchStr);
    if (status != 0)
        return status;

    status = TakeScriptString(argResult2, &gScratchStr2);
    if (status != 0)
        return status;

    v = IsLoopActive(&gScratchStr, &gScratchStr2);
    *(int *)(pResult + 1) = v;          /* value first, then the type word */
    *pResult = 2;
    pResult[3] = 0;
    return 0;
}


/* iscricket (name) -> short, opcode 20011: IsCricketActive of the name. */
short ScriptCmd_IsCricket(int *pCtx, short *pArgBlock, int *pTable,
                               short *pResult, int *pSkipCount) /* 0x00428390 */
{
    short  status;
    uint  flagVal;
    short argResult[4];

    status = ProcessScriptBlock(pCtx, pArgBlock, pTable,
                                (int *)argResult, pSkipCount);
    if (status != 0)
        return status;

    status = TakeScriptString(argResult, &gScratchStr);
    if (status != 0)
        return status;

    flagVal = IsCricketActive(&gScratchStr);
    *(int *)(pResult + 1) = (int)(short)flagVal;
    *pResult = 2;
    pResult[3] = 0;
    return 0;
}


/* stringtonum (string) -> int, opcode 20025: the leading integer of the string
 * (ParseLeadingIntFromPascalString). */
short __cdecl ScriptCmd_StringToNum(int *pCtx, short *pArgBlock, int *pTable,
                                                       short *pResult, int *pSkipCount) /* 0x00428400 */
{
    typedef struct { int w[2]; } TypedVal8;
    short    st;
    short   argBuf[4];
    uint    hash;

    st = ProcessScriptBlock((int)pCtx, pArgBlock, pTable, (int *)argBuf, pSkipCount);
    if (st != 0)
        return st;

    st = TakeScriptString(argBuf, gScratchStr);
    if (st != 0)
        return st;

    /* the argument block is reused as the result and copied out whole
     * (0x00428459..0x0042846a): its fourth word is whatever the evaluation
     * left there, not a written 0 */
    *(int *)(argBuf + 1) = ParseLeadingIntFromPascalString(gScratchStr);
    DF_TRACE("stringtonum: '%.*s' -> %d", (int)gScratchStr[0], (const char *)gScratchStr + 1, *(int *)(argBuf + 1));
    argBuf[0] = 4;
    *(TypedVal8 *)pResult = *(TypedVal8 *)argBuf;
    return 0;
}


/* numtostring (n) -> string, opcode 20026: n as a decimal Pascal string. Error 0x0e if n is not an int. */
short __cdecl ScriptCmd_NumToString(int *pCtx, short *pArgBlock, int *pTable,
                                                      short *pResult, int *pSkipCount) /* 0x00428480 */
{
    short    st;
    short   argBuf[4];
    uint    uval;

    st = ProcessScriptBlock((int)pCtx, pArgBlock, pTable, (int *)argBuf, pSkipCount);
    if (st != 0)
        return st;

    if (argBuf[0] != 4)
        return 0x0e;

    uval = *(uint *)(argBuf + 1);
    FormatUintAsPascalDecimalString(uval, (char *)gScratchStr);
    return AllocScriptString(gScratchStr, pResult);
}


/* freemem -> int, opcode 20027: GetHeapFreeBytes(). Takes no arguments. */
short __cdecl ScriptCmd_FreeMem(int *pCtx, short *pArgBlock, int *pTable,
                                short *pResult, int *pSkipCount) /* 0x004284F0 */
{
    int freeBytes;

    /* the exe reads pResult at [esp+0x10] and pSkipCount at [esp+0x14]: the
     * full five-argument handler shape, even though it uses only two */
    *pSkipCount = 0;
    freeBytes   = GetHeapFreeBytes();
    *pResult    = 4;
    *(int *)(pResult + 1) = freeBytes;
    pResult[3]            = 0;
    return 0;
}


/* sysmem -> int, opcode 20120: QueryTotalSystemMemory(). Takes no arguments. */
short __cdecl ScriptCmd_SysMem(int *pCtx, short *pArgBlock, int *pTable,
                               short *pResult, int *pSkipCount) /* 0x00428520 */
{
    size_t sz;

    *pSkipCount          = 0;           /* five-argument handler shape, see ScriptCmd_FreeMem */
    sz                   = QueryTotalSystemMemory();
    *pResult             = 4;
    *(unsigned *)(pResult + 1) = (unsigned)sz;
    pResult[3]           = 0;
    return (uint)(uintptr_t)pResult & 0xFFFF0000U;
}


/* heapsize -> int, opcode 20121: the dword at +0xc of the heap descriptor (GetHeapDescPtr). Takes
 * no arguments. */
short __cdecl ScriptCmd_HeapSize(int *pCtx, short *pArgBlock, int *pTable, short *pResult, int *pSkipCount) /* 0x00428550 */
{
    int    hp;

    *pSkipCount = 0;
    hp          = GetHeapDescPtr();

    *(uint *)(pResult + 1) = *(uint *)((byte *)(intptr_t)hp + 0xc);   /* value, tag, type: the exe's store order */
    pResult[3]               = 0;
    *pResult                 = 4;
    return 0;
}


/* framerate -> int, opcode 16022: gFrameRate, the scene resource operating mode. Takes no arguments. */
short ScriptCmd_Framerate(int *pCtx, short *pArgBlock, int *pTable,
                            short *pResult, int *pSkipCount) /* 0x00428580 */
{
    int v;

    *pSkipCount = 0;
    v = gFrameRate;                   /* read before the stores, as the exe does */
    *pResult = 4;
    *(int *)(pResult + 1) = v;
    pResult[3] = 0;
    return 0;
}


/* variable (name) -> value, opcode 16030: the named property, looked up in pTable first and then in the
 * globals node (gGlobalsNode), via ReadPropertyValue.
 *
 * Port note: five parameters. 0x004285b0 reads the skip-count out-pointer from the fifth stack slot;
 * declared with four, the dispatcher passed nothing and ProcessScriptBlock wrote the skip count
 * through whatever sat there. */
short ScriptCmd_Variable(int *pCtx, int pArgBlock, int *pTable,  /* 0x004285b0 */
                         short *pResult, int *pSkipCount)
{
    short status;                       /* one short status: every test is `test ax,ax` */
    short tagIndex;
    short argResult[4];

    status = ProcessScriptBlock(pCtx, (short *)pArgBlock, pTable,
                                (int *)argResult, pSkipCount);
    if (status != 0)
        return status;

    status = TakeScriptString(argResult, &gScratchStr);
    if (status != 0)
        return status;

    status = LookupPropertyByName(pTable, (byte *)&gScratchStr, &tagIndex, pArgBlock);
    if (status != 0) {
        /* not a local property: try the global table, whose read's failure is
         * returned as is and whose success is spelled out as 0 (the exe's
         * cmp/adc/and is the compiler's branchless form of this) */
        status = LookupPropertyByName(gGlobalsNode, (byte *)&gScratchStr, &tagIndex, pArgBlock);
        if (status != 0)
            return status;
        status = ReadPropertyValue(gGlobalsNode, tagIndex, pResult);
        if (status != 0)
            return status;
        return 0;
    }
    return ReadPropertyValue(pTable, tagIndex, pResult);
}


/* findword (text, separator, n) -> string, opcode 20054: the n-th separator-delimited field of text, or ""
 * if there is none. An empty separator selects the single character at position n.
 *
 * Port note: five parameters, see ScriptCmd_StringWidth. */
short __cdecl ScriptCmd_FindWord(int *pCtx, int pArgBlock, int *pTable,
                                                               short *pResult, int *pSkipCount) /* 0x00428680 */
{
    short uVar;
    short out1[4];
    short out2[4];
    short out3[4];
    short found;
    int iField;   /* first byte of the field being scanned */
    int iPos;     /* scan position */
    int iLimit;
    short sVar;
    /* K is the occurrence argument's own value slot, decremented in place
     * (0x00428771: dec dword ptr [esp+0x12]); the skip count goes straight
     * to the caller's pointer (the port copied it only on success). */
#define K (*(int *)((char *)out3 + 2))

    sVar = EvalThreeScriptArgs((int)pCtx, pArgBlock, pTable,
                               (int *)out1, (int *)out2, (int *)out3, pSkipCount);
    if (sVar != 0)
        return sVar;
    if (out3[0] != 4)
        return 0xe;

    uVar = TakeScriptString(out1, gScratchStr);
    if (uVar != 0)
        return uVar;
    uVar = TakeScriptString(out2, gScratchStr2);
    if (uVar != 0)
        return uVar;

    DF_TRACE("findword: hay(len %u)='%.*s' sep(len %u)='%.*s' n=%d slots=(%d,%d)",
             (unsigned)gScratchStr[0], (int)gScratchStr[0], (const char *)gScratchStr + 1,
             (unsigned)gScratchStr2[0], (int)gScratchStr2[0], (const char *)gScratchStr2 + 1, K,
             (int)out1[1], (int)out2[1]);

    iPos   = 1;
    iField = 1;
    found  = 0;
    if (gScratchStr2[0] != 0) {
        AppendPascalString(gScratchStr2, gScratchStr);
        iLimit = ((uint)gScratchStr[0] - (uint)gScratchStr2[0]) + 1;
        for (; iPos <= iLimit; iPos++) {
            if ((short)CompareRawByteSlicesCaseFolded(gScratchStr + iPos, &gScratchStr2[1],
                                                      (unsigned short)gScratchStr2[0]) != 0) {
                K--;
                if (K <= 0) {
                    _memcpy(&gScratchStr2[1], gScratchStr + iField, (size_t)(iPos - iField));
                    gScratchStr2[0] = (byte)(iPos - iField);
                    found = 1;
                    break;
                }
                iField = (int)(uint)gScratchStr2[0] + iPos;
            }
        }
    } else {
        if (K >= 1 && (int)(uint)gScratchStr[0] >= K) {
            gScratchStr2[0] = 1;
            gScratchStr2[1] = gScratchStr[K];
            found = 1;
        }
    }

    if (found == 0)
        gScratchStr2[0] = 0;
    return AllocScriptString(gScratchStr2, pResult);
#undef K
}


/* substring (text, pattern) -> int, opcode 20055: the 1-based position of the first case-insensitive
 * occurrence of pattern in text, or -1 (also for an empty pattern).
 *
 * Port note: five parameters, see ScriptCmd_StringWidth. */
short __cdecl ScriptCmd_SubString(int *pCtx, int pArgBlock, int *pTable,
                                                      short *pResult, int *pSkipCount) /* 0x00428820 */
{
    short sVar;
    short uVar;
    short out1[4], out2[4];
    int idx;
    int iPos;
    int iLimit;

    /* the skip count goes straight to the caller's pointer, as in the exe */
    sVar = EvalTwoScriptArgs((int)pCtx, pArgBlock, pTable, (int *)out1, (int *)out2, pSkipCount);
    if (sVar != 0)
        return sVar;

    uVar = TakeScriptString(out1, gScratchStr);
    if (uVar != 0)
        return uVar;
    uVar = TakeScriptString(out2, gScratchStr2);
    if (uVar != 0)
        return uVar;

    idx = -1;
    if (gScratchStr2[0] != 0) {
        iLimit = ((uint)gScratchStr[0] - (uint)gScratchStr2[0]) + 1;
        for (iPos = 1; iPos <= iLimit; iPos++) {
            if ((short)CompareRawByteSlicesCaseFolded(gScratchStr + iPos, &gScratchStr2[1],
                                                      (unsigned short)gScratchStr2[0]) != 0) {
                idx = iPos;
                break;
            }
        }
    }

    *pResult              = 4;
    *(int *)(pResult + 1) = idx;
    pResult[3]            = 0;
    return 0;
}


/* stringlength (string) -> int, opcode 20056: the string's length byte, zero-extended.
 *
 * Port note: the port copied four bytes out of the descriptor, so the length came back with the first
 * three characters above it (stringlength("17") gave 0x67350002); */
short __cdecl ScriptCmd_StringLength(int *pCtx, int pArgBlock, int *pTable,
                                                         short *pResult, int *pSkipCount) /* 0x00428900 */
{
    short   uVar;
    uint   len;
    short  typed[4];

    uVar = ProcessScriptBlock(pCtx, (short *)pArgBlock, pTable, (int *)typed, pSkipCount);
    if (uVar != 0)
        return uVar;

    uVar = TakeScriptString(typed, gScratchStr);
    if (uVar != 0)
        return uVar;

    len = gScratchStr[0];
    *pResult               = 4;
    *(uint *)(pResult + 1) = len;
    pResult[3]             = 0;
    return 0;
}


/* putword (text, separator, n, replacement) -> string, opcode 20057: text with its n-th
 * separator-delimited
 * field replaced, used by scripts as a list setter, e.g. cardlist = putword (cardlist, " ", count, "").
 * An empty separator inserts the replacement at byte offset n instead (n == length+1 appends).
 * Anything that does not resolve returns the empty string, not an error. Error 0x0e if n is not an int.
 * */
short __cdecl ScriptCmd_PutWord(int *pCtx, int pArgBlock, int *pTable,
                               short *pResult, int *pSkipCount)  /* 0x00428970 */
{
    short  sVar;
    short   uVar;
    short  aHay[4], aSep[4], aIndex[4], aRepl[4];
    short  bDone;
    int    iField;   /* first byte of the field being scanned */
    int    iPos;     /* scan position */
    int    iLimit;
    byte  *pField;
    /* K is the index argument's own value slot: the exe decrements it in
     * place (0x00428a68: dec dword ptr [esp+0x12]). */
#define K (*(int *)((char *)aIndex + 2))

    sVar = EvalFourScriptArgs((int)pCtx, pArgBlock, pTable,
                              aHay, aSep, aIndex, aRepl, pSkipCount);
    if (sVar != 0)
        return sVar;

    if (aIndex[0] != 4)
        return 0xe;

    uVar = TakeScriptString(aHay, gScratchStr);
    if (uVar != 0)
        return uVar;
    uVar = TakeScriptString(aSep, gScratchStr2);
    if (uVar != 0)
        return uVar;

    bDone  = 0;
    iField = 1;

    if (gScratchStr2[0] != 0)   /* the exe takes the separator case first */
    {
        /* Append the separator so the last field ends with one too. */
        AppendPascalString(gScratchStr2, gScratchStr);

        iLimit = ((uint)gScratchStr[0] - (uint)gScratchStr2[0]) + 1;
        for (iPos = 1; iPos <= iLimit; iPos++) {
            if ((short)CompareRawByteSlicesCaseFolded(gScratchStr + iPos, &gScratchStr2[1],
                                                      (unsigned short)gScratchStr2[0]) != 0) {
                K--;
                if (K <= 0) {
                    /* Drop the trailing separator, close the gap where
                     * the field was, then splice the replacement in. */
                    gScratchStr[0] -= gScratchStr2[0];
                    pField = gScratchStr + iField;
                    _memcpy(pField, gScratchStr + iPos,
                            (size_t)(((uint)gScratchStr[0] - iPos) + 1));
                    gScratchStr[0] += (byte)(iField - iPos);

                    uVar = TakeScriptString(aRepl, gScratchStr2);
                    if (uVar != 0)
                        return uVar;

                    _memcpy(gScratchStr + gScratchStr2[0] + iField,
                            pField,
                            (size_t)(((uint)gScratchStr[0] - iField) + 1));
                    gScratchStr[0] += gScratchStr2[0];
                    _memcpy(pField, &gScratchStr2[1],
                            (size_t)gScratchStr2[0]);
                    bDone = 1;
                    goto finish;
                }
                iField = (int)(uint)gScratchStr2[0] + iPos;
            }
        }
    }
    else
    {
        /* Empty separator: insert the replacement at byte offset K. */
        uVar = TakeScriptString(aRepl, gScratchStr2);
        if (uVar != 0)
            return uVar;

        if (K >= 1 && K <= (int)(uint)gScratchStr[0])
        {
            _memcpy(gScratchStr + gScratchStr2[0] + K, gScratchStr + K,
                    (size_t)(((uint)gScratchStr[0] - K) + 1));
            _memcpy(gScratchStr + K, &gScratchStr2[1],
                    (size_t)gScratchStr2[0]);
            gScratchStr[0] += gScratchStr2[0];
        }
        else
        {
            /* Only K == length+1 appends; anything else yields "". */
            if ((int)(uint)gScratchStr[0] - K != -1)
                goto finish;
            AppendPascalString(gScratchStr2, gScratchStr);
        }
        bDone = 1;
    }

finish:
    if (bDone == 0)
        gScratchStr[0] = 0;
    return AllocScriptString(gScratchStr, pResult);
#undef K
}


/* optionkey -> short, opcode 20058: IsMultimediaSequentialModifierKeyDown(). Takes no arguments. */
short __cdecl ScriptCmd_OptionKey(int *pCtx, short *pArgBlock, int *pTable, short *pResult, int *pSkipCount) /* 0x00428C00 */
{
    int seq;

    *pSkipCount = 0;
    seq = IsMultimediaSequentialModifierKeyDown();
    *(int *)(pResult + 1) = seq;   /* the key state as returned, not a `!= 0` */
    *pResult = 2;
    pResult[3] = 0;
    return 0;
}


/* shiftkey -> short, opcode 20059: IsMusicSequentialModifierKeyDown(). Takes no arguments. */
short __cdecl ScriptCmd_ShiftKey(int *pCtx, short *pArgBlock, int *pTable, short *pResult, int *pSkipCount) /* 0x00428C30 */
{
    int seq;

    *pSkipCount = 0;
    seq = IsMusicSequentialModifierKeyDown();
    *(int *)(pResult + 1) = seq;   /* the key state as returned, not a `!= 0` */
    *pResult = 2;
    pResult[3] = 0;
    return 0;
}


/* commandkey -> short, opcode 20060: IsSoundSequentialModifierKeyDown(). Takes no arguments. */
short __cdecl ScriptCmd_CommandKey(int *pCtx, short *pArgBlock, int *pTable, short *pResult, int *pSkipCount) /* 0x00428C60 */
{
    int seq;

    *pSkipCount = 0;
    seq = IsSoundSequentialModifierKeyDown();
    *(int *)(pResult + 1) = seq;   /* the key state as returned, not a `!= 0`; value first */
    *pResult = 2;
    pResult[3] = 0;
    return 0;
}


/* calcvectx (angle, distance) -> int, opcode 20061: AngleDistToDeltaX of the two arguments.
 * Error 0x0e if either is not an int.
 *
 * Port note: five parameters, see ScriptCmd_StringWidth. */
short __cdecl ScriptCmd_CalcVectX(int *pCtx, int pArgBlock, int *pTable,
                                                              short *pResult, int *pSkipCount) /* 0x00428C90 */
{
    short arg1[4], arg2[4];
    short sVar;
    short scroll;                       /* the callee's ax, widened at the store (movsx ecx,ax) */

    sVar = EvalTwoScriptArgs((int)pCtx, pArgBlock, pTable, (int *)arg1, (int *)arg2, pSkipCount);
    if (sVar != 0)
        return sVar;

    if (arg1[0] != 4)                   /* the first argument is checked first */
        return 0x0e;
    if (arg2[0] != 4)
        return 0x0e;

    scroll = AngleDistToDeltaX((uint)(ushort)*(int *)((char *)arg1 + 2),
                          (short)*(int *)((char *)arg2 + 2));
    *(int *)(pResult + 1) = scroll;     /* value first, then the type word */
    *pResult              = 4;
    pResult[3]            = 0;
    return 0;
}


/* calcvecty (angle, distance) -> int, opcode 20062: AngleDistToDeltaY of the two arguments.
 * Error 0x0e if either is not an int.
 *
 * Port note: five parameters, see ScriptCmd_StringWidth. */
short __cdecl ScriptCmd_CalcVectY(int *pCtx, int pArgBlock, int *pTable,
                                                              short *pResult, int *pSkipCount) /* 0x00428D10 */
{
    short arg1[4], arg2[4];
    short sVar;
    short scroll;                       /* the callee's ax, widened at the store (movsx ecx,ax) */

    sVar = EvalTwoScriptArgs((int)pCtx, pArgBlock, pTable, (int *)arg1, (int *)arg2, pSkipCount);
    if (sVar != 0)
        return sVar;

    if (arg1[0] != 4)                   /* the first argument is checked first */
        return 0x0e;
    if (arg2[0] != 4)
        return 0x0e;

    scroll = AngleDistToDeltaY((uint)(ushort)*(int *)((char *)arg1 + 2),
                          (short)*(int *)((char *)arg2 + 2));
    *(int *)(pResult + 1) = scroll;     /* value first, then the type word */
    *pResult              = 4;
    pResult[3]            = 0;
    return 0;
}


/* cameraxyz (sel) -> int, opcode 20063: sel 1, 2, 3 picks the camera's X, Y or Z; 4 returns X and Y
 * packed,
 * X in the high word. Error 0x28 if no set is active, 0x0e if sel is not an int or is outside 1-4. */
short __cdecl ScriptCmd_CameraXyz(int *pCtx, short *pArgBlock, int *pTable,
                                                                short *pResult, int *pSkipCount) /* 0x00428D90 */
{
    short   uVar;
    short  typed[4];
    uint   sel;
    short  pt[2];

    if (gSet.active == 0)
        return 0x28;     /* 0x00428d9d MOV AX,0x28: an error, not success */

    uVar = ProcessScriptBlock((int)pCtx, pArgBlock, pTable, (int *)typed, pSkipCount);
    if (uVar != 0)
        return uVar;

    if (typed[0] != 4)
        return 0xE;

    sel = (uint)*(int *)((char *)typed + 2);

    switch (sel) {
    case 1U:
        *(int *)(pResult + 1) = (int)gCameraX;
        break;
    case 2U:
        *(int *)(pResult + 1) = (int)gCameraY;
        break;
    case 3U:
        *(int *)(pResult + 1) = (int)gCameraZ;
        break;
    case 4U:
        pt[1] = gCameraX;    /* packed: x in the high word */
        pt[0] = gCameraY;
        *(int *)(pResult + 1) = *(int *)pt;
        break;
    default:
        return 0xE;
    }

    pResult[3] = 0;
    *pResult   = 4;
    return 0;
}


/* playerxyz (sel) -> int, opcode 20064: sel 1, 2, 3 picks the player's X, Y or Z; 4 returns X and Y
 * packed,
 * X in the high word. Error 0x28 if no set is active, 0x0e if sel is not an int or is outside 1-4. */
short __cdecl ScriptCmd_PlayerXyz(int *pCtx, short *pArgBlock, int *pTable,
                                                                short *pResult, int *pSkipCount) /* 0x00428E70 */
{
    short   uVar;
    short  typed[4];
    uint   sel;
    short  pt[2];

    if (gSet.active == 0)
        return 0x28;     /* 0x00428e70 returns 0x28 too: an error, not success */

    uVar = ProcessScriptBlock((int)pCtx, pArgBlock, pTable, (int *)typed, pSkipCount);
    if (uVar != 0)
        return uVar;

    if (typed[0] != 4)
        return 0xE;

    sel = (uint)*(int *)((char *)typed + 2);

    switch (sel) {
    case 1U:
        *(int *)(pResult + 1) = (int)gPlayerX;
        break;
    case 2U:
        *(int *)(pResult + 1) = (int)gPlayerY;
        break;
    case 3U:
        *(int *)(pResult + 1) = (int)gPlayerZ;
        break;
    case 4U:
        pt[1] = gPlayerX;    /* packed: x in the high word */
        pt[0] = gPlayerY;
        *(int *)(pResult + 1) = *(int *)pt;
        break;
    default:
        return 0xE;
    }

    pResult[3] = 0;
    *pResult   = 4;
    return 0;
}


/* Evaluates two script arguments separated by a 0xfb4 entry, each through ProcessScriptBlock, into
 * pResult1 and pResult2. *pSkipOut receives the entries consumed. Returns 0, 0x1c if the separator is
 * missing, or the failing status. */
short __cdecl EvalTwoScriptArgs(char *pCtx, int pArgBlock,  /* 0x00428f50 */
                          int *pTable, int *pResult1,
                          int *pResult2, int *pSkipOut)
{
    short status;
    int   start;
    int   skip;  /* local_4 */

    /* the argument block PARAMETER walks; its start is kept for the skip count */
    start = pArgBlock;
    status = ProcessScriptBlock(pCtx, (short *)pArgBlock, pTable, pResult1, &skip);
    if (status != 0)
        return status;
    pArgBlock += skip * 8;
    if (*(short *)pArgBlock != 0x0fb4)
        return 0x1c;
    pArgBlock += 8;                         /* past the separator entry */
    status = ProcessScriptBlock(pCtx, (short *)pArgBlock, pTable, pResult2, &skip);
    if (status != 0)
        return status;
    pArgBlock += skip * 8;
    *pSkipOut = (pArgBlock - start) >> 3;
    return 0;
}


/* Three-argument form of EvalTwoScriptArgs: the results go to out1-3, 0x1c if either separator is missing. */
short __cdecl EvalThreeScriptArgs(char *pCtx, int pArgBlock, int *pTable,  /* 0x00428fe0 */
                                  int *out1, int *out2, int *out3, int *pSkipOut)
{
    short status;
    int   start;
    int   localSkip;

    /* the argument block PARAMETER walks; its start is kept for the skip count */
    start = pArgBlock;
    status = ProcessScriptBlock(pCtx, (short *)pArgBlock, pTable, out1, &localSkip);
    if (status != 0)
        return status;
    pArgBlock += localSkip * 8;
    if (*(short *)pArgBlock != 0x0fb4)
        return 0x1c;
    pArgBlock += 8;
    status = ProcessScriptBlock(pCtx, (short *)pArgBlock, pTable, out2, &localSkip);
    if (status != 0)
        return status;
    pArgBlock += localSkip * 8;
    if (*(short *)pArgBlock != 0x0fb4)
        return 0x1c;
    pArgBlock += 8;
    status = ProcessScriptBlock(pCtx, (short *)pArgBlock, pTable, out3, &localSkip);
    if (status != 0)
        return status;
    pArgBlock += localSkip * 8;
    *pSkipOut = (pArgBlock - start) >> 3;
    return 0;
}


/* Four-argument form of EvalTwoScriptArgs: the results go to pOut1-4 and *pOffset receives the
 * entries consumed; 0x1c if a separator is missing. */
short __cdecl EvalFourScriptArgs(char *pCtx, int pArgBlock,  /* 0x004290a0 */
                                 int *pTable,
                                 void *pOut1, void *pOut2,
                                 void *pOut3, void *pOut4,
                                 int *pOffset)
{
    short   err;
    int     start;
    int     skip;

    /* the parameter walks; its start is kept in a local (the skipper shape) */
    start = pArgBlock;

    err = ProcessScriptBlock(pCtx, (short *)pArgBlock, pTable, (short *)pOut1, &skip);
    if (err != 0)
        return err;
    pArgBlock += skip * 8;
    if (*(short *)pArgBlock != 0xfb4)
        return 0x1c;
    pArgBlock += 8;

    err = ProcessScriptBlock(pCtx, (short *)pArgBlock, pTable, (short *)pOut2, &skip);
    if (err != 0)
        return err;
    pArgBlock += skip * 8;
    if (*(short *)pArgBlock != 0xfb4)
        return 0x1c;
    pArgBlock += 8;

    err = ProcessScriptBlock(pCtx, (short *)pArgBlock, pTable, (short *)pOut3, &skip);
    if (err != 0)
        return err;
    pArgBlock += skip * 8;
    if (*(short *)pArgBlock != 0xfb4)
        return 0x1c;
    pArgBlock += 8;

    err = ProcessScriptBlock(pCtx, (short *)pArgBlock, pTable, (short *)pOut4, &skip);
    if (err != 0)
        return err;

    pArgBlock += skip * 8;
    *pOffset = (pArgBlock - start) >> 3;
    return 0;
}

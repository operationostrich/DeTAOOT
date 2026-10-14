/*
 * keyword_tables.c -- the script language's reserved-keyword tables.
 *
 * Extracted from TI.EXE's own tables.
 *
 * A default list (punctuation and operators, descriptor at 0x0045d5e0) and one list
 * per initial letter (descriptors from 0x0045d5e8). Each name is its own writable
 * buffer: ConvertKeywordTablesToPascal (0x004212a0) rewrites it in place into a
 * Pascal string at startup. The ids are the script opcodes/keyword ids.
 *
 * Why the conversion exists: DreamFactory began on the Macintosh, where the engine
 * speaks Pascal strings and the compilers accept "\pabs" literals. VC++ does not, so
 * the Windows port most likely kept the tables as C strings and added this one-time
 * pass rather than retype them (docs/engine.md, "Macintosh Lineage").
 *
 * TI.EXE wrote the names as literals in the list initializers: VC++ 4.2 puts them in
 * writable .data, last first, ahead of the lists (0x0045bcb4..0x0045cd18), and the
 * empty names share one pooled literal. That is the DF_FIX_WRITABLE_PASCAL_LITERALS = 0
 * spelling; GCC puts literals in read-only memory, so the port names a buffer for each.
 */
#include "df_fixes.h"
#include "df_keywords.h"

#if DF_FIX_WRITABLE_PASCAL_LITERALS
static char s_kw_dflt_0[3] = "!=";
static char s_kw_dflt_1[3] = "<=";
static char s_kw_dflt_2[3] = ">=";
static char s_kw_dflt_3[2] = "&";
static char s_kw_dflt_4[2] = "(";
static char s_kw_dflt_5[2] = ")";
static char s_kw_dflt_6[2] = "*";
static char s_kw_dflt_7[2] = "+";
static char s_kw_dflt_8[2] = ",";
static char s_kw_dflt_9[2] = "-";
static char s_kw_dflt_10[2] = "/";
static char s_kw_dflt_11[2] = "<";
static char s_kw_dflt_12[2] = "=";
static char s_kw_dflt_13[2] = ">";
static char s_kw_dflt_14[2] = "@";
static char s_kw_dflt_15[2] = "|";
static char s_kw_dflt_16[2] = "\255";
static char s_kw_dflt_17[2] = "\262";
static char s_kw_dflt_18[2] = "\263";
static char s_kw_a_0[12] = "actorscript";
static char s_kw_a_1[10] = "actorturn";
static char s_kw_a_2[9] = "actordeg";
static char s_kw_a_3[10] = "actordist";
static char s_kw_a_4[10] = "actoris3d";
static char s_kw_a_5[10] = "actorpose";
static char s_kw_a_6[11] = "actorscale";
static char s_kw_a_7[9] = "actorset";
static char s_kw_a_8[11] = "actorspeed";
static char s_kw_a_9[10] = "actorstar";
static char s_kw_a_10[13] = "actorvisible";
static char s_kw_a_11[8] = "actorxy";
static char s_kw_a_12[9] = "actorxyz";
static char s_kw_a_13[14] = "actorinstance";
static char s_kw_a_14[12] = "actionframe";
static char s_kw_a_15[11] = "actorvalue";
static char s_kw_a_16[11] = "actorowner";
static char s_kw_a_17[11] = "actorzclip";
static char s_kw_a_18[12] = "actorexists";
static char s_kw_a_19[10] = "actorwarm";
static char s_kw_a_20[12] = "actordelete";
static char s_kw_a_21[10] = "actorhide";
static char s_kw_a_22[10] = "actorlock";
static char s_kw_a_23[4] = "and";
static char s_kw_b_0[14] = "barndoorclose";
static char s_kw_b_1[13] = "barndooropen";
static char s_kw_b_2[12] = "blackscreen";
static char s_kw_b_3[14] = "blacktoscreen";
static char s_kw_b_4[11] = "bootscript";
static char s_kw_b_5[10] = "bothsound";
static char s_kw_b_6[7] = "button";
static char s_kw_b_7[13] = "buttonscript";
static char s_kw_c_0[5] = "case";
static char s_kw_c_1[9] = "camerahi";
static char s_kw_c_2[11] = "castscript";
static char s_kw_c_3[14] = "closecastfile";
static char s_kw_c_4[16] = "closepuppetfile";
static char s_kw_c_5[13] = "closesetfile";
static char s_kw_c_6[14] = "closeshopfile";
static char s_kw_c_7[15] = "closetrackfile";
static char s_kw_c_8[15] = "closestagefile";
static char s_kw_c_9[5] = "code";
static char s_kw_c_10[5] = "clut";
static char s_kw_c_11[12] = "currentview";
static char s_kw_c_12[7] = "cursor";
static char s_kw_c_13[12] = "countactors";
static char s_kw_c_14[13] = "countbuttons";
static char s_kw_c_15[11] = "countcasts";
static char s_kw_c_16[11] = "countflats";
static char s_kw_c_17[11] = "countprops";
static char s_kw_c_18[13] = "currentscene";
static char s_kw_c_19[13] = "countpuppets";
static char s_kw_c_20[12] = "countscenes";
static char s_kw_c_21[11] = "countshops";
static char s_kw_c_22[15] = "countpaintings";
static char s_kw_c_23[12] = "countsounds";
static char s_kw_c_24[11] = "currentset";
static char s_kw_c_25[12] = "currentflat";
static char s_kw_c_26[13] = "countglobals";
static char s_kw_c_27[11] = "commandkey";
static char s_kw_c_28[14] = "currentpuppet";
static char s_kw_c_29[10] = "calcvectx";
static char s_kw_c_30[10] = "calcvecty";
static char s_kw_c_31[10] = "cameraxyz";
static char s_kw_c_32[13] = "currentstage";
static char s_kw_c_33[8] = "calcdeg";
static char s_kw_c_34[9] = "calcturn";
static char s_kw_c_35[12] = "counttracks";
static char s_kw_c_36[13] = "currentsound";
static char s_kw_c_37[13] = "currenttheme";
static char s_kw_c_38[13] = "currentvoice";
static char s_kw_c_39[9] = "calcdist";
static char s_kw_c_40[11] = "currentdeg";
static char s_kw_c_41[14] = "countcrickets";
static char s_kw_c_42[11] = "countwalks";
static char s_kw_c_43[12] = "countbevels";
static char s_kw_c_44[9] = "castwarm";
static char s_kw_c_45[10] = "currentcd";
static char s_kw_c_46[8] = "calcmod";
static char s_kw_c_47[11] = "countviews";
static char s_kw_c_48[9] = "castlock";
static char s_kw_c_49[11] = "countloops";
static char s_kw_d_0[11] = "drawstring";
static char s_kw_d_1[9] = "debugger";
static char s_kw_d_2[6] = "delay";
static char s_kw_d_3[11] = "dumpglobal";
static char s_kw_d_4[10] = "dumplocal";
static char s_kw_d_5[10] = "dualsound";
static char s_kw_e_0[5] = "else";
static char s_kw_e_1[8] = "endcode";
static char s_kw_e_2[7] = "endfor";
static char s_kw_e_3[6] = "endif";
static char s_kw_e_4[10] = "endswitch";
static char s_kw_e_5[9] = "endwhile";
static char s_kw_e_6[6] = "error";
static char s_kw_e_7[11] = "exportclut";
static char s_kw_e_8[9] = "exitcode";
static char s_kw_f_0[6] = "false";
static char s_kw_f_1[11] = "flatscript";
static char s_kw_f_2[12] = "flattoindex";
static char s_kw_f_3[12] = "flushevents";
static char s_kw_f_4[4] = "for";
static char s_kw_f_5[10] = "framerate";
static char s_kw_f_6[9] = "findword";
static char s_kw_f_7[11] = "fileexists";
static char s_kw_f_8[6] = "frame";
static char s_kw_f_9[8] = "freemem";
static char s_kw_f_10[12] = "forceupdate";
static char s_kw_g_0[7] = "global";
static char s_kw_g_1[9] = "gotoflat";
static char s_kw_h_0[10] = "haltsound";
static char s_kw_h_1[10] = "halttheme";
static char s_kw_h_2[10] = "haltvoice";
static char s_kw_h_3[9] = "heapsize";
static char s_kw_h_4[8] = "hittest";
static char s_kw_h_5[11] = "hidecursor";
static char s_kw_i_0[3] = "if";
static char s_kw_i_1[12] = "indextoview";
static char s_kw_i_2[13] = "indextoactor";
static char s_kw_i_3[16] = "indextopainting";
static char s_kw_i_4[14] = "indextobutton";
static char s_kw_i_5[12] = "indextocast";
static char s_kw_i_6[12] = "indextoflat";
static char s_kw_i_7[12] = "indextoprop";
static char s_kw_i_8[14] = "indextopuppet";
static char s_kw_i_9[13] = "indextoscene";
static char s_kw_i_10[12] = "indextoshop";
static char s_kw_i_11[13] = "indextosound";
static char s_kw_i_12[10] = "irisclose";
static char s_kw_i_13[9] = "irisopen";
static char s_kw_i_14[10] = "iscricket";
static char s_kw_i_15[7] = "isloop";
static char s_kw_i_16[7] = "iswalk";
static char s_kw_i_17[13] = "indextotrack";
static char s_kw_i_18[14] = "indextoglobal";
static char s_kw_i_19[12] = "indextoloop";
static char s_kw_i_20[12] = "indextowalk";
static char s_kw_i_21[15] = "indextocricket";
static char s_kw_j_0[1] = "";
static char s_kw_k_0[10] = "keyaborts";
static char s_kw_l_0[6] = "local";
static char s_kw_m_0[12] = "makecricket";
static char s_kw_m_1[10] = "makepoint";
static char s_kw_m_2[8] = "message";
static char s_kw_m_3[6] = "mouse";
static char s_kw_m_4[14] = "multiplesound";
static char s_kw_m_5[3] = "me";
static char s_kw_m_6[12] = "machinetype";
static char s_kw_m_7[13] = "machinespeed";
static char s_kw_m_8[12] = "menuvisible";
static char s_kw_m_9[8] = "mixclut";
static char s_kw_m_10[9] = "makeloop";
static char s_kw_n_0[4] = "not";
static char s_kw_n_1[11] = "notedialog";
static char s_kw_n_2[12] = "numtostring";
static char s_kw_n_3[11] = "netconnect";
static char s_kw_n_4[14] = "netdisconnect";
static char s_kw_n_5[10] = "networkon";
static char s_kw_n_6[16] = "netcountmembers";
static char s_kw_n_7[17] = "netindextomember";
static char s_kw_n_8[9] = "netourid";
static char s_kw_n_9[17] = "netmessagesender";
static char s_kw_n_10[16] = "netmessagegroup";
static char s_kw_n_11[13] = "netbroadcast";
static char s_kw_n_12[13] = "netjoingroup";
static char s_kw_n_13[7] = "nodraw";
static char s_kw_n_14[14] = "netleavegroup";
static char s_kw_o_0[13] = "opencastfile";
static char s_kw_o_1[15] = "openpuppetfile";
static char s_kw_o_2[12] = "opensetfile";
static char s_kw_o_3[13] = "openshopfile";
static char s_kw_o_4[14] = "opentrackfile";
static char s_kw_o_5[14] = "openstagefile";
static char s_kw_o_6[10] = "optionkey";
static char s_kw_o_7[9] = "opengame";
static char s_kw_o_8[3] = "or";
static char s_kw_p_0[9] = "passcode";
static char s_kw_p_1[6] = "plain";
static char s_kw_p_2[10] = "playmovie";
static char s_kw_p_3[10] = "playtheme";
static char s_kw_p_4[13] = "pointinactor";
static char s_kw_p_5[16] = "pointinpainting";
static char s_kw_p_6[14] = "pointinbutton";
static char s_kw_p_7[12] = "pointinprop";
static char s_kw_p_8[11] = "pointinset";
static char s_kw_p_9[13] = "pointinstage";
static char s_kw_p_10[7] = "pointx";
static char s_kw_p_11[7] = "pointy";
static char s_kw_p_12[5] = "path";
static char s_kw_p_13[8] = "propdeg";
static char s_kw_p_14[9] = "prophide";
static char s_kw_p_15[9] = "propdist";
static char s_kw_p_16[9] = "propis3d";
static char s_kw_p_17[10] = "propscale";
static char s_kw_p_18[8] = "propset";
static char s_kw_p_19[10] = "propspeed";
static char s_kw_p_20[9] = "propstar";
static char s_kw_p_21[9] = "propview";
static char s_kw_p_22[12] = "propvisible";
static char s_kw_p_23[7] = "propxy";
static char s_kw_p_24[8] = "propxyz";
static char s_kw_p_25[11] = "propscript";
static char s_kw_p_26[12] = "puppetbevel";
static char s_kw_p_27[12] = "puppetclear";
static char s_kw_p_28[12] = "puppetevent";
static char s_kw_p_29[13] = "puppetscript";
static char s_kw_p_30[8] = "putword";
static char s_kw_p_31[11] = "puppetgrab";
static char s_kw_p_32[11] = "puppetbase";
static char s_kw_p_33[10] = "playerxyz";
static char s_kw_p_34[12] = "puppetspeak";
static char s_kw_p_35[13] = "propinstance";
static char s_kw_p_36[7] = "plugin";
static char s_kw_p_37[9] = "pluginfx";
static char s_kw_p_38[10] = "propvalue";
static char s_kw_p_39[10] = "propowner";
static char s_kw_p_40[10] = "pauseloop";
static char s_kw_p_41[10] = "pausewalk";
static char s_kw_p_42[12] = "puppetparam";
static char s_kw_p_43[14] = "puppetvisible";
static char s_kw_p_44[15] = "puppetscramble";
static char s_kw_p_45[15] = "puppetsubtitle";
static char s_kw_p_46[10] = "propzclip";
static char s_kw_p_47[9] = "propwarm";
static char s_kw_p_48[15] = "paintingscript";
static char s_kw_p_49[11] = "postscript";
static char s_kw_p_50[11] = "propexists";
static char s_kw_p_51[13] = "pluginexists";
static char s_kw_p_52[11] = "propdelete";
static char s_kw_p_53[9] = "proplock";
static char s_kw_p_54[13] = "pausecricket";
static char s_kw_q_0[15] = "questiondialog";
static char s_kw_q_1[5] = "quit";
static char s_kw_r_0[7] = "random";
static char s_kw_r_1[7] = "return";
static char s_kw_r_2[10] = "roadahead";
static char s_kw_r_3[7] = "reboot";
static char s_kw_r_4[7] = "result";
static char s_kw_s_0[12] = "scenescript";
static char s_kw_s_1[17] = "sendtopaintingfx";
static char s_kw_s_2[14] = "screentoblack";
static char s_kw_s_3[11] = "scrolldown";
static char s_kw_s_4[10] = "scrolleft";
static char s_kw_s_5[12] = "scrollright";
static char s_kw_s_6[9] = "scrollup";
static char s_kw_s_7[12] = "sendtoactor";
static char s_kw_s_8[13] = "sendtobutton";
static char s_kw_s_9[11] = "sendtocast";
static char s_kw_s_10[11] = "sendtoflat";
static char s_kw_s_11[11] = "sendtoprop";
static char s_kw_s_12[13] = "sendtopuppet";
static char s_kw_s_13[12] = "sendtoscene";
static char s_kw_s_14[10] = "sendtoset";
static char s_kw_s_15[11] = "sendtoshop";
static char s_kw_s_16[12] = "sendtostage";
static char s_kw_s_17[10] = "setscript";
static char s_kw_s_18[11] = "setvisible";
static char s_kw_s_19[13] = "stagevisible";
static char s_kw_s_20[11] = "shopscript";
static char s_kw_s_21[11] = "showcursor";
static char s_kw_s_22[12] = "singlesound";
static char s_kw_s_23[10] = "sounddone";
static char s_kw_s_24[12] = "stagescript";
static char s_kw_s_25[5] = "step";
static char s_kw_s_26[10] = "stilldown";
static char s_kw_s_27[12] = "stopcricket";
static char s_kw_s_28[9] = "stoploop";
static char s_kw_s_29[9] = "stopwalk";
static char s_kw_s_30[12] = "stringtonum";
static char s_kw_s_31[13] = "stringlength";
static char s_kw_s_32[9] = "shiftkey";
static char s_kw_s_33[10] = "substring";
static char s_kw_s_34[9] = "savegame";
static char s_kw_s_35[8] = "starxyz";
static char s_kw_s_36[9] = "scenexyz";
static char s_kw_s_37[11] = "sendtoboot";
static char s_kw_s_38[9] = "soundvol";
static char s_kw_s_39[9] = "soundpan";
static char s_kw_s_40[10] = "soundloop";
static char s_kw_s_41[10] = "soundrate";
static char s_kw_s_42[7] = "switch";
static char s_kw_s_43[14] = "sendtoactorfx";
static char s_kw_s_44[15] = "sendtobuttonfx";
static char s_kw_s_45[13] = "sendtocastfx";
static char s_kw_s_46[13] = "sendtoflatfx";
static char s_kw_s_47[13] = "sendtopropfx";
static char s_kw_s_48[15] = "sendtopuppetfx";
static char s_kw_s_49[14] = "sendtoscenefx";
static char s_kw_s_50[12] = "sendtosetfx";
static char s_kw_s_51[13] = "sendtoshopfx";
static char s_kw_s_52[14] = "sendtostagefx";
static char s_kw_s_53[15] = "sendtopainting";
static char s_kw_s_54[13] = "sendtobootfx";
static char s_kw_s_55[13] = "sendtopostfx";
static char s_kw_s_56[15] = "sendtoserverfx";
static char s_kw_s_57[9] = "shopwarm";
static char s_kw_s_58[11] = "sendtopost";
static char s_kw_s_59[13] = "sendtoserver";
static char s_kw_s_60[13] = "serverscript";
static char s_kw_s_61[12] = "stringwidth";
static char s_kw_s_62[9] = "shoplock";
static char s_kw_s_63[9] = "setparam";
static char s_kw_s_64[11] = "stageparam";
static char s_kw_s_65[7] = "sysmem";
static char s_kw_s_66[5] = "sqrt";
static char s_kw_t_0[3] = "to";
static char s_kw_t_1[5] = "type";
static char s_kw_t_2[5] = "tick";
static char s_kw_t_3[11] = "textdialog";
static char s_kw_t_4[10] = "turntodeg";
static char s_kw_t_5[9] = "themevol";
static char s_kw_t_6[9] = "themepan";
static char s_kw_t_7[7] = "target";
static char s_kw_t_8[9] = "turnleft";
static char s_kw_t_9[10] = "turnright";
static char s_kw_t_10[7] = "turnup";
static char s_kw_t_11[9] = "turndown";
static char s_kw_t_12[13] = "turnhalfleft";
static char s_kw_t_13[14] = "turnhalfright";
static char s_kw_t_14[5] = "true";
static char s_kw_u_0[1] = "";
static char s_kw_v_0[9] = "venetian";
static char s_kw_v_1[13] = "visualeffect";
static char s_kw_v_2[11] = "voicesound";
static char s_kw_v_3[10] = "voicedone";
static char s_kw_v_4[9] = "variable";
static char s_kw_w_0[11] = "walktostar";
static char s_kw_w_1[9] = "walkdest";
static char s_kw_w_2[6] = "while";
static char s_kw_w_3[9] = "wipedown";
static char s_kw_w_4[9] = "wipeleft";
static char s_kw_w_5[10] = "wiperight";
static char s_kw_w_6[11] = "wavevolume";
static char s_kw_w_7[11] = "walkonpath";
static char s_kw_w_8[10] = "walktoxyz";
static char s_kw_w_9[11] = "walkonroad";
static char s_kw_w_10[13] = "walkonframes";
static char s_kw_w_11[7] = "wipeup";
static char s_kw_x_0[1] = "";
static char s_kw_y_0[1] = "";
static char s_kw_z_0[1] = "";

static KeywordEntry s_kwList_dflt[19] = {   /* 0x0045cd18 */
    { s_kw_dflt_0, 8009 },
    { s_kw_dflt_1, 8013 },
    { s_kw_dflt_2, 8012 },
    { s_kw_dflt_3, 8005 },
    { s_kw_dflt_4, 4018 },
    { s_kw_dflt_5, 4019 },
    { s_kw_dflt_6, 8003 },
    { s_kw_dflt_7, 8001 },
    { s_kw_dflt_8, 4020 },
    { s_kw_dflt_9, 8002 },
    { s_kw_dflt_10, 8004 },
    { s_kw_dflt_11, 8011 },
    { s_kw_dflt_12, 8008 },
    { s_kw_dflt_13, 8010 },
    { s_kw_dflt_14, 8007 },
    { s_kw_dflt_15, 8006 },
    { s_kw_dflt_16, 8009 },
    { s_kw_dflt_17, 8013 },
    { s_kw_dflt_18, 8012 },
};

static KeywordEntry s_kwList_a[24] = {   /* 0x0045cd90 */
    { s_kw_a_0, 12015 },
    { s_kw_a_1, 16036 },
    { s_kw_a_2, 16002 },
    { s_kw_a_3, 16012 },
    { s_kw_a_4, 16005 },
    { s_kw_a_5, 16014 },
    { s_kw_a_6, 16024 },
    { s_kw_a_7, 16021 },
    { s_kw_a_8, 16023 },
    { s_kw_a_9, 16006 },
    { s_kw_a_10, 16001 },
    { s_kw_a_11, 16004 },
    { s_kw_a_12, 16003 },
    { s_kw_a_13, 12075 },
    { s_kw_a_14, 20083 },
    { s_kw_a_15, 16044 },
    { s_kw_a_16, 16043 },
    { s_kw_a_17, 16051 },
    { s_kw_a_18, 20023 },
    { s_kw_a_19, 12085 },
    { s_kw_a_20, 12093 },
    { s_kw_a_21, 12104 },
    { s_kw_a_22, 12107 },
    { s_kw_a_23, 8005 },
};

static KeywordEntry s_kwList_b[8] = {   /* 0x0045ce20 */
    { s_kw_b_0, 24001 },
    { s_kw_b_1, 24002 },
    { s_kw_b_2, 12051 },
    { s_kw_b_3, 12049 },
    { s_kw_b_4, 12031 },
    { s_kw_b_5, 12025 },
    { s_kw_b_6, 20005 },
    { s_kw_b_7, 12065 },
};

static KeywordEntry s_kwList_c[50] = {   /* 0x0045ce50 */
    { s_kw_c_0, 4011 },
    { s_kw_c_1, 16035 },
    { s_kw_c_2, 12047 },
    { s_kw_c_3, 12014 },
    { s_kw_c_4, 12042 },
    { s_kw_c_5, 12033 },
    { s_kw_c_6, 12057 },
    { s_kw_c_7, 12020 },
    { s_kw_c_8, 12061 },
    { s_kw_c_9, 4001 },
    { s_kw_c_10, 12038 },
    { s_kw_c_11, 16011 },
    { s_kw_c_12, 12039 },
    { s_kw_c_13, 20012 },
    { s_kw_c_14, 20044 },
    { s_kw_c_15, 20029 },
    { s_kw_c_16, 20035 },
    { s_kw_c_17, 20031 },
    { s_kw_c_18, 16029 },
    { s_kw_c_19, 20046 },
    { s_kw_c_20, 20019 },
    { s_kw_c_21, 20033 },
    { s_kw_c_22, 20018 },
    { s_kw_c_23, 20014 },
    { s_kw_c_24, 20053 },
    { s_kw_c_25, 20038 },
    { s_kw_c_26, 20051 },
    { s_kw_c_27, 20060 },
    { s_kw_c_28, 20049 },
    { s_kw_c_29, 20061 },
    { s_kw_c_30, 20062 },
    { s_kw_c_31, 20063 },
    { s_kw_c_32, 20048 },
    { s_kw_c_33, 20071 },
    { s_kw_c_34, 20072 },
    { s_kw_c_35, 20075 },
    { s_kw_c_36, 20077 },
    { s_kw_c_37, 20079 },
    { s_kw_c_38, 20078 },
    { s_kw_c_39, 20081 },
    { s_kw_c_40, 16031 },
    { s_kw_c_41, 20104 },
    { s_kw_c_42, 20106 },
    { s_kw_c_43, 20107 },
    { s_kw_c_44, 12088 },
    { s_kw_c_45, 16034 },
    { s_kw_c_46, 20082 },
    { s_kw_c_47, 20118 },
    { s_kw_c_48, 12110 },
    { s_kw_c_49, 20105 },
};

static KeywordEntry s_kwList_d[6] = {   /* 0x0045cf80 */
    { s_kw_d_0, 12080 },
    { s_kw_d_1, 12040 },
    { s_kw_d_2, 12004 },
    { s_kw_d_3, 4029 },
    { s_kw_d_4, 4028 },
    { s_kw_d_5, 12024 },
};

static KeywordEntry s_kwList_e[9] = {   /* 0x0045cfa8 */
    { s_kw_e_0, 4008 },
    { s_kw_e_1, 4004 },
    { s_kw_e_2, 4015 },
    { s_kw_e_3, 4007 },
    { s_kw_e_4, 4010 },
    { s_kw_e_5, 4017 },
    { s_kw_e_6, 12053 },
    { s_kw_e_7, 12008 },
    { s_kw_e_8, 4005 },
};

static KeywordEntry s_kwList_f[11] = {   /* 0x0045cfe0 */
    { s_kw_f_0, 4022 },
    { s_kw_f_1, 12064 },
    { s_kw_f_2, 20037 },
    { s_kw_f_3, 12073 },
    { s_kw_f_4, 4012 },
    { s_kw_f_5, 16022 },
    { s_kw_f_6, 20054 },
    { s_kw_f_7, 20067 },
    { s_kw_f_8, 20074 },
    { s_kw_f_9, 20027 },
    { s_kw_f_10, 12052 },
};

static KeywordEntry s_kwList_g[2] = {   /* 0x0045d028 */
    { s_kw_g_0, 4002 },
    { s_kw_g_1, 12062 },
};

static KeywordEntry s_kwList_h[6] = {   /* 0x0045d038 */
    { s_kw_h_0, 12028 },
    { s_kw_h_1, 12029 },
    { s_kw_h_2, 12030 },
    { s_kw_h_3, 20121 },
    { s_kw_h_4, 20070 },
    { s_kw_h_5, 12002 },
};

static KeywordEntry s_kwList_i[22] = {   /* 0x0045d060 */
    { s_kw_i_0, 4006 },
    { s_kw_i_1, 20119 },
    { s_kw_i_2, 20013 },
    { s_kw_i_3, 20022 },
    { s_kw_i_4, 20045 },
    { s_kw_i_5, 20030 },
    { s_kw_i_6, 20036 },
    { s_kw_i_7, 20032 },
    { s_kw_i_8, 20047 },
    { s_kw_i_9, 20020 },
    { s_kw_i_10, 20034 },
    { s_kw_i_11, 20015 },
    { s_kw_i_12, 24003 },
    { s_kw_i_13, 24004 },
    { s_kw_i_14, 20011 },
    { s_kw_i_15, 20010 },
    { s_kw_i_16, 20009 },
    { s_kw_i_17, 20076 },
    { s_kw_i_18, 20052 },
    { s_kw_i_19, 20102 },
    { s_kw_i_20, 20103 },
    { s_kw_i_21, 20101 },
};

static KeywordEntry s_kwList_j[1] = {   /* 0x0045d0e8 */
    { s_kw_j_0, 1 },
};

static KeywordEntry s_kwList_k[1] = {   /* 0x0045d0f0 */
    { s_kw_k_0, 16045 },
};

static KeywordEntry s_kwList_l[1] = {   /* 0x0045d0f8 */
    { s_kw_l_0, 4003 },
};

static KeywordEntry s_kwList_m[11] = {   /* 0x0045d100 */
    { s_kw_m_0, 12007 },
    { s_kw_m_1, 20004 },
    { s_kw_m_2, 12001 },
    { s_kw_m_3, 20006 },
    { s_kw_m_4, 12023 },
    { s_kw_m_5, 4026 },
    { s_kw_m_6, 20065 },
    { s_kw_m_7, 20066 },
    { s_kw_m_8, 16037 },
    { s_kw_m_9, 12082 },
    { s_kw_m_10, 12005 },
};

static KeywordEntry s_kwList_n[15] = {   /* 0x0045d148 */
    { s_kw_n_0, 4023 },
    { s_kw_n_1, 12079 },
    { s_kw_n_2, 20026 },
    { s_kw_n_3, 12095 },
    { s_kw_n_4, 12096 },
    { s_kw_n_5, 20110 },
    { s_kw_n_6, 20111 },
    { s_kw_n_7, 20112 },
    { s_kw_n_8, 20113 },
    { s_kw_n_9, 20114 },
    { s_kw_n_10, 20115 },
    { s_kw_n_11, 12097 },
    { s_kw_n_12, 12098 },
    { s_kw_n_13, 24019 },
    { s_kw_n_14, 12099 },
};

static KeywordEntry s_kwList_o[9] = {   /* 0x0045d1a8 */
    { s_kw_o_0, 12013 },
    { s_kw_o_1, 12018 },
    { s_kw_o_2, 12032 },
    { s_kw_o_3, 12056 },
    { s_kw_o_4, 12019 },
    { s_kw_o_5, 12060 },
    { s_kw_o_6, 20058 },
    { s_kw_o_7, 12078 },
    { s_kw_o_8, 8006 },
};

static KeywordEntry s_kwList_p[55] = {   /* 0x0045d1e0 */
    { s_kw_p_0, 4025 },
    { s_kw_p_1, 24014 },
    { s_kw_p_2, 12017 },
    { s_kw_p_3, 12021 },
    { s_kw_p_4, 20039 },
    { s_kw_p_5, 20017 },
    { s_kw_p_6, 20043 },
    { s_kw_p_7, 20040 },
    { s_kw_p_8, 20041 },
    { s_kw_p_9, 20042 },
    { s_kw_p_10, 20002 },
    { s_kw_p_11, 20003 },
    { s_kw_p_12, 16009 },
    { s_kw_p_13, 16016 },
    { s_kw_p_14, 12105 },
    { s_kw_p_15, 16013 },
    { s_kw_p_16, 16019 },
    { s_kw_p_17, 16028 },
    { s_kw_p_18, 16027 },
    { s_kw_p_19, 16026 },
    { s_kw_p_20, 16020 },
    { s_kw_p_21, 16025 },
    { s_kw_p_22, 16015 },
    { s_kw_p_23, 16018 },
    { s_kw_p_24, 16017 },
    { s_kw_p_25, 12054 },
    { s_kw_p_26, 12044 },
    { s_kw_p_27, 12041 },
    { s_kw_p_28, 20028 },
    { s_kw_p_29, 12046 },
    { s_kw_p_30, 20057 },
    { s_kw_p_31, 12074 },
    { s_kw_p_32, 16053 },
    { s_kw_p_33, 20064 },
    { s_kw_p_34, 12043 },
    { s_kw_p_35, 12076 },
    { s_kw_p_36, 12027 },
    { s_kw_p_37, 20098 },
    { s_kw_p_38, 16042 },
    { s_kw_p_39, 16032 },
    { s_kw_p_40, 16046 },
    { s_kw_p_41, 16048 },
    { s_kw_p_42, 16049 },
    { s_kw_p_43, 16050 },
    { s_kw_p_44, 12083 },
    { s_kw_p_45, 12084 },
    { s_kw_p_46, 16052 },
    { s_kw_p_47, 12086 },
    { s_kw_p_48, 12037 },
    { s_kw_p_49, 12089 },
    { s_kw_p_50, 20024 },
    { s_kw_p_51, 20109 },
    { s_kw_p_52, 12094 },
    { s_kw_p_53, 12108 },
    { s_kw_p_54, 16047 },
};

static KeywordEntry s_kwList_q[2] = {   /* 0x0045d330 */
    { s_kw_q_0, 20068 },
    { s_kw_q_1, 12071 },
};

static KeywordEntry s_kwList_r[5] = {   /* 0x0045d340 */
    { s_kw_r_0, 20001 },
    { s_kw_r_1, 4024 },
    { s_kw_r_2, 20116 },
    { s_kw_r_3, 12106 },
    { s_kw_r_4, 16010 },
};

static KeywordEntry s_kwList_s[67] = {   /* 0x0045d360 */
    { s_kw_s_0, 12036 },
    { s_kw_s_1, 20090 },
    { s_kw_s_2, 12050 },
    { s_kw_s_3, 24005 },
    { s_kw_s_4, 24008 },
    { s_kw_s_5, 24007 },
    { s_kw_s_6, 24006 },
    { s_kw_s_7, 12016 },
    { s_kw_s_8, 12068 },
    { s_kw_s_9, 12048 },
    { s_kw_s_10, 12069 },
    { s_kw_s_11, 12055 },
    { s_kw_s_12, 12045 },
    { s_kw_s_13, 12034 },
    { s_kw_s_14, 12067 },
    { s_kw_s_15, 12059 },
    { s_kw_s_16, 12070 },
    { s_kw_s_17, 12035 },
    { s_kw_s_18, 16007 },
    { s_kw_s_19, 16008 },
    { s_kw_s_20, 12058 },
    { s_kw_s_21, 12003 },
    { s_kw_s_22, 12022 },
    { s_kw_s_23, 20016 },
    { s_kw_s_24, 12063 },
    { s_kw_s_25, 4014 },
    { s_kw_s_26, 20007 },
    { s_kw_s_27, 12012 },
    { s_kw_s_28, 12011 },
    { s_kw_s_29, 12010 },
    { s_kw_s_30, 20025 },
    { s_kw_s_31, 20056 },
    { s_kw_s_32, 20059 },
    { s_kw_s_33, 20055 },
    { s_kw_s_34, 12077 },
    { s_kw_s_35, 20073 },
    { s_kw_s_36, 20096 },
    { s_kw_s_37, 12081 },
    { s_kw_s_38, 16040 },
    { s_kw_s_39, 16038 },
    { s_kw_s_40, 16039 },
    { s_kw_s_41, 20080 },
    { s_kw_s_42, 4009 },
    { s_kw_s_43, 20084 },
    { s_kw_s_44, 20092 },
    { s_kw_s_45, 20087 },
    { s_kw_s_46, 20093 },
    { s_kw_s_47, 20088 },
    { s_kw_s_48, 20086 },
    { s_kw_s_49, 20085 },
    { s_kw_s_50, 20091 },
    { s_kw_s_51, 20089 },
    { s_kw_s_52, 20094 },
    { s_kw_s_53, 12066 },
    { s_kw_s_54, 20095 },
    { s_kw_s_55, 20021 },
    { s_kw_s_56, 20100 },
    { s_kw_s_57, 12087 },
    { s_kw_s_58, 12090 },
    { s_kw_s_59, 12092 },
    { s_kw_s_60, 12091 },
    { s_kw_s_61, 20117 },
    { s_kw_s_62, 12109 },
    { s_kw_s_63, 16055 },
    { s_kw_s_64, 16056 },
    { s_kw_s_65, 20120 },
    { s_kw_s_66, 20108 },
};

static KeywordEntry s_kwList_t[15] = {   /* 0x0045d4f8 */
    { s_kw_t_0, 4013 },
    { s_kw_t_1, 20050 },
    { s_kw_t_2, 20008 },
    { s_kw_t_3, 20069 },
    { s_kw_t_4, 12072 },
    { s_kw_t_5, 16041 },
    { s_kw_t_6, 16054 },
    { s_kw_t_7, 4027 },
    { s_kw_t_8, 24016 },
    { s_kw_t_9, 24015 },
    { s_kw_t_10, 24017 },
    { s_kw_t_11, 24018 },
    { s_kw_t_12, 24020 },
    { s_kw_t_13, 24021 },
    { s_kw_t_14, 4021 },
};

static KeywordEntry s_kwList_u[1] = {   /* 0x0045d558 */
    { s_kw_u_0, 1 },
};

static KeywordEntry s_kwList_v[5] = {   /* 0x0045d560 */
    { s_kw_v_0, 24009 },
    { s_kw_v_1, 12009 },
    { s_kw_v_2, 12026 },
    { s_kw_v_3, 20097 },
    { s_kw_v_4, 16030 },
};

static KeywordEntry s_kwList_w[12] = {   /* 0x0045d580 */
    { s_kw_w_0, 12006 },
    { s_kw_w_1, 20099 },
    { s_kw_w_2, 4016 },
    { s_kw_w_3, 24010 },
    { s_kw_w_4, 24013 },
    { s_kw_w_5, 24012 },
    { s_kw_w_6, 16033 },
    { s_kw_w_7, 12100 },
    { s_kw_w_8, 12101 },
    { s_kw_w_9, 12102 },
    { s_kw_w_10, 12103 },
    { s_kw_w_11, 24011 },
};

static KeywordEntry s_kwList_x[1] = {   /* 0x0045d5c8 */
    { s_kw_x_0, 1 },
};

static KeywordEntry s_kwList_y[1] = {   /* 0x0045d5d0 */
    { s_kw_y_0, 1 },
};

static KeywordEntry s_kwList_z[1] = {   /* 0x0045d5d8 */
    { s_kw_z_0, 1 },
};

#else
static KeywordEntry s_kwList_dflt[19] = {   /* 0x0045cd18 */
    { "!=", 8009 },
    { "<=", 8013 },
    { ">=", 8012 },
    { "&", 8005 },
    { "(", 4018 },
    { ")", 4019 },
    { "*", 8003 },
    { "+", 8001 },
    { ",", 4020 },
    { "-", 8002 },
    { "/", 8004 },
    { "<", 8011 },
    { "=", 8008 },
    { ">", 8010 },
    { "@", 8007 },
    { "|", 8006 },
    { "\255", 8009 },
    { "\262", 8013 },
    { "\263", 8012 },
};

static KeywordEntry s_kwList_a[24] = {   /* 0x0045cd90 */
    { "actorscript", 12015 },
    { "actorturn", 16036 },
    { "actordeg", 16002 },
    { "actordist", 16012 },
    { "actoris3d", 16005 },
    { "actorpose", 16014 },
    { "actorscale", 16024 },
    { "actorset", 16021 },
    { "actorspeed", 16023 },
    { "actorstar", 16006 },
    { "actorvisible", 16001 },
    { "actorxy", 16004 },
    { "actorxyz", 16003 },
    { "actorinstance", 12075 },
    { "actionframe", 20083 },
    { "actorvalue", 16044 },
    { "actorowner", 16043 },
    { "actorzclip", 16051 },
    { "actorexists", 20023 },
    { "actorwarm", 12085 },
    { "actordelete", 12093 },
    { "actorhide", 12104 },
    { "actorlock", 12107 },
    { "and", 8005 },
};

static KeywordEntry s_kwList_b[8] = {   /* 0x0045ce20 */
    { "barndoorclose", 24001 },
    { "barndooropen", 24002 },
    { "blackscreen", 12051 },
    { "blacktoscreen", 12049 },
    { "bootscript", 12031 },
    { "bothsound", 12025 },
    { "button", 20005 },
    { "buttonscript", 12065 },
};

static KeywordEntry s_kwList_c[50] = {   /* 0x0045ce50 */
    { "case", 4011 },
    { "camerahi", 16035 },
    { "castscript", 12047 },
    { "closecastfile", 12014 },
    { "closepuppetfile", 12042 },
    { "closesetfile", 12033 },
    { "closeshopfile", 12057 },
    { "closetrackfile", 12020 },
    { "closestagefile", 12061 },
    { "code", 4001 },
    { "clut", 12038 },
    { "currentview", 16011 },
    { "cursor", 12039 },
    { "countactors", 20012 },
    { "countbuttons", 20044 },
    { "countcasts", 20029 },
    { "countflats", 20035 },
    { "countprops", 20031 },
    { "currentscene", 16029 },
    { "countpuppets", 20046 },
    { "countscenes", 20019 },
    { "countshops", 20033 },
    { "countpaintings", 20018 },
    { "countsounds", 20014 },
    { "currentset", 20053 },
    { "currentflat", 20038 },
    { "countglobals", 20051 },
    { "commandkey", 20060 },
    { "currentpuppet", 20049 },
    { "calcvectx", 20061 },
    { "calcvecty", 20062 },
    { "cameraxyz", 20063 },
    { "currentstage", 20048 },
    { "calcdeg", 20071 },
    { "calcturn", 20072 },
    { "counttracks", 20075 },
    { "currentsound", 20077 },
    { "currenttheme", 20079 },
    { "currentvoice", 20078 },
    { "calcdist", 20081 },
    { "currentdeg", 16031 },
    { "countcrickets", 20104 },
    { "countwalks", 20106 },
    { "countbevels", 20107 },
    { "castwarm", 12088 },
    { "currentcd", 16034 },
    { "calcmod", 20082 },
    { "countviews", 20118 },
    { "castlock", 12110 },
    { "countloops", 20105 },
};

static KeywordEntry s_kwList_d[6] = {   /* 0x0045cf80 */
    { "drawstring", 12080 },
    { "debugger", 12040 },
    { "delay", 12004 },
    { "dumpglobal", 4029 },
    { "dumplocal", 4028 },
    { "dualsound", 12024 },
};

static KeywordEntry s_kwList_e[9] = {   /* 0x0045cfa8 */
    { "else", 4008 },
    { "endcode", 4004 },
    { "endfor", 4015 },
    { "endif", 4007 },
    { "endswitch", 4010 },
    { "endwhile", 4017 },
    { "error", 12053 },
    { "exportclut", 12008 },
    { "exitcode", 4005 },
};

static KeywordEntry s_kwList_f[11] = {   /* 0x0045cfe0 */
    { "false", 4022 },
    { "flatscript", 12064 },
    { "flattoindex", 20037 },
    { "flushevents", 12073 },
    { "for", 4012 },
    { "framerate", 16022 },
    { "findword", 20054 },
    { "fileexists", 20067 },
    { "frame", 20074 },
    { "freemem", 20027 },
    { "forceupdate", 12052 },
};

static KeywordEntry s_kwList_g[2] = {   /* 0x0045d028 */
    { "global", 4002 },
    { "gotoflat", 12062 },
};

static KeywordEntry s_kwList_h[6] = {   /* 0x0045d038 */
    { "haltsound", 12028 },
    { "halttheme", 12029 },
    { "haltvoice", 12030 },
    { "heapsize", 20121 },
    { "hittest", 20070 },
    { "hidecursor", 12002 },
};

static KeywordEntry s_kwList_i[22] = {   /* 0x0045d060 */
    { "if", 4006 },
    { "indextoview", 20119 },
    { "indextoactor", 20013 },
    { "indextopainting", 20022 },
    { "indextobutton", 20045 },
    { "indextocast", 20030 },
    { "indextoflat", 20036 },
    { "indextoprop", 20032 },
    { "indextopuppet", 20047 },
    { "indextoscene", 20020 },
    { "indextoshop", 20034 },
    { "indextosound", 20015 },
    { "irisclose", 24003 },
    { "irisopen", 24004 },
    { "iscricket", 20011 },
    { "isloop", 20010 },
    { "iswalk", 20009 },
    { "indextotrack", 20076 },
    { "indextoglobal", 20052 },
    { "indextoloop", 20102 },
    { "indextowalk", 20103 },
    { "indextocricket", 20101 },
};

static KeywordEntry s_kwList_j[1] = {   /* 0x0045d0e8 */
    { "", 1 },
};

static KeywordEntry s_kwList_k[1] = {   /* 0x0045d0f0 */
    { "keyaborts", 16045 },
};

static KeywordEntry s_kwList_l[1] = {   /* 0x0045d0f8 */
    { "local", 4003 },
};

static KeywordEntry s_kwList_m[11] = {   /* 0x0045d100 */
    { "makecricket", 12007 },
    { "makepoint", 20004 },
    { "message", 12001 },
    { "mouse", 20006 },
    { "multiplesound", 12023 },
    { "me", 4026 },
    { "machinetype", 20065 },
    { "machinespeed", 20066 },
    { "menuvisible", 16037 },
    { "mixclut", 12082 },
    { "makeloop", 12005 },
};

static KeywordEntry s_kwList_n[15] = {   /* 0x0045d148 */
    { "not", 4023 },
    { "notedialog", 12079 },
    { "numtostring", 20026 },
    { "netconnect", 12095 },
    { "netdisconnect", 12096 },
    { "networkon", 20110 },
    { "netcountmembers", 20111 },
    { "netindextomember", 20112 },
    { "netourid", 20113 },
    { "netmessagesender", 20114 },
    { "netmessagegroup", 20115 },
    { "netbroadcast", 12097 },
    { "netjoingroup", 12098 },
    { "nodraw", 24019 },
    { "netleavegroup", 12099 },
};

static KeywordEntry s_kwList_o[9] = {   /* 0x0045d1a8 */
    { "opencastfile", 12013 },
    { "openpuppetfile", 12018 },
    { "opensetfile", 12032 },
    { "openshopfile", 12056 },
    { "opentrackfile", 12019 },
    { "openstagefile", 12060 },
    { "optionkey", 20058 },
    { "opengame", 12078 },
    { "or", 8006 },
};

static KeywordEntry s_kwList_p[55] = {   /* 0x0045d1e0 */
    { "passcode", 4025 },
    { "plain", 24014 },
    { "playmovie", 12017 },
    { "playtheme", 12021 },
    { "pointinactor", 20039 },
    { "pointinpainting", 20017 },
    { "pointinbutton", 20043 },
    { "pointinprop", 20040 },
    { "pointinset", 20041 },
    { "pointinstage", 20042 },
    { "pointx", 20002 },
    { "pointy", 20003 },
    { "path", 16009 },
    { "propdeg", 16016 },
    { "prophide", 12105 },
    { "propdist", 16013 },
    { "propis3d", 16019 },
    { "propscale", 16028 },
    { "propset", 16027 },
    { "propspeed", 16026 },
    { "propstar", 16020 },
    { "propview", 16025 },
    { "propvisible", 16015 },
    { "propxy", 16018 },
    { "propxyz", 16017 },
    { "propscript", 12054 },
    { "puppetbevel", 12044 },
    { "puppetclear", 12041 },
    { "puppetevent", 20028 },
    { "puppetscript", 12046 },
    { "putword", 20057 },
    { "puppetgrab", 12074 },
    { "puppetbase", 16053 },
    { "playerxyz", 20064 },
    { "puppetspeak", 12043 },
    { "propinstance", 12076 },
    { "plugin", 12027 },
    { "pluginfx", 20098 },
    { "propvalue", 16042 },
    { "propowner", 16032 },
    { "pauseloop", 16046 },
    { "pausewalk", 16048 },
    { "puppetparam", 16049 },
    { "puppetvisible", 16050 },
    { "puppetscramble", 12083 },
    { "puppetsubtitle", 12084 },
    { "propzclip", 16052 },
    { "propwarm", 12086 },
    { "paintingscript", 12037 },
    { "postscript", 12089 },
    { "propexists", 20024 },
    { "pluginexists", 20109 },
    { "propdelete", 12094 },
    { "proplock", 12108 },
    { "pausecricket", 16047 },
};

static KeywordEntry s_kwList_q[2] = {   /* 0x0045d330 */
    { "questiondialog", 20068 },
    { "quit", 12071 },
};

static KeywordEntry s_kwList_r[5] = {   /* 0x0045d340 */
    { "random", 20001 },
    { "return", 4024 },
    { "roadahead", 20116 },
    { "reboot", 12106 },
    { "result", 16010 },
};

static KeywordEntry s_kwList_s[67] = {   /* 0x0045d360 */
    { "scenescript", 12036 },
    { "sendtopaintingfx", 20090 },
    { "screentoblack", 12050 },
    { "scrolldown", 24005 },
    { "scrolleft", 24008 },
    { "scrollright", 24007 },
    { "scrollup", 24006 },
    { "sendtoactor", 12016 },
    { "sendtobutton", 12068 },
    { "sendtocast", 12048 },
    { "sendtoflat", 12069 },
    { "sendtoprop", 12055 },
    { "sendtopuppet", 12045 },
    { "sendtoscene", 12034 },
    { "sendtoset", 12067 },
    { "sendtoshop", 12059 },
    { "sendtostage", 12070 },
    { "setscript", 12035 },
    { "setvisible", 16007 },
    { "stagevisible", 16008 },
    { "shopscript", 12058 },
    { "showcursor", 12003 },
    { "singlesound", 12022 },
    { "sounddone", 20016 },
    { "stagescript", 12063 },
    { "step", 4014 },
    { "stilldown", 20007 },
    { "stopcricket", 12012 },
    { "stoploop", 12011 },
    { "stopwalk", 12010 },
    { "stringtonum", 20025 },
    { "stringlength", 20056 },
    { "shiftkey", 20059 },
    { "substring", 20055 },
    { "savegame", 12077 },
    { "starxyz", 20073 },
    { "scenexyz", 20096 },
    { "sendtoboot", 12081 },
    { "soundvol", 16040 },
    { "soundpan", 16038 },
    { "soundloop", 16039 },
    { "soundrate", 20080 },
    { "switch", 4009 },
    { "sendtoactorfx", 20084 },
    { "sendtobuttonfx", 20092 },
    { "sendtocastfx", 20087 },
    { "sendtoflatfx", 20093 },
    { "sendtopropfx", 20088 },
    { "sendtopuppetfx", 20086 },
    { "sendtoscenefx", 20085 },
    { "sendtosetfx", 20091 },
    { "sendtoshopfx", 20089 },
    { "sendtostagefx", 20094 },
    { "sendtopainting", 12066 },
    { "sendtobootfx", 20095 },
    { "sendtopostfx", 20021 },
    { "sendtoserverfx", 20100 },
    { "shopwarm", 12087 },
    { "sendtopost", 12090 },
    { "sendtoserver", 12092 },
    { "serverscript", 12091 },
    { "stringwidth", 20117 },
    { "shoplock", 12109 },
    { "setparam", 16055 },
    { "stageparam", 16056 },
    { "sysmem", 20120 },
    { "sqrt", 20108 },
};

static KeywordEntry s_kwList_t[15] = {   /* 0x0045d4f8 */
    { "to", 4013 },
    { "type", 20050 },
    { "tick", 20008 },
    { "textdialog", 20069 },
    { "turntodeg", 12072 },
    { "themevol", 16041 },
    { "themepan", 16054 },
    { "target", 4027 },
    { "turnleft", 24016 },
    { "turnright", 24015 },
    { "turnup", 24017 },
    { "turndown", 24018 },
    { "turnhalfleft", 24020 },
    { "turnhalfright", 24021 },
    { "true", 4021 },
};

static KeywordEntry s_kwList_u[1] = {   /* 0x0045d558 */
    { "", 1 },
};

static KeywordEntry s_kwList_v[5] = {   /* 0x0045d560 */
    { "venetian", 24009 },
    { "visualeffect", 12009 },
    { "voicesound", 12026 },
    { "voicedone", 20097 },
    { "variable", 16030 },
};

static KeywordEntry s_kwList_w[12] = {   /* 0x0045d580 */
    { "walktostar", 12006 },
    { "walkdest", 20099 },
    { "while", 4016 },
    { "wipedown", 24010 },
    { "wipeleft", 24013 },
    { "wiperight", 24012 },
    { "wavevolume", 16033 },
    { "walkonpath", 12100 },
    { "walktoxyz", 12101 },
    { "walkonroad", 12102 },
    { "walkonframes", 12103 },
    { "wipeup", 24011 },
};

static KeywordEntry s_kwList_x[1] = {   /* 0x0045d5c8 */
    { "", 1 },
};

static KeywordEntry s_kwList_y[1] = {   /* 0x0045d5d0 */
    { "", 1 },
};

static KeywordEntry s_kwList_z[1] = {   /* 0x0045d5d8 */
    { "", 1 },
};

#endif

KeywordList g_keywordDefault = { s_kwList_dflt, 19 };   /* 0x0045d5e0 */

KeywordList g_keywordByLetter[26] = {   /* 0x0045d5e8, 'a'..'z' */
    { s_kwList_a, 24 },   /* 'a' */
    { s_kwList_b, 8 },   /* 'b' */
    { s_kwList_c, 50 },   /* 'c' */
    { s_kwList_d, 6 },   /* 'd' */
    { s_kwList_e, 9 },   /* 'e' */
    { s_kwList_f, 11 },   /* 'f' */
    { s_kwList_g, 2 },   /* 'g' */
    { s_kwList_h, 6 },   /* 'h' */
    { s_kwList_i, 22 },   /* 'i' */
    { s_kwList_j, 1 },   /* 'j' */
    { s_kwList_k, 1 },   /* 'k' */
    { s_kwList_l, 1 },   /* 'l' */
    { s_kwList_m, 11 },   /* 'm' */
    { s_kwList_n, 15 },   /* 'n' */
    { s_kwList_o, 9 },   /* 'o' */
    { s_kwList_p, 55 },   /* 'p' */
    { s_kwList_q, 2 },   /* 'q' */
    { s_kwList_r, 5 },   /* 'r' */
    { s_kwList_s, 67 },   /* 's' */
    { s_kwList_t, 15 },   /* 't' */
    { s_kwList_u, 1 },   /* 'u' */
    { s_kwList_v, 5 },   /* 'v' */
    { s_kwList_w, 12 },   /* 'w' */
    { s_kwList_x, 1 },   /* 'x' */
    { s_kwList_y, 1 },   /* 'y' */
    { s_kwList_z, 1 },   /* 'z' */
};

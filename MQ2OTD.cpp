

// MQ2Otd.cpp : Displays an overhead compass showing
//     target direction and range on the hud.
// Author: Omnictrl
// Version: 0.0.2
// Date: 20051001
// Note: Check forum for update, read the two lines above in posted source.

// I am the king of updating all the version information in the comments
// and not updating the following line.
#define OTD_Version "MQ2Otd (Overhead Target Direction) ver 0.0.2"

/*
* Version 0.0.1 - 20050927
* - Genesis & Beta.
*
* Version 0.0.2 20051001
* - Added ability to move range below circumference of direction pointer
* based on Sorcier's input. use '/otd drawtight' to toggle this.
* - Centered range output based on Sorcier's input. If I wanted to get
* fancy, I could call the font output directly, using the font members
* to find the pixel width. Maybe later. Though it would be cool if there
* was a DrawHudTextCentered already there.
* - Load ini properly when changing characters or plugin reloaded,
* based on htw's input.
* - Changed clockPositions to directionPositions, since there are actually
* only 8 characters used (|/_\|/_\) instead of 12 (for clock positions)
* to make the display more accurate. This is currently a boolean, in case
* it doesn't work as well for you (OTD_bActual), you can turn off.
* - Added getloc command to show the current x,y position.
* - Added Ini support for colors, as suggested by Sorcier:
* Add [COLORS] section, and add settings in the following format:
* RANGECOLORN=HEX_RGB_COLOR RANGE COLOR_MORPH_FLAG
* Note: these setting must be in ascending range order,
* or none will be loaded (defaults used instead).
* Also, there is a LABELCOLOR setting that is used for coloring
* the range number.
* Here is an example
* [COLORS]
* LABELCOLOR=19FF19
* RANGECOLOR1=FF1919 10 0
* RANGECOLOR2=1919FF 11 0
* RANGECOLOR3=19FF19 200 1
* RANGECOLOR4=C8C820 201 0
* RANGECOLOR5=505020 500 1
* RANGECOLOR6=F0F0F0 501 0
*/

// ToDo
// Find out the size of the screen, and limit moveto
// Allow saving location by screen size? Anyone need this?
// Allow alternate contrast colors?

#include "../MQ2Plugin.h"

#define MAX_RANGE_COLORS 20
#define size 5  // pick a number here, 4 and 5 are good

PreSetup("MQ2Otd");

bool bChangedCharacter=true;

                //   1    2  3   4    5   6   7   8   9   0   A    B
char clockVals[12] = {'/','_','_','_','\\','|','/','_','_','_','\\','|'};
                  // 1  2  3  4  5  6  7  8  9 10 11 12
int xOffsets[12] = { 5, 7, 7, 7, 5, 3,-1,-6,-6,-6,-1, 3};
int yOffsets[12] = {-8,-6,-6,-6, 7, 8, 7,-6,-6,-6,-8,-10};

                //     1   2   3    4   5   6   7    8
char clockValsAct[8] = {'/','_','\\','|','/','_','\\','|'};
                    // 1  2  3  4  5  6  7  8
int xOffsetsAct[8] = { 5, 7, 5, 3,-1,-6,-1, 3};
int yOffsetsAct[8] = {-8,-6, 7, 8, 7,-6,-8,-10};

// These are changed by settings, atm.

int yLabelOffset=20;
int xLabelOffset=0;

// Maybe Ini Var later.

bool OTD_bActual=true;

// Ini vars

bool OTD_bIsOn=true;
bool OTD_bRangeIsOn=true;
bool OTD_bDrawTight=false;
bool OTD_bColorChange=true;
int OTD_BasePosX=200;
int OTD_BasePosY=200;

ARGBCOLOR OTD_LabelColor;

struct tagRangeColor
{
   int range;
   bool bColorMorph; // Morph from last color to this color
   ARGBCOLOR color;
};

// Yes, I know it's lazy to not dynamically allocate this memory.
struct tagRangeColor OTD_RangeColorList[MAX_RANGE_COLORS];
int OTD_iRangeColorsUsed;

// Default colors as would be loaded in ini.
// Format: RGB color in hex, distance, colorMorph on or off
// color morph controls whether color is morped to this color from last
char *OTD_ppszDefaultRangeColors[] =
{
   // RGB
   "FF1919 10 0", // 255, 25, 25  - mostly red
   "19FF19 11 0", // 25, 255, 25  - mostly green
   "AFFF19 300 1", // 175, 255, 255
   "FFFF96 301 0", // 255, 255, 150
   "C8C896 800 1", // 200, 200, 150
   "C89696 801 0", // 200, 150, 150
   NULL
};

unsigned char charHexValue(char ch)
{
   // return FF on invalid.
   ch = toupper(ch);
   if (isdigit(ch)) return ch-'0';
   else if (strchr("ABCDEF", ch) != NULL) return 0x0A + (ch-'A');
   return 0xFF;
}

bool parseHexColor(char *szHexColor, BYTE *colorHolder)
{
   unsigned char parsed;

   if (szHexColor)
   {
      parsed = charHexValue(szHexColor[0]);
      if (parsed == 0xFF) return false;
      *colorHolder = parsed * 0x10; // same as << 4
      parsed = charHexValue(szHexColor[1]);
      if (parsed == 0xFF) return false;
      *colorHolder |= parsed;
      return true;
   }

   return false;
}

bool parseColor(char *szHexColor, ARGBCOLOR *pColor)
{
   // return false if cannot parse.
   pColor->A = 0xFF;
   return (parseHexColor(szHexColor, &pColor->R) &&
      parseHexColor(&szHexColor[2], &pColor->G) &&
      parseHexColor(&szHexColor[4], &pColor->B));
}

char *pastNextCh(char *szStr, char ch)
{
   if (szStr == NULL) return NULL;

   // if didn't start on this character, move past others
   if (*szStr && *szStr != ch)
   {
      while (*szStr && *szStr != ch) szStr++;
   }

   // move forward until we get past the given character
   while (*szStr && *szStr == ch) szStr++;

   // return pointer only if not at terminator
   return (*szStr) ? szStr : NULL;
}

bool OTD_parseRangeColor(char *szRangeColorInfo, struct tagRangeColor *prc)
{
   char *pszOffset;
   if (prc == NULL) return false;

   memset(prc, 0, sizeof(*prc));

   if (parseColor(szRangeColorInfo, &prc->color) == false) return false;

   if ((pszOffset = pastNextCh(szRangeColorInfo, ' ')) == NULL) return false;

   prc->range = atoi(pszOffset);

   if ((pszOffset = pastNextCh(pszOffset, ' ')) == NULL) return false;

   prc->bColorMorph = (atoi(pszOffset) != 0);

   return true;
}

VOID dumpColors(void)
{
   char szTemp[255];
   struct tagRangeColor *prc;

   sprintf_s(szTemp, "OTD - Color Dump: %d colors loaded", OTD_iRangeColorsUsed);
   WriteChatColor(szTemp);

   for (int i=0; i < OTD_iRangeColorsUsed; i++)
   {
      prc = &OTD_RangeColorList[i];
      sprintf_s(szTemp, "OTD - RGB %d:%d:%d [%02X%02X%02X], range: %d, morph: %s",
         (int)prc->color.R,
         (int)prc->color.G,
         (int)prc->color.B,
         (int)prc->color.R,
         (int)prc->color.G,
         (int)prc->color.B,
         (int)prc->range,
         (prc->bColorMorph) ? "ON" : "OFF");
      WriteChatColor(szTemp);
   }

   sprintf_s(szTemp, "OTD - LABEL RGB %d:%d:%d [%02X%02X%02X]",
      (int)OTD_LabelColor.R,
      (int)OTD_LabelColor.G,
      (int)OTD_LabelColor.B,
      (int)OTD_LabelColor.R,
      (int)OTD_LabelColor.G,
      (int)OTD_LabelColor.B);

   WriteChatColor(szTemp);

}

VOID OTD_loadDefaultColors()
{
   int i;

   OTD_iRangeColorsUsed = 0;

   for (i = 0;
      OTD_ppszDefaultRangeColors[i] != NULL && i < MAX_RANGE_COLORS;
      i++)
   {
      if (OTD_parseRangeColor(OTD_ppszDefaultRangeColors[i],
         &OTD_RangeColorList[OTD_iRangeColorsUsed]) == false)
      {
         char szTemp[255];
         sprintf_s(szTemp, "OTD - Program error, default RangeColor failed (%d): %s", OTD_iRangeColorsUsed, OTD_ppszDefaultRangeColors[i]);
         WriteChatColor(szTemp);
      }

      OTD_iRangeColorsUsed++;
   }
}

bool OTD_loadIniColors()
{
   bool bLoadFail = false;
   char szKeyName[50];
   char szBuf[256];
   char szMsg[512];
   int i;
   int lastRange = -1;

   OTD_iRangeColorsUsed = 0;

   for (i=0; i <= MAX_RANGE_COLORS+1 && bLoadFail == false; i++)
   {
      sprintf_s(szKeyName, "RANGECOLOR%d", i);
      GetPrivateProfileString("COLORS", szKeyName, "---", szBuf, 255, INIFileName);
      if (strcmp(szBuf, "---") != 0)
      {
         if (OTD_parseRangeColor(szBuf, &OTD_RangeColorList[OTD_iRangeColorsUsed]))
         {
            if (OTD_RangeColorList[OTD_iRangeColorsUsed].range > lastRange)
            {
               lastRange = OTD_RangeColorList[OTD_iRangeColorsUsed].range;
               OTD_iRangeColorsUsed++;
            }
            else
            {
               sprintf_s(szMsg, "OTD - Failed to load RangeColor [%s] - range is less than previously loaded.", szKeyName);
               WriteChatColor(szMsg);
               bLoadFail = true;
            }
         }
         else
         {
            sprintf_s(szMsg, "OTD - Failed to load RangeColor [%s] - line was: %s", szKeyName, szBuf);
            WriteChatColor(szMsg);
            bLoadFail = true;
         }
      }
   }

   if (bLoadFail == true)
   {
      WriteChatColor("OTD - Error loading RangeColors from INI, using defaults instead");
   }

   if (OTD_iRangeColorsUsed == 0) bLoadFail = true;

#ifdef DEVLEVEL
   dumpColors();
#endif

   return (bLoadFail == false);
}

VOID OTD_loadIni(PSPAWNINFO pChar)
{
   char szBuf[256];
   char *szCharName = (pChar) ? pChar->Name : NULL;

   if (szCharName)
   {
      OTD_bIsOn = (GetPrivateProfileInt(szCharName,"VISIBLE",(int)OTD_bIsOn,INIFileName) > 0);
      OTD_bRangeIsOn = (GetPrivateProfileInt(szCharName,"RANGE_ON",(int)OTD_bRangeIsOn,INIFileName) > 0);
   }

   OTD_bDrawTight = (GetPrivateProfileInt("GLOBAL","DRAW_TIGHT",(int)OTD_bDrawTight,INIFileName) > 0);
   OTD_bColorChange = (GetPrivateProfileInt("GLOBAL","COLOR_CHANGE",(int)OTD_bColorChange,INIFileName) > 0);

   if (szCharName)
   {
      OTD_BasePosX = GetPrivateProfileInt(szCharName,"BASE_X",OTD_BasePosX,INIFileName);
      OTD_BasePosY = GetPrivateProfileInt(szCharName,"BASE_Y",OTD_BasePosY,INIFileName);
   }

   // This may change later, especially if it gets put into Ini for some reason
   yLabelOffset = (OTD_bDrawTight) ? 10 : 20;

   if (OTD_loadIniColors() == false)
   {
      OTD_loadDefaultColors();
   }

   // Default label color to green.
   parseColor("FF2525", &OTD_LabelColor);

   // Load label color
   GetPrivateProfileString("COLORS", "LABELCOLOR", "---", szBuf, 255, INIFileName);
   if (strcmp(szBuf, "---") != 0)
   {
      ARGBCOLOR tempColor;
      if (parseColor(szBuf, &tempColor))
      {
         parseColor(szBuf, &OTD_LabelColor);
      }
   }
}

bool OTD_isValid(void)
{
   return (gGameState==GAMESTATE_INGAME &&
         ppTarget && pTarget && pCharSpawn &&
         OTD_bIsOn && pTarget != pCharSpawn &&
         bChangedCharacter == false);
}

bool OTD_calcCurrentDegrees(FLOAT *pFloat)
{
   if (pFloat == NULL) return false;

   FLOAT degrees = 0.0f;
   PSPAWNINFO psTarget = NULL;

   if (ppTarget && pTarget)
   {
      psTarget = (PSPAWNINFO)pTarget;
   }

   if (psTarget == NULL || pCharSpawn == NULL) return false;

   FLOAT headingTo=(FLOAT)(atan2f(((PSPAWNINFO)pCharSpawn)->Y - psTarget->Y, psTarget->X - ((PSPAWNINFO)pCharSpawn)->X) * 180.0f / PI + 90.0f);
   FLOAT myHeading=((PSPAWNINFO)pCharSpawn)->Heading*0.703125f;

   degrees = myHeading - headingTo;

   while (degrees >= 360.0f) degrees -= 360.0f;
   while (degrees < 0.0f) degrees += 360.0f;

    *pFloat = degrees;

   return true;
}

VOID OTD_setColorOld(FLOAT fDistance, ARGBCOLOR *color)
{
   color->A = 0xFF;
   color->G = 255;
   color->R = 25;
   color->B = 25;

   if (fDistance > 10)
   {
      if (fDistance < 300)
      {
         color->R += ((int)fDistance/2);
      }
      else
      {
         color->R = 200;
         color->B = 150;
         color->G = 150;
         if (fDistance < 800)
         {
            color->R = 255 - ((int)fDistance)/16;
            color->G = 255 - ((int)fDistance)/16;
         }
      }
   }
   else
   {
      color->B=25;
      color->G=25;
      color->R=255;
   }
}

void DebugHud2(char *szString, int i)
{
#ifdef DEV_LEVEL
   DrawHUDText(szString,OTD_BasePosX-5,OTD_BasePosY+50+(i*10),0xFFFFFFFF,size);
#endif
}

void DebugHud(char *szString)
{
   DebugHud2(szString, 0);
}

BYTE OTD_calcMorphColor(BYTE thisColor, BYTE lastColor, FLOAT pctChange, int i)
{
   //FLOAT f = ((FLOAT)lastColor)+(((FLOAT)(thisColor-lastColor))*pctChange);
   int change = thisColor-lastColor;
   float delta = (float)pctChange * change;
   FLOAT f = lastColor + delta;

#ifdef DEV_LEVEL
   char szTemp[200];

   sprintf_s(szTemp, "%X(%d)->%X(%d), pctchg:%f, chg:%d del:%f new:%d",
      (int)lastColor, (int)lastColor, (int)thisColor, (int)thisColor, pctChange, change, delta, (int)f);
   DebugHud2(szTemp, i);
#endif

   return (BYTE)f;
}

VOID OTD_morphRangeColor(int iIndex, ARGBCOLOR *color, int distance)
{
   // These pointers are just for readability.
   struct tagRangeColor *prcThis = &OTD_RangeColorList[iIndex];
   struct tagRangeColor *prcLast = &OTD_RangeColorList[iIndex-1];
   float numerator = (float)(distance - prcLast->range);
   float divisor = (float)(prcThis->range - prcLast->range);
   float pctChange;

   pctChange = (divisor == 0.0f) ? 0.0f : numerator/divisor;

   color->R = OTD_calcMorphColor(prcThis->color.R, prcLast->color.R, pctChange, 0);
   color->G = OTD_calcMorphColor(prcThis->color.G, prcLast->color.G, pctChange, 1);
   color->B = OTD_calcMorphColor(prcThis->color.B, prcLast->color.B, pctChange, 2);
}

VOID OTD_DebugColor(ARGBCOLOR *color, bool bFound)
{
   char szTemp[100];
   sprintf_s(szTemp, "Colors: %c %d:%d:%d [%02X%02X%02X]",
      (bFound) ? 'Y' : 'N',
      (int)color->R,
      (int)color->G,
      (int)color->B,
      (int)color->R,
      (int)color->G,
      (int)color->B
      );
   DrawHUDText(szTemp,OTD_BasePosX-5,OTD_BasePosY+40,color->ARGB,size);
}

VOID OTD_setColor(FLOAT fDistance, ARGBCOLOR *color)
{
   color->A = 0xFF;
   // default to something sane (red, in this case)
   color->B=25;
   color->G=25;
   color->R=255;
   int i;
   bool bFound = false;

   for (i=0; i < OTD_iRangeColorsUsed && bFound == false; i++)
   {
      if (OTD_RangeColorList[i].range > (int)fDistance)
      {
         bFound = true;
         if (i > 0 && OTD_RangeColorList[i].bColorMorph)
         {
            OTD_morphRangeColor(i, color, (int)fDistance);
         }
         else
         {
            DebugHud("NO Morph");
            color->R = OTD_RangeColorList[i].color.R;
            color->G = OTD_RangeColorList[i].color.G;
            color->B = OTD_RangeColorList[i].color.B;
         }
      }
   }

   if (bFound == false)
   {
      DebugHud("Not found");
      i = (OTD_iRangeColorsUsed - 1); // Default to last color
      if (i >= 0)
      {
         color->R = OTD_RangeColorList[i].color.R;
         color->G = OTD_RangeColorList[i].color.G;
         color->B = OTD_RangeColorList[i].color.B;
      }
   }

#ifdef DEV_LEVEL
   OTD_DebugColor(color, bFound);
#endif
}

int OTD_directionPos(FLOAT fCurrentDegrees)
{
   // Are we using actual (8) directions, or the 12 clock directions

   int directions = (OTD_bActual) ? 8 : 12;
   float divisor = 360.0f/(FLOAT)directions;

   float fPos;
   int iPos;

   // adjust back by half an angle
   fPos = fCurrentDegrees - (divisor/2.0f);
   // Handle wrap under
   if (fPos < 0.0f) fPos += 360.0f;
   // get angle section
   fPos /= divisor;

   // Sanity check, should never be off but
   // such bad things can happen if it is

   iPos = ((int)fPos)%directions;

   // Since position is not zero based. So add 1.
   return iPos+1;
}

void OTD_render(void)
{
   char szTemp[50];
   int directions = (OTD_bActual) ? 8 : 12;
   char *pcVals = (OTD_bActual) ? clockValsAct : clockVals;
   int *piOffsetsX = (OTD_bActual) ? xOffsetsAct : xOffsets;
   int *piOffsetsY = (OTD_bActual) ? yOffsetsAct : yOffsets;

   FLOAT fDegrees = 0.0f;
   ARGBCOLOR Color;
   Color.A=0xFF;
   Color.G = 255;
   Color.R = 25;
   Color.B = 25;

   if (OTD_calcCurrentDegrees(&fDegrees))
   {
      FLOAT fDistance = GetDistance((PSPAWNINFO)pCharSpawn, (PSPAWNINFO)pTarget);
      if (OTD_bRangeIsOn)
      {
         sprintf_s(szTemp, "%d ft", (int)fDistance);
         DrawHUDText(szTemp,OTD_BasePosX+xLabelOffset-int((strlen(szTemp)-1)*2.5),OTD_BasePosY+yLabelOffset,OTD_LabelColor.ARGB,size);
      }

      int clockPos = OTD_directionPos(fDegrees);

      OTD_setColor(fDistance, &Color);

      DrawHUDText("O",OTD_BasePosX,OTD_BasePosY,Color.ARGB,size);

      if (clockPos < 1) clockPos = 1;
      if (clockPos > directions) clockPos = directions;

      clockPos--;

      sprintf_s(szTemp, "%c", pcVals[clockPos]);
      DrawHUDText(szTemp,OTD_BasePosX+piOffsetsX[clockPos],OTD_BasePosY+piOffsetsY[clockPos],Color.ARGB,size);

      //sprintf_s(szTemp, "%d - %d", clockPos+1, (int)fDegrees);
      //DrawHUDText(szTemp,OTD_BasePosX-5,OTD_BasePosY+30,Color.ARGB);

      //sprintf_s(szTemp, "Screen: %d:%d", GetWidth(), GetHeight());
      //DrawHUDText(szTemp,OTD_BasePosX-5,OTD_BasePosY+30,Color.ARGB);
   }
}

VOID OTD_userCommand(PSPAWNINFO pChar, PCHAR szLine)
{
   static CHAR szOn[] = "on";
   static CHAR szOff[] = "off";
   static CHAR szRange[] = "range";
   static char szGetLoc[] = "getloc";
   static char szMoveTo[] = "moveto";
   static char szMoveBy[] = "moveby";
   static char szDrawTight[] = "drawtight";
   static char szShowVersion[] = "version";
   static char szColorChange[] = "colorchange"; // undocumented - not used anymore
   static char szReload[] = "reload";
   static char szDump[] = "dump";  // not documented
   char szArg[MAX_STRING];

   GetArg(szArg,szLine,1);

   if (_strcmpi(szOn, szArg) == 0)
   {
      OTD_bIsOn = true;
      WriteChatColor("OTD: on");
      WritePrivateProfileString(pChar->Name, "VISIBLE", "1", INIFileName);
   }
   else
   if (_strcmpi(szOff, szArg) == 0)
   {
      OTD_bIsOn = false;
      WriteChatColor("OTD: off");
      WritePrivateProfileString(pChar->Name, "VISIBLE", "0", INIFileName);
   }
   else
   if (_strcmpi(szRange, szArg) == 0)
   {
      OTD_bRangeIsOn = (!OTD_bRangeIsOn);
      WritePrivateProfileString(pChar->Name, "RANGE_ON", (OTD_bRangeIsOn) ? "1" : "0", INIFileName);
      WriteChatColor((OTD_bRangeIsOn) ? "OTD - Range: on" : "OTD - Range: off");
   }
   else
   if (_strcmpi(szGetLoc, szArg) == 0)
   {
      sprintf_s(szArg, "OTD - Loc=x:%d, y:%d", OTD_BasePosX, OTD_BasePosY);
      WriteChatColor(szArg);
   }
   else
   if (_strcmpi(szDrawTight, szArg) == 0)
   {
      OTD_bDrawTight = (!OTD_bDrawTight);
      yLabelOffset = (OTD_bDrawTight) ? 10 : 20;
      WritePrivateProfileString("GLOBAL", "DRAW_TIGHT", (OTD_bDrawTight) ? "1" : "0", INIFileName);
      WriteChatColor((OTD_bDrawTight) ? "OTD - DrawTight: on" : "OTD - DrawTight: off");
   }
   else
   if (_strcmpi(szColorChange, szArg) == 0)
   {
      OTD_bColorChange = (!OTD_bColorChange);
      WritePrivateProfileString("GLOBAL", "COLOR_CHANGE", (OTD_bColorChange) ? "1" : "0", INIFileName);
      WriteChatColor((OTD_bColorChange) ? "OTD - ColorChange: on" : "OTD - ColorChange: off");
      WriteChatColor("OTD - This setting controls whether color is adjusted between defined colors. It does not turn off defined colors.");
   }
   else
   if (_strcmpi(szMoveTo, szArg) == 0 || _strcmpi(szMoveBy, szArg) == 0)
   {
      int x=0, y=0;
      int emptyOffset = 200;

      if (_strcmpi(szMoveBy, szArg) == 0)
      {
         x = OTD_BasePosX;
         y = OTD_BasePosY;
         emptyOffset = 0;
      }

      GetArg(szArg,szLine,2);
      x += (*szArg && (isdigit(*szArg) || *szArg=='-')) ? atoi(szArg) : emptyOffset;

      GetArg(szArg,szLine,3);
      y += (*szArg && (isdigit(*szArg) || *szArg=='-')) ? atoi(szArg) : emptyOffset;

      if (x < 0) x = 200;
      if (y < 0) y = 200;

      OTD_BasePosX = x;
      OTD_BasePosY = y;

      sprintf_s(szArg, "%d", x);
      WritePrivateProfileString(pChar->Name, "BASE_X", szArg, INIFileName);

      sprintf_s(szArg, "%d", y);
      WritePrivateProfileString(pChar->Name, "BASE_Y", szArg, INIFileName);

      sprintf_s(szArg, "OTD - Moved to: x=%d, y=%d", x, y);

      WriteChatColor(szArg);
   }
   else
   if (_strcmpi(szMoveTo, szArg) == 0)
   {
      int x, y;

      GetArg(szArg,szLine,2);
      x = (*szArg && isdigit(*szArg)) ? atoi(szArg) : 200;

      GetArg(szArg,szLine,3);
      y = (*szArg && isdigit(*szArg)) ? atoi(szArg) : 200;

      if (x < 0) x = 200;
      if (y < 0) y = 200;

      OTD_BasePosX = x;
      OTD_BasePosY = y;

      sprintf_s(szArg, "%d", x);
      WritePrivateProfileString(pChar->Name, "BASE_X", szArg, INIFileName);

      sprintf_s(szArg, "%d", y);
      WritePrivateProfileString(pChar->Name, "BASE_Y", szArg, INIFileName);

      sprintf_s(szArg, "OTD - Moved to: x=%d, y=%d", x, y);

      WriteChatColor(szArg);
   }
   else
   if (_strcmpi(szReload, szArg) == 0)
   {
      OTD_loadIni(pChar);
      WriteChatColor("OTD - Ini reloaded");
   }
   else
   if (_strcmpi(szShowVersion, szArg) == 0)
   {
      WriteChatColor(OTD_Version);
   }
   else
   if (_strcmpi(szDump, szArg) == 0)
   {
      dumpColors();
   }
   else
   {
      WriteChatColor(OTD_Version);
      WriteChatColor("Usage: /otd <on|off|range|drawtight|getloc|reload|version|moveto x y|moveby x y> (Overhead Target Direction Compass)");
   }
}

// Called once, when the plugin is to initialize
PLUGIN_API VOID InitializePlugin(VOID)
{
   DebugSpewAlways("Initializing MQ2Otd");
   AddCommand("/otd",OTD_userCommand);

   OTD_loadDefaultColors();

#ifdef DEV_LEVEL
   dumpColors();
#endif

   OTD_loadIni((PSPAWNINFO)pCharSpawn);
   bChangedCharacter = false;
}

// Called once, when the plugin is to shutdown
PLUGIN_API VOID ShutdownPlugin(VOID)
{
   DebugSpewAlways("Shutting down MQ2Otd");
   RemoveCommand("/otd");
}

// Called every frame that the "HUD" is drawn -- e.g. net status / packet loss bar
PLUGIN_API VOID OnDrawHUD(VOID)
{
   // DONT leave in this debugspew, even if you leave in all the others
//   DebugSpewAlways("MQ2Otd::OnDrawHUD()");

   if (bChangedCharacter && gGameState==GAMESTATE_INGAME && pCharSpawn)
   {
      OTD_loadIni((PSPAWNINFO)pCharSpawn);
      bChangedCharacter = false;
   }

   if (OTD_isValid())
   {
      OTD_render();
   }
}

PLUGIN_API VOID SetGameState(DWORD GameState)
{
   if (GameState==GAMESTATE_CHARSELECT) {
      bChangedCharacter = true;
   }
}
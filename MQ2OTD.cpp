// MQ2OTD.cpp : Overhead Target Direction
//				  Show target direction and range on the hud.
// Author: Omnictrl

// TODO
// Lots of narrowing conversions and what looks like some overflows
// Find out the size of the screen, and limit moveto
// Allow saving location by screen size? Anyone need this?
// Allow alternate contrast colors?

#include <mq/Plugin.h>

PreSetup("MQ2OTD");
PLUGIN_VERSION(0.2);

constexpr int MAX_RANGE_COLORS = 20;
constexpr int DRAW_SIZE = 5;  // pick a number here, 4 and 5 are good

bool bChangedCharacter=true;

//							1	 2  3	4	 5	6	7	8	9	0	A	 B
char clockVals[12] = {'/','_','_','_','\\','|','/','_','_','_','\\','|'};
//						 1  2  3  4  5  6  7  8  9 10 11 12
int xOffsets[12] = { 5, 7, 7, 7, 5, 3,-1,-6,-6,-6,-1, 3};
int yOffsets[12] = {-8,-6,-6,-6, 7, 8, 7,-6,-6,-6,-8,-10};

//							  1	2	3	 4	5	6	7	 8
char clockValsAct[8] = {'/','_','\\','|','/','_','\\','|'};
//							1  2  3  4  5  6  7  8
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
	nullptr
};

unsigned char charHexValue(char ch)
{
	// return FF on invalid.
	ch = toupper(ch);
	if (isdigit(ch)) return ch-'0';
	if (strchr("ABCDEF", ch) != nullptr) return 0x0A + (ch-'A');
	return 0xFF;
}

bool parseHexColor(char *szHexColor, BYTE *colorHolder)
{
	if (szHexColor)
	{
		unsigned char parsed = charHexValue(szHexColor[0]);
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

// FIXME:  This is overly complicated for what it does and the logic doesn't look right.
char *pastNextCh(char *szStr, char ch)
{
	if (szStr == nullptr) return nullptr;

	// if didn't start on this character, move past others
	if (*szStr && *szStr != ch)
	{
		while (*szStr && *szStr != ch) szStr++;
	}

	// move forward until we get past the given character
	while (*szStr && *szStr == ch) szStr++;

	// return pointer only if not at terminator
	return (*szStr) ? szStr : nullptr;
}

bool OTD_parseRangeColor(char *szRangeColorInfo, struct tagRangeColor *prc)
{
	char *pszOffset;
	if (prc == nullptr) return false;

	memset(prc, 0, sizeof(*prc));

	if (parseColor(szRangeColorInfo, &prc->color) == false) return false;

	if ((pszOffset = pastNextCh(szRangeColorInfo, ' ')) == nullptr) return false;

	prc->range = atoi(pszOffset);

	if ((pszOffset = pastNextCh(pszOffset, ' ')) == nullptr) return false;

	prc->bColorMorph = (atoi(pszOffset) != 0);

	return true;
}

VOID dumpColors()
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

	for (i = 0; OTD_ppszDefaultRangeColors[i] != nullptr && i < MAX_RANGE_COLORS; i++)
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

	return (bLoadFail == false);
}

void OTD_loadIni(PSPAWNINFO pChar)
{
	char szBuf[256];
	char *szCharName = (pChar) ? pChar->Name : nullptr;

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

bool OTD_calcCurrentDegrees(FLOAT *pFloat)
{
	if (pFloat == nullptr) return false;

	FLOAT degrees = 0.0f;
	PSPAWNINFO psTarget = nullptr;

	if (pTarget)
	{
		psTarget = (PSPAWNINFO)pTarget;
	}

	if (psTarget == nullptr || pCharSpawn == nullptr) return false;

	// FIXME:  This looks like an overflow.
	FLOAT headingTo=(FLOAT)(atan2f(((PSPAWNINFO)pCharSpawn)->Y - psTarget->Y, psTarget->X - ((PSPAWNINFO)pCharSpawn)->X) * 180.0f / PI + 90.0f);
	FLOAT myHeading=((PSPAWNINFO)pCharSpawn)->Heading*0.703125f;

	degrees = myHeading - headingTo;

	while (degrees >= 360.0f) degrees -= 360.0f;
	while (degrees < 0.0f) degrees += 360.0f;

	 *pFloat = degrees;

	return true;
}

void OTD_setColorOld(FLOAT fDistance, ARGBCOLOR *color)
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

BYTE OTD_calcMorphColor(BYTE thisColor, BYTE lastColor, FLOAT pctChange, int i)
{
	//FLOAT f = ((FLOAT)lastColor)+(((FLOAT)(thisColor-lastColor))*pctChange);
	int change = thisColor-lastColor;
	float delta = (float)pctChange * change;
	FLOAT f = lastColor + delta;

	return (BYTE)f;
}

void OTD_morphRangeColor(int iIndex, ARGBCOLOR *color, int distance)
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

void OTD_DebugColor(ARGBCOLOR *color, bool bFound)
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
	DrawHUDText(szTemp, OTD_BasePosX-5, OTD_BasePosY+40, color->ARGB, DRAW_SIZE);
}

void OTD_setColor(FLOAT fDistance, ARGBCOLOR *color)
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
				color->R = OTD_RangeColorList[i].color.R;
				color->G = OTD_RangeColorList[i].color.G;
				color->B = OTD_RangeColorList[i].color.B;
			}
		}
	}

	if (bFound == false)
	{
		i = (OTD_iRangeColorsUsed - 1); // Default to last color
		if (i >= 0)
		{
			color->R = OTD_RangeColorList[i].color.R;
			color->G = OTD_RangeColorList[i].color.G;
			color->B = OTD_RangeColorList[i].color.B;
		}
	}
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

void OTD_render()
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
			DrawHUDText(szTemp, OTD_BasePosX+xLabelOffset-int((strlen(szTemp)-1)*2.5), OTD_BasePosY+yLabelOffset, OTD_LabelColor.ARGB, DRAW_SIZE);
		}

		int clockPos = OTD_directionPos(fDegrees);

		OTD_setColor(fDistance, &Color);

		DrawHUDText("O", OTD_BasePosX, OTD_BasePosY, Color.ARGB, DRAW_SIZE);

		if (clockPos < 1) clockPos = 1;
		if (clockPos > directions) clockPos = directions;

		clockPos--;

		sprintf_s(szTemp, "%c", pcVals[clockPos]);
		DrawHUDText(szTemp, OTD_BasePosX+piOffsetsX[clockPos], OTD_BasePosY+piOffsetsY[clockPos], Color.ARGB, DRAW_SIZE);

		//sprintf_s(szTemp, "%d - %d", clockPos+1, (int)fDegrees);
		//DrawHUDText(szTemp,OTD_BasePosX-5,OTD_BasePosY+30,Color.ARGB);

		//sprintf_s(szTemp, "Screen: %d:%d", GetWidth(), GetHeight());
		//DrawHUDText(szTemp,OTD_BasePosX-5,OTD_BasePosY+30,Color.ARGB);
	}
}

void OTD_userCommand(PSPAWNINFO pChar, PCHAR szLine)
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
	else if (_strcmpi(szOff, szArg) == 0)
	{
		OTD_bIsOn = false;
		WriteChatColor("OTD: off");
		WritePrivateProfileString(pChar->Name, "VISIBLE", "0", INIFileName);
	}
	else if (_strcmpi(szRange, szArg) == 0)
	{
		OTD_bRangeIsOn = (!OTD_bRangeIsOn);
		WritePrivateProfileString(pChar->Name, "RANGE_ON", (OTD_bRangeIsOn) ? "1" : "0", INIFileName);
		WriteChatColor((OTD_bRangeIsOn) ? "OTD - Range: on" : "OTD - Range: off");
	}
	else if (_strcmpi(szGetLoc, szArg) == 0)
	{
		sprintf_s(szArg, "OTD - Loc=x:%d, y:%d", OTD_BasePosX, OTD_BasePosY);
		WriteChatColor(szArg);
	}
	else if (_strcmpi(szDrawTight, szArg) == 0)
	{
		OTD_bDrawTight = (!OTD_bDrawTight);
		yLabelOffset = (OTD_bDrawTight) ? 10 : 20;
		WritePrivateProfileString("GLOBAL", "DRAW_TIGHT", (OTD_bDrawTight) ? "1" : "0", INIFileName);
		WriteChatColor((OTD_bDrawTight) ? "OTD - DrawTight: on" : "OTD - DrawTight: off");
	}
	else if (_strcmpi(szColorChange, szArg) == 0)
	{
		OTD_bColorChange = (!OTD_bColorChange);
		WritePrivateProfileString("GLOBAL", "COLOR_CHANGE", (OTD_bColorChange) ? "1" : "0", INIFileName);
		WriteChatColor((OTD_bColorChange) ? "OTD - ColorChange: on" : "OTD - ColorChange: off");
		WriteChatColor("OTD - This setting controls whether color is adjusted between defined colors. It does not turn off defined colors.");
	}
	else if (_strcmpi(szMoveTo, szArg) == 0 || _strcmpi(szMoveBy, szArg) == 0)
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
	else if (_strcmpi(szMoveTo, szArg) == 0)
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
	else if (_strcmpi(szReload, szArg) == 0)
	{
		OTD_loadIni(pChar);
		WriteChatColor("OTD - Ini reloaded");
	}
	else if (_strcmpi(szShowVersion, szArg) == 0)
	{
		WriteChatf("%s version %.2f", "MQ2OTD (Overhead Target Direction)", MQ2Version);
	}
	else if (_strcmpi(szDump, szArg) == 0)
	{
		dumpColors();
	}
	else
	{
		WriteChatf("%s version %.2f", "MQ2OTD (Overhead Target Direction)", MQ2Version);
		WriteChatColor("Usage: /otd <on|off|range|drawtight|getloc|reload|version|moveto x y|moveby x y> (Overhead Target Direction Compass)");
	}
}

// Called once, when the plugin is to initialize
PLUGIN_API void InitializePlugin()
{
	DebugSpewAlways("Initializing MQ2OTD");
	AddCommand("/otd", OTD_userCommand);

	OTD_loadDefaultColors();

	OTD_loadIni((PSPAWNINFO)pCharSpawn);
	bChangedCharacter = false;
}

// Called once, when the plugin is to shutdown
PLUGIN_API void ShutdownPlugin()
{
	DebugSpewAlways("Shutting down MQ2OTD");
	RemoveCommand("/otd");
}

// Called every frame that the "HUD" is drawn -- e.g. net status / packet loss bar
PLUGIN_API void OnDrawHUD()
{
	if (bChangedCharacter && gGameState==GAMESTATE_INGAME && pCharSpawn)
	{
		OTD_loadIni((PSPAWNINFO)pCharSpawn);
		bChangedCharacter = false;
	}

	if (gGameState==GAMESTATE_INGAME &&
			pTarget && pCharSpawn &&
			OTD_bIsOn && pTarget->Name != pCharSpawn->Name &&
			bChangedCharacter == false)
	{
		OTD_render();
	}
}

PLUGIN_API void SetGameState(int GameState)
{
	if (GameState == GAMESTATE_CHARSELECT) {
		bChangedCharacter = true;
	}
}
#include "v_font.h"
#include "lz.h"

font_t menuFont;
font_t titleFont;
font_t creditFont;
font_t titleNumberFont;
font_t hudNumberFont;

void V_FontInit()
{
    menuFont.lumpStart = W_GetNumForName("STCFN022");
    menuFont.lumpStartChar = 22;
    menuFont.minChar = 22;
    menuFont.maxChar = 126;
    menuFont.fixedWidth = true;
    menuFont.fixedWidthSize = 8;
    menuFont.spaceWidthSize = 4;
    menuFont.verticalOffset = 8;
    menuFont.charCacheLength = 0;
    menuFont.charCache = NULL;

    titleFont.lumpStart = W_GetNumForName("LTFNT065");
    titleFont.lumpStartChar = 65;
    titleFont.minChar = 65;
    titleFont.maxChar = 122;
    titleFont.fixedWidth = false;
    titleFont.fixedWidth = 0;
    titleFont.spaceWidthSize = 16;
    titleFont.verticalOffset = 16;
    titleFont.charCacheLength = 0;
    titleFont.charCache = NULL;

    creditFont.lumpStart = W_GetNumForName("CRFNT065");
    creditFont.lumpStartChar = 65;
    creditFont.minChar = 46;
    creditFont.maxChar = 90;
    creditFont.fixedWidth = false;
    creditFont.fixedWidthSize = 16;
    creditFont.spaceWidthSize = 8;
    creditFont.verticalOffset = 16;
    creditFont.charCacheLength = 0;
    creditFont.charCache = NULL;

    titleNumberFont.lumpStart = W_GetNumForName("TTL01");
    titleNumberFont.lumpStartChar = '1';
    titleNumberFont.minChar = '1';
    titleNumberFont.maxChar = '3';
    titleNumberFont.fixedWidth = false;
    titleNumberFont.fixedWidthSize = 0;
    titleNumberFont.verticalOffset = 29;
    titleNumberFont.charCacheLength = 0;
    titleNumberFont.charCache = NULL;

    hudNumberFont.lumpStart = W_GetNumForName("STTNUM0");
    hudNumberFont.lumpStartChar = '0';
    hudNumberFont.minChar = '0';
    hudNumberFont.maxChar = '9';
    hudNumberFont.fixedWidth = true;
    hudNumberFont.fixedWidthSize = 8;
    hudNumberFont.spaceWidthSize = 4;
    hudNumberFont.spaceWidthSize = 11;
    hudNumberFont.charCacheLength = 0;
    hudNumberFont.charCache = NULL;
}

int V_GetStringWidth(const font_t *font, const char *string)
{
    int width = 0;
    int i,c;
    byte *lump;
    jagobj_t *jo;

    if (font->fixedWidth) {
        for (i = mystrlen(string)-1; i >= 0; i--)
	    {
            c = string[i];
            if (c == 0x20) // Space
                width += font->spaceWidthSize;
            else if (c >= font->minChar && c <= font->maxChar)
		    {
                width += font->fixedWidthSize;
            }
        }
    }
    else {
        for (i = mystrlen(string)-1; i >= 0; i--)
	    {
            c = string[i];
            if (c == 0x20) // Space
                width += font->spaceWidthSize;
            else if (c >= font->minChar && c <= font->maxChar)
		    {
                int lumpnum = font->lumpStart + (c - font->lumpStartChar);
                lump = W_POINTLUMPNUM(lumpnum);
	            if (!(lumpinfo[lumpnum].name[0] & 0x80))
	            {
    		        jo = (jagobj_t*)lump;
		            width += jo->width;
	            }
            }
        }
    }

    return width;
}

int V_DrawStringLeftWithColormap(const font_t *font, int x, int y, const char *string, int colormap)
{
	int i,c;
    int startX = x;

	for (i = 0; i < mystrlen(string); i++)
	{
		c = string[i];
	
        if (c == '\n') // Basic newline support
        {
            x = startX;
            y += font->verticalOffset;
        }
        else if (c == 0x20) // Space
            x += font->spaceWidthSize;
		else if (c >= font->minChar && c <= font->maxChar)
		{
			if (font->fixedWidth)
            {
                int charnum = (c - font->lumpStartChar);
                if (font->charCache != NULL && font->charCacheLength > charnum && font->charCache[charnum] != NULL) {
                    if (colormap) {
                        DrawJagobjWithColormap(font->charCache[charnum], x, y, NULL, NULL, NULL, NULL, I_OverwriteBuffer(), colormap);
                    }
                    else {
                        DrawJagobj(font->charCache[charnum], x, y);
                    }
                }
                else {
                    if (colormap) {
    			        DrawJagobjLumpWithColormap(font->lumpStart + charnum, x, y, NULL, NULL, colormap);
                    }
                    else {
                        DrawJagobjLump(font->lumpStart + charnum, x, y, NULL, NULL);
                    }
                }
                
			    x += font->fixedWidthSize;
            }
            else
            {
                int charnum = (c - font->lumpStartChar);
                if (font->charCache != NULL && font->charCacheLength > charnum && font->charCache[charnum] != NULL) {
                    if (colormap) {
                        DrawJagobjWithColormap(font->charCache[charnum], x, y, NULL, NULL, NULL, NULL, I_OverwriteBuffer(), colormap);
                    }
                    else {
                        DrawJagobj(font->charCache[charnum], x, y);
                    }

                    x += font->charCache[charnum]->width;
                }
                else {
                    int lumpnum = font->lumpStart + charnum;
                    byte *lump = W_POINTLUMPNUM(lumpnum);
                    jagobj_t *jo;

                    if (!(lumpinfo[lumpnum].name[0] & 0x80))
                    {
                        jo = (jagobj_t*)lump;
                        if (colormap)
                            DrawJagobjWithColormap(jo, x, y + font->verticalOffset - jo->height, 0, 0, 0, 0, I_OverwriteBuffer(), colormap);
                        else
                            DrawJagobj(jo, x, y + font->verticalOffset - jo->height);
                    }
                    else
                    {
                        // Can draw compressed characters
                        LZSTATE gfx_lz;
                        uint8_t lz_buf[32];
                        LzSetup(&gfx_lz, lump, lz_buf, 32);
                        if (LzReadPartial(&gfx_lz, 16) != 16)
                            continue;

                        jo = (jagobj_t*)gfx_lz.output;
                        if (colormap)
                            DrawJagobjLumpWithColormap(lumpnum, x, y + font->verticalOffset - jo->height, NULL, NULL, colormap);
                        else
                            DrawJagobjLump(lumpnum, x, y + font->verticalOffset - jo->height, NULL, NULL);
                    }

                    x += jo->width;
                }
            }
		}
	}

    return x;
}

int V_DrawStringRightWithColormap(const font_t *font, int x, int y, const char *string, int colormap)
{
    int i,c;
    byte *lump;
    jagobj_t *jo;

	for (i = mystrlen(string)-1; i >= 0; i--)
	{
		c = string[i];
	
        if (c == 0x20) // Space
            x -= font->spaceWidthSize;
		else if (c >= font->minChar && c <= font->maxChar)
		{
            if (font->fixedWidth)
            {
                int charnum = (c - font->lumpStartChar);
                if (font->charCache != NULL && font->charCacheLength > charnum && font->charCache[charnum] != NULL) {
                    if (colormap) {
                        DrawJagobjWithColormap(font->charCache[charnum], x, y, NULL, NULL, NULL, NULL, I_OverwriteBuffer(), colormap);
                    }
                    else {
                        DrawJagobj(font->charCache[charnum], x, y);
                    }
                }
                else {
                    if (colormap)
                        DrawJagobjLumpWithColormap(font->lumpStart + (c - font->lumpStartChar), x, y, NULL, NULL, colormap);
                    else
    			        DrawJagobjLump(font->lumpStart + (c - font->lumpStartChar), x, y, NULL, NULL);
                }
                
                x -= font->fixedWidthSize;
            }
            else
            {
                int charnum = (c - font->lumpStartChar);
                if (font->charCache != NULL && font->charCacheLength > charnum && font->charCache[charnum] != NULL) {
                    if (colormap) {
                        DrawJagobjWithColormap(font->charCache[charnum], x, y, NULL, NULL, NULL, NULL, I_OverwriteBuffer(), colormap);
                    }
                    else {
                        DrawJagobj(font->charCache[charnum], x, y);
                    }

                    x += font->charCache[charnum]->width;
                }
                else {
                    int lumpnum = font->lumpStart + (c - font->lumpStartChar);
                    lump = W_POINTLUMPNUM(lumpnum);
                    if (!(lumpinfo[lumpnum].name[0] & 0x80))
                    {
                        jo = (jagobj_t*)lump;
                        x -= jo->width;

                        if (colormap)
                            DrawJagobjWithColormap(jo, x, y + font->verticalOffset - jo->height, 0, 0, 0, 0, I_OverwriteBuffer(), colormap);
                        else
                            DrawJagobj(jo, x, y + font->verticalOffset - jo->height);
                    }
                }
            }
		}
	}

    return x;
}

int V_DrawStringLeft(const font_t *font, int x, int y, const char *string)
{
	return V_DrawStringLeftWithColormap(font, x, y, string, 0);
}

int V_DrawStringRight(const font_t *font, int x, int y, const char *string)
{
    return V_DrawStringRightWithColormap(font, x, y, string, 0);
}

int V_DrawStringCenterWithColormap(const font_t *font, int x, int y, const char *string, int colormap)
{
    int c;

    // Slow, difficult...
    for (int i = 0; i < mystrlen(string); i++)
    {
        c = string[i];
        
        if (c == 0x20) // Space
            x -= font->spaceWidthSize / 2;
		else if (c >= font->minChar && c <= font->maxChar)
		{
            if (font->fixedWidth)
            {
                x -= font->fixedWidthSize / 2;
            }
            else
            {
                int charnum = (c - font->lumpStartChar);
                if (font->charCache != NULL && font->charCacheLength > charnum && font->charCache[charnum] != NULL) {
                    x -= font->charCache[charnum]->width / 2;
                }
                else {
                    int lumpnum = font->lumpStart + charnum;
                    byte *lump = W_POINTLUMPNUM(lumpnum);
                    jagobj_t *jo;

                    if (!(lumpinfo[lumpnum].name[0] & 0x80))
                    {
                        jo = (jagobj_t*)lump;
                        x -= jo->width / 2;
                    }
                    else
                    {
                        // Can draw compressed characters
                        LZSTATE gfx_lz;
                        uint8_t lz_buf[32];
                        LzSetup(&gfx_lz, lump, lz_buf, 32);
                        if (LzReadPartial(&gfx_lz, 16) != 16)
                            continue;

                        jo = (jagobj_t*)gfx_lz.output;
                        x -= jo->width / 2;
                    }
                }
            }
		}
    }

    return V_DrawStringLeftWithColormap(font, x, y, string, colormap);
}

int V_DrawStringCenter(const font_t *font, int x, int y, const char *string)
{
    return V_DrawStringCenterWithColormap(font, x, y, string, 0);
}

void V_DrawValueLeft(const font_t *font, int x, int y, int value)
{
	char	v[12];

	valtostr(v,value);

    V_DrawStringLeft(font, x, y, v);
}

void V_DrawValueRight(const font_t *font, int x, int y, int value)
{
	char	v[12];

	valtostr(v,value);

    V_DrawStringRight(font, x, y, v);
}

void V_DrawValueCenter(const font_t *font, int x, int y, int value)
{
	char	v[12];

	valtostr(v,value);

    V_DrawStringCenter(font, x, y, v);
}

// Font MUST be fixedWidth = true
void V_DrawValuePaddedRight(const font_t *font, int x, int y, int value, int pad)
{
	char	v[12];
	int		i;

	valtostr(v,value);

	for (i = mystrlen(v)-1; i >= 0; i--)
	{
		int c = v[i];

        x -= font->fixedWidthSize;
	
        if (c == 0x20) // Space
            x -= font->spaceWidthSize;
		else if (c >= font->minChar && c <= font->maxChar) {
            int charnum = (c - font->lumpStartChar);
            if (font->charCache != NULL && font->charCacheLength > charnum && font->charCache[charnum] != NULL) {
                DrawJagobj(font->charCache[charnum], x, y);
            }
            else {
			    DrawJagobjLump(font->lumpStart + (c - font->lumpStartChar), x, y, NULL, NULL);
            }
        }

        pad--;
	}

	while (pad > 0)
	{
		x -= font->fixedWidthSize;
        int charnum = ('0' - font->lumpStartChar);
        if (font->charCache != NULL && font->charCacheLength > charnum && font->charCache[charnum] != NULL) {
            DrawJagobj(font->charCache[charnum], x, y);
        }
        else {
		    DrawJagobjLump(font->lumpStart + charnum, x, y, NULL, NULL);
        }
		pad--;
	}
}

/*================================================= */
/* */
/*	Convert an int to a string (my_itoa?) */
/* */
/*================================================= */
void valtostr(char *string, int val)
{
	char temp[12];
	int	index = 0, i, dindex = 0;
	
	do
	{
		temp[index++] = val%10 + '0';
		val /= 10;
	} while(val);
	
	string[index] = 0;
	for (i = index - 1; i >= 0; i--)
		string[dindex++] = temp[i];
}
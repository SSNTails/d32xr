#include "v_font.h"
#include "lz.h"
#include "r_local.h"

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

int V_DrawChar(const font_t *font, int x, int y, char c)
{
    if (c == 0x20) // Space
        x += font->spaceWidthSize;
    else if (c >= font->minChar && c <= font->maxChar)
    {
        if (font->fixedWidth)
        {
            int charnum = (c - font->lumpStartChar);
            if (font->charCache != NULL && font->charCacheLength > charnum && font->charCache[charnum] != NULL) {
                DrawJagobj(font->charCache[charnum], x, y);
            }
            else {
                DrawJagobjLump(font->lumpStart + charnum, x, y, NULL, NULL);
            }
            
            x += font->fixedWidthSize;
        }
        else
        {
            int charnum = (c - font->lumpStartChar);
            if (font->charCache != NULL && font->charCacheLength > charnum && font->charCache[charnum] != NULL) {
                DrawJagobj(font->charCache[charnum], x, y);

                x += font->charCache[charnum]->width;
            }
            else {
                int lumpnum = font->lumpStart + charnum;
                byte *lump = W_POINTLUMPNUM(lumpnum);
                jagobj_t *jo;

                if (!(lumpinfo[lumpnum].name[0] & 0x80))
                {
                    jo = (jagobj_t*)lump;
                    DrawJagobj(jo, x, y + font->verticalOffset - jo->height);
                }
                else
                {
                    // Can draw compressed characters
                    LZSTATE gfx_lz;
                    uint8_t lz_buf[32];
                    LzSetup(&gfx_lz, lump, lz_buf, 32);
                    if (LzReadPartial(&gfx_lz, 16) == 16)
                    {
                        jo = (jagobj_t*)gfx_lz.output;
                        DrawJagobjLump(lumpnum, x, y + font->verticalOffset - jo->height, NULL, NULL);
                    }
                }

                x += jo->width;
            }
        }
    }

    return x;
}

static uint16_t V_PaletteColor15bpp(uint8_t paletteIndex)
{
    if (paletteIndex == 0)
        return 0x0000;

    const uint8_t *color = dc_playpals + paletteIndex * 3;

    return 0x8000 | ((color[2] >> 3) << 10) | ((color[1] >> 3) << 5) | (color[0] >> 3);
}

static void V_DrawJagobj15bpp(jagobj_t *jo, int x, int y)
{
    int width = BIGSHORT(jo->width);
    int height = BIGSHORT(jo->height);
    int depth = BIGSHORT(jo->depth);
    int flags = BIGSHORT(jo->flags);
    int index = BIGSHORT(jo->index);
    int sourceX = 0;
    int sourceY = 0;
    int rowStride = width;
    uint16_t *framebuffer = (uint16_t *)I_FrameBuffer();

    if (width < 1 || height < 1)
        return;

    if (x < 0) {
        sourceX = -x;
        width += x;
        x = 0;
    }
    if (y < 0) {
        sourceY = -y;
        height += y;
        y = 0;
    }
    if (x + width > 320)
        width = 320 - x;
    if (y + height > 204)
        height = 204 - y;
    if (width < 1 || height < 1)
        return;

    if (depth == 2) {
        rowStride >>= 1;
        index = (index << 1) + ((flags & 2) ? 1 : 0);
    }

    for (int row = 0; row < height; row++) {
        const uint8_t *source = jo->data + (sourceY + row) * rowStride;
        uint16_t *dest = framebuffer + (y + row) * 320 + x;

        for (int column = 0; column < width; column++) {
            int sourceColumn = sourceX + column;
            uint8_t paletteIndex;

            if (depth == 2) {
                uint8_t packed = source[sourceColumn >> 1];
                uint8_t colorIndex = (sourceColumn & 1) ? (packed & 0x0F) : (packed >> 4);
                paletteIndex = index + colorIndex * 2;
            }
            else {
                paletteIndex = source[sourceColumn];
            }

            if (paletteIndex != COLOR_THRU)
                dest[column] = V_PaletteColor15bpp(paletteIndex);
        }
    }
}

static void V_DrawCompressedJagobj15bpp(int lumpnum, int x, int y, int width, int height)
{
    LZSTATE gfx_lz;
    uint8_t lz_buf[LZ_BUF_SIZE];
    uint8_t *lump = W_POINTLUMPNUM(lumpnum);
    uint16_t *framebuffer = (uint16_t *)I_FrameBuffer();
    uint8_t *source;
    int sourceX = 0;
    int sourceY = 0;
    int drawWidth = width;
    int drawHeight = height;
    int ringPos = 16;

    LzSetup(&gfx_lz, lump, lz_buf, LZ_BUF_SIZE);
    if (LzReadPartial(&gfx_lz, 16) != 16)
        return;

    source = gfx_lz.output;
    if (x < 0) {
        sourceX = -x;
        drawWidth += x;
        x = 0;
    }
    if (y < 0) {
        sourceY = -y;
        drawHeight += y;
        y = 0;
    }
    if (x + drawWidth > 320)
        drawWidth = 320 - x;
    if (y + drawHeight > 204)
        drawHeight = 204 - y;

    for (int row = 0; row < height; row++) {
        if (LzReadPartial(&gfx_lz, width) != width)
            return;

        if (row >= sourceY && row < sourceY + drawHeight && drawWidth > 0) {
            uint16_t *dest = framebuffer + (y + row - sourceY) * 320 + x;

            for (int column = 0; column < drawWidth; column++) {
                uint8_t paletteIndex = source[(ringPos + sourceX + column) & (LZ_BUF_SIZE - 1)];
                if (paletteIndex != COLOR_THRU)
                    dest[column] = V_PaletteColor15bpp(paletteIndex);
            }
        }

        ringPos = (ringPos + width) & (LZ_BUF_SIZE - 1);
    }
}

int V_DrawChar_15bpp(const font_t *font, int x, int y, char c)
{
    if (c == 0x20) {
        x += font->spaceWidthSize;
    }
    else if (c >= font->minChar && c <= font->maxChar) {
        int charnum = c - font->lumpStartChar;
        jagobj_t *jo = NULL;
        int glyphWidth = 0;
        int glyphHeight = 0;
        int cached = font->charCache != NULL && charnum >= 0 &&
            font->charCacheLength > charnum && font->charCache[charnum] != NULL;

        if (cached) {
            jo = font->charCache[charnum];
            glyphWidth = BIGSHORT(jo->width);
            glyphHeight = BIGSHORT(jo->height);
            V_DrawJagobj15bpp(jo, x, y);
        }
        else {
            int lumpnum = font->lumpStart + charnum;
            uint8_t *lump = W_POINTLUMPNUM(lumpnum);

            if (lumpinfo[lumpnum].name[0] & 0x80) {
                LZSTATE gfx_lz;
                uint8_t lz_buf[32];
                LzSetup(&gfx_lz, lump, lz_buf, sizeof(lz_buf));
                if (LzReadPartial(&gfx_lz, 16) != 16)
                    return x;
                jo = (jagobj_t *)gfx_lz.output;
                glyphWidth = BIGSHORT(jo->width);
                glyphHeight = BIGSHORT(jo->height);
                V_DrawCompressedJagobj15bpp(lumpnum, x,
                    font->fixedWidth ? y : y + font->verticalOffset - glyphHeight,
                    glyphWidth, glyphHeight);
            }
            else {
                jo = (jagobj_t *)lump;
                glyphWidth = BIGSHORT(jo->width);
                glyphHeight = BIGSHORT(jo->height);
                V_DrawJagobj15bpp(jo, x,
                    font->fixedWidth ? y : y + font->verticalOffset - glyphHeight);
            }
        }

        if (font->fixedWidth)
            x += font->fixedWidthSize;
        else
            x += glyphWidth;
    }

    return x;
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
                        DrawJagobjWithColormap(font->charCache[charnum], x, y, 0, 0, 0, 0, I_OverwriteBuffer(), colormap);
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
                        DrawJagobjWithColormap(font->charCache[charnum], x, y, 0, 0, 0, 0, I_OverwriteBuffer(), colormap);
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
                        DrawJagobjWithColormap(font->charCache[charnum], x, y, 0, 0, 0, 0, I_OverwriteBuffer(), colormap);
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
                        DrawJagobjWithColormap(font->charCache[charnum], x, y, 0, 0, 0, 0, I_OverwriteBuffer(), colormap);
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
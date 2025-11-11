#include "v_font.h"
#include "lzexe.h"

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

    titleFont.lumpStart = W_GetNumForName("LTFNT065");
    titleFont.lumpStartChar = 65;
    titleFont.minChar = 65;
    titleFont.maxChar = 122;
    titleFont.fixedWidth = false;
    titleFont.fixedWidth = 0;
    titleFont.spaceWidthSize = 16;
    titleFont.verticalOffset = 20;

    creditFont.lumpStart = W_GetNumForName("CRFNT065");
    creditFont.lumpStartChar = 65;
    creditFont.minChar = 46;
    creditFont.maxChar = 90;
    creditFont.fixedWidth = false;
    creditFont.fixedWidthSize = 16;
    creditFont.spaceWidthSize = 8;
    creditFont.verticalOffset = 16;

    titleNumberFont.lumpStart = W_GetNumForName("TTL01");
    titleNumberFont.lumpStartChar = '1';
    titleNumberFont.minChar = '1';
    titleNumberFont.maxChar = '3';
    titleNumberFont.fixedWidth = false;
    titleNumberFont.fixedWidthSize = 0;
    titleNumberFont.verticalOffset = 29;

    hudNumberFont.lumpStart = W_GetNumForName("STTNUM0");
    hudNumberFont.lumpStartChar = '0';
    hudNumberFont.minChar = '0';
    hudNumberFont.maxChar = '9';
    hudNumberFont.fixedWidth = true;
    hudNumberFont.fixedWidthSize = 8;
    hudNumberFont.spaceWidthSize = 4;
    hudNumberFont.spaceWidthSize = 11;
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

jagobj_t *V_CacheStringWithColormap(const font_t *font, const char *string, int colormap)
{
    //Note: Compressed fonts are *NOT* currently supported!

    int x = 0;
    int y = 0;
    int i,c;

    // Create buffer
    int width = V_GetStringWidth(font, string);
    int height = font->verticalOffset;

    for (i = 0; i < mystrlen(string); i++) {
        if (string[i] == '\n') {
            height += font->verticalOffset;
        }
    }

    jagobj_t *jo = NULL;
    int size = sizeof(jagobj_t) + (width * height);
    jo = Z_Calloc(size+1, PU_STATIC);
    jo->width = width;
    jo->height = height;
    jo->depth = 8;
    jo->flags = 0;
    jo->index = 0;
    ((char *)jo)[size] = '\0';

    jagobj_t *character[128];
    for (i=0; i < 128; i++) {
        character[i] = NULL;
    }

    // Draw the string to the buffer.
    for (i = 0; i < mystrlen(string); i++)
	{
		c = string[i];

        if (character[c] == NULL) {
            // Cache any characters that are used in the string.
            character[c] = W_CacheLumpNum(font->lumpStart + (c - font->lumpStartChar), PU_STATIC);
        }
	
        if (c == '\n') // Basic newline support
        {
            x = 0;
            y += font->verticalOffset;
        }
        else if (c == 0x20) // Space
        {
            x += font->spaceWidthSize;
        }
		else if (c >= font->minChar && c <= font->maxChar)
		{
			if (font->fixedWidth)
            {
                if (colormap) {
                    DrawJagobjWithColormap(character[c], x, y, 0, 0, character[c]->width, character[c]->height, jo->width, jo->data, colormap);
    			}
                else {
                    DrawJagobj3(character[c], x, y, 0, 0, character[c]->width, character[c]->height, jo->width, jo->data);
                }
                
			    x += font->fixedWidthSize;
            }
            else
            {
                if (colormap)
                    DrawJagobjWithColormap(character[c], x, y, 0, 0, 0, 0, 320, I_OverwriteBuffer(), colormap);
                else
                    DrawJagobj(character[c], x, y);

                x += jo->width;
	        }
		}
	}

    // Free temporary cached characters.
    for (i = mystrlen(string)-1; i >= 0; i--)
	{
		c = string[i];
        if (character[c] != NULL) {
            Z_Free(character[c]);
            character[c] = NULL;
        }
    }

    return jo;
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
                if (colormap)
    			    DrawJagobjLumpWithColormap(font->lumpStart + (c - font->lumpStartChar), x, y, NULL, NULL, colormap);
                else
                    DrawJagobjLump(font->lumpStart + (c - font->lumpStartChar), x, y, NULL, NULL);
                
			    x += font->fixedWidthSize;
            }
            else
            {
                int lumpnum = font->lumpStart + (c - font->lumpStartChar);
                byte *lump = W_POINTLUMPNUM(lumpnum);
                jagobj_t *jo;

	            if (!(lumpinfo[lumpnum].name[0] & 0x80))
	            {
    		        jo = (jagobj_t*)lump;
                    if (colormap)
                        DrawJagobjWithColormap(jo, x, y + font->verticalOffset - jo->height, 0, 0, 0, 0, 320, I_OverwriteBuffer(), colormap);
                    else
                        DrawJagobj(jo, x, y + font->verticalOffset - jo->height);
	            }
                else
                {
                    // Can draw compressed characters
                    lzexe_state_t gfx_lzexe;
	                uint8_t lzexe_buf[32];
                    lzexe_setup(&gfx_lzexe, lump, lzexe_buf, 32);
                    if (lzexe_read_partial(&gfx_lzexe, 16) != 16)
                        continue;

                    jo = (jagobj_t*)gfx_lzexe.output;
                    if (colormap)
                        DrawJagobjLumpWithColormap(lumpnum, x, y + font->verticalOffset - jo->height, NULL, NULL, colormap);
                    else
                        DrawJagobjLump(lumpnum, x, y + font->verticalOffset - jo->height, NULL, NULL);
                }

                x += jo->width;
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
                if (colormap)
                    DrawJagobjLumpWithColormap(font->lumpStart + (c - font->lumpStartChar), x, y, NULL, NULL, colormap);
                else
    			    DrawJagobjLump(font->lumpStart + (c - font->lumpStartChar), x, y, NULL, NULL);
                
                x -= font->fixedWidthSize;
            }
            else
            {
                int lumpnum = font->lumpStart + (c - font->lumpStartChar);
                lump = W_POINTLUMPNUM(lumpnum);
	            if (!(lumpinfo[lumpnum].name[0] & 0x80))
	            {
    		        jo = (jagobj_t*)lump;
		            x -= jo->width;

                    if (colormap)
                        DrawJagobjWithColormap(jo, x, y + font->verticalOffset - jo->height, 0, 0, 0, 0, 320, I_OverwriteBuffer(), colormap);
                    else
                        DrawJagobj(jo, x, y + font->verticalOffset - jo->height);
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
                int lumpnum = font->lumpStart + (c - font->lumpStartChar);
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
                    lzexe_state_t gfx_lzexe;
	                uint8_t lzexe_buf[32];
                    lzexe_setup(&gfx_lzexe, lump, lzexe_buf, 32);
                    if (lzexe_read_partial(&gfx_lzexe, 16) != 16)
                        continue;

                    jo = (jagobj_t*)gfx_lzexe.output;
		            x -= jo->width / 2;
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
		else if (c >= font->minChar && c <= font->maxChar)
			DrawJagobjLump(font->lumpStart + (c - font->lumpStartChar), x, y, NULL, NULL);

        pad--;
	}

	while (pad > 0)
	{
		x -= font->fixedWidthSize;
		DrawJagobjLump(font->lumpStart + ('0' - font->lumpStartChar), x, y, NULL, NULL);
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
/*
* << Haru Free PDF Library >> -- hpdf_fontdef_base14.c
*
* URL: http://libharu.org
*
* Copyright (c) 1999-2006 Takeshi Kanno <takeshi_kanno@est.hi-ho.ne.jp>
* Copyright (c) 2007-2009 Antony Dovgal <tony@daylessday.org>
* Copyright (c) 2015 Patrick Schlangen <patrick@schlangen.me>
*
* Permission to use, copy, modify, distribute and sell this software
* and its documentation for any purpose is hereby granted without fee,
* provided that the above copyright notice appear in all copies and
* that both that copyright notice and this permission notice appear
* in supporting documentation.
* It is provided "as is" without express or implied warranty.
*
*/

#include "hpdf_conf.h"
#include "hpdf_utils.h"
#include "hpdf_fontdef.h"

static void
FreeFunc(HPDF_FontDef  fontdef);

HPDF_FontDef
HPDF_Type3RasterFontDef_New  (HPDF_MMgr        mmgr,
                              const char  *font_name,
							  HPDF_REAL    pt_size,
							  HPDF_UINT16  dpi_x,
							  HPDF_UINT16  dpi_y)
{
	HPDF_FontDef fontdef;
	HPDF_Type3RasterFontDefAttr fontdef_attr;

	HPDF_PTRACE((" HPDF_Type3RasterFontDef_New\n"));

	if (!mmgr)
		return NULL;

	fontdef = HPDF_GetMem(mmgr, sizeof(HPDF_FontDef_Rec));
	if (!fontdef)
		return NULL;

	HPDF_MemSet(fontdef, 0, sizeof(HPDF_FontDef_Rec));
	fontdef->sig_bytes = HPDF_FONTDEF_SIG_BYTES;
	HPDF_StrCpy(fontdef->base_font, font_name,
		fontdef->base_font + HPDF_LIMIT_MAX_NAME_LEN);
	fontdef->mmgr = mmgr;
	fontdef->error = mmgr->error;
	fontdef->type = HPDF_FONTDEF_TYPE_TYPE3RASTER;
	fontdef->free_fn = FreeFunc;

	fontdef_attr = HPDF_GetMem(mmgr, sizeof(HPDF_Type3RasterFontDefAttr_Rec));
	if (!fontdef_attr) {
		HPDF_FreeMem(fontdef->mmgr, fontdef);
		return NULL;
	}

	fontdef->attr = fontdef_attr;
	HPDF_MemSet((HPDF_BYTE *)fontdef_attr, 0, sizeof(HPDF_Type3RasterFontDefAttr_Rec));

	fontdef_attr->dpi_x = dpi_x; 
	fontdef_attr->dpi_y = dpi_y;
	fontdef_attr->pt_size = pt_size;
	fontdef_attr->chars_count = 256;
	HPDF_RasterCharData *cdata = (HPDF_RasterCharData*)HPDF_GetMem(mmgr,
		sizeof(HPDF_RasterCharData) * fontdef_attr->chars_count);
	HPDF_MemSet(cdata, 0, sizeof(HPDF_RasterCharData) * fontdef_attr->chars_count);

	fontdef_attr->chars = cdata;

	fontdef->flags = HPDF_FONT_STD_CHARSET; // TODO: ?

	return fontdef;
}

HPDF_STATUS
HPDF_Type3RasterFontDef_AddChar(HPDF_FontDef fontdef,
	const HPDF_RasterCharData *data)
{
	HPDF_Type3RasterFontDefAttr attr = (HPDF_Type3RasterFontDefAttr)fontdef->attr;

	if (data->codepoint < 0 || data->codepoint > 255)
		return HPDF_INVALID_PARAMETER;

	HPDF_MemCpy((HPDF_BYTE *)&attr->chars[data->codepoint], (const HPDF_BYTE *)data, sizeof(HPDF_RasterCharData));

	attr->chars[data->codepoint].raster_data = HPDF_GetMem(fontdef->mmgr, data->raster_data_size);

	HPDF_MemCpy(attr->chars[data->codepoint].raster_data,
		data->raster_data,
		data->raster_data_size);

	return HPDF_OK;
}

HPDF_INT16
HPDF_Type3RasterFontDef_GetWidth(HPDF_FontDef  fontdef,
	HPDF_UNICODE codepoint)
{
	HPDF_Type3RasterFontDefAttr attr = (HPDF_Type3RasterFontDefAttr)fontdef->attr;
	HPDF_RasterCharData *cdata = attr->chars;
	HPDF_UINT i;

	HPDF_PTRACE((" HPDF_Type3RasterFontDef_GetWidth\n"));

	for (i = 0; i < attr->chars_count; i++) {
		if (cdata->codepoint == codepoint)
			return cdata->width;
		cdata++;
	}

	return fontdef->missing_width;
}

static void
FreeFunc(HPDF_FontDef  fontdef)
{
	HPDF_Type3RasterFontDefAttr attr = (HPDF_Type3RasterFontDefAttr)fontdef->attr;
	HPDF_RasterCharData *cdata = attr->chars;
	HPDF_UINT i;

	HPDF_PTRACE((" FreeFunc\n"));

	for (i = 0; i < attr->chars_count; i++) {
		if (cdata->raster_data != NULL)
			HPDF_FreeMem(fontdef->mmgr, cdata->raster_data);
		cdata++;
	}

	HPDF_FreeMem(fontdef->mmgr, attr->chars);
	HPDF_FreeMem(fontdef->mmgr, attr);
}


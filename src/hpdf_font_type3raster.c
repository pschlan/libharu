/*
* << Haru Free PDF Library >> -- hpdf_font_type1.c
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
#include "hpdf_font.h"

static HPDF_STATUS
Type3RasterFont_OnWrite(HPDF_Dict    obj,
	HPDF_Stream  stream);

static void
Type3RasterFont_OnFree(HPDF_Dict  obj);

static HPDF_TextWidth
Type3RasterFont_TextWidth(HPDF_Font        font,
	const HPDF_BYTE  *text,
	HPDF_UINT        len);


static HPDF_UINT
Type3RasterFont_MeasureText(HPDF_Font          font,
	const HPDF_BYTE   *text,
	HPDF_UINT          len,
	HPDF_REAL          width,
	HPDF_REAL          font_size,
	HPDF_REAL          char_space,
	HPDF_REAL          word_space,
	HPDF_BOOL          wordwrap,
	HPDF_REAL         *real_width);

/*
static HPDF_STATUS
Type1Font_CreateDescriptor(HPDF_MMgr  mmgr,
HPDF_Font  font,
HPDF_Xref  xref);
*/

HPDF_Font
HPDF_Type3RasterFont_New(HPDF_MMgr        mmgr,
	HPDF_FontDef     fontdef,
	HPDF_Encoder     encoder,
	HPDF_Xref        xref)
{
	HPDF_Dict font;
	HPDF_FontAttr attr;
	HPDF_Type3RasterFontDefAttr fontdef_attr;
	HPDF_BasicEncoderAttr encoder_attr;
	HPDF_STATUS ret = 0;
	HPDF_UINT i;

	HPDF_PTRACE((" HPDF_Type3RasterFont_New\n"));

	/* check whether the fontdef object and the encoder object is valid. */
	if (encoder->type != HPDF_ENCODER_TYPE_SINGLE_BYTE) {
		HPDF_SetError(mmgr->error, HPDF_INVALID_ENCODER_TYPE, 0);
		return NULL;
	}

	if (fontdef->type != HPDF_FONTDEF_TYPE_TYPE3RASTER) {
		HPDF_SetError(mmgr->error, HPDF_INVALID_FONTDEF_TYPE, 0);
		return NULL;
	}

	font = HPDF_Dict_New(mmgr);
	if (!font)
		return NULL;

	font->header.obj_class |= HPDF_OSUBCLASS_FONT;

	attr = HPDF_GetMem(mmgr, sizeof(HPDF_FontAttr_Rec));
	if (!attr) {
		HPDF_Dict_Free(font);
		return NULL;
	}

	font->header.obj_class |= HPDF_OSUBCLASS_FONT;
	font->write_fn = Type3RasterFont_OnWrite;
	font->free_fn = Type3RasterFont_OnFree;

	HPDF_MemSet(attr, 0, sizeof(HPDF_FontAttr_Rec));

	font->attr = attr;
	attr->type = HPDF_FONT_TYPE3;
	attr->writing_mode = HPDF_WMODE_HORIZONTAL;
	attr->text_width_fn = Type3RasterFont_TextWidth;
	attr->measure_text_fn = Type3RasterFont_MeasureText;
	attr->fontdef = fontdef;
	attr->encoder = encoder;
	attr->xref = xref;

	/* singlebyte-font has a widths-array which is an array of 256 signed
	 * short integer.
	 */
	attr->widths = HPDF_GetMem(mmgr, sizeof(HPDF_INT16) * 256);
	if (!attr->widths) {
		HPDF_Dict_Free(font);
		return NULL;
	}

	encoder_attr = (HPDF_BasicEncoderAttr)encoder->attr;

	HPDF_MemSet(attr->widths, 0, sizeof(HPDF_INT16) * 256);
	for (i = 0 /*encoder_attr->first_char*/; i <= 255 /*encoder_attr->last_char*/; i++) {
		//HPDF_UNICODE u = encoder_attr->unicode_map[i];

		HPDF_UINT16 w = HPDF_Type3RasterFontDef_GetWidth(fontdef, i);
		attr->widths[i] = w;
	}

	fontdef_attr = (HPDF_Type3RasterFontDefAttr)fontdef->attr;

	ret += HPDF_Dict_AddName(font, "Name", fontdef->base_font);
	ret += HPDF_Dict_AddName(font, "Type", "Font");
	ret += HPDF_Dict_AddName(font, "Subtype", "Type3");
	//ret += HPDF_Dict_AddNumber(font, "FirstChar", 0);
	//ret += HPDF_Dict_AddNumber(font, "LastChar", 255);

	HPDF_REAL scale_x = 72.0f / (HPDF_REAL)fontdef_attr->dpi_x;
	HPDF_REAL scale_y = 72.0f / (HPDF_REAL)fontdef_attr->dpi_y;

	HPDF_TransMatrix font_matrix;
	font_matrix.a = scale_x;
	font_matrix.b = 0;
	font_matrix.c = 0;
	font_matrix.d = scale_y;
	font_matrix.x = 0;
	font_matrix.y = 0;

	HPDF_Array array = HPDF_Array_New(mmgr);
	ret += HPDF_Array_AddReal(array, font_matrix.a);
	ret += HPDF_Array_AddReal(array, font_matrix.b);
	ret += HPDF_Array_AddReal(array, font_matrix.c);
	ret += HPDF_Array_AddReal(array, font_matrix.d);
	ret += HPDF_Array_AddReal(array, font_matrix.x);
	ret += HPDF_Array_AddReal(array, font_matrix.y);
	ret += HPDF_Dict_Add(font, "FontMatrix", array);

	HPDF_Array diff_array = HPDF_Array_New(mmgr);
	ret += HPDF_Array_AddNumber(diff_array, 0);
	for (i = 0; i < 256; ++i) {
		char name[3];
		sprintf(name, "%02X", i);
		ret += HPDF_Array_AddName(diff_array, name);
	}
	HPDF_Dict encoding = HPDF_Dict_New(mmgr);
	ret += HPDF_Dict_Add(encoding, "Differences", diff_array);
	ret += HPDF_Dict_Add(font, "Encoding", encoding);

	HPDF_Dict char_procs = HPDF_Dict_New(mmgr);
	for (i = 0; i < 256; ++i) {
		char name[3];
		sprintf(name, "%02X", i);

		HPDF_Dict char_stream_dict = HPDF_DictStream_New(mmgr, xref);
		HPDF_Stream_WriteReal(char_stream_dict->stream, fontdef_attr->chars[i].width / scale_x);
		HPDF_Stream_WriteStr(char_stream_dict->stream, " 0 ");
		HPDF_Stream_WriteReal(char_stream_dict->stream, fontdef_attr->chars[i].left / scale_x);
		HPDF_Stream_WriteStr(char_stream_dict->stream, " ");
		HPDF_Stream_WriteReal(char_stream_dict->stream, fontdef_attr->chars[i].bottom / scale_y);
		HPDF_Stream_WriteStr(char_stream_dict->stream, " ");
		HPDF_Stream_WriteReal(char_stream_dict->stream, fontdef_attr->chars[i].right / scale_x);
		HPDF_Stream_WriteStr(char_stream_dict->stream, " ");
		HPDF_Stream_WriteReal(char_stream_dict->stream, fontdef_attr->chars[i].top / scale_y);
		HPDF_Stream_WriteStr(char_stream_dict->stream, " d1\012");

		HPDF_Stream_WriteStr(char_stream_dict->stream, "q\012");
		HPDF_Stream_WriteReal(char_stream_dict->stream, fontdef_attr->chars[i].pixels_x);
		HPDF_Stream_WriteStr(char_stream_dict->stream, " 0 0 ");
		HPDF_Stream_WriteReal(char_stream_dict->stream, fontdef_attr->chars[i].pixels_y);
		HPDF_Stream_WriteStr(char_stream_dict->stream, " ");
		HPDF_Stream_WriteReal(char_stream_dict->stream, fontdef_attr->chars[i].left / scale_x);
		HPDF_Stream_WriteStr(char_stream_dict->stream, " ");
		HPDF_Stream_WriteReal(char_stream_dict->stream, fontdef_attr->chars[i].bottom / scale_y);
		HPDF_Stream_WriteStr(char_stream_dict->stream, " cm\012");

		/*HPDF_Stream_WriteStr(char_stream_dict->stream, "q\012");
		HPDF_Stream_WriteStr(char_stream_dict->stream, "1 0 0 1 ");
		HPDF_Stream_WriteReal(char_stream_dict->stream, fontdef_attr->chars[i].left);
		HPDF_Stream_WriteStr(char_stream_dict->stream, " ");
		HPDF_Stream_WriteReal(char_stream_dict->stream, fontdef_attr->chars[i].bottom);
		HPDF_Stream_WriteStr(char_stream_dict->stream, " cm\012");*/

		/*HPDF_Stream_WriteReal(char_stream_dict->stream, fontdef_attr->chars[i].pixels_x * 0.24f);
		HPDF_Stream_WriteStr(char_stream_dict->stream, " 0 0 ");
		HPDF_Stream_WriteReal(char_stream_dict->stream, fontdef_attr->chars[i].pixels_y * 0.24f);
		HPDF_Stream_WriteStr(char_stream_dict->stream, " 0 0 cm\012");*/
		HPDF_Stream_WriteStr(char_stream_dict->stream, "BI\012");
		HPDF_Stream_WriteStr(char_stream_dict->stream, "/IM true\012");
		HPDF_Stream_WriteStr(char_stream_dict->stream, "/W ");
		HPDF_Stream_WriteInt(char_stream_dict->stream, fontdef_attr->chars[i].pixels_x);
		HPDF_Stream_WriteStr(char_stream_dict->stream, " /H ");
		HPDF_Stream_WriteInt(char_stream_dict->stream, fontdef_attr->chars[i].pixels_y);
		HPDF_Stream_WriteStr(char_stream_dict->stream, "\012/BPC 1\012");
		HPDF_Stream_WriteStr(char_stream_dict->stream, "/D [1 0]\012");
		HPDF_Stream_WriteStr(char_stream_dict->stream, "ID\012");

		if (fontdef_attr->chars[i].raster_data != NULL)
			HPDF_Stream_Write(char_stream_dict->stream, fontdef_attr->chars[i].raster_data, fontdef_attr->chars[i].raster_data_size);
		
		HPDF_Stream_WriteStr(char_stream_dict->stream, "\012EI\012");
		HPDF_Stream_WriteStr(char_stream_dict->stream, "Q");

		ret += HPDF_Dict_Add(char_procs, name, char_stream_dict);

		if (fontdef_attr->chars[i].left < fontdef->font_bbox.left)
			fontdef->font_bbox.left = fontdef_attr->chars[i].left;
		if (fontdef_attr->chars[i].bottom < fontdef->font_bbox.bottom)
			fontdef->font_bbox.bottom = fontdef_attr->chars[i].bottom;
		if (fontdef_attr->chars[i].right > fontdef->font_bbox.right)
			fontdef->font_bbox.right = fontdef_attr->chars[i].right;
		if (fontdef_attr->chars[i].top > fontdef->font_bbox.top)
			fontdef->font_bbox.top = fontdef_attr->chars[i].top;
	}

	fontdef->font_bbox.left /= scale_x;
	fontdef->font_bbox.bottom /= scale_y;
	fontdef->font_bbox.right /= scale_x;
	fontdef->font_bbox.top /= scale_y;

	ret += HPDF_Dict_Add(font, "CharProcs", char_procs);

	array = HPDF_Box_Array_New(mmgr, fontdef->font_bbox);
	ret += HPDF_Dict_Add(font, "FontBBox", array);

	if (ret != HPDF_OK) {
		HPDF_Dict_Free(font);
		return NULL;
	}

	if (HPDF_Xref_Add(xref, font) != HPDF_OK)
		return NULL;

	return font;
}

static HPDF_TextWidth
Type3RasterFont_TextWidth(HPDF_Font        font,
	const HPDF_BYTE  *text,
	HPDF_UINT        len)
{
	HPDF_FontAttr attr = (HPDF_FontAttr)font->attr;
	HPDF_TextWidth ret = { 0, 0, 0, 0 };
	HPDF_UINT i;
	HPDF_BYTE b = 0;

	HPDF_PTRACE((" HPDF_Type3RasterFont_TextWidth\n"));

	if (attr->widths) {
		for (i = 0; i < len; i++) {
			b = text[i];
			ret.numchars++;
			ret.width += attr->widths[b];

			if (HPDF_IS_WHITE_SPACE(b)) {
				ret.numspace++;
				ret.numwords++;
			}
		}
	}
	else
		HPDF_SetError(font->error, HPDF_FONT_INVALID_WIDTHS_TABLE, 0);

	/* 2006.08.19 add. */
	if (HPDF_IS_WHITE_SPACE(b))
		; /* do nothing. */
	else
		ret.numwords++;

	return ret;
}


static HPDF_UINT
Type3RasterFont_MeasureText(HPDF_Font          font,
	const HPDF_BYTE   *text,
	HPDF_UINT          len,
	HPDF_REAL          width,
	HPDF_REAL          font_size,
	HPDF_REAL          char_space,
	HPDF_REAL          word_space,
	HPDF_BOOL          wordwrap,
	HPDF_REAL         *real_width)
{
	HPDF_REAL w = 0;
	HPDF_UINT tmp_len = 0;
	HPDF_UINT i;
	HPDF_FontAttr attr = (HPDF_FontAttr)font->attr;

	HPDF_PTRACE((" HPDF_Type3RasterFont_MeasureText\n"));

	for (i = 0; i < len; i++) {
		HPDF_BYTE b = text[i];

		if (HPDF_IS_WHITE_SPACE(b)) {
			tmp_len = i + 1;

			if (real_width)
				*real_width = w;

			w += word_space;
		}
		else if (!wordwrap) {
			tmp_len = i;

			if (real_width)
				*real_width = w;
		}

		w += attr->widths[b] * font_size / 1000;

		/* 2006.08.04 break when it encountered  line feed */
		if (w > width || b == 0x0A)
			return tmp_len;

		if (i > 0)
			w += char_space;
	}

	/* all of text can be put in the specified width */
	if (real_width)
		*real_width = w;

	return len;
}


static HPDF_STATUS
Type3RasterFont_OnWrite(HPDF_Dict    obj,
	HPDF_Stream  stream)
{
	HPDF_FontAttr attr = (HPDF_FontAttr)obj->attr;
	HPDF_Type3RasterFontDefAttr fontdef_attr =
		(HPDF_Type3RasterFontDefAttr)attr->fontdef->attr;
	HPDF_BasicEncoderAttr encoder_attr =
		(HPDF_BasicEncoderAttr)attr->encoder->attr;
	HPDF_UINT i;
	HPDF_STATUS ret;
	char buf[HPDF_TMP_BUF_SIZ];
	char *eptr = buf + HPDF_TMP_BUF_SIZ - 1;

	HPDF_PTRACE((" HPDF_Font_Type3RasterFont_OnWrite\n"));

	/* if font is base14-font these entries is not required */
	if (1) {
		char *pbuf;

		pbuf = (char *)HPDF_StrCpy(buf, "/FirstChar ", eptr);
		pbuf = HPDF_IToA(pbuf, /*encoder_attr->first_char*/0, eptr);
		HPDF_StrCpy(pbuf, "\012", eptr);
		if ((ret = HPDF_Stream_WriteStr(stream, buf)) != HPDF_OK)
			return ret;

		pbuf = (char *)HPDF_StrCpy(buf, "/LastChar ", eptr);
		pbuf = HPDF_IToA(pbuf, /*encoder_attr->last_char*/255, eptr);
		HPDF_StrCpy(pbuf, "\012", eptr);
		if ((ret = HPDF_Stream_WriteStr(stream, buf)) != HPDF_OK)
			return ret;

		/* Widths entry */
		if ((ret = HPDF_Stream_WriteEscapeName(stream, "Widths")) != HPDF_OK)
			return ret;

		if ((ret = HPDF_Stream_WriteStr(stream, " [\012")) != HPDF_OK)
			return ret;

		pbuf = buf;
		for (i = 0 /*encoder_attr->first_char*/; i <= 255/*encoder_attr->last_char*/; i++) {

			pbuf = HPDF_FToA(pbuf, fontdef_attr->chars[i].width / (72.0f / (HPDF_REAL)fontdef_attr->dpi_x), eptr);
			*pbuf++ = ' ';

			if ((i + 1) % 16 == 0) {
				HPDF_StrCpy(pbuf, "\012", eptr);
				if ((ret = HPDF_Stream_WriteStr(stream, buf)) != HPDF_OK)
					return ret;
				pbuf = buf;
			}
		}

		HPDF_StrCpy(pbuf, "]\012", eptr);

		if ((ret = HPDF_Stream_WriteStr(stream, buf)) != HPDF_OK)
			return ret;
	}

	return ret; // attr->encoder->write_fn(attr->encoder, stream);
}

static void
Type3RasterFont_OnFree(HPDF_Dict  obj)
{
	HPDF_FontAttr attr = (HPDF_FontAttr)obj->attr;

	HPDF_PTRACE((" HPDF_Type3RasterFont_OnFree\n"));

	if (attr) {
		if (attr->widths) {
			HPDF_FreeMem(obj->mmgr, attr->widths);
		}
		HPDF_FreeMem(obj->mmgr, attr);
	}
}

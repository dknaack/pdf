#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#define TTF_TAG(a, b, c, d) (((a) << 24) | ((b) << 16) | ((c) << 8) | (d))
#define ASSERT(x) ((x) ? (void)0 : (void)(*(volatile int *)0 = 0))
#define S(x) (str){(x), sizeof(x)-1}

typedef uintptr_t usize;
typedef uint64_t  u64;
typedef uint32_t  u32;
typedef uint16_t  u16;
typedef uint8_t   u8;

typedef intptr_t isize;
typedef int64_t  i64;
typedef int32_t  i32;
typedef int16_t  i16;
typedef int8_t   i8;

typedef double f64;
typedef float  f32;

typedef i32 b32;

typedef struct {
	char *at;
	isize length;
} str;

typedef struct {
	str data;
	isize pos;
} reader;

typedef struct {
	str data, hmtx, cmap, kern;
	u16 num_hmetrics;
	u16 num_glyphs;
	u16 upem;
} font;

typedef enum {
	FONT_SERIF,
	FONT_ITALIC,
	FONT_BOLD,
	FONT_BOLD_ITALIC,
	FONT_COUNT,
} font_id;

typedef struct pdf_object pdf_object;
struct pdf_object {
	pdf_object *next;
	isize offset;
	b32 allocated;
};

typedef struct {
	FILE *file;
	pdf_object *objects;
	i32 object_count;
	i32 max_object_count;
	i32 resources;
	i32 page_tree;
	i32 catalog;
} pdf_file;

typedef struct pdf_stream pdf_stream;
struct pdf_stream {
	pdf_stream *prev;
	char *at;
	isize size;
	isize max_size;
};

static u8 read_u8(reader *r)
{
	u8 result = 0;

	if (0 <= r->pos && r->pos < r->data.length) {
		result = r->data.at[r->pos++];
	}

	return result;
}

static u16 read_u16be(reader *r)
{
	u16 result = 0;

	result |= read_u8(r);
	result <<= 8;
	result |= read_u8(r);

	return result;
}

static u32 read_u32be(reader *r)
{
	u32 result = 0;

	result |= read_u8(r);
	result <<= 8;
	result |= read_u8(r);
	result <<= 8;
	result |= read_u8(r);
	result <<= 8;
	result |= read_u8(r);

	return result;
}

static str substr(str s, isize start, isize end)
{
	str result = {0};

	if (end < 0 || end > s.length) {
		end = s.length;
	}

	if (start < end) {
		result.at = s.at + start;
		result.length = end - start;
	}

	return result;
}

static isize length(char *s)
{
	isize result = 0;
	while (*s++) {
		result++;
	}

	return result;
}

static u16 get_glyph_index(str cmap, u32 c)
{
	u32 glyph_index = 0;
	reader r = {cmap, 0};

	r.pos += 2; // version
	u16 num_tables = read_u16be(&r);
	isize prev_pos = r.pos;
	for (i32 i = 0; i < num_tables; i++) {
		r.pos = prev_pos;
		u16 platform_id = read_u16be(&r);
		r.pos += 2;
		if (platform_id != 0) {
			continue;
		}

		u32 subtable_offset = read_u32be(&r);
		prev_pos = r.pos;

		// Read the table
		r.pos = subtable_offset;
		u16 format = read_u16be(&r);
		switch (format) {
		case 4:
			// Read the number of segments
			r.pos += 2 + 2;
			u16 seg_count = read_u16be(&r) / 2;
			r.pos += 2 + 2 + 2;

			// Read end code, find first segment with c <= end_code
			i32 segment = -1;
			u16 end_code = 0;
			for (i32 i = 0; i < seg_count; i++) {
				end_code = read_u16be(&r);
				if (c <= end_code) {
					segment = i;
					break;
				}
			}

			r.pos += 2 * (seg_count - segment);
			if (segment < 0) {
				continue;
			}

			// Read start code
			r.pos += 2 * segment;
			u16 start_code = read_u16be(&r);
			r.pos += 2 * (seg_count - segment - 1);
			if (start_code > c) {
				continue;
			}

			// Read the id delta
			r.pos += 2 * segment;
			i16 id_delta = read_u16be(&r);
			r.pos += 2 * (seg_count - segment - 1);
			r.pos += 2 * segment;

			u16 id_range_offset = read_u16be(&r);
			if (id_range_offset != 0) {
				r.pos += 2 * (id_range_offset / 2 - 1 + c - start_code);
				glyph_index = read_u16be(&r);
			} else {
				glyph_index = (i16)c + id_delta;
			}
		}

		if (glyph_index != 0) {
			break;
		}
	}

	return glyph_index;
}

static u16 get_glyph_advance(font f, u16 i)
{
	reader hmtx = {f.hmtx, 0};
	if (i < f.num_hmetrics) {
		hmtx.pos += i * 4;
	} else {
		hmtx.pos += (f.num_hmetrics - 1) * 4;
	}

	u16 w = read_u16be(&hmtx);
	return w;
}

static i16 get_kerning(font f, u16 lhs, u16 rhs)
{
	reader r = {f.kern, 0};
	r.pos += 2;

	u16 num_tables = read_u16be(&r);
	for (u16 i = 0; i < num_tables; i++) {
		u16 start = r.pos;
		r.pos += 2;
		u16 len = read_u16be(&r);
		u16 cov = read_u16be(&r);

		u16 fmt = cov >> 8;
		if (fmt == 0) {
			u16 num_pairs = read_u16be(&r);
			r.pos += 3 * 2;

			for (u16 j = 0; j < num_pairs; j++) {
				u16 lhs_value = read_u16be(&r);
				u16 rhs_value = read_u16be(&r);
				u16 kern = read_u16be(&r);
				if (lhs == lhs_value && rhs == rhs_value) {
					return kern;
				}
			}
		}

		r.pos = start + len;
	}

	return 0;
}

static str read_file(const char *filename)
{
	str result = {0};
	FILE *file = fopen(filename, "rb");
	if (file) {
		fseek(file, 0, SEEK_END);
		result.length = ftell(file);
		fseek(file, 0, SEEK_SET);

		result.at = malloc(result.length + 1);
		if (result.at) {
			fread(result.at, result.length, 1, file);
			result.at[result.length] = '\0';
		}

		fclose(file);
	}

	return result;
}

static font read_font(char *filename)
{
	font font = {0};
	reader r = {0};
	r.data = font.data = read_file(filename);

	r.pos += 4;
	u16 num_tables = read_u16be(&r);
	r.pos += 2 + 2 + 2;

	reader hhea = {0};
	reader head = {0};
	for (u16 i = 0; i < num_tables; i++) {
		u32 tag = read_u32be(&r);
		r.pos += 4;
		u32 offset = read_u32be(&r);
		u32 length = read_u32be(&r);
		str table = substr(substr(r.data, offset, -1), 0, length);

		switch (tag) {
		case TTF_TAG('h', 'm', 't', 'x'):
			font.hmtx = table;
			break;
		case TTF_TAG('c', 'm', 'a', 'p'):
			font.cmap = table;
			break;
		case TTF_TAG('k', 'e', 'r', 'n'):
			font.kern = table;
			break;
		case TTF_TAG('h', 'h', 'e', 'a'):
			hhea.data = table;
			break;
		case TTF_TAG('h', 'e', 'a', 'd'):
			head.data = table;
			break;
		}
	}

	// Read the number of hmetrics
	hhea.pos += 17 * 2;
	font.num_hmetrics = read_u16be(&hhea);

	// Read the units per em
	head.pos += 3 * 2 + 3 * 4;
	font.upem = read_u16be(&head);
	return font;
}

static f32 inch(f32 x)
{
	f32 pt = 72 * x;
	return pt;
}

static f32 cm(f32 x)
{
	f32 pt = inch(0.393701 * x);
	return pt;
}

static b32 is_space(char c)
{
	b32 result = (c == ' ' || c == '\n' || c == '\r' || c == '\t' || c == '\f' || c == '\v');
	return result;
}

static isize get_text_width(str s, font f)
{
	isize result = 0;
	u16 prev = 0;

	for (isize i = 0; i < s.length; i++) {
		u16 curr = get_glyph_index(f.cmap, s.at[i]);
		if (prev != 0) {
			int kern = get_kerning(f, prev, curr);
			result += (kern * 1000) / f.upem;
		}

		int advance = get_glyph_advance(f, curr);
		result += (advance * 1000) / f.upem;
	}

	return result;
}

//
// PDF-specific functions
//

static i32 pdf_new_object(pdf_file *pdf)
{
	ASSERT(pdf->object_count < pdf->max_object_count);

	i32 id = pdf->object_count++;
	return id;
}

static void pdf_begin_object(pdf_file *pdf, i32 id)
{
	pdf->objects[id].offset = ftell(pdf->file);
	pdf->objects[id].allocated = 1;
	fprintf(pdf->file, "%d 0 obj\n", id);
}

static i32 pdf_begin_new_object(pdf_file *pdf)
{
	i32 id = pdf_new_object(pdf);
	pdf_begin_object(pdf, id);
	return id;
}

static void pdf_end_object(pdf_file *pdf)
{
	fprintf(pdf->file, "endobj\n");
}

static i32 pdf_new_page(pdf_file *pdf, i32 width, i32 height, i32 contents)
{
	i32 page = pdf_begin_new_object(pdf);
	fprintf(pdf->file,
		"<< /Type /Page\n"
		"/Parent %d 0 R\n"
		"/MediaBox [0 0 %d %d]\n"
		"/Contents %d 0 R\n"
		"/Resources %d 0 R\n"
		">>\n", pdf->page_tree, width, height, contents, pdf->resources);
	pdf_end_object(pdf);
	return page;
}

static pdf_stream *pdf_stream_create(isize size)
{
	if (size < 0) {
		size = 1024;
	}

	pdf_stream *s = calloc(1, sizeof(*s));
	s->at = calloc(size, 1);
	s->max_size = size;
	return s;
}

static void pdf_stream_write(pdf_stream *s, void *data, isize count)
{
	char *byte = data;
	while (count-- > 0) {
		if (s->size == s->max_size) {
			pdf_stream *prev = pdf_stream_create(s->max_size);
			pdf_stream tmp = *prev;
			*prev = *s;
			*s = tmp;

			s->prev = prev;
		}

		s->at[s->size++] = *byte++;
	}
}

static void pdf_stream_puts(pdf_stream *s, char *text)
{
	pdf_stream_write(s, text, length(text));
}

static void pdf_stream_printf(pdf_stream *s, char *fmt, ...)
{
	char tmp[1024] = {0};
	va_list ap;
	va_start(ap, fmt);
	int n = vsnprintf(tmp, sizeof(tmp), fmt, ap);
	va_end(ap);

	pdf_stream_write(s, tmp, n);
}

static void pdf_stream_flush(pdf_stream *s, pdf_file *pdf)
{
	isize total = 0;
	pdf_stream *prev = NULL;
	while (s) {
		pdf_stream *next = s->prev;
		total += s->size;
		s->prev = prev;
		prev = s;
		s = next;
	}

	s = prev;
	fprintf(pdf->file, "<< /Length %zd >>\nstream\n", total);
	while (s) {
		pdf_stream *next = s->prev;
		fwrite(s->at, s->size, 1, pdf->file);
		free(s->at);
		free(s);
		s = next;
	}

	fprintf(pdf->file, "\nendstream\n");
}

static i32 pdf_embed_font(pdf_file *pdf, font font, i32 font_id)
{
	i32 stream = pdf_new_object(pdf);
	i32 descriptor = pdf_new_object(pdf);
	i32 widths = pdf_new_object(pdf);
	i32 object = pdf_new_object(pdf);

	// Font object
	pdf_begin_object(pdf, object);
	fprintf(pdf->file,
		"<< /Type /Font "
		"/Subtype /TrueType "
		"/BaseFont /F%d "
		"/FontDescriptor %d 0 R "
		"/FirstChar 32 "
		"/LastChar 255 "
		"/Width %d 0 R >>\n",
		font_id, descriptor, widths);
	pdf_end_object(pdf);

	// Characters widths
	pdf_begin_object(pdf, widths);
	fprintf(pdf->file, "[ ");
	for (u32 c = 32; c <= 255; c++) {
		u16 glyph = get_glyph_index(font.cmap, c);
		i32 glyph_advance = get_glyph_advance(font, glyph);
		int advance = (glyph_advance * 1000) / font.upem;
		fprintf(pdf->file, "%d ", advance);
	}
	fprintf(pdf->file, " ]\n");
	pdf_end_object(pdf);

	// Font descriptor
	pdf_begin_object(pdf, descriptor);
	fprintf(pdf->file, "<< /Type /FontDescriptor /FontName /F%d /FontFile2 %d 0 R >>\n", font_id, stream);
	pdf_end_object(pdf);

	// Font data stream
	pdf_begin_object(pdf, stream);
	fprintf(pdf->file, "<< /Length %zd >>\nstream\n", font.data.length);
	fwrite(font.data.at, 1, font.data.length, pdf->file);
	fprintf(pdf->file, "endstream\n");
	pdf_end_object(pdf);
	return object;
}

int
main(void)
{
	str text = read_file("example.md");

	// Create the pdf file
	pdf_file pdf = {0};
	pdf.file = fopen("output.pdf", "wb");
	pdf.object_count = 1;
	pdf.max_object_count = 1024;
	pdf.objects = calloc(pdf.max_object_count, sizeof(*pdf.objects));
	if (pdf.file == NULL) {
		perror("Error opening the PDF file");
		return 1;
	}

	fprintf(pdf.file, "%%PDF-1.7\n");
	pdf.resources = pdf_new_object(&pdf);
	pdf.catalog = pdf_new_object(&pdf);
	pdf.page_tree = pdf_new_object(&pdf);

	// Embed a font
	font fonts[FONT_COUNT] = {0};
	i32 font_objects[FONT_COUNT] = {0};
	char *font_filenames[] = {
		"/usr/share/fonts/TTF/Times.TTF",
		"/usr/share/fonts/TTF/Timesi.TTF",
		"/usr/share/fonts/TTF/Timesbd.TTF",
		"/usr/share/fonts/TTF/Timesbi.TTF",
	};

	for (font_id i = 0; i < FONT_COUNT; i++) {
		fonts[i] = read_font(font_filenames[i]);
		font_objects[i] = pdf_embed_font(&pdf, fonts[i], FONT_SERIF);
	}

	// Page content
	i32 page_content = pdf_begin_new_object(&pdf);
	{
		font_id font_id = FONT_SERIF;
		pdf_stream *s = pdf_stream_create(1024);
		pdf_stream_printf(s,
			"BT\n"
			"/F%d 10 Tf\n"
			"11 TL\n"
			"100 700 Td\n", font_id);

		isize begin = 0;
		isize page_width = 612;
		isize width = 0.0f;
		while (begin < text.length) {
			if (text.at[begin] == '*' || text.at[begin] == '_') {
				begin++;
				font_id ^= FONT_ITALIC;
				pdf_stream_printf(s, "/F%d 10 Tf\n", font_id);
			}

			isize end = begin;

			// skip to end of word
			while (end < text.length && !is_space(text.at[end])) {
				end++;
			}

			b32 is_italic = text.at[end - 1] == '*';
			if (is_italic) {
				end--;
			}

			str word = substr(text, begin, end);
			isize word_width = get_text_width(word, fonts[font_id]);
			isize font_height = 9;
			isize margin = 200;
			if (font_height * (word_width + width) > (page_width - margin) * 1000) {
				pdf_stream_puts(s, "T*\n");
				width = 0;
			}

			pdf_stream_puts(s, "[(");

			u16 prev = 0;
			for (isize i = begin; i < end; i++) {
				if (text.at[i] == '\n') {
					continue;
				}

				u16 curr = get_glyph_index(fonts[font_id].cmap, text.at[i]);
				if (prev != 0) {
					i16 k = get_kerning(fonts[font_id], prev, curr);
					if (k != 0) {
						pdf_stream_printf(s, ") %d (", -k);
					}
				}

				pdf_stream_printf(s, "%c", text.at[i]);
				prev = curr;
			}

			pdf_stream_puts(s, " )] TJ\n");
			if (is_italic) {
				end++;
				font_id ^= FONT_ITALIC;
				pdf_stream_printf(s, "/F%d 10 Tf\n", font_id);
			}

			// skip to next word
			while (end < text.length && is_space(text.at[end])) {
				end++;
			}

			width += word_width + get_text_width(S(" "), fonts[font_id]);
			begin = end;
		}

		pdf_stream_puts(s,
			")] TJ\n"
			"ET\n");
		pdf_stream_flush(s, &pdf);
		pdf_end_object(&pdf);
	}

	i32 page = pdf_new_page(&pdf, 612, 792, page_content);

	// Resources
	pdf_begin_object(&pdf, pdf.resources);
	fprintf(pdf.file, "<< /Font <<");
	for (font_id i = 0; i < FONT_COUNT; i++) {
		fprintf(pdf.file, " /F%d %d 0 R", i, font_objects[i]);
	}
	fprintf(pdf.file, " >> >>\n");
	pdf_end_object(&pdf);

	// Page Tree
	pdf_begin_object(&pdf, pdf.page_tree);
	fprintf(pdf.file, "<< /Type /Pages /Kids [%d 0 R] /Count 1 >>\n", page);
	pdf_end_object(&pdf);

	// Document Catalog
	pdf_begin_object(&pdf, pdf.catalog);
	fprintf(pdf.file, "<< /Type /Catalog /Pages %d 0 R >>\n", pdf.page_tree);
	pdf_end_object(&pdf);

	// Cross-reference table
	isize xref_offset = ftell(pdf.file);
	fprintf(pdf.file, "xref\n");
	fprintf(pdf.file, "0 %d\n", pdf.object_count);
	fprintf(pdf.file, "0000000000 65535 f \n");
	for (i32 i = 1; i < pdf.object_count; i++) {
		pdf_object *o = &pdf.objects[i];
		fprintf(pdf.file, "%010zd %05d %c\n", o->offset,
			o->allocated ? 0 : 65535, o->allocated ? 'n' : 'f');
	}

	// Trailer
	fprintf(pdf.file, "trailer\n");
	fprintf(pdf.file, "<< /Size %d /Root %d 0 R >>\n", pdf.object_count + 1, pdf.catalog);
	fprintf(pdf.file, "startxref\n");
	fprintf(pdf.file, "%zd\n", xref_offset);
	fprintf(pdf.file, "%%EOF\n");

	fclose(pdf.file);
	return 0;
}

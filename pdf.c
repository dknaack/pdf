#include <hb.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#define ASSERT(x) ((x) ? (void)0 : (void)(*(volatile int *)0 = 0))

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

typedef double  f64;
typedef float   f32;
typedef int32_t b32;

typedef struct {
	char *at;
	usize length;
} str;

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
} pdf_file;

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

int
main(void)
{
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

	i32 catalog = pdf_begin_new_object(&pdf);
	i32 pages = pdf_new_object(&pdf);
	i32 page = pdf_new_object(&pdf);
	i32 page_content = pdf_new_object(&pdf);
	i32 resources = pdf_new_object(&pdf);

	// Catalog and Pages objects
	fprintf(pdf.file, "<< /Type /Catalog /Pages %d 0 R >>\n", pages);
	pdf_end_object(&pdf);

	pdf_begin_object(&pdf, pages);
	fprintf(pdf.file, "<< /Type /Pages /Kids [%d 0 R] /Count 1 >>\n", page);
	pdf_end_object(&pdf);

	// Page object
	pdf_begin_object(&pdf, page);
	fprintf(pdf.file,
		"<< /Type /Page\n"
		"/Parent %d 0 R\n"
		"/MediaBox [0 0 612 792]\n"
		"/Contents %d 0 R\n"
		"/Resources << /Font << /F1 %d 0 R >> >>\n"
		">>\n", pages, page_content, resources);
	pdf_end_object(&pdf);

	// Calculate the width of a word
	hb_blob_t *blob = hb_blob_create_from_file_or_fail("/usr/share/fonts/TTF/Arial.TTF");
	hb_face_t *face = hb_face_create(blob, 0);
	hb_font_t *hb_font = hb_font_create(face);

	hb_buffer_t *buf = hb_buffer_create();
	hb_buffer_set_direction(buf, HB_DIRECTION_LTR);
	hb_buffer_set_script(buf, HB_SCRIPT_LATIN);
	hb_buffer_set_language(buf, hb_language_from_string("en", -1));

	char *word = "Hello";
	hb_buffer_add_utf8(buf, word, -1, 0, -1);
	hb_shape(hb_font, buf, NULL, 0);

	u32 glyph_count;
	hb_glyph_info_t *glyph_info    = hb_buffer_get_glyph_infos(buf, &glyph_count);
	hb_glyph_position_t *glyph_pos = hb_buffer_get_glyph_positions(buf, &glyph_count);

	double total_width = 0.0;
	for (unsigned int i = 0; i < glyph_count; i++) {
		total_width += glyph_pos[i].x_advance / 64.0;
	}

	printf("Total width of the word '%s': %f\n", word, total_width);

	// Page content
	pdf_begin_object(&pdf, page_content);
	fprintf(pdf.file,
		"<< /Length 44 >>\n"
		"stream\n"
		"BT\n"
		"/F1 12 Tf\n"
		"100 700 Td\n"
		"12 TL\n"
		"<%04X> Tj\n"
		"T* (And here is the second line of the paragraph.) Tj\n"
		"T* (The text can continue with as many lines as needed.) Tj\n"
		"ET\n"
		"endstream\n", glyph_info[0].codepoint);
	pdf_end_object(&pdf);

	// Resources
	i32 font = resources;
	i32 font_descriptor = pdf_new_object(&pdf);
	i32 font_stream = pdf_new_object(&pdf);

	// Font object
	pdf_begin_object(&pdf, font);
	fprintf(pdf.file,
		"<< /Type /Font "
		"   /Subtype /TrueType"
		"   /BaseFont /F1"
		"   /FontDescriptor %d 0 R"
		"   /FirstChar 32 "
		"   /LastChar 255 ",
		font_descriptor);
	fprintf(pdf.file, "/Widths [");
	for (int i = 32; i <= 255; i++) {
		hb_position_t glyph_advance = 0;

		hb_codepoint_t glyph;
		if (hb_font_get_glyph(hb_font, i, 0, &glyph)) {
			glyph_advance = hb_font_get_glyph_h_advance(hb_font, glyph);
		}

		int advance = (glyph_advance * 1000) / 2000;
		fprintf(pdf.file, "      %d\n", advance);
	}

	fprintf(pdf.file, "] >>\n");
	pdf_end_object(&pdf);

	// Font descriptor
	pdf_begin_object(&pdf, font_descriptor);
	fprintf(pdf.file, "<< /Type /FontDescriptor /FontName /F1 /FontFile2 %d 0 R >>\n", font_stream);
	pdf_end_object(&pdf);

	// Font data stream
	str font_data = read_file("/usr/share/fonts/TTF/Arial.TTF");
	pdf_begin_object(&pdf, font_stream);
	fprintf(pdf.file, "<< /Length %zd >>\nstream\n", font_data.length);
	fwrite(font_data.at, 1, font_data.length, pdf.file);
	fprintf(pdf.file, "endstream\n");
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
	fprintf(pdf.file, "<< /Size %d /Root %d 0 R >>\n", pdf.object_count + 1, catalog);
	fprintf(pdf.file, "startxref\n");
	fprintf(pdf.file, "%zd\n", xref_offset);
	fprintf(pdf.file, "%%EOF\n");

	fclose(pdf.file);
	return 0;
}

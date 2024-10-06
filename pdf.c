#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

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
} string;

typedef struct pdf_object pdf_object;
struct pdf_object {
	pdf_object *next;
	isize offset;
};

typedef struct {
	FILE *file;
	pdf_object *objects;
	pdf_object **tail;
	i32 object_count;
} pdf_file;

static i32 pdf_begin_object(pdf_file *pdf)
{
	pdf_object *o = calloc(1, sizeof(pdf_object));
	o->offset = ftell(pdf->file);
	*pdf->tail = o;
	pdf->tail = &o->next;

	i32 i = ++pdf->object_count;
	fprintf(pdf->file, "%d 0 obj\n", i);
	return i;
}

static void pdf_end_object(pdf_file *pdf)
{
	fprintf(pdf->file, "endobj\n");
}

int
main(void)
{
	// Create the pdf file
	pdf_file pdf = {0};
	pdf.tail = &pdf.objects;
    pdf.file = fopen("output.pdf", "wb");
    if (pdf.file == NULL) {
        perror("Error opening the PDF file");
        return 1;
    }

	fprintf(pdf.file, "%%PDF-1.7\n");

    // Catalog and Pages objects
	i32 catalog = pdf_begin_object(&pdf);
    fprintf(pdf.file, "<< /Type /Catalog /Pages 2 0 R >>\n");
	pdf_end_object(&pdf);

	pdf_begin_object(&pdf);
    fprintf(pdf.file, "<< /Type /Pages /Kids [3 0 R] /Count 1 >>\n");
	pdf_end_object(&pdf);

    // Page object
	pdf_begin_object(&pdf);
	fprintf(pdf.file,
		"<< /Type /Page\n"
			"/Parent 2 0 R\n"
			"/MediaBox [0 0 612 792]\n"
			"/Contents 4 0 R\n"
			"/Resources << /Font << /F1 5 0 R >> >>\n"
		">>\n");
	pdf_end_object(&pdf);

    // Page content
	pdf_begin_object(&pdf);
	fprintf(pdf.file,
		"<< /Length 44 >>\n"
		"stream\n"
		"BT\n"
		"/F1 24 Tf\n"
		"100 700 Td\n"
		"(Hello, PDF!) Tj\n"
		"ET\n"
		"endstream\n");
	pdf_end_object(&pdf);

    // Resources
	pdf_begin_object(&pdf);
	fprintf(pdf.file,
		"<< /Type /Font"
			"/Subtype /Type1"
			"/BaseFont /Helvetica >>");
	pdf_end_object(&pdf);

    // Font
	pdf_begin_object(&pdf);
    fprintf(pdf.file, "<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica >>\n");
	pdf_end_object(&pdf);

    // Cross-reference table
    isize xref_offset = ftell(pdf.file);
    fprintf(pdf.file, "xref\n");
    fprintf(pdf.file, "0 %d\n", pdf.object_count);
    fprintf(pdf.file, "0000000000 65535 f \n");
	for (pdf_object *o = pdf.objects; o; o = o->next) {
		fprintf(pdf.file, "%010zd 00000 n\n", o->offset);
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

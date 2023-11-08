#include <stdio.h>

int main() {
    FILE *pdfFile;
    pdfFile = fopen("output.pdf", "wb");

    if (pdfFile == NULL) {
        perror("Error opening the PDF file");
        return 1;
    }

    // PDF header
    fprintf(pdfFile, "%PDF-1.7\n");

    // Catalog and Pages objects
    fprintf(pdfFile, "1 0 obj\n");
    fprintf(pdfFile, "<< /Type /Catalog /Pages 2 0 R >>\n");
    fprintf(pdfFile, "endobj\n");

    fprintf(pdfFile, "2 0 obj\n");
    fprintf(pdfFile, "<< /Type /Pages /Kids [3 0 R] /Count 1 >>\n");
    fprintf(pdfFile, "endobj\n");

    // Page object
    fprintf(pdfFile, "3 0 obj\n");
    fprintf(pdfFile, "<< /Type /Page /Parent 2 0 R /Resources 4 0 R /MediaBox [0 0 595 842] /Contents 5 0 R >>\n");
    fprintf(pdfFile, "endobj\n");

    // Page content
    fprintf(pdfFile, "5 0 obj\n");
    fprintf(pdfFile, "<< /Length 6 0 R >>\n");
    fprintf(pdfFile, "stream\n");
    fprintf(pdfFile, "BT\n/F1 12 Tf\n100 700 Td\n(Hello, World!) Tj\nET\n");
    fprintf(pdfFile, "endstream\n");
    fprintf(pdfFile, "endobj\n");

    // Resources
    fprintf(pdfFile, "4 0 obj\n");
    fprintf(pdfFile, "<< /Font << /F1 7 0 R >> >>\n");
    fprintf(pdfFile, "endobj\n");

    // Font
    fprintf(pdfFile, "7 0 obj\n");
    fprintf(pdfFile, "<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica >>\n");
    fprintf(pdfFile, "endobj\n");

    // Cross-reference table
    long xref_offset = ftell(pdfFile);
    fprintf(pdfFile, "xref\n");
    fprintf(pdfFile, "0 8\n");
    fprintf(pdfFile, "0000000000 65535 f \n");
    fprintf(pdfFile, "0000000009 00000 n \n");
    fprintf(pdfFile, "0000000053 00000 n \n");
    fprintf(pdfFile, "0000000107 00000 n \n");
    fprintf(pdfFile, "0000000153 00000 n \n");
    fprintf(pdfFile, "0000000189 00000 n \n");
    fprintf(pdfFile, "0000000215 00000 n \n");
    fprintf(pdfFile, "0000000359 00000 n \n");

    // Trailer
    fprintf(pdfFile, "trailer\n");
    fprintf(pdfFile, "<< /Size 8 /Root 1 0 R >>\n");
    fprintf(pdfFile, "startxref\n");
    fprintf(pdfFile, "%ld\n", xref_offset);
    fprintf(pdfFile, "%%EOF\n");

    fclose(pdfFile);
    return 0;
}

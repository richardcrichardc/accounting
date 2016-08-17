# Copyright (C) 2012 Richard Collins
#
# Permission is hereby granted, free of charge, to any person obtaining a copy of
# this software and associated documentation files (the "Software"), to deal in
# the Software without restriction, including without limitation the rights to
# use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
# of the Software, and to permit persons to whom the Software is furnished to do
# so, subject to the following conditions:

# The above copyright notice and this permission notice shall be included in all
# copies or substantial portions of the Software.

# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
# SOFTWARE.

from reportlab.pdfgen import canvas
from reportlab.lib.pagesizes import A4
from reportlab.lib.utils import simpleSplit

cm = 28.346 # points

width, height = A4
margin = 2.5 * cm
left = margin
right = width - margin
top = height - margin
bottom = margin

envelope_hole = 1.5 * cm, 21.75 * cm, 9 * cm, 4.5 * cm
address_origin = envelope_hole[0] + 2 * cm, envelope_hole[1] + 3 * cm

tax_invoice_baseline = envelope_hole[1] + envelope_hole[3] - 30 * 1.2
tr_labels_baseline = tax_invoice_baseline - 20
tr_labels_text_width = right - 2.5 * cm

date_format = "%d %b %Y"

table_top = envelope_hole[1] - 1 * cm
table_text_size = 10
table_row_height = 12
table_text_offset = 9
table_rows = 25
table_long_rows = table_rows + 3

table_left_pad = 4
table_right_pad = 6

col1_width = 1.5 * cm
col3_width = 2 * cm
col4_width = 2 * cm
col2_width = width - 2 * margin - col1_width - col3_width - col4_width

col1_x = left
col2_x = col1_x + col1_width
col3_x = col2_x + col2_width
col4_x = col3_x + col3_width


def _tr_label(c, x ,y , label, text):
    c.setFont('Times-Bold', 10)
    c.drawRightString(x, y, label + ": ")
    c.setFont('Courier', 10)
    c.drawString(x, y, text)
    return x, y - 10 * 1.2


class invoice(object):
    def __init__(self):
        self.date = None
        self.number = None
        self.gst_no = None
        self.gst_rate = 0.0

    def _render_header(self, c):
        y = height - 2.75 *cm
        c.setFont('Times-Bold', 30)
        c.drawString(left, y, 'Richard Collins')
        c.setFont('Times-Roman', 10)
        c.drawRightString(right, y, 'Software Development / Linux Sysadmin')
        y -= 0.25 * cm
        c.line(left, y, right, y)
        y -= 1.2 * 9
        c.setFont('Times-Roman', 9)
        c.drawString(left, y, '16 Smithfield Road, Whanganui, NZ  Email: richardcrichardc@gmail.com  Telephone: +64 6 927 6635')


    def render(self, filename):
        c = canvas.Canvas(filename, pagesize=A4)

        self._render_header(c)

        c.setLineWidth(0.5)

        #print c.getAvailableFonts()
        #c.rect(left, bottom, right - left, top - bottom)
        #c.rect(0, 0 , width, height)

        # Address
        #c.rect(*envelope_hole)
        to = c.beginText()
        to.setTextOrigin(*address_origin)
        to.setFont('Courier', 10)
        address = self.address.split('\n') # work around bug in textLines
        to.textLines(address)
        c.drawText(to)

        # Stuff to right of Address
        c.setFont('Times-Bold', 20)
        c.drawRightString(right, tax_invoice_baseline, "Tax Invoice")

        x, y = tr_labels_text_width, tr_labels_baseline
        x, y = _tr_label(c, x, y, 'Date', self.date.strftime(date_format))
        x, y = _tr_label(c, x, y, 'Invoice #', self.number)
        x, y = _tr_label(c, x, y, 'GST #', self.gst_no)

        # Table lines
        c.line(left, table_top, right, table_top)
        c.line(left, table_top - table_row_height, right, table_top - table_row_height)
        c.line(left, table_top - table_rows * table_row_height, right, table_top - table_rows * table_row_height)
        c.line(left, table_top, left, table_top - table_rows * table_row_height)
        c.line(col2_x, table_top, col2_x, table_top - table_rows * table_row_height)
        c.line(col3_x, table_top, col3_x, table_top - table_long_rows * table_row_height)
        c.line(col4_x, table_top, col4_x, table_top - table_long_rows * table_row_height)
        c.line(right, table_top, right, table_top - table_long_rows * table_row_height)
        for row in range(table_rows + 1, table_long_rows + 1):
            c.line(col3_x, table_top - row * table_row_height, right, table_top - row * table_row_height)

        # Table headings
        y = table_top - table_text_offset
        c.setFont('Times-Bold', table_text_size)
        c.drawCentredString(col1_x + col1_width / 2, y, "Qty")
        c.drawCentredString(col2_x + col2_width / 2, y, "Description")
        c.drawCentredString(col3_x + col3_width / 2, y, "Unit Price")
        c.drawCentredString(col4_x + col4_width / 2, y, "Total")

        # Total headings
        y = table_top - table_text_offset - table_rows * table_row_height
        c.setFont('Times-Bold', table_text_size)
        c.drawRightString(col4_x - table_left_pad, y, "Sub-total:")
        y -= table_row_height
        c.drawRightString(col4_x - table_left_pad, y, "GST:")
        y -= table_row_height
        c.drawRightString(col4_x - table_left_pad, y, "Total:")


        # Lines
        y = table_top - table_text_offset
        c.setFont('Courier', table_text_size)
        oo_width = c.stringWidth("00")
        sub_total = 0.0
        lines = 1
        for line in self.lines:
            y -= table_row_height * lines
            line_total = float(line[0]) * float(line[2])
            sub_total += line_total
            c.drawCentredString(col1_x + col1_width / 2, y, str(line[0]))

            # wrap description
            desc_lines = simpleSplit(line[1], 'Courier', table_text_size, col2_width)
            lines = len(desc_lines)
            for i in range(lines):
              c.drawString(col2_x + table_left_pad, y - i* table_row_height, desc_lines[i])

            c.drawAlignedString(col4_x -  table_right_pad - oo_width, y, "%.2f" % (float(line[2]),))
            c.drawAlignedString(right - table_right_pad - oo_width, y, "%.2f" % (line_total,))

        # Totals
        gst = sub_total * self.gst_rate
        total = gst + sub_total
        y = table_top - table_text_offset - table_rows * table_row_height
        c.drawAlignedString(right - table_right_pad - oo_width, y, "%s%.2f" % (self.currency_symbol, sub_total))
        y -= table_row_height
        c.drawAlignedString(right - table_right_pad - oo_width, y, "%s%.2f" % (self.currency_symbol, gst))
        y -= table_row_height
        c.drawAlignedString(right - table_right_pad - oo_width, y, "%s%.2f" % (self.currency_symbol, total))

        # Notes
        y = table_top - table_text_offset - table_rows * table_row_height - 5

        to = c.beginText()
        to.setTextOrigin(left, y)
        to.setFont('Times-Roman', 10)
        to.textLines(self.notes)
        c.drawText(to)


        c.showPage()
        c.save()

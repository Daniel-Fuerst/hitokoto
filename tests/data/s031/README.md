# S031 test data

Clipboard contents for S031 (pasting a table as a Markdown table), taken from test tables with
invented content. Nothing from real mails or documents enters this folder, also nothing derived
from it (S031, requirement "Test data"; review O5). The lint test
`S031_SamplesCarryNoPersonalData` checks every file here.

## Template

`templates/tables.html` holds the tables T1 to T12. Firefox opens it directly, Word and Excel open
the same file; close Word and Excel without saving. For Outlook classic, open the template in Word,
copy the table there, paste it into a new mail, select it in the mail and copy it again; discard the
mail.

| Table | What it covers |
| --- | --- |
| T1 | the story's example: two paragraphs in one cell, a line break in another, prices as text |
| T2 | text before and after a table |
| T3 | merged cells: `colspan`, `rowspan` and both in one cell |
| T4 | characters to escape |
| T5 | spaces, no-break spaces, empty lines and an empty cell |
| T6 | the example for Excel: `<br style='mso-data-placement:same-cell'>`, `"`, an empty cell |
| T7 | a column and a row that are empty throughout |
| T8 | header cells, a link, bold and italic text, an image without text |
| T9 | two tables with text between |
| T10, T11 | a table with one row, a table with one column |
| T12 | a table inside a cell |

## Samples

`samples/<name>.html` holds the clipboard's HTML as Qt hands it to the editor on Windows: the bytes
of "HTML Format" from `StartHTML` to `EndHTML`, every carriage return removed (Qt 6.12,
`QWindowsMimeHtml::convertToMime`). `samples/<name>.txt` holds the clipboard's Unicode text as
UTF-8 with line feeds (Qt replaces CR LF by LF). Git does not normalise these files
(`.gitattributes`).

| Name | Table | Copied in | Selection |
| --- | --- | --- | --- |
| `word-t1-example` | T1 | Word | the table |
| `word-t3-merged` | T3 | Word | the table |
| `outlook-t1-example` | T1 | Outlook classic | the table |
| `outlook-t2-text-around` | T2 | Outlook classic | the line before, the table, the line after |
| `outlook-t4-escapes` | T4 | Outlook classic | the table |
| `outlook-t5-lines` | T5 | Outlook classic | the table |
| `excel-t6-example` | T6 | Excel | the table's cells |
| `excel-t7-empty` | T7 | Excel | the table's cells, the empty column and row included |
| `firefox-t4-escapes` | T4 | Firefox | the table |
| `firefox-t8-web` | T8 | Firefox | the table |
| `firefox-t9-two-tables` | T9 | Firefox | both tables and the text between |
| `firefox-t10-one-row` | T10 | Firefox | the table |
| `firefox-t11-one-column` | T11 | Firefox | the table |
| `firefox-t12-nested` | T12 | Firefox | the outer table |

## Before a sample is committed

The raw clipboard contents stay outside the repository. Before a sample is committed, its HTML is
reduced:

- no links with the `file` scheme and no `<link>` elements (Office's `File-List`, `themeData`,
  `colorSchemeMapping`),
- no generator (`<meta name=Generator>`, `<meta name=ProgId>`, `<meta name=Originator>`) and no
  author,
- nothing else changes: styles, conditional comments and the markup of the tables stay as copied.

The large tables of the benchmarks are generated from the markup pattern of these samples.

extern "C" {
  
#include "print.h"
#include "winpriv.h"  // win_prefix_title, win_unprefix_title

#include <winbase.h>
#include <wingdi.h>
#include <winspool.h>

#include "charset.h"


static HANDLE printer;

static const DOC_INFO_1 doc_info = {
  pDocName : const_cast<char *>("Fatty ANSI printer output"),
  pOutputFile : null,
  pDatatype : const_cast<char *>("TEXT")
};

void
(printer_start_job)(struct term* term_p, wstring printer_name)
{
  if (OpenPrinterW((wchar *)printer_name, &printer, 0)) {
    if (StartDocPrinter(printer, 1, (LPBYTE)&doc_info)) {
      if (StartPagePrinter(printer)) {
        win_tab_prefix_title(L"[Printing...] ");
        return;
      }
      EndDocPrinter(printer);
    }
    ClosePrinter(printer);
    printer = 0;
  }
}

void
printer_write(char *data, uint len)
{
  if (printer) {
    DWORD written;
    WritePrinter(printer, data, len, &written);
  }
}

void
printer_wwrite(wchar * wdata, uint len)
{
  if (printer) {
    char * data = cs__wcstombs(wdata);
    uint mlen = len * sizeof(char);
    printer_write(data, mlen);
    free(data);
  }
}

void
(printer_finish_job)(struct term* term_p)
{
  if (printer) {
    EndPagePrinter(printer);
    EndDocPrinter(printer);
    ClosePrinter(printer);
    printer = 0;
    win_tab_unprefix_title(L"[Printing...] ");
  }
}

}

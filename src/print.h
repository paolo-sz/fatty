#ifndef PRINT_H
#define PRINT_H

#include "std.h"

// printers information
extern uint printer_start_enum(void);
extern wstring printer_get_name(uint);
extern void printer_finish_enum(void);
extern wstring printer_get_default(void);

// printer output
#define printer_start_job(...) (printer_start_job)(term_p, ##__VA_ARGS__)
extern void (printer_start_job)(struct term* term_p, wstring printer_name);
extern void printer_write(char *, uint len);
extern void printer_wwrite(wchar *, uint len);
#define printer_finish_job(...) (printer_finish_job)(term_p, ##__VA_ARGS__)
extern void (printer_finish_job)(struct term* term_p);

#endif

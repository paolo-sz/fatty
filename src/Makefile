SRC := charset.c child.c config.c ctrls.c minibidi.c std.c term.c termclip.c \
	termline.c termmouse.c termout.c winclip.c winctrls.c windialog.c wininput.c \
	winmain.c winprint.c wintext.c wintip.c mcwidth.c winsearch.c printers.c \
	childxx.cc winxx.cc \
	res.rc
BIN ?= fatty.exe
CXX = g++
CC = gcc
RC = windres

CXXFLAGS ?= -Wall -Wextra -std=gnu++11
CFLAGS ?= -std=gnu99 -include std.h -Wall -Wextra -Wundef -Werror
LDFLAGS := -static-libgcc -mwindows -lcomctl32 -limm32 -lwinspool -lole32 -luuid -ld2d1 -lwinmm

TARGET := $(shell $(CC) -dumpmachine)
CPPFLAGS := -DTARGET=$(TARGET) -D_GNU_SOURCE

BUILD_DIR = build/

LDFLAGS := -L$(shell $(CC) -print-file-name=w32api) $(LDFLAGS)

ifdef DEBUG
  extraflags := -g -O0
else
  extraflags := -Os
  CPPFLAGS += -DNDEBUG
endif
CFLAGS += $(extraflags)
CXXFLAGS += $(extraflags)

objfiles := $(addprefix $(BUILD_DIR), $(addsuffix .o, $(basename $(SRC))))
depfiles := $(objfiles:.o=.d)

$(BIN): $(objfiles)
	$(CXX) -o $(BIN) $(objfiles) $(LDFLAGS)

#############################################################################
# generate

rgb.t:	# /usr/share/X11/rgb.txt # X11 color names
#	255 250 250		snow
#	->
#		{255, 250, 250, "snow"},
	sed -e 's,	, ,g' -e 's/ *\([0-9][0-9]*\) *\([0-9][0-9]*\) *\([0-9][0-9]*\) *\([^ ].*[^ ]\) */	{\1, \2, \3, "\4"},/' /usr/share/X11/rgb.txt > rgb.t

combined.t:	# UnicodeData.txt
	sh ./mkcombinedt > combined.t

#############################################################################

-include $(depfiles)

$(BUILD_DIR)config.o: rgb.t
$(BUILD_DIR)wininput.o: vk_names.t combined.t
$(BUILD_DIR)winmain.o: wm_names.t
winprint.c:
	ln -s ../attic/winprint.c .

prepare_build = mkdir -pv $(dir $@)

compileflags = -c $< -o $@ -MMD -MT $@
$(BUILD_DIR)%.o: %.cc Makefile
	@$(prepare_build)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(compileflags)
$(BUILD_DIR)%.o: %.c Makefile
	@$(prepare_build)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(compileflags)
$(BUILD_DIR)%.o: %.rc
	@$(prepare_build)
	$(RC) --preprocessor '$(CC) -E -xc -DRC_INVOKED -MMD -MT $@ $(CPPFLAGS)' $< $*.o
	@mv -v res.o res.d $(BUILD_DIR)

.PHONY: clean
clean:
	-rm -rf build $(BIN) *.t

#############################################################################
# development and debug targets

wm_names.t:	# Windows Message names, only for debugging
#	#define WM_NULL 0x0000
#	->
#		{0x0000, "WM_NULL"},
	sed -e 's/^#define \(WM_[^ ]*\) \(0x[0-9A-Fa-f]*\)$$/	{\2, "\1"},/' -e t -e d /usr/include/w32api/winuser.h > wm_names.t

vk_names.t:	# Windows Virtual Key Code names, only for debugging
#	#define VK_TAB 0x09
#	->
#		{0x09, "VK_TAB"},
	sed -e 's/^#define \(VK_[^ ]*\) \(0x[0-9A-Fa-f]*\)$$/	{\2, "\1"},/' -e t -e d /usr/include/w32api/winuser.h > vk_names.t

#############################################################################

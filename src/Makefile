SRC := charset.c child.c config.c ctrls.c minibidi.c std.c term.c termclip.c \
	termline.c termmouse.c termout.c winclip.c winctrls.c windialog.c wininput.c \
	winmain.c winprint.c wintext.c wintip.c mcwidth.c winsearch.c printers.c \
	winimg.c sixel.c sixel_hls.c base64.c \
	childxx.cc winxx.cc \
	res.rc
BIN ?= fatty.exe
CXX = g++
CC = gcc
RC = windres

CXXFLAGS ?= -Wall -Wextra -std=gnu++11
CFLAGS ?= -std=gnu99 -include std.h -Wall -Wextra -Wundef -Werror
LDFLAGS := -static-libgcc -mwindows -lcomctl32 -limm32 -lwinspool -lole32 -luuid -ld2d1 -lwinmm -lusp10

TARGET := $(shell $(CC) -dumpmachine)
CPPFLAGS := -DTARGET=$(TARGET)

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

#############################################################################

winprint.c:
	ln -s ../attic/winprint.c .

prepare_build = mkdir -pv $(dir $@)

compileflags = -c $< -o $@ -MMD -MT $@
$(BUILD_DIR)%.o: %.cc Makefile $(BUILD_DIR)%.d
	@$(prepare_build)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(compileflags)
$(BUILD_DIR)%.o: %.c Makefile $(BUILD_DIR)%.d
	@$(prepare_build)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(compileflags)
$(BUILD_DIR)%.o: %.rc $(BUILD_DIR)%.d
	@$(prepare_build)
	$(RC) --preprocessor '$(CC) -E -xc -DRC_INVOKED -MMD -MT $@ $(CPPFLAGS)' $< $*.o
	@mv -v res.o res.d $(BUILD_DIR)

-include $(depfiles)

.PHONY: clean
clean:
	-rm -rf build $(BIN) $(debug_t) rgb.t

# check whether sources are free of remaining debugging code
SRCDEBUG:= -e "^(f*printf|\#define debug)" # \# make-escape to #
SRCDEBUG+= -e "^(if|do|for|while) *[({]"
SRCDEBUG+= -e "\((false|0) *&"
SRCDEBUG+= -e "\((true|1) *\|"
SRCDEBUG+= -e "& 0($|[^bx])"
SRCDEBUG+= -e "^///"
checksrc:
	if egrep -a $(SRCDEBUG) *.[hc]; then false; fi

#############################################################################
# development and debug targets

debug_t = _wm.t _vk.t _winidm.t

_wm.t:	# Windows Message names, only for debugging
#	#define WM_NULL 0x0000
#	->
#		{0x0000, "WM_NULL"},
	sed -e 's/^#define \(WM_[^ ]*\) *\(0x[0-9A-Fa-f]*\)$$/  {\2, "\1"},/' -e t -e d /usr/include/w32api/winuser.h | tr 'abcdef' 'ABCDEF' > _wm.t
	echo '  {0x02E0, "WM_DPICHANGED"},' >> _wm.t
	# or, with include file copied from Windows SDK:
	#sed -e 's/^#define \(WM_[^ ]*\) *\(0x[0-9A-Fa-f]*\)$$/  {\2, "\1"},/' -e t -e d etc/WinUser.h | tr 'abcdef' 'ABCDEF' > _wm.t

_vk.t:	# Windows Virtual Key Code names, only for debugging
#	#define VK_TAB 0x09
#	->
#		{0x09, "VK_TAB"},
	sed -e 's/^#define \(VK_[^ ]*\) \(0x[0-9A-Fa-f]*\)$$/	{\2, "\1"},/' -e t -e d /usr/include/w32api/winuser.h > _vk.t

_winidm.t:	winids.h
	sed -e 's/#define *\(IDM_[^ ]*\).*/	{\1, "\1"},/' -e t -e d winids.h > _winidm.t

#############################################################################

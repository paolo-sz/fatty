#ifndef APPINFO_H
#define APPINFO_H

#define APPNAME "FaTTY"
#define WEBSITE "http://github.com/juho-p/fatty"

#define MAJOR_VERSION  1
#define MINOR_VERSION  7
#define PATCH_NUMBER   0
#define BUILD_NUMBER   0

// needed for res.rc
#define APPDESC "Terminal"
#define AUTHOR  "Juho Peltonen"
#define YEAR    "2015"


#define CONCAT_(a,b) a##b
#define CONCAT(a,b) CONCAT_(a,b)
#define STRINGIFY_(s) #s
#define STRINGIFY(s) STRINGIFY_(s)

#if BUILD_NUMBER
  #define VERSION STRINGIFY(MAJOR_VERSION.MINOR_VERSION-CONCAT(beta,BUILD_NUMBER))
#else
  #define VERSION STRINGIFY(MAJOR_VERSION.MINOR_VERSION.PATCH_NUMBER)
#endif

#if defined SVN_DIR && defined SVN_REV	// deprecated
  #undef BUILD_NUMBER
  #define BUILD_NUMBER SVN_REV
  #undef VERSION
  #define VERSION STRINGIFY(svn-SVN_DIR-CONCAT(r,SVN_REV))
#endif


// needed for res.rc
#define POINT_VERSION \
  STRINGIFY(MAJOR_VERSION.MINOR_VERSION.PATCH_NUMBER)
#define COMMA_VERSION \
  MAJOR_VERSION,MINOR_VERSION,PATCH_NUMBER
#define COPYRIGHT "(C) " YEAR " " AUTHOR

// needed for secondary device attributes report
#define DECIMAL_VERSION \
  (MAJOR_VERSION * 10000 + MINOR_VERSION * 100 + PATCH_NUMBER)

// needed for fatty -V and Options... - About...
#define VERSION_TEXT \
  APPNAME " " VERSION " (" STRINGIFY(TARGET) ")\n" \
  COPYRIGHT "\n" \
  "License GPLv3+: GNU GPL version 3 or later\n" \
  "There is no warranty, to the extent permitted by law.\n"

// needed for Options... - About...
#define ABOUT_TEXT \
  "Thanks to creators of mintty and PuTTY for creating working\n"\
  "terminal emulator for windows. This software is largely based\n"\
  "on their work.\n"\
  "Thanks to KDE's Oxygen team for creating the program icon.\n"\
  "For more info, check out " WEBSITE ".\n"

#endif

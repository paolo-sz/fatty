extern "C" {
  
#include "appinfo.h"
#include "appinfo.t"
#define VERSION_DESCRIPTION APPNAME " " VERSION_STAMP VERSION_APPENDIX

char *
version(void)
{
  return const_cast<char *>(VERSION_DESCRIPTION);
}

}
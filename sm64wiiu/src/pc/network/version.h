#ifndef NETWORK_VERSION_H
#define NETWORK_VERSION_H

#define SM64COOPDX_VERSION "wiiu-dev"
#define VERSION_TEXT "v"
#define VERSION_NUMBER 0
#define MINOR_VERSION_NUMBER 0
#define MAX_VERSION_LENGTH 128

const char* get_version(void);
#ifdef COMPILE_TIME
const char* get_version_with_build_date(void);
#endif

#endif

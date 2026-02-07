#ifndef CONFIGFILE_H
#define CONFIGFILE_H

#define CONFIGFILE_DEFAULT "sm64config.txt"

extern bool         configFullscreen;
extern unsigned int configKeyA;
extern unsigned int configKeyB;
extern unsigned int configKeyStart;
extern unsigned int configKeyR;
extern unsigned int configKeyZ;
extern unsigned int configKeyCUp;
extern unsigned int configKeyCDown;
extern unsigned int configKeyCLeft;
extern unsigned int configKeyCRight;
extern unsigned int configKeyStickUp;
extern unsigned int configKeyStickDown;
extern unsigned int configKeyStickLeft;
extern unsigned int configKeyStickRight;
#ifdef TARGET_WII_U
extern bool configN64FaceButtons;
#endif

void configfile_load(void);
void configfile_save(void);
const char *configfile_name(void);

#endif

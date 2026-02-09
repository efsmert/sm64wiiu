#ifndef LEVEL_INFO_H
#define LEVEL_INFO_H

#include <PR/ultratypes.h>
#include <stdbool.h>

void **get_course_name_table(void);
void **get_course_name_table_original(void);
void **get_act_name_table(void);
void **get_act_name_table_original(void);
u8 *convert_string_ascii_to_sm64(u8 *str64, const char *strAscii, bool menu);
char *convert_string_sm64_to_ascii(char *strAscii, const u8 *str64);
const char *get_level_name_ascii(s16 courseNum, s16 levelNum, s16 areaIndex, s16 charCase);
const u8 *get_level_name_sm64(s16 courseNum, s16 levelNum, s16 areaIndex, s16 charCase);
const char *get_level_name(s16 courseNum, s16 levelNum, s16 areaIndex);
const char *get_star_name_ascii(s16 courseNum, s16 starNum, s16 charCase);
const u8 *get_star_name_sm64(s16 courseNum, s16 starNum, s16 charCase);
const char *get_star_name(s16 courseNum, s16 starNum);

#endif

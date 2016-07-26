/******************************************************************************
 *
 * FILENAME:
 *     cfg.h
 *
 * DESCRIPTION:
 *     Define functions to load setting from configuration file
 *
 * REVISION(MM/DD/YYYY):
 *     07/25/2016  Shengkui Leng (shengkui.leng@advantech.com.cn)
 *     - Initial version 
 *
 ******************************************************************************/
#ifndef _CFG_H_
#define _CFG_H_

#define MAX_LINE_LENGTH     260

int ini_get_key_value(const char *ini_file, const char *sect,
    const char *item_key, char *item_val);

char *right_trim(char *str);
char *left_trim(const char *str);

#endif /* _CFG_H_ */

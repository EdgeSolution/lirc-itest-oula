/******************************************************************************
 *
 * FILENAME:
 *     cfg.h
 *
 * DESCRIPTION:
 *     Define some debug macros.
 *
 * REVISION(MM/DD/YYYY):
 *     07/25/2016  Shengkui Leng (shengkui.leng@advantech.com.cn)
 *     - Initial version 
 *
 ******************************************************************************/
#ifndef _CFG_H_
#define _CFG_H_


int ini_get_key_value(const char *ini_file, const char *sect,
    const char *item_key, char *item_val);

int load_config(char *config_file);

#endif /* _CFG_H_ */

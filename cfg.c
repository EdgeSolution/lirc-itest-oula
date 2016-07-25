/******************************************************************************
 *
 * FILENAME:
 *     cfg.c
 *
 * DESCRIPTION:
 *     Load default parameters from config file
 *
 * REVISION(MM/DD/YYYY):
 *     07/25/2016  Shengkui Leng (shengkui.leng@advantech.com.cn)
 *     - Initial version
 *
 ******************************************************************************/
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>

#define MAX_LINE_LENGTH     260


/*
 * Trim whitespace chars from the end of string.
 */
static char *right_trim(char *str)
{
    char *p = str + strlen(str);

    while ((p > str) && isspace(*--p)) {
        *p = '\0';
    }
    return str;
}


/* 
 * Return pointer to first non-whitespace char in string.
 */
static char *left_trim(const char *str)
{
    while (*str && isspace(*str)) {
        str++;
    }
    return (char *)str;
}


/*
 * Return pointer to first specified char or ';' in string, or pointer to
 * null if neither found.
 */
static char *find_char_or_comment(const char *str, char ch)
{
    while (*str && (*str != ch) && (*str != ';')) {
        str++;
    }
    return (char *)str;
}


/*
 * Enhance version of strncpy that ensures dest is null-terminated.
 */
static char *strncpy0(char *dest, const char *src, size_t size)
{
    strncpy(dest, src, size);
    if (size > 0) {
        dest[size - 1] = '\0';
    }
    return dest;
}


/******************************************************************************
 * NAME:
 *      ini_get_key_value
 *
 * DESCRIPTION:
 *      Read a string type value(of specified section & key) from ini file
 *
 * PARAMETERS:
 *      ini_file  - The config file
 *      sect      - Section name
 *      item_key  - The key name
 *      item_val  - Output the value of key
 *
 * RETURN:
 *      0 - OK
 *      Others - Error
 ******************************************************************************/
int ini_get_key_value(const char *ini_file, const char *sect,
    const char *item_key, char *item_val)
{
    char line[MAX_LINE_LENGTH];
    char section[MAX_LINE_LENGTH] = "";
    FILE *file;
    char *start;
    char *end;
    char *name;
    char *value;
    int lineno = 0;
    int error = 0;
    int found_sect = 0;

    if ((!ini_file) || (!sect) || (!item_key) || (!item_val)) {
        printf("Invalid parameters\n");
        return -1;
    }

    file = fopen(ini_file, "r");
    if (!file) {
        printf("Open config file error: %s\n", ini_file);
        return -1;
    }

    /* Search specified section */
    while (fgets(line, sizeof(line), file) != NULL) {
        lineno++;
        start = left_trim(right_trim(line));

        if (*start == '[') {
            /* A "[section]" line */
            end = find_char_or_comment(start + 1, ']');
            if (*end == ']') {
                *end = '\0';
                strncpy0(section, start + 1, sizeof(section));
                start = left_trim(right_trim(section));
                if (!strcmp(start, sect)) {
                    found_sect = 1;
                    break;
                }
            } else if (!error) {
                /* No ']' found on section line */
                error = lineno;
            }
        }
        //else if (*start && (*start != ';')) {
            /* Not a comment, must be a key=value pair */
        //}
    }
    if (!found_sect) {
        fclose(file);
        return -2;
    }

    /* Search key=val */
    while (fgets(line, sizeof(line), file) != NULL) {
        lineno++;
        start = left_trim(right_trim(line));

        if (*start == '[') {
            /* Another section!, jump out */
            error = -3;
            break;

        } else if (*start && (*start != ';')) {
            /* Not a comment, must be a key=value pair */
            end = find_char_or_comment(start, '=');
            if (*end == '=') {
                *end = '\0';
                name = right_trim(start);
                value = left_trim(end + 1);
                end = find_char_or_comment(value, ';');
                if (*end == ';') {
                    *end = '\0';
                }
                right_trim(value);

                /* A valid key=value pair found, is this key we want? */
                if (!strcmp(name, item_key)) {
                    /* The key found! */
                    strcpy(item_val, value);
                    error = 0;
                    break;
                }

            } else if (!error) {
                /* No '=' found on key=value line */
                error = lineno;
            }
        }
    }

    fclose(file);
    return error;
}


/******************************************************************************
 * NAME:
 *      load_config
 *
 * DESCRIPTION:
 *      Load default parameters of diag function from config file.
 *
 * PARAMETERS:
 *      config_file - The config file.
 *      serv        - The dui_serv_x structure.
 *
 * RETURN:
 *      0 - OK
 *      Others - Error
 ******************************************************************************/
int load_config(char *config_file)
{
/*
    char value[MAX_LINE_LENGTH];
    int f;
    int p;
    char *service_name;
    char *func_name;
    char *param_name;


    if (!ini_get_key_value(config_file, sect_name, param_name, value)) {
        DBG_PRINT("%s: %s = %s\n", sect_name, param_name, value);
    }
*/
    return 0;
}


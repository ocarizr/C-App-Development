#include <assert.h>
#include <ctype.h>
#include <limits.h>
#include <math.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

char* readline();
char* ltrim(char*);
char* rtrim(char*);

int parse_int(char*);



/*
 * Complete the 'reformatDate' function below.
 *
 * The function is expected to return a STRING_ARRAY.
 * The function accepts STRING_ARRAY dates as parameter.
 */

/*
 * To return the string array from the function, you should:
 *     - Store the size of the array to be returned in the result_count variable
 *     - Allocate the array statically or dynamically
 *
 * For example,
 * char** return_string_array_using_static_allocation(int* result_count) {
 *     *result_count = 5;
 *
 *     static char* a[5] = {"static", "allocation", "of", "string", "array"};
 *
 *     return a;
 * }
 *
 * char** return_string_array_using_dynamic_allocation(int* result_count) {
 *     *result_count = 5;
 *
 *     char** a = malloc(5 * sizeof(char*));
 *
 *     for (int i = 0; i < 5; i++) {
 *         *(a + i) = malloc(20 * sizeof(char));
 *     }
 *
 *     *(a + 0) = "dynamic";
 *     *(a + 1) = "allocation";
 *     *(a + 2) = "of";
 *     *(a + 3) = "string";
 *     *(a + 4) = "array";
 *
 *     return a;
 * }
 *
 */
 char* GetDay(char* date) {
     int i = 0;
     char* day;

     for (; i < strlen(date); i++) {
        if (date[i] == 's' || date[i] == 'n' || date[i] == 'r' || date[i] == 't') {
            break;
        }
     }
     day = (char *) malloc((i == 1 ? 2 : 3) * sizeof(char));
     memcpy(day, date, i * sizeof(char));

     day[i] = '\0';

     if (strlen(day) == 1) {
        char* temp = day;
        day = malloc(3 * sizeof(char));
        day[0] = '0';
        strcat(day, temp);
     }

     return day;
 }

 char* GetMonth(char* date) {
     int i = 0;
     int j = 0;
     int spaceCount = 0;
     char* month;
     for (; i < strlen(date); i++) {
        if (date[i] == ' ') {
            break;
        }
     }
     month = (char*)malloc(3 * sizeof(char));
     memcpy(month, (date + i + 1), 3 * sizeof(char));

     if (strcmp(month, "Jan") == 0) {
        return "01";
     } else if (strcmp(month, "Feb") == 0) {
        return "02";
     } else if (strcmp(month, "Mar") == 0) {
        return "03";
     } else if (strcmp(month, "Apr") == 0) {
        return "04";
     } else if (strcmp(month, "May") == 0) {
        return "05";
     } else if (strcmp(month, "Jun") == 0) {
        return "06";
     } else if (strcmp(month, "Jul") == 0) {
        return "07";
     } else if (strcmp(month, "Aug") == 0) {
        return "08";
     } else if (strcmp(month, "Sep") == 0) {
        return "09";
     } else if (strcmp(month, "Oct") == 0) {
        return "10";
     } else if (strcmp(month, "Nov") == 0) {
        return "11";
     } else {
        return "12";
     }
 }

char* GetYear(char* date) {
    int i = 0;
    int spaceCount = 0;
    char* year;
    for (; i < strlen(date); i++) {
        if (spaceCount == 2) {
            break;
        }
        if (date[i] == ' ') {
            spaceCount++;
        }
    }
    year = (char*) malloc(5 * sizeof(char));
    memcpy(year, (date + i), 4 * sizeof(char));

    year[4] = '\0';

    return year;
}

char** reformatDate(int dates_count, char** dates, int* result_count) {
    char** listOfReformatedDate = (char **) malloc(dates_count * sizeof(char *));

    int i;
    char* reformatedDate;

    for (i = 0; i < dates_count; i++) {
        char *day = GetDay(*(dates + i));
        char *month = GetMonth(*(dates + i));
        char *year = GetYear(*(dates + i));

        reformatedDate = malloc(11 * sizeof(char));
        reformatedDate[0] = '\0';
        strcat(reformatedDate, year);
        strcat(reformatedDate, "-");
        strcat(reformatedDate, month);
        strcat(reformatedDate, "-");
        strcat(reformatedDate, day);

        *(listOfReformatedDate + i) = (char*) malloc((strlen(reformatedDate)) * sizeof(char));
        *(listOfReformatedDate + i) = reformatedDate;
    }
    *(result_count) = dates_count;
    return listOfReformatedDate;
}

int main()
{
    /*
    FILE* fptr = fopen(getenv("OUTPUT_PATH"), "w");

    int dates_count = parse_int(ltrim(rtrim(readline())));

    char** dates = malloc(dates_count * sizeof(char*));

    for (int i = 0; i < dates_count; i++) {
        char* dates_item = readline();

        *(dates + i) = dates_item;
    }

    int result_count;
    char** result = reformatDate(dates_count, dates, &result_count);

    for (int i = 0; i < result_count; i++) {
        fprintf(fptr, "%s", *(result + i));

        if (i != result_count - 1) {
            fprintf(fptr, "\n");
        }
    }

    fprintf(fptr, "\n");

    fclose(fptr);

    */
    int dates_count = 3;
    char** dates = malloc(dates_count * sizeof(char*));

    *dates = "4th Nov 2030";
    *(dates + 1) = "16th Dec 2018";
    *(dates + 2) = "6th Jun 1933";

    int result_count;
    char** result = reformatDate(dates_count, dates, &result_count);

    int i;

    for (i = 0; i < result_count; i++) {
        printf("%s", *(result + i));

        if (i != result_count - 1) {
            printf("\n");
        }
    }

    return 0;
}

char* readline() {
    size_t alloc_length = 1024;
    size_t data_length = 0;

    char* data = malloc(alloc_length);

    while (true) {
        char* cursor = data + data_length;
        char* line = fgets(cursor, alloc_length - data_length, stdin);

        if (!line) {
            break;
        }

        data_length += strlen(cursor);

        if (data_length < alloc_length - 1 || data[data_length - 1] == '\n') {
            break;
        }

        alloc_length <<= 1;

        data = realloc(data, alloc_length);

        if (!data) {
            data = '\0';

            break;
        }
    }

    if (data[data_length - 1] == '\n') {
        data[data_length - 1] = '\0';

        data = realloc(data, data_length);

        if (!data) {
            data = '\0';
        }
    } else {
        data = realloc(data, data_length + 1);

        if (!data) {
            data = '\0';
        } else {
            data[data_length] = '\0';
        }
    }

    return data;
}

char* ltrim(char* str) {
    if (!str) {
        return '\0';
    }

    if (!*str) {
        return str;
    }

    while (*str != '\0' && isspace(*str)) {
        str++;
    }

    return str;
}

char* rtrim(char* str) {
    if (!str) {
        return '\0';
    }

    if (!*str) {
        return str;
    }

    char* end = str + strlen(str) - 1;

    while (end >= str && isspace(*end)) {
        end--;
    }

    *(end + 1) = '\0';

    return str;
}

int parse_int(char* str) {
    char* endptr;
    int value = strtol(str, &endptr, 10);

    if (endptr == str || *endptr != '\0') {
        exit(EXIT_FAILURE);
    }

    return value;
}

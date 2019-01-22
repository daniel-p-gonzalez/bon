#define END_COLOR_CODE "\x1b[0m"
#define RED_COLOR_CODE "\x1b[31m"
#define WHITE_COLOR_CODE "\x1b[97m"
#define RED(str) RED_COLOR_CODE str END_COLOR_CODE
#define WHITE(str) WHITE_COLOR_CODE str END_COLOR_CODE
#define BOLD(str) "\x1b[1m" str END_COLOR_CODE

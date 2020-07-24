typedef struct URL {
  char scheme[64];
  char base[512];
  char route[1024];
} URL;

int parse_url(char *urlstring, URL *url);
int relpath_is_safe(char *relpath);
int append_index(char *relpath);

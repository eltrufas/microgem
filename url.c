#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include "url.h"
#define MAX_URL_LEN 1024

// TODO: Actually parse port, domain, userinfo, etc
int
accept_base(char *urlstring)
{
  int i = 0;
  while (urlstring[i] != '\0' && urlstring[i] != '/' && urlstring[i] != '?'
      && urlstring[i] != '#') {
    i++;
  }

  return i;
}

int
accept_rel(char *urlstring)
{
  int i = 0;
  while (urlstring[i] != '\0' && urlstring[i] != '?'
      && urlstring[i] != '#') {
    i++;
  }

  return i;
}

int
accept_scheme(char *urlstring)
{
  int i;
  char c;

  for (i = 0; urlstring[i] != '\0'; i++) {
    c = urlstring[i];

    switch(c) {
      case ':':
      case '\0':
        return i;
    }

    if (!isalpha(c) && c != '-' && c != '+' && c != '.') {
      return -1;
    }
  }

  return -1;
}

int
accept_chars(char *urlstring, char* target)
{
  int i;
  i = 0;
  while (1) {
    if (target[i] == '\0') {
      return i;
    }

    if (urlstring[i] == '\0') {
      return -1;
    }

    if (urlstring[i] != target[i]) {
      return -1;
    }
    i++;
  }
}

int
parse_url(char *urlstring, URL *url)
{
  int i = 0;

  int d = accept_scheme(urlstring);
  if (d == -1) {
    return -1;
  }

  strncpy(url->scheme, urlstring, d);
  url->scheme[d] = '\0';
  i += d;

  if (accept_chars(urlstring + i, "://") == -1) {
    return -1;
  }
  i += 3;

  i += accept_base(urlstring + i);

  d = accept_rel(urlstring + i);
  if (d == 0) {
    strcpy(url->route, "/");
  } else {
    strncpy(url->route, urlstring + i, d);
    if (d >= 1024) {
      return -1;
    }

    url->route[d] = '\0';
  }

  return 0;
}

int
check_segment(char *relpath, size_t start, size_t end) {
  if (end - start != 2) {
    return 1;
  }

  char *c = relpath + start;
  if (c[0] == '.' && c[1] == '.') {
    return 0;
  }

  return 1;
};

int
relpath_is_safe(char *relpath)
{
  size_t i = 0;
  size_t s = 0;

  while (relpath[i] != '\0') {
    if (relpath[i] == '/') {
      if (!check_segment(relpath, s, i)) {
        return 0;
      }
      s = i + 1;
    }
    i++;
  }

  return check_segment(relpath, s, i);
}

int
append_index(char *relpath)
{
  size_t len = strlen(relpath);
  if (relpath[len - 1] == '/') {
    strcat(relpath, "index.gmi");
    return 1;
  }

  return 0;
}

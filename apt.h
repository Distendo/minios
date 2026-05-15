#ifndef APT_H
#define APT_H

int apt_update(void);
int apt_list(void);
int apt_install(const char *pkg);
int apt_remove(const char *pkg);
int apt_download(const char *url, const char *outname);

#endif

#ifndef __REQUEST_H__

void request_handle(int fd, const char *root_dir);

int request_parse_headers(int fd);
void request_serve_dynamic_post(int fd, char *filename, char *cgiargs, char *post_data, int content_length);

#endif // __REQUEST_H__
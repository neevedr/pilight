/*
	Copyright (C) 2013 CurlyMo

	This file is part of pilight.

	pilight is free software: you can redistribute it and/or modify it under the
	terms of the GNU General Public License as published by the Free Software
	Foundation, either version 3 of the License, or (at your option) any later
	version.

	pilight is distributed in the hope that it will be useful, but WITHOUT ANY
	WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR
	A PARTICULAR PURPOSE.  See the GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with pilight. If not, see	<http://www.gnu.org/licenses/>
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>
#include <string.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <assert.h>
#include <syslog.h>
#include <signal.h>
#include <stdarg.h>
#include <pthread.h>
#include <sys/socket.h>
#include <pwd.h>

#include "../../pilight.h"
#include "common.h"
#include "mongoose.h"
#include "devices.h"
#include "gc.h"
#include "log.h"
#include "threads.h"
#include "json.h"
#include "socket.h"
#include "webserver.h"
<<<<<<< HEAD
#include "socket.h"
<<<<<<< HEAD
=======
#include "settings.h"
>>>>>>> upstream/development
#include "ssdp.h"
=======
>>>>>>> origin/master
#include "fcache.h"

static int webserver_port = WEBSERVER_PORT;
static int webserver_cache = 1;
static int webgui_websockets = WEBGUI_WEBSOCKETS;
static char *webserver_user = NULL;
static char *webserver_authentication_username = NULL;
static char *webserver_authentication_password = NULL;
static unsigned short webserver_loop = 1;
static unsigned short webserver_php = 1;
static char *webserver_root = NULL;
static char *webgui_tpl = NULL;
static struct mg_server *mgserver[WEBSERVER_WORKERS];

static char *recvBuff = NULL;
static unsigned short webgui_tpl_free = 0;
static unsigned short webserver_root_free = 0;
static unsigned short webserver_user_free = 0;

static int sockfd = 0;

typedef enum {
	WELCOME,
	IDENTIFY,
	REJECT,
	SYNC
} steps_t;

<<<<<<< HEAD
struct per_session_data__http {
	struct libwebsocket *wsi;
	unsigned short loggedin;
};

struct libwebsocket_protocols libwebsocket_protocols[] = {
	{ "http-only", webserver_callback_http, sizeof(struct per_session_data__http), 0 },
	{ "data", webserver_callback_data, 0, 0 },
	{ NULL, NULL, 0, 0 }
};

<<<<<<< HEAD
=======
>>>>>>> upstream/development
typedef struct webqueue_t {
	char *message;
	struct webqueue_t *next;
} webqueue_t;

<<<<<<< HEAD
struct webqueue_t *webqueue;
struct webqueue_t *webqueue_head;
pthread_mutex_t webqueue_lock = PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP;
pthread_cond_t webqueue_signal = PTHREAD_COND_INITIALIZER;
int webqueue_number = 0;
=======
struct per_session_data__http {
	struct libwebsocket *wsi;
};
>>>>>>> origin/master
=======
static struct webqueue_t *webqueue;
static struct webqueue_t *webqueue_head;

static pthread_mutex_t webqueue_lock;
static pthread_cond_t webqueue_signal;
static pthread_mutexattr_t webqueue_attr;

static int webqueue_number = 0;
>>>>>>> upstream/development

int webserver_gc(void) {
	logprintf(LOG_STACK, "%s(...)", __FUNCTION__);

	int i = 0;

	webserver_loop = 0;

	pthread_mutex_unlock(&webqueue_lock);
	pthread_cond_signal(&webqueue_signal);

	if(webserver_root_free) {
		sfree((void *)&webserver_root);
	}
	if(webgui_tpl_free) {
		sfree((void *)&webgui_tpl);
	}
	if(webserver_user_free) {
		sfree((void *)&webserver_user);
	}

	for(i=0;i<WEBSERVER_WORKERS;i++) {
		if(mgserver[i] != NULL) {
			mg_wakeup_server(mgserver[i]);
		}
		mg_destroy_server(&mgserver[i]);
	}

	fcache_gc();
	logprintf(LOG_DEBUG, "garbage collected webserver library");
	return 1;
}

static struct filehandler_t {
	unsigned char *bytes;
	FILE *fp;
	unsigned int ptr;
	unsigned int length;
	unsigned short free;
} filehandler_t;

void webserver_create_header(unsigned char **p, const char *message, char *mimetype, unsigned int len) {
	logprintf(LOG_STACK, "%s(...)", __FUNCTION__);

	*p += sprintf((char *)*p,
		"HTTP/1.0 %s\r\n"
		"Server: pilight\r\n"
		"Content-Type: %s\r\n",
		message, mimetype);
<<<<<<< HEAD
	*p += sprintf((char *)*p,
		"Content-Length: %u\r\n\r\n",
		len);
}

static void webserver_create_minimal_header(unsigned char **p, const char *message, unsigned int len) {
	logprintf(LOG_STACK, "%s(...)", __FUNCTION__);

	*p += sprintf((char *)*p,
<<<<<<< HEAD
		"HTTP/1.1 401 Authorization Required\r\n"
		"WWW-Authenticate: Basic realm=\"pilight\"\r\n"
		"Server: pilight\r\n"
		"Content-Length: 40\r\n"
		"Content-Type: text/html\r\n\r\n");
	*p += sprintf((char *)*p, "401 Authorization Required");
=======
	*p += sprintf((char *)*p,
		"Content-Length: %u\r\n\r\n",
		len);
}

void webserver_create_wsi(struct libwebsocket **wsi, int fd, unsigned char *stream, size_t size) {
	(*wsi)->u.http.fd = fd;
	(*wsi)->u.http.stream = stream;
	(*wsi)->u.http.filelen = size;
	(*wsi)->u.http.filepos = 0;
	(*wsi)->u.http.choke = 1;
	(*wsi)->state = WSI_STATE_HTTP_ISSUING_FILE;
>>>>>>> origin/master
=======
		"HTTP/1.1 %s\r\n"
		"Server: pilight\r\n",
		message);
	*p += sprintf((char *)*p,
		"Content-Length: %u\r\n\r\n",
		len);
>>>>>>> upstream/development
}

static void webserver_create_404(const char *in, unsigned char **p) {
	logprintf(LOG_STACK, "%s(...)", __FUNCTION__);

	char mimetype[] = "text/html";
	webserver_create_header(p, "404 Not Found", mimetype, (unsigned int)(202+strlen((const char *)in)));
	*p += sprintf((char *)*p, "<!DOCTYPE HTML PUBLIC \"-//IETF//DTD HTML 2.0//EN\">\x0d\x0a"
		"<html><head>\x0d\x0a"
		"<title>404 Not Found</title>\x0d\x0a"
		"</head><body>\x0d\x0a"
		"<h1>Not Found</h1>\x0d\x0a"
		"<p>The requested URL %s was not found on this server.</p>\x0d\x0a"
		"</body></html>",
		(const char *)in);
}

char *webserver_mimetype(const char *str) {
	logprintf(LOG_STACK, "%s(...)", __FUNCTION__);

	char *mimetype = malloc(strlen(str)+1);
	if(!mimetype) {
		logprintf(LOG_ERR, "out of memory");
		exit(EXIT_FAILURE);
	}
	memset(mimetype, '\0', strlen(str)+1);
	strcpy(mimetype, str);
	return mimetype;
}

static char *webserver_shell(const char *format_str, struct mg_connection *conn, char *request, ...) {
	logprintf(LOG_STACK, "%s(...)", __FUNCTION__);

	size_t n = 0;
	char *output = NULL;
	const char *type = NULL;
	const char *cookie = NULL;
	int uid = 0;
	va_list ap;

	va_start(ap, request);
	n = (size_t)vsnprintf(NULL, 0, format_str, ap) + strlen(format_str) + 1; // EOL + dual NL
	va_end(ap);

	char *command[n];
	va_start(ap, request);
	vsprintf((char *)command, format_str, ap);
	va_end(ap);

	setenv("SCRIPT_FILENAME", request, 1);
	setenv("REDIRECT_STATUS", "200", 1);
	setenv("SERVER_PROTOCOL", "HTTP/1.1", 1);
	setenv("REMOTE_HOST", "127.0.0.1", 1);
	char sn[1024] = {'\0'};
	if(conn->remote_port != 80) {
		sprintf(sn, "http://%s:%d", conn->remote_ip, conn->remote_port);
	} else {
		sprintf(sn, "http://%s", conn->remote_ip);
	}
	setenv("SERVER_NAME", sn, 1);
	setenv("HTTPS", "off", 1);
	setenv("HTTP_ACCEPT", "text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8", 1);

	if(strcmp(conn->request_method, "POST") == 0) {
		setenv("REQUEST_METHOD", "POST", 1);
		if((type = mg_get_header(conn, "Content-Type")) != NULL) {
			setenv("CONTENT_TYPE", type, 1);
		}
		char len[10];
		sprintf(len, "%d", (int)conn->content_len);
		setenv("CONTENT_LENGTH", len, 1);
	}
	if((cookie = mg_get_header(conn, "Cookie")) != NULL) {
		setenv("HTTP_COOKIE", cookie, 1);
	}
	if(conn->query_string != NULL) {
		setenv("QUERY_STRING", conn->query_string, 1);
	}
	if(strcmp(conn->request_method, "GET") == 0) {
		setenv("REQUEST_METHOD", "GET", 1);
	}
	FILE *fp = NULL;
	if((uid = name2uid(webserver_user)) != -1) {
		if(setuid((uid_t)uid) > -1) {
			if((fp = popen((char *)command, "r")) != NULL) {
				size_t total = 0;
				size_t chunk = 0;
				unsigned char buff[1024] = {'\0'};
				while(!feof(fp)) {
					chunk = fread(buff, sizeof(char), 1024, fp);
					total += chunk;
					output = realloc(output, total+1);
					if(!output) {
						logprintf(LOG_ERR, "out of memory");
						exit(EXIT_FAILURE);
					}
					memcpy(&output[total-chunk], buff, chunk);
				}
				output[total] = '\0';
				unsetenv("SCRIPT_FILENAME");
				unsetenv("REDIRECT_STATUS");
				unsetenv("SERVER_PROTOCOL");
				unsetenv("REMOTE_HOST");
				unsetenv("SERVER_NAME");
				unsetenv("HTTPS");
				unsetenv("HTTP_ACCEPT");
				unsetenv("HTTP_COOKIE");
				unsetenv("REQUEST_METHOD");
				unsetenv("CONTENT_TYPE");
				unsetenv("CONTENT_LENGTH");
				unsetenv("QUERY_STRING");
				unsetenv("REQUEST_METHOD");

				pclose(fp);
				return output;
			}
		} else {
			logprintf(LOG_DEBUG, "failed to change webserver uid");
		}
	} else {
		logprintf(LOG_DEBUG, "webserver user \"%s\" does not exist", webserver_user);
	}

	if(setuid(0) == -1) {
		logprintf(LOG_DEBUG, "failed to restore webserver uid");
	}

	return NULL;
}

static int webserver_auth_handler(struct mg_connection *conn) {
	logprintf(LOG_STACK, "%s(...)", __FUNCTION__);

	if(webserver_authentication_username != NULL && webserver_authentication_password != NULL) {
		return mg_authorize_input(conn, webserver_authentication_username, webserver_authentication_password, mg_get_option(mgserver[0], "auth_domain"));
	} else {
		return MG_TRUE;
	}
}

static int webserver_request_handler(struct mg_connection *conn) {
	logprintf(LOG_STACK, "%s(...)", __FUNCTION__);

	char *request = NULL;
	char *ext = NULL;
	char *mimetype = NULL;
	int size = 0;
	unsigned char *p;
	static unsigned char buffer[4096];
	struct filehandler_t *filehandler = (struct filehandler_t *)conn->connection_param;
	unsigned int chunk = WEBSERVER_CHUNK_SIZE;
	struct stat st;

	if(!conn->is_websocket) {
		if(filehandler != NULL) {
			char buff[WEBSERVER_CHUNK_SIZE];
			if((filehandler->length-filehandler->ptr) < chunk) {
				chunk = (unsigned int)(filehandler->length-filehandler->ptr);
			}
			if(filehandler->fp != NULL) {
				chunk = (unsigned int)fread(buff, sizeof(char), WEBSERVER_CHUNK_SIZE-1, filehandler->fp);
				mg_send_data(conn, buff, (int)chunk);
			} else {
				mg_send_data(conn, &filehandler->bytes[filehandler->ptr], (int)chunk);
			}
			filehandler->ptr += chunk;

			if(filehandler->ptr == filehandler->length || conn->wsbits != 0) {
				if(filehandler->fp != NULL) {
					fclose(filehandler->fp);
					filehandler->fp = NULL;
				}
				if(filehandler->free) {
					sfree((void *)&filehandler->bytes);
				}
				sfree((void *)&filehandler);
				conn->connection_param = NULL;
				return MG_TRUE;
			} else {
				return MG_MORE;
			}
		} else if(conn->uri != NULL) {
			if(strcmp(conn->uri, "/send") == 0) {
				if(conn->query_string != NULL) {
					char out[strlen(conn->query_string)+1];
					urldecode(conn->query_string, out);
					if(json_validate(out) == true) {
						socket_write(sockfd, out);
						mg_printf_data(conn, "{\"message\":\"success\"}");
						return MG_TRUE;
					}
				}
				mg_printf_data(conn, "{\"message\":\"failed\"}");
				return MG_TRUE;
			} else if(strcmp(conn->uri, "/config") == 0) {
				char media[15];
				int internal = 1;
				strcpy(media, "web");
				if(conn->query_string != NULL) {
					sscanf(conn->query_string, "media=%14s%*[ \n\r]", media);
					if(strstr(conn->query_string, "internal") != NULL) {
						internal = 0;
					}
				}
				JsonNode *jsend = config_print(internal, media);
				char *output = json_stringify(jsend, NULL);
				mg_printf_data(conn, output);
				json_delete(jsend);
				sfree((void *)&output);
				jsend = NULL;
				return MG_TRUE;
			} else if(strcmp(conn->uri, "/values") == 0) {
				char media[15];
				strcpy(media, "web");
				if(conn->query_string != NULL) {
					sscanf(conn->query_string, "media=%14s%*[ \n\r]", media);
				}
				JsonNode *jsend = devices_values(media);
				char *output = json_stringify(jsend, NULL);
				mg_printf_data(conn, output);
				json_delete(jsend);
				sfree((void *)&output);
				jsend = NULL;
				return MG_TRUE;
			} else if(strcmp(&conn->uri[(rstrstr(conn->uri, "/")-conn->uri)], "/") == 0) {
				char indexes[255];
				strcpy(indexes, mg_get_option(mgserver[0], "index_files"));
				char *pch = strtok((char *)indexes, ",");
				/* Check if the webserver_root is terminated by a slash. If not, than add it */
				while(pch) {
					size_t l = strlen(webserver_root)+strlen(webgui_tpl)+strlen(conn->uri)+strlen(pch)+4;
					request = realloc(request, l);
					if(!request) {
						logprintf(LOG_ERR, "out of memory");
						exit(EXIT_FAILURE);
					}
					memset(request, '\0', l);
					if(webserver_root[strlen(webserver_root)-1] == '/') {
#ifdef __FreeBSD__
						sprintf(request, "%s%s/%s%s", webserver_root, webgui_tpl, conn->uri, pch);
#else
						sprintf(request, "%s%s%s%s", webserver_root, webgui_tpl, conn->uri, pch);
#endif
					} else {
						sprintf(request, "%s/%s/%s%s", webserver_root, webgui_tpl, conn->uri, pch);
					}
					if(access(request, F_OK) == 0) {
						break;
					}
					pch = strtok(NULL, ",");
				}
			} else if(webserver_root != NULL && webgui_tpl != NULL && conn->uri != NULL) {
				size_t wlen = strlen(webserver_root)+strlen(webgui_tpl)+strlen(conn->uri)+2;
				request = malloc(wlen);
				if(!request) {
					logprintf(LOG_ERR, "out of memory");
					exit(EXIT_FAILURE);
				}
				memset(request, '\0', wlen);
				/* If a file was requested add it to the webserver path to create the absolute path */
				if(webserver_root[strlen(webserver_root)-1] == '/') {
					if(conn->uri[0] == '/')
						sprintf(request, "%s%s%s", webserver_root, webgui_tpl, conn->uri);
					else
						sprintf(request, "%s%s/%s", webserver_root, webgui_tpl, conn->uri);
				} else {
					if(conn->uri[0] == '/')
						sprintf(request, "%s/%s%s", webserver_root, webgui_tpl, conn->uri);
					else
						sprintf(request, "%s/%s/%s", webserver_root, webgui_tpl, conn->uri);
				}
			}
			if(request == NULL) {
				return MG_FALSE;
			}

			char *dot = NULL;
			/* Retrieve the extension of the requested file and create a mimetype accordingly */
			dot = strrchr(request, '.');
			if(!dot || dot == request) {
				mimetype = webserver_mimetype("text/plain");
			} else {
				ext = realloc(ext, strlen(dot)+1);
				if(!ext) {
					logprintf(LOG_ERR, "out of memory");
					exit(EXIT_FAILURE);
				}
				memset(ext, '\0', strlen(dot)+1);
				strcpy(ext, dot+1);

				if(strcmp(ext, "html") == 0) {
					mimetype = webserver_mimetype("text/html");
				} else if(strcmp(ext, "xml") == 0) {
					mimetype = webserver_mimetype("text/xml");
				} else if(strcmp(ext, "png") == 0) {
					mimetype = webserver_mimetype("image/png");
				} else if(strcmp(ext, "gif") == 0) {
					mimetype = webserver_mimetype("image/gif");
				} else if(strcmp(ext, "ico") == 0) {
					mimetype = webserver_mimetype("image/x-icon");
				} else if(strcmp(ext, "jpg") == 0) {
					mimetype = webserver_mimetype("image/jpg");
				} else if(strcmp(ext, "css") == 0) {
					mimetype = webserver_mimetype("text/css");
				} else if(strcmp(ext, "js") == 0) {
					mimetype = webserver_mimetype("text/javascript");
				} else if(strcmp(ext, "php") == 0) {
					mimetype = webserver_mimetype("application/x-httpd-php");
				} else {
					mimetype = webserver_mimetype("text/plain");
				}
			}
			sfree((void *)&ext);

<<<<<<< HEAD
			ext = realloc(ext, strlen(dot)+1);
			memset(ext, '\0', strlen(dot)+1);
			strcpy(ext, dot+1);

			if(strcmp(ext, "html") == 0) {
				mimetype = malloc(10);
				memset(mimetype, '\0', 10);
				strcpy(mimetype, "text/html");
<<<<<<< HEAD
			} else if(strcmp(ext, "xml") == 0) {
				mimetype = malloc(10);
				memset(mimetype, '\0', 10);
				strcpy(mimetype, "text/xml");
=======
>>>>>>> origin/master
			} else if(strcmp(ext, "png") == 0) {
				mimetype = malloc(10);
				memset(mimetype, '\0', 10);
				strcpy(mimetype, "image/png");
			} else if(strcmp(ext, "ico") == 0) {
				mimetype = malloc(13);
				memset(mimetype, '\0', 13);
				strcpy(mimetype, "image/x-icon");
			} else if(strcmp(ext, "css") == 0) {
				mimetype = malloc(9);
				memset(mimetype, '\0', 9);
				strcpy(mimetype, "text/css");
			} else if(strcmp(ext, "js") == 0) {
				mimetype = malloc(16);
				memset(mimetype, '\0', 16);
				strcpy(mimetype, "text/javascript");
=======
			memset(buffer, '\0', 4096);
			p = buffer;

			if(access(request, F_OK) == 0) {
				stat(request, &st);
				if(webserver_cache && st.st_size <= MAX_CACHE_FILESIZE &&
				  strcmp(mimetype, "application/x-httpd-php") != 0 &&
				  fcache_get_size(request, &size) != 0 && fcache_add(request) != 0) {
					sfree((void *)&mimetype);
					goto filenotfound;
				}
			} else {
				sfree((void *)&mimetype);
				goto filenotfound;
>>>>>>> upstream/development
			}

			const char *cl = NULL;
			if((cl = mg_get_header(conn, "Content-Length"))) {
				if(atoi(cl) > MAX_UPLOAD_FILESIZE) {
					char line[1024] = {'\0'};
					sfree((void *)&mimetype);
					mimetype = webserver_mimetype("text/plain");
					sprintf(line, "Webserver Warning: POST Content-Length of %d bytes exceeds the limit of %d bytes in Unknown on line 0", MAX_UPLOAD_FILESIZE, atoi(cl));
					webserver_create_header(&p, "200 OK", mimetype, (unsigned int)strlen(line));
					mg_write(conn, buffer, (int)(p-buffer));
					mg_write(conn, line, (int)strlen(line));
					sfree((void *)&mimetype);
					sfree((void *)&request);
					return MG_TRUE;
				}
			}

			/* If webserver caching is enabled, first load all files in the memory */
			if(strcmp(mimetype, "application/x-httpd-php") == 0 && webserver_php == 1) {
				char *raw = NULL;
				if(strcmp(conn->request_method, "POST") == 0) {
					/* Store all (binary) post data in a file so we
					   can feed it directly into php-cgi */
					char file[20];
					strcpy(file, "/tmp/php");
					char name[11];
					alpha_random(name, 10);
					strcat(file, name);
					int f = open(file, O_CREAT | O_TRUNC | O_WRONLY, 0644);
					if(write(f, conn->content, conn->content_len) != conn->content_len) {
						logprintf(LOG_ERR, "php cache file truncated");
					}
					close(f);
					raw = webserver_shell("cat %s | php-cgi %s 2>&1 | base64", conn, request, file, request);
					unlink(file);
				} else {
					raw = webserver_shell("php-cgi %s  2>&1 | base64", conn, request, request);
				}

				if(raw != NULL) {
					char *output = malloc(strlen(raw)+1);
					if(!output) {
						logprintf(LOG_ERR, "out of memory");
						exit(EXIT_FAILURE);
					}
					memset(output, '\0', strlen(raw)+1);
					size_t olen = (size_t)base64decode((unsigned char *)output, (unsigned char *)raw, (int)strlen(raw));
					sfree((void *)&raw);

					char *ptr = strstr(output, "\n\r");
					char *xptr = strstr(output, "X-Powered-By:");
					char *sptr = strstr(output, "Status:");
					char *nptr = NULL;
					if(sptr) {
						nptr = sptr;
					} else {
						nptr = xptr;
					}

					if(ptr != NULL && nptr != NULL) {
						size_t pos = (size_t)(ptr-output);
						size_t xpos = (size_t)(nptr-output);
						char *header = malloc((pos-xpos)+(size_t)1);
						if(!header) {
							logprintf(LOG_ERR, "out of memory");
							exit(EXIT_FAILURE);
						}

						/* Extract header info from PHP output */
						strncpy(&header[0], &output[xpos], pos-xpos);
						header[(pos-xpos)] = '\0';

						/* Extract content info from PHP output */
						memmove(&output[xpos], &output[pos+3], olen-(pos+2));
						olen-=((pos+2)-xpos);

						/* Retrieve the PHP content type */
						char ite[pos-xpos];
						strcpy(ite, header);
						char *pch = strtok(ite, "\n\r");
						char type[255];
						while(pch) {
							if(sscanf(pch, "Content-type:%*[ ]%s%*[ \n\r]", type)) {
								break;
							}
							if(sscanf(pch, "Content-Type:%*[ ]%s%*[ \n\r]", type)) {
								break;
							}
							pch = strtok(NULL, "\n\r");
						}

						if(strstr(header, "Status: 302 Moved Temporarily") != NULL) {
							webserver_create_minimal_header(&p, "302 Moved Temporarily", (unsigned int)olen);
						} else {
							webserver_create_minimal_header(&p, "200 OK", (unsigned int)olen);
						}

						/* Merge HTML header with PHP header */
						char *hptr = strstr((char *)buffer, "\n\r");
						size_t hlen = (size_t)(hptr-(char *)buffer);
						pos = strlen(header);
						memcpy((char *)&buffer[hlen], header, pos);
						memcpy((char *)&buffer[hlen+pos], "\n\r\n\r", 4);

						if(strlen(type) > 0 && strstr(type, "text") != NULL) {
							mg_write(conn, buffer, (int)strlen((char *)buffer));
							mg_write(conn, output, (int)olen);
							sfree((void *)&output);
						} else {
							if(filehandler == NULL) {
								filehandler = malloc(sizeof(struct filehandler_t));
								filehandler->bytes = malloc(olen);
								memcpy(filehandler->bytes, output, olen);
								filehandler->length = (unsigned int)olen;
								filehandler->ptr = 0;
								filehandler->free = 1;
								filehandler->fp = NULL;
								conn->connection_param = filehandler;
							}
							sfree((void *)&output);
							chunk = WEBSERVER_CHUNK_SIZE;
							if(filehandler != NULL) {
								if((filehandler->length-filehandler->ptr) < chunk) {
									chunk = (filehandler->length-filehandler->ptr);
								}
								mg_send_data(conn, &filehandler->bytes[filehandler->ptr], (int)chunk);
								filehandler->ptr += chunk;

								sfree((void *)&mimetype);
								sfree((void *)&request);
								sfree((void *)&header);
								if(filehandler->ptr == filehandler->length || conn->wsbits != 0) {
									sfree((void *)&filehandler->bytes);
									sfree((void *)&filehandler);
									conn->connection_param = NULL;
									return MG_TRUE;
								} else {
									return MG_MORE;
								}
							}
						}
						sfree((void *)&header);
					}

					sfree((void *)&mimetype);
					sfree((void *)&request);
					return MG_TRUE;
				} else {
					logprintf(LOG_NOTICE, "(webserver) invalid php-cgi output from %s", request);
					webserver_create_404(conn->uri, &p);
					sfree((void *)&mimetype);
					sfree((void *)&request);

					return MG_TRUE;
				}
			} else {
				stat(request, &st);
				if(!webserver_cache || st.st_size > MAX_CACHE_FILESIZE) {
					FILE *fp = fopen(request, "rb");
					fseek(fp, 0, SEEK_END);
					size = (int)ftell(fp);
					fseek(fp, 0, SEEK_SET);
					if(strstr(mimetype, "text") != NULL || st.st_size < WEBSERVER_CHUNK_SIZE) {
						webserver_create_header(&p, "200 OK", mimetype, (unsigned int)size);
						mg_write(conn, buffer, (int)(p-buffer));
						size_t total = 0;
						chunk = 0;
						unsigned char buff[1024];
						while(total < size) {
							chunk = (unsigned int)fread(buff, sizeof(char), 1024, fp);
							mg_write(conn, buff, (int)chunk);
							total += chunk;
						}
						fclose(fp);
					} else {
						if(filehandler == NULL) {
							filehandler = malloc(sizeof(filehandler_t));
							filehandler->bytes = NULL;
							filehandler->length = (unsigned int)size;
							filehandler->ptr = 0;
							filehandler->free = 0;
							filehandler->fp = fp;
							conn->connection_param = filehandler;
						}
						char buff[WEBSERVER_CHUNK_SIZE];
						if(filehandler != NULL) {
							if((filehandler->length-filehandler->ptr) < chunk) {
								chunk = (filehandler->length-filehandler->ptr);
							}
							chunk = (unsigned int)fread(buff, sizeof(char), WEBSERVER_CHUNK_SIZE, fp);
							mg_send_data(conn, buff, (int)chunk);
							filehandler->ptr += chunk;

							sfree((void *)&mimetype);
							sfree((void *)&request);
							if(filehandler->ptr == filehandler->length || conn->wsbits != 0) {
								if(filehandler->fp != NULL) {
									fclose(filehandler->fp);
									filehandler->fp = NULL;
								}
								sfree((void *)&filehandler);
								conn->connection_param = NULL;
								return MG_TRUE;
							} else {
								return MG_MORE;
							}
						}
					}

					sfree((void *)&mimetype);
					sfree((void *)&request);
					return MG_TRUE;
				} else {
					if(fcache_get_size(request, &size) == 0) {
						if(strstr(mimetype, "text") != NULL) {
							webserver_create_header(&p, "200 OK", mimetype, (unsigned int)size);
							mg_write(conn, buffer, (int)(p-buffer));
							mg_write(conn, fcache_get_bytes(request), size);
							sfree((void *)&mimetype);
							sfree((void *)&request);
							return MG_TRUE;
						} else {
							if(filehandler == NULL) {
								filehandler = malloc(sizeof(filehandler_t));
								filehandler->bytes = fcache_get_bytes(request);
								filehandler->length = (unsigned int)size;
								filehandler->ptr = 0;
								filehandler->free = 0;
								filehandler->fp = NULL;
								conn->connection_param = filehandler;
							}
							chunk = WEBSERVER_CHUNK_SIZE;
							if(filehandler != NULL) {
								if((filehandler->length-filehandler->ptr) < chunk) {
									chunk = (filehandler->length-filehandler->ptr);
								}
								mg_send_data(conn, &filehandler->bytes[filehandler->ptr], (int)chunk);
								filehandler->ptr += chunk;

								sfree((void *)&mimetype);
								sfree((void *)&request);
								if(filehandler->ptr == filehandler->length || conn->wsbits != 0) {
									sfree((void *)&filehandler);
									conn->connection_param = NULL;
									return MG_TRUE;
								} else {
									return MG_MORE;
								}
							}
						}
					}
				}
				sfree((void *)&mimetype);
				sfree((void *)&request);
			}
			return 0;
		}
<<<<<<< HEAD
		break;
		case LWS_CALLBACK_FILTER_NETWORK_CONNECTION:
			if((int)in > 0) {
				struct sockaddr_in address;
				int addrlen = sizeof(address);
				getpeername((int)in, (struct sockaddr*)&address, (socklen_t*)&addrlen);
				if(socket_check_whitelist(inet_ntoa(address.sin_addr)) != 0) {
					logprintf(LOG_INFO, "rejected client, ip: %s, port: %d", inet_ntoa(address.sin_addr), ntohs(address.sin_port));
					return -1;
<<<<<<< HEAD
				} else {
					logprintf(LOG_INFO, "client connected, ip %s, port %d", inet_ntoa(address.sin_addr), ntohs(address.sin_port));
=======
				} else {			
					logprintf(LOG_INFO, "client connected, ip %s, port %d", inet_ntoa(address.sin_addr), ntohs(address.sin_port));				
>>>>>>> origin/master
					return 0;
				}
			}
		break;		
<<<<<<< HEAD
		case LWS_CALLBACK_HTTP:
		case LWS_CALLBACK_HTTP_FILE_COMPLETION:
		case LWS_CALLBACK_HTTP_WRITEABLE:
		case LWS_CALLBACK_ESTABLISHED:
		case LWS_CALLBACK_CLIENT_CONNECTION_ERROR:
		case LWS_CALLBACK_CLIENT_FILTER_PRE_ESTABLISH:
		case LWS_CALLBACK_CLIENT_ESTABLISHED:
		case LWS_CALLBACK_CLOSED:
		case LWS_CALLBACK_CLOSED_HTTP:
		case LWS_CALLBACK_CLIENT_RECEIVE:
		case LWS_CALLBACK_CLIENT_RECEIVE_PONG:
		case LWS_CALLBACK_CLIENT_WRITEABLE:
=======
>>>>>>> origin/master
		case LWS_CALLBACK_FILTER_PROTOCOL_CONNECTION:
		case LWS_CALLBACK_OPENSSL_LOAD_EXTRA_CLIENT_VERIFY_CERTS:
		case LWS_CALLBACK_OPENSSL_LOAD_EXTRA_SERVER_VERIFY_CERTS:
		case LWS_CALLBACK_OPENSSL_PERFORM_CLIENT_CERT_VERIFICATION:
		case LWS_CALLBACK_CLIENT_APPEND_HANDSHAKE_HEADER:
		case LWS_CALLBACK_CONFIRM_EXTENSION_OKAY:
		case LWS_CALLBACK_CLIENT_CONFIRM_EXTENSION_SUPPORTED:
		case LWS_CALLBACK_PROTOCOL_INIT:
		case LWS_CALLBACK_PROTOCOL_DESTROY:
		case LWS_CALLBACK_ADD_POLL_FD:
		case LWS_CALLBACK_DEL_POLL_FD:
		case LWS_CALLBACK_SET_MODE_POLL_FD:
		case LWS_CALLBACK_CLEAR_MODE_POLL_FD:
		default:
		break;
=======
	} else if(webgui_websockets == 1) {
		char input[conn->content_len+1];
		strncpy(input, conn->content, conn->content_len);
		input[conn->content_len] = '\0';

		if(json_validate(input) == true) {
			JsonNode *json = json_decode(input);
			char *action = NULL;
			if(json_find_string(json, "action", &action) == 0) {
				if(strcmp(action, "request config") == 0) {
					JsonNode *jsend = config_print(0, "web");
					char *output = json_stringify(jsend, NULL);
					size_t output_len = strlen(output);
					mg_websocket_write(conn, 1, output, output_len);
					sfree((void *)&output);
					json_delete(jsend);
				} else if(strcmp(action, "request values") == 0) {
					JsonNode *jsend = devices_values("web");
					char *output = json_stringify(jsend, NULL);
					size_t output_len = strlen(output);
					mg_websocket_write(conn, 1, output, output_len);
					sfree((void *)&output);
					json_delete(jsend);
				} else if(strcmp(action, "control") == 0 || strcmp(action, "registry") == 0) {
					/* Write all codes coming from the webserver to the daemon */
					socket_write(sockfd, input);
				}
			}
			json_delete(json);
		}
		return MG_TRUE;
>>>>>>> upstream/development
	}
	return MG_MORE;

filenotfound:
	logprintf(LOG_NOTICE, "(webserver) could not read %s", request);
	webserver_create_404(conn->uri, &p);
	mg_write(conn, buffer, (int)(p-buffer));
	sfree((void *)&mimetype);
	sfree((void *)&request);
	return MG_TRUE;
}

static int webserver_connect_handler(struct mg_connection *conn) {
	logprintf(LOG_STACK, "%s(...)", __FUNCTION__);

	char ip[17];
	strcpy(ip, conn->remote_ip);
	if(whitelist_check(conn->remote_ip) != 0) {
		logprintf(LOG_INFO, "rejected client, ip: %s, port: %d", ip, conn->remote_port);
		return MG_FALSE;
	} else {
		logprintf(LOG_INFO, "client connected, ip %s, port %d", ip, conn->remote_port);
		return MG_TRUE;
	}
	return MG_FALSE;
}

static void *webserver_worker(void *param) {
	logprintf(LOG_STACK, "%s(...)", __FUNCTION__);
	while(webserver_loop) {
		if(mg_poll_server(mgserver[(intptr_t)param], 1000) == 0) {
			sleep(1);
		}
	}
	return NULL;
}

static void webserver_queue(char *message) {
	logprintf(LOG_STACK, "%s(...)", __FUNCTION__);

	pthread_mutex_lock(&webqueue_lock);
	if(webqueue_number <= 1024) {
		struct webqueue_t *wnode = malloc(sizeof(struct webqueue_t));
		if(!wnode) {
			logprintf(LOG_ERR, "out of memory");
			exit(EXIT_FAILURE);
		}
		wnode->message = malloc(strlen(message)+1);
		if(!wnode->message) {
			logprintf(LOG_ERR, "out of memory");
			exit(EXIT_FAILURE);
		}
		strcpy(wnode->message, message);

		if(webqueue_number == 0) {
			webqueue = wnode;
			webqueue_head = wnode;
		} else {
			webqueue_head->next = wnode;
			webqueue_head = wnode;
		}

		webqueue_number++;
	} else {
		logprintf(LOG_ERR, "webserver queue full");
	}
	pthread_mutex_unlock(&webqueue_lock);
	pthread_cond_signal(&webqueue_signal);
}

void *webserver_broadcast(void *param) {
	logprintf(LOG_STACK, "%s(...)", __FUNCTION__);

	int i = 0;
	pthread_mutex_lock(&webqueue_lock);
	struct mg_connection *c = NULL;
	
	while(webserver_loop) {
		if(webqueue_number > 0) {
			pthread_mutex_lock(&webqueue_lock);

			logprintf(LOG_STACK, "%s::unlocked", __FUNCTION__);

			for(i=0;i<WEBSERVER_WORKERS;i++) {
				for(c=mg_next(mgserver[i], NULL); c != NULL; c = mg_next(mgserver[i], c)) {
					if(c->is_websocket && webserver_loop == 1) {
						mg_websocket_write(c, 1, webqueue->message, strlen(webqueue->message));
					}
				}
			}

			struct webqueue_t *tmp = webqueue;
			sfree((void *)&webqueue->message);
			webqueue = webqueue->next;
			sfree((void *)&tmp);
			webqueue_number--;
			pthread_mutex_unlock(&webqueue_lock);
		} else {
			pthread_cond_wait(&webqueue_signal, &webqueue_lock);
		}
	}
	return (void *)NULL;
}

void *webserver_clientize(void *param) {
	logprintf(LOG_STACK, "%s(...)", __FUNCTION__);

	unsigned int failures = 0;
	while(webserver_loop && failures <= 5) {
		struct ssdp_list_t *ssdp_list = NULL;
		int standalone = 0;
		settings_find_number("standalone", &standalone);
		if(ssdp_seek(&ssdp_list) == -1 || standalone == 1) {
			logprintf(LOG_DEBUG, "no pilight ssdp connections found");
			char server[16] = "127.0.0.1";
			if((sockfd = socket_connect(server, (unsigned short)socket_get_port())) == -1) {
				logprintf(LOG_DEBUG, "could not connect to pilight-daemon");
				failures++;
				continue;
			}
		} else {
			if((sockfd = socket_connect(ssdp_list->ip, ssdp_list->port)) == -1) {
				logprintf(LOG_DEBUG, "could not connect to pilight-daemon");
				failures++;
				continue;
			}
		}
		if(ssdp_list) {
			ssdp_free(ssdp_list);
		}

		struct JsonNode *jclient = json_mkobject();
		struct JsonNode *joptions = json_mkobject();
		json_append_member(jclient, "action", json_mkstring("identify"));
		json_append_member(joptions, "config", json_mknumber(1, 0));
		json_append_member(joptions, "core", json_mknumber(1, 0));
		json_append_member(jclient, "options", joptions);
		json_append_member(jclient, "media", json_mkstring("web"));
		char *out = json_stringify(jclient, NULL);
		socket_write(sockfd, out);
		sfree((void *)&out);
		json_delete(jclient);

		if(socket_read(sockfd, &recvBuff) != 0
			 || strcmp(recvBuff, "{\"status\":\"success\"}") != 0) {
				failures++;
			continue;
		}
		failures = 0;
		while(webserver_loop) {
			if(socket_read(sockfd, &recvBuff) != 0) {
				break;
			} else {
				webserver_queue(recvBuff);
			}
		}
	}

	if(recvBuff) {
		sfree((void *)&recvBuff);
		recvBuff = NULL;
	}
	socket_close(sockfd);
	return 0;
}

static int webserver_handler(struct mg_connection *conn, enum mg_event ev) {
	logprintf(LOG_STACK, "%s(...)", __FUNCTION__);

	if(ev == MG_REQUEST || (ev == MG_POLL && !conn->is_websocket)) {
		if(ev == MG_POLL ||
		  (!conn->is_websocket && webserver_connect_handler(conn) == MG_TRUE) ||
		  conn->is_websocket) {
			return webserver_request_handler(conn);
		} else {
			return MG_FALSE;
		}
	} else if(ev == MG_AUTH) {
		return webserver_auth_handler(conn);
	} else {
		return MG_FALSE;
	}
}

int webserver_start(void) {
	logprintf(LOG_STACK, "%s(...)", __FUNCTION__);

	if(which("php-cgi") != 0) {
		webserver_php = 0;
		logprintf(LOG_NOTICE, "php support disabled due to missing php-cgi executable");
	}
	if(which("cat") != 0) {
		webserver_php = 0;
		logprintf(LOG_NOTICE, "php support disabled due to missing cat executable");
	}
	if(which("base64") != 0) {
		webserver_php = 0;
		logprintf(LOG_NOTICE, "php support disabled due to missing base64 executable");
	}

	pthread_mutexattr_init(&webqueue_attr);
	pthread_mutexattr_settype(&webqueue_attr, PTHREAD_MUTEX_RECURSIVE);
	pthread_mutex_init(&webqueue_lock, &webqueue_attr);
	pthread_cond_init(&webqueue_signal, NULL);

	/* Check on what port the webserver needs to run */
	settings_find_number("webserver-port", &webserver_port);
	if(settings_find_string("webserver-root", &webserver_root) != 0) {
		/* If no webserver port was set, use the default webserver port */
		webserver_root = malloc(strlen(WEBSERVER_ROOT)+1);
		if(!webserver_root) {
			logprintf(LOG_ERR, "out of memory");
			exit(EXIT_FAILURE);
		}
		strcpy(webserver_root, WEBSERVER_ROOT);
		webserver_root_free = 1;
	}
	if(settings_find_string("webgui-template", &webgui_tpl) != 0) {
		/* If no webserver port was set, use the default webserver port */
		webgui_tpl = malloc(strlen(WEBGUI_TEMPLATE)+1);
		if(!webgui_tpl) {
			logprintf(LOG_ERR, "out of memory");
			exit(EXIT_FAILURE);
		}
		strcpy(webgui_tpl, WEBGUI_TEMPLATE);
		webgui_tpl_free = 1;
	}
	settings_find_number("webgui-websockets", &webgui_websockets);

	/* Do we turn on webserver caching. This means that all requested files are
	   loaded into the memory so they aren't read from the FS anymore */
	settings_find_number("webserver-cache", &webserver_cache);
<<<<<<< HEAD
	settings_find_number("webserver-authentication", &webserver_authentication);
	settings_find_string("webserver-password", &webserver_password);
	settings_find_string("webserver-username", &webserver_username);

	/* Default websockets info */
	memset(&info, 0, sizeof info);
	info.port = webserver_port;
	info.protocols = libwebsocket_protocols;
	info.ssl_cert_filepath = NULL;
	info.ssl_private_key_filepath = NULL;
	info.gid = -1;
	info.uid = -1;

	/* Start the libwebsocket server */
	struct libwebsocket_context *context = libwebsocket_create_context(&info);
	if(context == NULL) {
		lwsl_err("libwebsocket init failed\n");
	} else {
		/* Register a seperate thread in which the webserver communicates
		   the main daemon as if it where a gui */
		threads_register("webserver client", &webserver_clientize, (void *)NULL);
<<<<<<< HEAD
		threads_register("webserver broadcast", &webserver_broadcast, (void *)NULL);
=======
		sleep(1);
>>>>>>> origin/master
		/* Main webserver loop */
		while(n >= 0 && webserver_loop) {
			n = libwebsocket_service(context, 50);
=======
	settings_find_string("webserver-authentication-password", &webserver_authentication_password);
	settings_find_string("webserver-authentication-username", &webserver_authentication_username);
	if(settings_find_string("webserver-user", &webserver_user) != 0) {
		/* If no webserver port was set, use the default webserver port */
		webserver_user = malloc(strlen(WEBSERVER_USER)+1);
		if(!webserver_user) {
			logprintf(LOG_ERR, "out of memory");
			exit(EXIT_FAILURE);
>>>>>>> upstream/development
		}
		strcpy(webserver_user, WEBSERVER_USER);
		webserver_user_free = 1;
	}
	char webport[10] = {'\0'};
	sprintf(webport, "%d", webserver_port);
	int i = 0;
	for(i=0;i<WEBSERVER_WORKERS;i++) {
		char id[2];
		sprintf(id, "%d", i);
		mgserver[i] = mg_create_server((void *)id, webserver_handler);
		mg_set_option(mgserver[i], "listening_port", webport);
		mg_set_option(mgserver[i], "auth_domain", "pilight");
		char msg[25];
		sprintf(msg, "webserver worker #%d", i);
		threads_register(msg, &webserver_worker, (void *)(intptr_t)i, 0);
	}

	logprintf(LOG_DEBUG, "webserver listening to port %s", webport);

	return 0;
}

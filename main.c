#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <getopt.h>
#include <fcntl.h>
#include <event2/event.h>
#include <event2/listener.h>
#include <event2/buffer.h>
#include <event2/bufferevent.h>
#include <event2/bufferevent_ssl.h>
#include <event2/visibility.h>
#include <event2/event-config.h>
#include <event2/util.h>
#include <openssl/ssl.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "url.h"

extern int errno;
extern char *optarg;

int sockfd;
SSL_CTX *sslctx;

char *res;
size_t reslen;

#define MAX_HOSTNAME_LEN 512
#define MAX_PATH_LEN 2048

char hostname[MAX_HOSTNAME_LEN];
char sockaddr[MAX_HOSTNAME_LEN];
long int port;
char certpath[MAX_PATH_LEN];
char keypath[MAX_PATH_LEN];
char rootdir[MAX_PATH_LEN];
char path[MAX_PATH_LEN * 2];

enum RequestState{
	REQUEST_READING,
	REQUEST_WRITING,
};

struct Request {
	char *request;
	size_t len;
	enum RequestState state;
};

int
init_socket()
{
	sockfd = socket(AF_INET, SOCK_STREAM, 0);

	fcntl(sockfd, F_SETFL, O_NONBLOCK);

	int reuseaddr = 1;
	setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &reuseaddr,
		sizeof(reuseaddr));

	struct sockaddr_in addr = {};
	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);

	if (inet_pton(AF_INET, sockaddr, &addr.sin_addr) == 0) {
		return -1;
	}

	if (bind(sockfd, (struct sockaddr *)&addr, sizeof(struct sockaddr_in))
			== -1) {
		puts("unable to bind to socket");
		return -1;
	}

	if (listen(sockfd, 2048) == -1) {
		return -1;
	}

	return 0;
}

int
init_ssl_ctx() {
	SSL_library_init();

	sslctx = SSL_CTX_new(TLS_method());
	SSL_CTX_set_min_proto_version(sslctx, TLS1_1_VERSION);

	if (SSL_CTX_use_certificate_file(sslctx, "cert.pem", SSL_FILETYPE_PEM)
			!= 1) {
		return -1;
	}

	if (SSL_CTX_use_PrivateKey_file(sslctx, "key.pem", SSL_FILETYPE_PEM)
			!= 1) {
		return -1;
	}

	return 0;
}

int
read_static_content(char **dest)
{
	FILE *fp = fopen(rootdir, "r");
	if (!fp) {
		return -1;
	}

	fseek(fp, 0, SEEK_END);
	size_t len = ftell(fp);

	fseek(fp, 0, SEEK_SET);

	char *buf = calloc(len, sizeof(char));

	fread(buf, sizeof(char), len, fp);
	
	*dest = buf;

	fclose(fp);

	return len;
}

int
read_args(int argc, char **argv)
{
	int opt;
	char *end;

	// Default values
	strcpy(hostname, "tenshi");
	strcpy(sockaddr, "127.0.0.1");
	port = 1965;
	strcpy(certpath, "./cert.pem");
	strcpy(keypath, "./key.pem");
	strcpy(rootdir, "./content");

	while ((opt = getopt(argc, argv, "b:h:p:c:k:d:")) != -1) {
		switch (opt) {
		case 'h':
			strncpy(hostname, optarg, MAX_HOSTNAME_LEN);
			break;
		case 'b':
			strncpy(sockaddr, optarg, MAX_HOSTNAME_LEN);
			break;
		case 'p':
			port = strtol(optarg, &end, 10);
			if (end == optarg || *end != '\0' || errno == ERANGE
					|| port <= 0) {
				puts("Invalid port number");
				return 2;
			}
			break;
		case 'c':
			strncpy(certpath, optarg, MAX_PATH_LEN);
			break;
		case 'k':
			strncpy(keypath, optarg, MAX_PATH_LEN);
			break;
		case 's':
			strncpy(rootdir, optarg, MAX_PATH_LEN);
			break;
		}
	}

	return 0;
}

void
handle_event(struct bufferevent *bev, short events, void *ctx)
{
		struct Request *req = ctx;

		if (events & (BEV_EVENT_ERROR|BEV_EVENT_EOF)) {
				printf("Closing\n");

				if (req->request) {
					free(req->request);
					req->request = 0;
				}
				free(req);
				bufferevent_free(bev);
		}
}

void
write_header(struct evbuffer *bev, char code, char* meta)
{
	char header[1024];
	uint32_t len = sprintf(header, "%d %s\r\n", code, meta);
	evbuffer_add(bev, header, len);
}

void
write_response(struct bufferevent *bev, void *ctx)
{
		struct Request *req = ctx;
		struct evbuffer *output;

		if (req->state != REQUEST_WRITING) {
			return;
		}

		output = bufferevent_get_output(bev);

		size_t len = evbuffer_get_length(output);
		if (!len) {
			free(req->request);
			free(req);
			bufferevent_free(bev);
		}
}

void
read_request(struct bufferevent *bev, void *ctx)
{
	struct evbuffer *input;
	struct evbuffer *output;
	char *line;
	struct Request *req = ctx;

	if (req->state != REQUEST_READING) {
		return;
	}

	input = bufferevent_get_input(bev);
	output = bufferevent_get_output(bev);

	line = evbuffer_readln(input, 0, EVBUFFER_EOL_CRLF);

	if (!line) {
		return;
	}

	req->request = line;
	req->state = REQUEST_WRITING;

	struct URL url;
	int ret = parse_url(line, &url);

	if (ret == -1) {
		write_header(output, 59, "Unable to parse uri");
		return;
	}

	if (!relpath_is_safe(url.route)) {
		write_header(output, 59, "Illegal path");
		return;
	}


	strcpy(path, rootdir);
	append_index(url.route);
	strcat(path, url.route);
	int filefd = open(path, O_RDONLY);
	if (filefd == -1) {
		write_header(output, 51, "File not found");
		return;
	}

	off_t fsize;

	fsize = lseek(filefd, 0, SEEK_END);
	lseek(filefd, 0, SEEK_SET);

	write_header(output, 20, "text/gemini");
	evbuffer_add_file(output, filefd, 0, fsize);
	bufferevent_write(bev, res, reslen);
}

void
accept_conn(struct evconnlistener *serv, int sock, struct sockaddr *sa,
		int sa_len, void *arg)
{
	struct bufferevent *bev;
	struct event_base *base;
	base = evconnlistener_get_base(serv);

	SSL *ssl = SSL_new(sslctx);

	bev = bufferevent_openssl_socket_new(base, sock, ssl,
			BUFFEREVENT_SSL_ACCEPTING, BEV_OPT_CLOSE_ON_FREE);

	struct Request *req = calloc(1, sizeof(struct Request));
	req->state = REQUEST_READING;

	bufferevent_setcb(bev, read_request, write_response, handle_event, req);
	bufferevent_enable(bev, EV_READ|EV_WRITE);
}

int
main(int argc, char **argv)
{
	if (read_args(argc, argv)) {
		return 2;
	}

	if (init_ssl_ctx()) {
		return 2;
	}

	struct sockaddr_in addr = {};
	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);

	if (inet_pton(AF_INET, sockaddr, &addr.sin_addr) == 0) {
		return -2;
	}

	struct event_base *base = event_base_new();

	struct evconnlistener *listener;

	event_base_dispatch(base);

	puts("help");
	listener = evconnlistener_new_bind(base, accept_conn, 0,
		LEV_OPT_CLOSE_ON_FREE | LEV_OPT_REUSEABLE, 1024,
		(struct sockaddr *)&addr, sizeof(addr));

	if (!listener) {
		return -2;
	}

	event_base_loop(base, 0);

	evconnlistener_free(listener);
	return 0;
}

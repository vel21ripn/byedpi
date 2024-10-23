
struct http_req_param {
    char    *method;
    char    *host;
    char    *path;
    char    *ua;
    char    *vers;
    char    *accept;
    char    *req_buf;
    size_t  req_buf_len;
    int	    eoh;
};

struct http_req_param *gen_http_init(void);
struct http_req_param * gen_http_parser(struct http_req_param *http,const char * const opts);
char *gen_http_req(struct http_req_param *http, size_t len);
void gen_http_free(struct http_req_param *http);

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <strings.h>
#include <string.h>

#include "gen_http_req.h"
//#ifndef TESTING
#include "error.h"

extern char *tls_sni_name;

static char * http_req_tmpl = "%s %s %s\r\n"
    "Host: %s\r\n"
    "User-Agent: %s\r\n"
    "Accept: %s\r\n";


struct http_req_param http_req_default = {
    .method = "GET",
    .path   = "/index.html",
    .vers   = "HTTP/1.1",
    .host   = "yandex.ru",
    .ua     = "Mozilla/5.0 (X11; Linux x86_64; rv:128.0) Gecko/20100101 Firefox/128.0",
    .accept = "text/html,application/xhtml+xml,application/xml;q=0.9,image/avif,image/webp,image/png,image/svg+xml,*/*;q=0.8" };

struct http_req_param *gen_http_init(void) {
    return (struct http_req_param *)calloc(1,sizeof(struct http_req_param));
}

#define FREEPAR(a) if(http->a && http->a != http_req_default.a) free(http->a)

void gen_http_free(struct http_req_param *http) {
    if(!http) return;
    FREEPAR(method);
    FREEPAR(host);
    FREEPAR(path);
    FREEPAR(vers);
    FREEPAR(ua);
    FREEPAR(accept);
    if(http->req_buf) free(http->req_buf);
    free(http);
}
#undef FREEPAR

#define COPYPAR(a) if(!http->a) http->a = http_req_default.a

static void gen_http_default(struct http_req_param *http) {
#ifndef TESTING
    if(!http->host && tls_sni_name) http->host = strdup(tls_sni_name);
#endif
    COPYPAR(host);
    COPYPAR(accept);
    COPYPAR(method);
    COPYPAR(path);
    COPYPAR(vers);
    COPYPAR(ua);
}
#undef COPYPAR

static inline size_t min_val(size_t a, size_t b) {
    return a < b ? a:b;
}
static char r_encode_c[]="0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";

static char *get_random_str(char *buf, size_t len, size_t buf_len) {
    size_t l;
    unsigned long int rnd = 0,x;
    char *o = buf;
    
    l =  min_val(len,buf_len);

    while(o < &buf[l]) {
        if(rnd == 0) rnd = random();
        x = rnd % (sizeof(r_encode_c)-1);
        *o++ = r_encode_c[x];
        rnd /= sizeof(r_encode_c)-1;
    }
    *o = '\0';
    return buf;
}

char *gen_http_req(struct http_req_param *http,size_t len) {
    char *req;
    char req_header[32+66],rnd_data[66];
    size_t l;
    int i = 0,eoh;

    if(!http) return NULL;
    if(!len) return NULL;

    if(http->req_buf && len > http->req_buf_len) {
        free(http->req_buf);
        http->req_buf = NULL;
        http->req_buf_len = 0;
    }
    if(!http->req_buf) {
        req = malloc(len+1);
        if(!req) return NULL;
        http->req_buf = req;
        http->req_buf_len = len;
    } else
        req = http->req_buf;

    gen_http_default(http);
    eoh = http->eoh ? 2:0;

    snprintf(req,len-eoh,http_req_tmpl,
            http->method,http->path,http->vers,
            http->host,
            http->ua,
            http->accept);
    l = strlen(req);
#ifdef TESTING
    fprintf(stderr,"len=%ld l=%ld eoh=%d\n",len,l,eoh);
#endif
    while(l < len-eoh) {
        size_t s;
#ifdef TESTING
        fprintf(stderr,"tail=%ld\n",len-eoh-l);
#endif
        if(l > len-2*eoh-14) { /* length "Cookie: idX=Y" */
            if(i) { 
                s = len - l + 2; // override EOL
                get_random_str(rnd_data,s,sizeof(rnd_data)-1);
                strncpy(&req[l-2],rnd_data,s); // override EOL
#ifdef TESTING
                fprintf(stderr,"2: l=%ld s=%ld\n",l,s);
#endif
            } else {
                while(l > 1 && ( req[l-1] == 012 || req[l-1] == 015) ) req[--l]= '\0';
#ifdef TESTING
                fprintf(stderr,"3: l=%ld s=%ld\n",l,len-(2*eoh)-l);
#endif
                while(l < len-(2*eoh)) req[l++] = ' ';
            }
            l = len - (2*eoh);
            if(eoh) {
                req[l++] = '\015';
                req[l++] = '\012';
            }
        } else {
            get_random_str(rnd_data,len-l-2*eoh,sizeof(rnd_data)-1);
            snprintf(req_header,sizeof(req_header)-1,"Cookie: id%d=%s",i,rnd_data);
            s = min_val(strlen(req_header),len-l-2*eoh);
            memcpy(&req[l],req_header,s);
#ifdef TESTING
            fprintf(stderr,"1: l=%ld s=%ld\n",l,s);
#endif
            l += s;
            i++;
            if(l < len-2*eoh) {
                req[l++] = '\015';
                req[l++] = '\012';
            }
        }
    }
    if(eoh) {
        req[l++] = '\015';
        req[l++] = '\012';
    }
    req[l] = '\0'; // needed for printing
#ifdef TESTING
    fprintf(stderr,"l=%ld\n",l);
#endif
    return req;
}

static void update_req_par(char **field,const char * const old,const char * const orig) {
    char *tmp = *field;
    if(!tmp) {
        *field = strdup(old);
        return;
    }
    if(!strcmp(tmp,old)) return;
    char *new = strdup(old);
    if(new) {
        if(tmp != orig) free(tmp);
        *field = new;
    }
}

struct http_req_param * gen_http_parser(struct http_req_param *http,const char * const opts) {
    char *topt,*c,*d = NULL,*e = NULL;
    int err;

    topt = strdup(opts);
    if(!topt) return NULL;
    c = topt;
    err = 0;
    while(*c) {
        err = 3;
        d = strchr(c,'=');
        if(!d) break;
        *d++ = 0;
        if(!*d) break;

        err = 2;

        if(*d == '"' || *d == '\'') {
            e = strchr(d+1,*d);
            if(!e) break;
            *e = '\0';
            if(e[1] == ';') e+=2;
                else if(!e[1]) e++;
                    else break;
            d++;
        } else {
            e = strchr(d,';');
            if(!e) e=d+strlen(d);
                else
                    *e++ = '\0';
        }
#define UPDPAR(a) update_req_par(&http->a,d,http_req_default.a)
        if(!strcmp(c,"method"))      UPDPAR(method);
        else if(!strcmp(c,"path"))   UPDPAR(path);
        else if(!strcmp(c,"vers"))   UPDPAR(vers);
        else if(!strcmp(c,"host"))   UPDPAR(host);
        else if(!strcmp(c,"ua"))     UPDPAR(ua);
        else if(!strcmp(c,"accept")) UPDPAR(accept);
        else if(!strcmp(c,"eoh"))    http->eoh = *d == '1';
#undef UPDPAR
        else {
            err = 1; break;
        }
        err = 0;
        c = e;
    }
#ifdef TESTING
    if(err == 1) fprintf(stderr,"gen_http: unknown option '%s'='%s'\n",c,d);
    if(err == 2) fprintf(stderr,"gen_http: bad option '%s'='%s'\n",c,d);
    if(err == 3) fprintf(stderr,"gen_http: bad option '%s'\n",c);
#else
    if(err == 1) LOG(LOG_E, "gen_http: unknown option '%s'='%s'\n",c,d);
    if(err == 2) LOG(LOG_E, "gen_http: bad option '%s'='%s'\n",c,d);
    if(err == 3) LOG(LOG_E, "gen_http: bad option '%s'\n",c);
#endif
    free(topt);
    return err ? NULL : http;
}

#ifdef TESTING 
int main(int argc,char **argv) {
    char *r;
    int rl = 512;
    struct http_req_param *http = gen_http_init();
    if(!argv[1] || !argv[2]) exit(1);
    if(argv[2]) {
        if(!gen_http_parser(http,argv[2])) abort();
    }
    if(argv[1]) rl = atoi(argv[1]);
    r = gen_http_req(http,rl);
    puts(r);
//    r = gen_http_req(http,rl);
//    puts(r);
    gen_http_free(http);
}
#endif

/*
 * vim: set ts=4 sw=4 sts=4 et : 
 */

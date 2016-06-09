#include "git-compat-util.h"
#include "strbuf.h"
#include "parse-options.h"
#include "builtin.h"

int main(int argc, const char *argv[])
{
    int i,j,size = 0, nb = 0;

    if (!strcmp(argv[2], "1")) {
        nb = 100;
        size = 2;
    } else if (!strcmp(argv[2], "2")) {
        nb = 500;
        size = 2;
    } else if (!strcmp(argv[2], "3")) {
        nb = 1000;
        size = 2;
    } else if (!strcmp(argv[2], "4")) {
        nb = 100;
        size = 32;
    } else if (!strcmp(argv[2], "5")) {
        nb = 500;
        size = 32;
    } else if (!strcmp(argv[2], "6")) {
        nb = 1000;
        size = 32;
    } else if (!strcmp(argv[2], "7")) {
        nb = 100;
        size = 100;
    } else if (!strcmp(argv[2], "8")) {
        nb = 500;
        size = 100;
    } else if (!strcmp(argv[2], "9")) {
        nb = 1000;
        size = 100;
    }
     char chaine[size];
     for (i  = 0; i < size; i++ ) {
        strcat(chaine,"a");
     }
    if (!strcmp(argv[1], "1")) {
        struct strbuf buf = STRBUF_INIT_ON_STACK(100000);
        //struct strbuf buf = STRBUF_INIT;
        for (i = 0; i < 1000000; i++) {
            for (j = 0; j < nb; j++)
                strbuf_add(&buf, chaine, size);
            strbuf_setlen(&buf, 0);
        }
        strbuf_release(&buf);
    } else if (!strcmp(argv[1],"2")){
        struct strbuf buf = STRBUF_INIT;
        //struct strbuf buf = STRBUF_INIT;
        for (i = 0; i < 1000000; i++) {
            for (j = 0; j < nb; j++)
                strbuf_add(&buf, chaine, size);
            strbuf_setlen(&buf, 0);
        }
        strbuf_release(&buf);
    } else  if (!strcmp(argv[1],"3")) {
        struct strbuf buf;
        strbuf_init(&buf, 32);
        //struct strbuf buf = STRBUF_INIT;
        for (i = 0; i < 1000000; i++) {
            for (j = 0; j < nb; j++)
                strbuf_add(&buf, chaine, size);
            strbuf_setlen(&buf, 0);
        }
        strbuf_release(&buf);
    } else  if (!strcmp(argv[1],"4")) {
        struct strbuf buf = STRBUF_INIT;
        strbuf_attach(&buf, NULL, 0, 33 );
        //struct strbuf buf = STRBUF_INIT;
        for (i = 0; i < 1000000; i++) {
            for (j = 0; j < nb; j++)
                strbuf_add(&buf, chaine, size);
            strbuf_setlen(&buf, 0);
        }
        /* No release here */
    }
    return 0;
}

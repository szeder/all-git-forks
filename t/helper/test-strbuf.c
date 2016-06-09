#include "git-compat-util.h"
#include "strbuf.h"
#include "parse-options.h"
#include "builtin.h"

int main(int argc, const char *argv[])
{
    int i = 0, j = 0;

    struct strbuf buf = STRBUF_INIT_ON_STACK(100000);
    //struct strbuf buf = STRBUF_INIT;
    for (i = 0; i < 1000000; i++) {
        for (j = 0; j < 1000; j++)
            strbuf_add(&buf, "lolilolololololololololololololo", 32);
        strbuf_setlen(&buf, 0);
    }
    strbuf_release(&buf);

    return 0;
}

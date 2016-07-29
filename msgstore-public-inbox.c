#include "git-compat-util.h"
#include <curl/curl.h>

#include "strbuf.h"


static void fetch_message(const char *listname, char *message_id)
{
	CURL *curl = curl_easy_init();
	struct strbuf url = STRBUF_INIT;
	CURLcode res;
	char *redirect;

	if (!curl)
		die("failed to init curl");

	strbuf_addf(&url, "http://public-inbox.org/%s/%s/raw",
			listname, message_id);
	curl_easy_setopt(curl, CURLOPT_URL, url.buf);
	curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);

	res = curl_easy_perform(curl);
	if (res != CURLE_OK)
		die("%s", curl_easy_strerror(res));

	curl_easy_cleanup(curl);
}

int main(int argc, char *argv[])
{
	if (argc != 3) {
		fprintf(stderr, "usage: %s <list> <message-id>\n", argv[0]);
		exit(1);
	}

	curl_global_init(CURL_GLOBAL_ALL);

	fetch_message(argv[1], argv[2]);

	curl_global_cleanup();
	return 0;
}

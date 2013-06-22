#include "git-compat-util.h"
#include <curl/curl.h>

#include "strbuf.h"


static void fetch_message(char *message_id)
{
	CURL *curl = curl_easy_init();
	struct strbuf url = STRBUF_INIT;
	CURLcode res;
	char *redirect;

	if (!curl)
		die("failed to init curl");

	/* First request the message by ID to find out Gmane's name for it. */
	strbuf_addf(&url, "http://mid.gmane.org/%s", message_id);
	curl_easy_setopt(curl, CURLOPT_URL, url.buf);
	curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 0L);

	res = curl_easy_perform(curl);
	if (res != CURLE_OK)
		die("%s", curl_easy_strerror(res));

	/* Now append "/raw" to Gmane's canonical URL for the message. */
	strbuf_reset(&url);
	res = curl_easy_getinfo(curl, CURLINFO_REDIRECT_URL, &redirect);
	if (res != CURLE_OK)
		die("%s", curl_easy_strerror(res));
	strbuf_addf(&url, "%s/raw", redirect);

	curl_easy_setopt(curl, CURLOPT_URL, url.buf);
	curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);

	res = curl_easy_perform(curl);
	if (res != CURLE_OK)
		die("%s", curl_easy_strerror(res));

	curl_easy_cleanup(curl);
}

int main(int argc, char *argv[])
{
	if (argc != 2) {
		fprintf(stderr, "usage: %s <message-id>\n", argv[0]);
		exit(1);
	}

	curl_global_init(CURL_GLOBAL_ALL);

	fetch_message(argv[1]);

	curl_global_cleanup();
	return 0;
}

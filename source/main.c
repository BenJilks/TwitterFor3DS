#include <stdio.h>
#include <stdlib.h>
#include <3ds.h>
#include "ezxml.h"

void ask_input(const char *msg, char *buffer, int buffer_size)
{
    static SwkbdState swkdb;

    swkbdInit(&swkdb, SWKBD_TYPE_WESTERN, 1, -1);
    swkbdSetValidation(&swkdb, SWKBD_NOTEMPTY_NOTBLANK, SWKBD_FILTER_DIGITS, 2);
    swkbdSetFeatures(&swkdb, SWKBD_MULTILINE);
    swkbdSetHintText(&swkdb, msg);
    swkbdInputText(&swkdb, buffer, buffer_size);
}

Result getRequest(const char *url, u8 **output, u32 *out_size)
{
    httpcContext context;
    Result ret;
    char *new_url = NULL;
    u32 status_code = 0;
    u32 content_size = 0;
    u32 read_size = 0;
    u32 size = 0;
    u8 *buf, *last_buf;

    printf("Downloading %s\n", url);

    do {
        ret = httpcOpenContext(&context, HTTPC_METHOD_GET, url, 1);
        printf("return from httpcOpenContext: %li\n",ret);

        // This disables SSL cert verification, so https:// will be usable
        ret = httpcSetSSLOpt(&context, SSLCOPT_DisableVerify);
        printf("return from httpcSetSSLOpt: %li\n",ret);

        // Enable Keep-Alive connections
        ret = httpcSetKeepAlive(&context, HTTPC_KEEPALIVE_ENABLED);
        printf("return from httpcSetKeepAlive: %li\n",ret);

        // Set a User-Agent header so websites can identify your application
        ret = httpcAddRequestHeaderField(&context, "User-Agent", "httpc-example/1.0.0");
        printf("return from httpcAddRequestHeaderField (set user agent): %li\n",ret);

        // Tell the server we can support Keep-Alive connections.
        // This will delay connection teardown momentarily (typically 5s)
        // in case there is another request made to the same server.
        ret = httpcAddRequestHeaderField(&context, "Connection", "Keep-Alive");
        printf("return from httpcAddRequestHeaderField (set connection): %li\n",ret);

        httpcAddRequestHeaderField(&context, "oauth_token", "4818940900-GKADNwqAUnwjZ9K26irjXwZM8MhNETw2oK7Dilr");

        ret = httpcBeginRequest(&context);
        if(ret != 0)
        {
            printf("Error: unable to begin request :(\n");
            httpcCloseContext(&context);
            return ret;
        }

        ret = httpcGetResponseStatusCode(&context, &status_code);
        if(ret != 0)
        {
            printf("Error: invalid response :(\n");
            httpcCloseContext(&context);
            return ret;
        }

		// This download loop resizes the buffer as data is read.
        if ((status_code >= 301 && status_code <= 303) || (status_code >= 307 && status_code <= 308)) 
        {
                // One 4K page for new URL
                if(new_url == NULL) 
                    new_url = (char*)malloc(0x1000);
                
                if (new_url == NULL)
                {
                    printf("Error reading redirect url :(\n");
                    httpcCloseContext(&context);
                    return -1;
                }

                ret = httpcGetResponseHeader(&context, "Location", new_url, 0x1000);
                url = new_url; // Change pointer to the url that we just learned
                printf("redirecting to url: %s\n", url);
                httpcCloseContext(&context); // Close this context before we try the next
		}
	} while ((status_code >= 301 && status_code <= 303) || (status_code >= 307 && status_code <= 308));

    if(status_code != 200)
    {
		printf("URL returned status: %lu\n", status_code);
		httpcCloseContext(&context);
		if(new_url != NULL) 
            free(new_url);
		return -2;
	}

    printf("reported size: %lu\n",content_size);

    // Start with a single page buffer
    buf = (u8*)malloc(0x1000);
    do {
		// This download loop resizes the buffer as data is read.
		ret = httpcDownloadData(&context, buf + size, 0x1000, &read_size);
		size += read_size; 
        printf("Downloaded %li\n", size);
		if (ret == (s32) HTTPC_RESULTCODE_DOWNLOADPENDING)
        {
            last_buf = buf; // Save the old pointer, in case realloc() fails.
            buf = (u8*)realloc(buf, size + 0x1000);
            if(buf == NULL)
            { 
                httpcCloseContext(&context);
                free(last_buf);
                if(new_url != NULL) 
                    free(new_url);
                return -1;
            }
        }
	} while (ret == (s32) HTTPC_RESULTCODE_DOWNLOADPENDING);

    if(ret!=0)
    {
		httpcCloseContext(&context);
		if(new_url != NULL) 
            free(new_url);
		free(buf);
		return -1;
	}

    // Resize the buffer back down to our actual final size
	last_buf = buf;
	buf = (u8*)realloc(buf, size);
    printf("downloaded size: %lu\n", size);
    
    httpcCloseContext(&context);
	if (new_url != NULL) 
        free(new_url);
    
    *output = buf;
    *out_size = size;
    return 0;
}

void search_node(ezxml_t node)
{
    printf("Searching node %s\n", node->name);

    int i;
    for (i = 0;;i+=2)
    {
        char *name = node->attr[i];
        char *value = node->attr[i+1];
        if (name == NULL)
            break;
        
        printf(" => attr %s = %s\n", name, value);
        if (!strcmp(name, "class") && !strcmp(value, "ProfileHeaderCard-nameLink u-textInheritColor js-nav"))
        {
            printf("%s\n", node->txt);
        }
    }

    if (node->next != NULL)
        search_node(node->next);
    
    if (node->sibling != NULL)
        search_node(node->sibling);
    
    if (node->child != NULL)
        search_node(node->child);
}

int main()
{
    gfxInitDefault();
    consoleInit(GFX_TOP, NULL);
    httpcInit(0);

    u8 *src;
    u32 size;
    if (getRequest("http://google.com", &src, &size) == 0)
    {
        ezxml_t doc = ezxml_parse_str(src, size);
        search_node(doc);
        
        ezxml_free(doc);
        free(src);
        printf("Finished parsing twitter page\n");
    }
    else
    {
        printf("Error loading twitter page :(\n");
    }
    
    printf("\n\n              Press Start to exit.\n");
    while (aptMainLoop())
    {
        hidScanInput();
        u32 k_down = hidKeysDown();
        if (k_down & KEY_START)
            break;

        gfxFlushBuffers();
        gfxSwapBuffers();
        gspWaitForVBlank();
    }

    gfxExit();
    return 0;
}

/***************************************************************************
 *                                  _   _ ____  _
 *  Project                     ___| | | |  _ \| |
 *                             / __| | | | |_) | |
 *                            | (__| |_| |  _ <| |___
 *                             \___|\___/|_| \_\_____|
 *
 * Copyright (C) 1998 - 2017, Daniel Stenberg, <daniel@haxx.se>, et al.
 *
 * This software is licensed as described in the file COPYING, which
 * you should have received as part of this distribution. The terms
 * are also available at https://curl.haxx.se/docs/copyright.html.
 *
 * You may opt to use, copy, modify, merge, publish, distribute and/or sell
 * copies of the Software, and permit persons to whom the Software is
 * furnished to do so, under the terms of the COPYING file.
 *
 * This software is distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY
 * KIND, either express or implied.
 *
 ***************************************************************************/
/* <DESC>
 * using the multi interface to do a multipart formpost without blocking
 * </DESC>
 */

#include <stdio.h>
#include <string.h>
#include <sys/time.h>

#include <curl/curl.h>

int main(void)
{
  CURL *curl;
  FILE *fp;
  CURLM *multi_handle;
  //int still_running;

  curl_mime *form = NULL;
  curl_mimepart *field = NULL;
  struct curl_slist *headerlist = NULL;
  static const char buf[] = "Expect:";
  char buf1[2000000] = {1,1,0,1,};
  char *buf2[3]={buf1, buf1, buf1};
  curl_global_init(CURL_GLOBAL_ALL);
  
  int i = 0;
  
  if((fp = (FILE*) fopen("test.pcm", "rb") ) != NULL)
  {
        int ret = 0;
        ret = fread(buf1, 2000000, 1, fp);
        printf("fread %d (bytes)\n", ret * 20000);
  }
	  
  for (i=0;i<3;i++)
{
	  int still_running;
	  curl = curl_easy_init();
	  multi_handle = curl_multi_init();
	printf("curl: %d\n", curl?"not null":"null");
	printf("multi_handle: %d\n", multi_handle?"not null":"null");

	  if(curl && multi_handle) {
	    /* Create the form */
	    form = curl_mime_init(curl);

	    /* Fill in the file upload field */
	    field = curl_mime_addpart(form);
	    curl_mime_name(field, "datetime");
	    curl_mime_data(field, "1525938089028099", CURL_ZERO_TERMINATED);

	    /* Fill in the filename field */
	    field = curl_mime_addpart(form);
	    curl_mime_name(field, "deviceId");
	    curl_mime_data(field, "0302041802000244", CURL_ZERO_TERMINATED);

#if 1
	    /* Fill in the submit field too, even if this is rarely needed */
	    field = curl_mime_addpart(form);
	    curl_mime_name(field, "mics");
	    curl_mime_filename(field, "mics.pcm");
	    curl_mime_data(field,buf2[i] , sizeof(buf1));
	    //#define FILE_CONTENTTYPE_DEFAULT        "application/octet-stream"
	    curl_mime_type(field, "application/octet-stream");
	    //curl_mime_data(field, buf1, sizeof(buf1));
#endif

	    /* initialize custom header list (stating that Expect: 100-continue is not
	       wanted */
	    headerlist = curl_slist_append(headerlist, buf);
	    headerlist = curl_slist_append(headerlist, "Content-Type: multipart/form-data");
	    
	    /* what URL that receives this POST */
	    curl_easy_setopt(curl, CURLOPT_URL, "http://10.88.10.48:8080/events/vt");
	    curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);

	    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headerlist);
	    curl_easy_setopt(curl, CURLOPT_MIMEPOST, form);
	    
	/* size of the POST data */ /* curl_mime_data_cb */
	  //curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE_LARGE, sizeof(buf1));
	 
	  /* pass in a pointer to the data - libcurl will not copy */
	  //curl_easy_setopt(curl, CURLOPT_POSTFIELDS, buf1);


	    curl_multi_add_handle(multi_handle, curl);

	    curl_multi_perform(multi_handle, &still_running);
	   printf("still_running: %d\n", still_running);
	    do {
	      struct timeval timeout;
	      int rc; /* select() return code */
	      CURLMcode mc; /* curl_multi_fdset() return code */

	      fd_set fdread;
	      fd_set fdwrite;
	      fd_set fdexcep;
	      int maxfd = -1;

	      long curl_timeo = -1;

	      FD_ZERO(&fdread);
	      FD_ZERO(&fdwrite);
	      FD_ZERO(&fdexcep);

	      /* set a suitable timeout to play around with */
	      timeout.tv_sec = 1;
	      timeout.tv_usec = 0;

	      curl_multi_timeout(multi_handle, &curl_timeo);
	      if(curl_timeo >= 0) {
	        timeout.tv_sec = curl_timeo / 1000;
	        if(timeout.tv_sec > 1)
	          timeout.tv_sec = 1;
	        else
	          timeout.tv_usec = (curl_timeo % 1000) * 1000;
	      }

	      /* get file descriptors from the transfers */
	      mc = curl_multi_fdset(multi_handle, &fdread, &fdwrite, &fdexcep, &maxfd);

	      if(mc != CURLM_OK) {
	        fprintf(stderr, "curl_multi_fdset() failed, code %d.\n", mc);
	        break;
	      }

	      /* On success the value of maxfd is guaranteed to be >= -1. We call
	         select(maxfd + 1, ...); specially in case of (maxfd == -1) there are
	         no fds ready yet so we call select(0, ...) --or Sleep() on Windows--
	         to sleep 100ms, which is the minimum suggested value in the
	         curl_multi_fdset() doc. */

	      if(maxfd == -1) {
#ifdef _WIN32
	        Sleep(100);
	        rc = 0;
#else
	        /* Portable sleep for platforms other than Windows. */
	        struct timeval wait = { 0, 100 * 1000 }; /* 100ms */
	        rc = select(0, NULL, NULL, NULL, &wait);
#endif
	      }
	      else {
	        /* Note that on some platforms 'timeout' may be modified by select().
	           If you need access to the original value save a copy beforehand. */
	        rc = select(maxfd + 1, &fdread, &fdwrite, &fdexcep, &timeout);
	      }

	      switch(rc) {
	      case -1:
	        /* select error */
	        break;
	      case 0:
	      default:
	        /* timeout or readable/writable sockets */
	        printf("perform!\n");
	        curl_multi_perform(multi_handle, &still_running);
	        printf("running: %d!\n", still_running);
	        break;
	      }
	    } while(still_running);

	    curl_multi_cleanup(multi_handle);

	    /* always cleanup */
	    curl_easy_cleanup(curl);
	
	}
	else
	{
		printf("error!!!!!!!!!!!!!!!!!\n");
	}
	  //curl_easy_reset(multi_handle);
}


    /* then cleanup the form */
    curl_mime_free(form);

    /* free slist */
    curl_slist_free_all(headerlist);


  if (fp)
  {
        fclose(fp);
  }
  return 0;
}

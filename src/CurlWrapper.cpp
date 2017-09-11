#include "CurlWrapper.h"

namespace modworks
{
  map<CURL*, JsonResponseHandler*> ongoing_calls;

  int call_count = 0;
  int ongoing_call = 0;

  void initCurl()
  {
    if(curl_global_init(CURL_GLOBAL_ALL))
      writeLogLine("Curl initialized", verbose);
    else
      writeLogLine("Error initializing curl", error);
  }

  int getCallCount()
  {
    return call_count;
  }

  int getOngoingCall()
  {
    return ongoing_call;
  }

  void advanceCallCount()
  {
    call_count++;
  }

  void advanceOngoingCall()
  {
    ongoing_call++;
  }

  void lockCall(int call_number)
  {
    while(call_number!=getOngoingCall());
  }

  JsonResponseHandler::JsonResponseHandler()
  {
    this->response = "";
  }

  struct data
  {
    char trace_ascii; /* 1 or 0 */
  };

  int json_response_trace(CURL *handle, curl_infotype type,
               char *data, size_t size,
               void *userp)
  {
    (void)handle; /* prevent compiler warning */
    if(type == CURLINFO_DATA_IN)
    {
      cout<<endl<<"IIIIIIIIIIIIIIIIIIIIIIIIKKKK"<<endl<<endl;
      ongoing_calls[handle]->response.append(data, size);
      //ongoing_calls[handle]->response += data;
      cout<<endl<<"AAAAAAAAAAAAAAAAAAAAAAXXXXX"<<endl<<endl;
    }

    return 0;
  }

  void get(int call_number, string url, vector<string> headers, function<void(int call_number, json response)> callback)
  {
    writeLogLine("getJsonCall call to " + url, verbose);
    lockCall(call_number);
    CURL *curl;
    CURLcode res;

    struct data config;
    curl_global_init(CURL_GLOBAL_DEFAULT);

    curl = curl_easy_init();

    ongoing_calls[curl] = new JsonResponseHandler();
    if(curl) {
      struct curl_slist *chunk = NULL;
      for(int i=0;i<(int)headers.size();i++)
        chunk = curl_slist_append(chunk, headers[i].c_str());

      curl_easy_setopt(curl, CURLOPT_HTTPHEADER, chunk);

      curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
      curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
      curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);

      curl_easy_setopt(curl, CURLOPT_DEBUGFUNCTION, json_response_trace);
      curl_easy_setopt(curl, CURLOPT_DEBUGDATA, &config);
      curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);
      /* Perform the request, res will get the return code */
      res = curl_easy_perform(curl);
      /* Check for errors */
      if(res != CURLE_OK)
        writeLogLine(string("curl_easy_perform() failed ") + url, verbose);
      /* always cleanup */
      curl_easy_cleanup(curl);
    }

    curl_global_cleanup();
    cout<<endl<<endl<<"-!>"<<ongoing_calls[curl]->response<<"<!-"<<endl<<endl<<endl;
    ongoing_calls[curl]->response = dataToJsonString(ongoing_calls[curl]->response);
    cout<<endl<<endl<<"->"<<ongoing_calls[curl]->response<<"<-"<<endl<<endl<<endl;
    json json_response = json::parse(ongoing_calls[curl]->response);

    callback(call_number, json_response);
    advanceOngoingCall();
    writeLogLine("getJsonCall call to " + url + "finished", verbose);
  }

  size_t write_data(void *ptr, size_t size, size_t nmemb, void *stream)
  {
    size_t written = fwrite(ptr, size, nmemb, (FILE *)stream);
    return written;
  }

  static int download_file_trace(CURL *handle, curl_infotype type,
               char *data, size_t size,
               void *userp)
  {
    (void)handle; /* prevent compiler warning */

    if(type == CURLINFO_DATA_IN)
    {
      //BYTES_DOWNLOADED+=size;
    }
    return 0;
  }


  void download(int call_number, string url, string path, function< void(int, int, string, string) > callback)
  {
    writeLogLine("downloadFile call to " + url, verbose);
    lockCall(call_number);
    CURL *curl;
    FILE *file;

    struct data config;

    config.trace_ascii = 1; /* enable ascii tracing */
    //FILE_SIZE = curlGetFileSize(url);
    curl = curl_easy_init();
    if(curl)
    {
      curl_easy_setopt(curl, CURLOPT_DEBUGFUNCTION, download_file_trace);
      curl_easy_setopt(curl, CURLOPT_DEBUGDATA, &config);
      curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);

      file = fopen(path.c_str(),"wb");
      curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
      curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
      curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);

      curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);

      curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_data);
      curl_easy_setopt(curl, CURLOPT_WRITEDATA, file);

      curl_easy_perform(curl);

      curl_easy_cleanup(curl);

      fclose(file);
    }
    callback(call_number, 200, url, path);
    advanceOngoingCall();
    writeLogLine("getJsonCall call to " + url + " finished", verbose);
  }

  void postForm(int call_number, string url, vector<string> headers, map<string, string> curlform_copycontents, map<string, string> curlform_files, function<void(int call_number, json response)> callback)
  {
    writeLogLine(string("postForm call to ") + url, verbose);
    lockCall(call_number);
    CURL *curl;
    CURLcode res;

    struct curl_httppost *formpost=NULL;
    struct curl_httppost *lastptr=NULL;
    struct curl_slist *headerlist=NULL;
    static const char buf[] = "Expect:";

    curl_global_init(CURL_GLOBAL_ALL);

    for(map<string,string>::iterator i = curlform_files.begin();
          i!=curlform_files.end();
          i++)
    {
      curl_formadd(&formpost, &lastptr, CURLFORM_COPYNAME, (*i).first.c_str(),
        CURLFORM_FILE, (*i).second.c_str(), CURLFORM_END);
    }

    for(map<string,string>::iterator i = curlform_copycontents.begin();
          i!=curlform_copycontents.end();
          i++)
    {
      curl_formadd(&formpost, &lastptr, CURLFORM_COPYNAME, (*i).first.c_str(),
        CURLFORM_COPYCONTENTS, (*i).second.c_str(), CURLFORM_END);
    }

    /* Fill in the submit field too, even if this is rarely needed */
    curl_formadd(&formpost,
                 &lastptr,
                 CURLFORM_COPYNAME, "submit",
                 CURLFORM_COPYCONTENTS, "send",
                 CURLFORM_END);

    curl = curl_easy_init();

    ongoing_calls[curl] = new JsonResponseHandler();

    struct curl_slist *chunk = NULL;
    for(int i=0;i<(int)headers.size();i++)
      chunk = curl_slist_append(chunk, headers[i].c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, chunk);
    /* initialize custom header list (stating that Expect: 100-continue is not
       wanted */
    headerlist = curl_slist_append(headerlist, buf);
    if(curl)
    {
      /* what URL that receives this POST */
      curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
      curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
      curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);

      curl_easy_setopt(curl, CURLOPT_DEBUGFUNCTION, json_response_trace);

      curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);
      //if((argc == 2) && (!strcmp(argv[1], "noexpectheader")))
        /* only disable 100-continue header if explicitly requested */
        //curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headerlist);
      curl_easy_setopt(curl, CURLOPT_HTTPPOST, formpost);
      res = curl_easy_perform(curl);

      if(res != CURLE_OK)
        writeLogLine(string("curl_easy_perform() failed: ") + curl_easy_strerror(res), error);

      curl_easy_cleanup(curl);
      curl_formfree(formpost);
      curl_slist_free_all(headerlist);
    }

    ongoing_calls[curl]->response = dataToJsonString(ongoing_calls[curl]->response);
    json json_response = json::parse(ongoing_calls[curl]->response);

    callback(call_number, json_response);
    advanceOngoingCall();
    writeLogLine(string("postForm call to ") + url + " finished", verbose);
  }

  void post(int call_number, string url, map<string, string> data, function<void(int call_number, json response)> callback)
  {
    writeLogLine(string("post call to ") + url, verbose);
    lockCall(call_number);

    CURL *curl;
    CURLcode res;
    curl_global_init(CURL_GLOBAL_ALL);
    curl = curl_easy_init();

    ongoing_calls[curl] = new JsonResponseHandler();

    if(curl)
    {
      curl_easy_setopt(curl, CURLOPT_URL, url.c_str());

      string str_data = "";
      for(map<string, string>::iterator i = data.begin(); i!=data.end(); i++)
      {
        if(i!=data.begin())
          str_data += "&";
        str_data += (*i).first + "=" + (*i).second;
      }

      curl_easy_setopt(curl, CURLOPT_POSTFIELDS, str_data.c_str());
      curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
      curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);

      curl_easy_setopt(curl, CURLOPT_DEBUGFUNCTION, json_response_trace);

      curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);

      res = curl_easy_perform(curl);

      if(res != CURLE_OK)
      {
        writeLogLine(string("curl_easy_perform() failed: ") + curl_easy_strerror(res), error);
        //callback(-1);
      }

      curl_easy_cleanup(curl);
    }
    curl_global_cleanup();

    ongoing_calls[curl]->response = dataToJsonString(ongoing_calls[curl]->response);
    json json_response = json::parse(ongoing_calls[curl]->response);

    callback(call_number, json_response);
    advanceOngoingCall();
    writeLogLine(string("post call to ") + url + " finished", verbose);
  }
}

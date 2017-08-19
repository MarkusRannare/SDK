#include "CurlWrapper.h"

map<CURL*, GetJsonHandler*> ongoing_calls;
map<CURL*, DownloadRedirectHandler*> download_redirect_calls;

int call_count = 0;
int ongoing_call = 0;

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

GetJsonHandler::GetJsonHandler(function< void(vector<Mod*>) > callback)
{
  this->response = "";
  this->callback = callback;
}

DownloadFileHandler::DownloadFileHandler(function< void(int, Mod*) > callback)
{
  this->callback = callback;
}

DownloadRedirectHandler::DownloadRedirectHandler(Mod* mod, string path, string destination_path, function< void(int, Mod*, string) > callback, int call_number)
{
  this->mod = mod;
  this->path = path;
  this->callback = callback;
  this->destination_path = destination_path;
  this->call_number = call_number;
}

struct data
{
  char trace_ascii; /* 1 or 0 */
};

int get_json_trace(CURL *handle, curl_infotype type,
             char *data, size_t size,
             void *userp)
{
  (void)handle; /* prevent compiler warning */

  if(type == CURLINFO_DATA_IN)
  {
    ongoing_calls[handle]->response = dataToJsonString(data, size);
  }

  return 0;
}

void getJson(string url, vector<string> headers, function< void(vector<Mod*>) > callback, int call_number)
{
  lockCall(call_number);
  CURL *curl;
  CURLcode res;

  struct data config;

  curl_global_init(CURL_GLOBAL_DEFAULT);

  curl = curl_easy_init();

  ongoing_calls[curl] = new GetJsonHandler(callback);

  if(curl) {
    struct curl_slist *chunk = NULL;
    for(int i=0;i<(int)headers.size();i++)
      chunk = curl_slist_append(chunk, headers[i].c_str());

    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, chunk);

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_DEBUGFUNCTION, get_json_trace);
    curl_easy_setopt(curl, CURLOPT_DEBUGDATA, &config);
    curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);

    /* Perform the request, res will get the return code */
    res = curl_easy_perform(curl);
    /* Check for errors */
    if(res != CURLE_OK)
      fprintf(stderr, "curl_easy_perform() failed: %s\n",
              curl_easy_strerror(res));

    /* always cleanup */
    curl_easy_cleanup(curl);
  }

  curl_global_cleanup();
  string json_string = ongoing_calls[curl]->response;
  json json_response = json::parse(json_string);

  vector<Mod*> mods;
  for(int i=0;i<(int)json_response["data"].size();i++)
  {
    Mod* mod = new Mod(json_response["data"][i]);
    mods.push_back(mod);
  }
  ongoing_calls[curl]->callback(mods);
  advanceOngoingCall();
}

double curlGetFileSize(string url)
{
  CURL *curl;
  curl = curl_easy_init();

  curl_easy_setopt(curl, CURLOPT_HEADER, 1);
  curl_easy_setopt(curl, CURLOPT_NOBODY, 1);
  curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
  curl_easy_perform(curl);

  double result;
  curl_easy_getinfo(curl, CURLINFO_CONTENT_LENGTH_DOWNLOAD, &result);
  curl_easy_cleanup(curl);

  return result;
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

static int redirect_trace(CURL *handle, curl_infotype type,
             char *data, size_t size,
             void *userp)
{
  (void)handle; /* prevent compiler warning */
  if(type == CURLINFO_HEADER_IN
    && size > 10 /* "location: " */
    && strncmp(data,"location: ",10) == 0)
  {
    string url = "";
    for(int i = 10; i< (int)size; i++)
    {
      if(data[i]=='\n' || data[i]=='\t' || data[i]==' ' || data[i]==13 /* CR */)
        break;
      url += data[i];
    }

    DownloadRedirectHandler* handler = download_redirect_calls[handle];
    downloadZipFile(handler->mod, url, handler->path, handler->destination_path, handler->callback, handler->call_number);
  }
  return 0;
}

size_t write_data(void *ptr, size_t size, size_t nmemb, void *stream)
{
  size_t written = fwrite(ptr, size, nmemb, (FILE *)stream);
  return written;
}

void downloadFile(string url, string path)
{
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
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_data);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, file);

    curl_easy_perform(curl);

    curl_easy_cleanup(curl);

    fclose(file);
  }
}

void downloadModFile(Mod* mod, string url, string path, function< void(int, Mod*, string) > callback, int call_number)
{
  lockCall(call_number);
  downloadFile(url, path);
  callback(1,mod,path);
  advanceOngoingCall();
}

void downloadRedirect(Mod* mod, string url, string path, string destination_path, function< void(int, Mod*, string) > callback, int call_number)
{
  lockCall(call_number);

  CURL *curl;

  struct data config;

  config.trace_ascii = 1; /* enable ascii tracing */
  //FILE_SIZE = curlGetFileSize(url);
  curl = curl_easy_init();

  if(curl)
  {
    download_redirect_calls[curl] = new DownloadRedirectHandler(mod, path, destination_path, callback, call_number);

    curl_easy_setopt(curl, CURLOPT_DEBUGFUNCTION, redirect_trace);
    curl_easy_setopt(curl, CURLOPT_DEBUGDATA, &config);
    curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());

    curl_easy_perform(curl);

    curl_easy_cleanup(curl);
  }
}

void downloadZipFile(Mod* mod, string url, string path, string destination, function< void(int, Mod*, string) > callback, int call_number)
{
  lockCall(call_number);
  downloadFile(url, path);
  extract(path, destination);
  callback(1,mod,path);
  advanceOngoingCall();
}

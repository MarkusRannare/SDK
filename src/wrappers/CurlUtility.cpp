#include "wrappers/CurlUtility.h"

namespace modio
{
namespace curlwrapper
{
CURLM *curl_multi_handle;

std::map<CURL *, JsonResponseHandler *> ongoing_calls;
std::map<CURL *, OngoingDownload *> ongoing_downloads;

std::list<QueuedModDownload *> mod_download_queue;
std::list<QueuedModfileUpload *> modfile_upload_queue;

FILE *current_mod_download_file;

CURL *current_mod_download_curl_handle;
CURL *current_modfile_upload_curl_handle;

struct curl_slist *current_mod_download_slist;
struct curl_slist *current_modfile_upload_slist;
struct curl_httppost *current_modfile_upload_httppost;

QueuedModDownload *current_queued_mod_download;
QueuedModfileUpload *current_queued_modfile_upload;
CurrentDownloadHandle *current_download_handle;

u32 call_count;
u32 ongoing_call;

std::list<QueuedModDownload *> getModDownloadQueue()
{
  return mod_download_queue;
}

std::list<QueuedModfileUpload *> getModfileUploadQueue()
{
  return modfile_upload_queue;
}

#ifdef MODIO_WINDOWS_DETECTED
  JsonResponseHandler::JsonResponseHandler(u32 call_number, struct curl_slist * slist, curl_mime *mime_form, std::function<void(u32 call_number, u32 response_code, nlohmann::json response_json)> callback)
  {
    this->response = "";
    this->call_number = call_number;
    this->slist = slist;
    this->mime_form = mime_form;
    this->callback = callback;
  }
#elif defined(MODIO_OSX_DETECTED) || defined(MODIO_LINUX_DETECTED)
  JsonResponseHandler::JsonResponseHandler(u32 call_number, struct curl_slist * slist, struct curl_httppost *formpost, std::function<void(u32 call_number, u32 response_code, nlohmann::json response_json)> callback)
  {
    this->response = "";
    this->call_number = call_number;
    this->slist = slist;
    this->formpost = formpost;
    this->callback = callback;
  }
#endif

JsonResponseHandler::~JsonResponseHandler()
{
  curl_slist_free_all(this->slist);
  #ifdef MODIO_WINDOWS_DETECTED
    curl_mime_free(this->mime_form);
  #elif defined(MODIO_OSX_DETECTED) || defined(MODIO_LINUX_DETECTED)
    curl_formfree(this->formpost);
  #endif
}

OngoingDownload::OngoingDownload(u32 call_number, std::string url, struct curl_slist * slist, std::function<void(u32 call_number, u32 response_code)> callback)
{
  this->url = url;
  this->call_number = call_number;
  this->callback = callback;
  this->slist = slist;
}

OngoingDownload::~OngoingDownload()
{
  curl_slist_free_all(this->slist);
}

void updateModDownloadQueue()
{
  nlohmann::json mod_download_queue_json = openJson(modio::getModIODirectory() + "mod_download_queue.json");

  for(auto &queued_mod_download : mod_download_queue)
  {
    delete queued_mod_download;
  }
  mod_download_queue.clear();

  for(auto &queued_mod_download_json : mod_download_queue_json)
  {
    ModioQueuedModDownload modio_queued_mod_download;
    modioInitQueuedModDownload(&modio_queued_mod_download, queued_mod_download_json);
    QueuedModDownload* queued_mod_download = new QueuedModDownload();
    queued_mod_download->initialize(modio_queued_mod_download);
    mod_download_queue.push_back(queued_mod_download);
    modioFreeQueuedModDownload(&modio_queued_mod_download);
  }
}

void updateModDownloadQueueFile()
{
  nlohmann::json mod_download_queue_json;
  for(auto &queued_mod_download : mod_download_queue)
  {
    mod_download_queue_json.push_back(modio::toJson(*queued_mod_download));
  }
  writeJson(modio::getModIODirectory() + "mod_download_queue.json",mod_download_queue_json);
}

void updateModUploadQueueFile()
{
  nlohmann::json mod_upload_queue_json;
  for(auto &queued_mod_upload : modfile_upload_queue)
  {
    mod_upload_queue_json.push_back(modio::toJson(*queued_mod_upload));
  }
  writeJson(modio::getModIODirectory() + "mod_upload_queue.json",mod_upload_queue_json);
}

void prioritizeModDownload(u32 mod_id)
{
  nlohmann::json result_json;
  nlohmann::json mod_download_queue_json = openJson(modio::getModIODirectory() + "mod_download_queue.json");

  for(auto &queued_mod_download_json : mod_download_queue_json)
  {
    if(modio::hasKey(queued_mod_download_json,"mod")
        && modio::hasKey(queued_mod_download_json["mod"],"id")
        && queued_mod_download_json["mod"]["id"] == mod_id)
    {
      result_json.push_back(queued_mod_download_json);      
    }
  }

  for(auto &queued_mod_download_json : mod_download_queue_json)
  {
    if(!modio::hasKey(queued_mod_download_json,"mod")
        || !modio::hasKey(queued_mod_download_json["mod"],"id")
        || queued_mod_download_json["mod"]["id"] != mod_id)
    {
      result_json.push_back(queued_mod_download_json);      
    }
  }

  writeJson(modio::getModIODirectory() + "mod_download_queue.json",result_json);

  if(current_queued_mod_download)
    current_queued_mod_download->state = MODIO_PRIORITIZING_OTHER_DOWNLOAD;
}

void downloadNextQueuedMod()
{
  if (mod_download_queue.size() > 0)
  {
    downloadMod(mod_download_queue.front());
  }
}

void setHeaders(std::vector<std::string> headers, CURL *curl)
{
  struct curl_slist *chunk = NULL;
  for (u32 i = 0; i < headers.size(); i++)
    chunk = curl_slist_append(chunk, headers[i].c_str());

  curl_easy_setopt(curl, CURLOPT_HTTPHEADER, chunk);
}

void setVerifies(CURL *curl)
{
  curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
  curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
}

void setJsonResponseWrite(CURL *curl)
{
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, onGetJsonData);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, curl);
  curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, &headerCallback);
  curl_easy_setopt(curl, CURLOPT_HEADERDATA, curl);
}

}
}
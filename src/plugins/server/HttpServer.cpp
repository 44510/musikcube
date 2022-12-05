//////////////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2004-2021 musikcube team
//
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
//    * Redistributions of source code must retain the above copyright notice,
//      this list of conditions and the following disclaimer.
//
//    * Redistributions in binary form must reproduce the above copyright
//      notice, this list of conditions and the following disclaimer in the
//      documentation and/or other materials provided with the distribution.
//
//    * Neither the name of the author nor the names of other contributors may
//      be used to endorse or promote products derived from this software
//      without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.
//
//////////////////////////////////////////////////////////////////////////////

#include "HttpServer.h"
#include "Constants.h"
#include "Util.h"
#include "Transcoder.h"
#include "TranscodingAudioDataStream.h"

#include <musikcore/sdk/ITrack.h>
#include <musikcore/sdk/String.h>

#pragma warning(push, 0)
#include <websocketpp/base64/base64.hpp>
#pragma warning(pop, 0)

#include <unordered_map>
#include <string>
#include <cstdlib>
#include <filesystem>

#include <fcntl.h>
#include <stdio.h>

#ifdef WIN32
#include <io.h>
#endif

#include <vector>

#define HTTP_416_DISABLED true
#define ENABLE_DEBUG 1

static const char* ENVIRONMENT_DISABLE_HTTP_SERVER_AUTH = "MUSIKCUBE_DISABLE_HTTP_SERVER_AUTH";
static const char* TAG = "HttpServer";

namespace std {
    namespace fs = std::filesystem;
}

using namespace musik::core::sdk;

std::unordered_map<std::string, std::string> CONTENT_TYPE_MAP = {
    { ".mp3", "audio/mpeg" },
    { ".ogg", "audio/ogg" },
    { ".opus", "audio/ogg" },
    { ".oga", "audio/ogg" },
    { ".spx", "audio/ogg" },
    { ".flac", "audio/flac" },
    { ".aac", "audio/aac" },
    { ".mp4", "audio/mp4" },
    { ".m4a", "audio/mp4" },
    { ".wav", "audio/wav" },
    { ".mpc", "audio/x-musepack" },
    { ".mp+", "audio/x-musepack" },
    { ".mpp", "audio/x-musepack" },
    { ".ape", "audio/monkeys-audio" },
    { ".wma", "audio/x-ms-wma" },
    { ".jpg", "image/jpeg" }
};

struct Range {
    size_t from;
    size_t to;
    size_t total;
    IDataStream* file;

    std::string HeaderValue() {
        return "bytes " + std::to_string(from) + "-" + std::to_string(to) + "/" + std::to_string(total);
    }
};

static std::string contentType(const std::string& fn) {
    try {
        std::fs::path p(std::fs::u8path(fn));
        std::string ext = str::ToLowerCopy(str::Trim(p.extension().u8string()));
        auto it = CONTENT_TYPE_MAP.find(ext);
        if (it != CONTENT_TYPE_MAP.end()) {
            return it->second;
        }
    }
    catch (...) {
    }

    return "application/octet-stream";
}

static std::string fileExtension(const std::string& fn) {
    try {
        std::fs::path p(std::fs::u8path(fn));
        std::string ext = str::ToLowerCopy(str::Trim(p.extension().u8string()));
        if (ext.size()) {
            return ext[0] == '.' ? ext.substr(1) : ext;
        }
    }
    catch (...) {
    }

    return "mp3";
}

static ssize_t fileReadCallback(void *cls, uint64_t pos, char *buf, size_t max) {
    Range* range = static_cast<Range*>(cls);

    size_t offset = (size_t) pos + range->from;
    offset = std::min(range->to ? range->to : (size_t) SIZE_MAX, offset);

    size_t avail = range->total ? (range->total - offset) : SIZE_MAX;
    size_t count = std::min(avail, max);

    if (range->file->Seekable()) {
        if (!range->file->SetPosition(offset)) {
            return MHD_CONTENT_READER_END_OF_STREAM;
        }
    }

    count = range->file->Read(buf, count);
    if (count > 0) {
        return count;
    }

    return MHD_CONTENT_READER_END_OF_STREAM;
}

static void fileFreeCallback(void *cls) {
    Range* range = static_cast<Range*>(cls);
    if (range->file) {
        range->file->Close(); /* lazy destroy */
        range->file = nullptr;
    }
    delete range;
}

static Range* parseRange(IDataStream* file, const char* range) {
    Range* result = new Range();

    size_t size = file ? file->Length() : 0;

    result->file = file;
    result->total = size;
    result->from = 0;
    result->to = (size <= 0) ? 0 : size - 1;

    if (range) {
        std::string str(range);

        if (str.substr(0, 6) == "bytes=") {
            str = str.substr(6);
            std::vector<std::string> parts = str::Split(str, "-");
            if (parts.size() == 2) {
                try {
                    size_t from = (size_t) std::max(0, std::stoi(str::Trim(parts[0])));
                    size_t to = size;

                    if (parts.at(1).size()) {
                        to = (size_t) std::min((int) size, std::stoi(str::Trim(parts[1])));
                    }

                    if (to > from) {
                        result->from = from;
                        if (to == 0) {
                            result->to = 0;
                        }
                        else if (to >= size) {
                            result->to = (size == 0) ? 0 : size - 1;
                        }
                        else {
                            result->to = (to == 0) ? 0 : to - 1;
                        }
                    }
                }
                catch (...) {
                    /* return false below */
                }
            }
        }
    }

    return result;
}

static size_t getUnsignedUrlParam(
    struct MHD_Connection *connection,
    const std::string& argument,
    size_t defaultValue)
{
    const char* stringValue =
        MHD_lookup_connection_value(
            connection,
            MHD_GET_ARGUMENT_KIND,
            argument.c_str());

    if (stringValue != 0) {
        try {
            return std::stoul(urlDecode(stringValue));
        }
        catch (...) {
            /* invalid bitrate */
        }
    }

    return defaultValue;
}

static std::string getStringUrlParam(
    struct MHD_Connection *connection,
    const std::string& argument,
    std::string defaultValue)
{
    const char* stringValue =
        MHD_lookup_connection_value(
            connection,
            MHD_GET_ARGUMENT_KIND,
            argument.c_str());

    return stringValue ? std::string(stringValue) : defaultValue;
}

static bool isAuthenticated(MHD_Connection *connection, Context& context) {
    const char* disableAuth = std::getenv(ENVIRONMENT_DISABLE_HTTP_SERVER_AUTH);
    if (disableAuth && std::string(disableAuth) == "1") {
        return true;
    }

    const char* authPtr = MHD_lookup_connection_value(
        connection, MHD_HEADER_KIND, "Authorization");

    if (authPtr && strlen(authPtr)) {
        std::string auth(authPtr);
        if (auth.find("Basic ") == 0) {
            std::string encoded = auth.substr(6);
            if (encoded.size()) {
                std::string decoded = websocketpp::base64_decode(encoded);
                std::vector<std::string> userPass = str::Split(decoded, ":");
                if (userPass.size() == 2) {
                    std::string password = GetPreferenceString(context.prefs, key::password, defaults::password);
                    return userPass[0] == "default" && userPass[1] == password;
                }
            }
        }
    }

    return false;
}

HttpServer::HttpServer(Context& context)
: context(context)
, running(false) {
    this->httpServer = nullptr;
}

HttpServer::~HttpServer() {
    this->Stop();
}

void HttpServer::Wait() {
    std::unique_lock<std::mutex> lock(this->exitMutex);
    while (this->running) {
        this->exitCondition.wait(lock);
    }
}

bool HttpServer::Start() {
    if (this->Stop()) {
        Transcoder::RemoveTempTranscodeFiles(this->context);

        MHD_FLAG ipVersion = MHD_NO_FLAG;
        if (context.prefs->GetBool(prefs::use_ipv6.c_str(), defaults::use_ipv6)) {
            ipVersion = MHD_USE_IPv6;
        }

        int serverFlags =
#if MHD_VERSION >= 0x00095300
            MHD_USE_AUTO | MHD_USE_INTERNAL_POLLING_THREAD | MHD_USE_THREAD_PER_CONNECTION | ipVersion;
#else
            MHD_USE_SELECT_INTERNALLY | MHD_USE_THREAD_PER_CONNECTION | ipVersion;
#endif

        int serverPort =
            context.prefs->GetInt(prefs::http_server_port.c_str(), defaults::http_server_port);

        httpServer = MHD_start_daemon(
            serverFlags,
            serverPort,
            nullptr,                                    /* accept() policy callback */
            nullptr,                                    /* accept() policy callback data */
            &HttpServer::HandleRequest,                 /* request handler callback */
            this,                                       /* request handler callback data */
            MHD_OPTION_UNESCAPE_CALLBACK,               /* option to configure unescaping */
            &HttpServer::HandleUnescape,                /* callback to be called for unescaping data */
            this,                                       /* unescape data callback data */
            MHD_OPTION_LISTENING_ADDRESS_REUSE,         /* option to configure address reuse */
            1,                                          /* enable address reuse */
            MHD_OPTION_END);                            /* terminal option */

        this->running = (httpServer != nullptr);
        return running;
    }

    return false;
}

bool HttpServer::Stop() {
    if (httpServer) {
        MHD_stop_daemon(this->httpServer);
        this->httpServer = nullptr;
    }

    this->running = false;
    this->exitCondition.notify_all();

    return true;
}

size_t HttpServer::HandleUnescape(void * cls, struct MHD_Connection *c, char *s) {
    /* don't do anything. the default implementation will decode the
    entire path, which breaks if we have individually decoded segments. */
    return strlen(s);
}

MHD_Result HttpServer::HandleRequest(
    void *cls,
    struct MHD_Connection *connection,
    const char *url,
    const char *method,
    const char *version,
    const char *upload_data,
    size_t *upload_data_size,
    void **con_cls)
{
    auto server = static_cast<HttpServer*>(cls);

#ifdef ENABLE_DEBUG
    server->context.debug->Info(TAG, str::Format("new request: %s", url).c_str());
#endif

    struct MHD_Response* response = nullptr;
    int ret = MHD_NO;
    int status = MHD_HTTP_NOT_FOUND;

    try {
        if (method && std::string(method) == "GET") {
            if (!isAuthenticated(connection, server->context)) {
                status = 401; /* unauthorized */
                static const char* error = "unauthorized";
                response = MHD_create_response_from_buffer(strlen(error), (void*)error, MHD_RESPMEM_PERSISTENT);
#ifdef ENABLE_DEBUG
                server->context.debug->Warning(TAG, "unauthorized request, returning a 401");
#endif
            }
            else {
                /* if we get here we're authenticated */
                std::string urlStr(url);

                if (urlStr[0] == '/') {
                    urlStr = urlStr.substr(1);
                }

                std::vector<std::string> parts = str::Split(urlStr, "/");
                if (parts.size() > 0) {
                    /* /audio/id/<id> OR /audio/external_id/<external_id> */
                    if (parts.at(0) == fragment::audio && parts.size() == 3) {
                        status = HandleAudioTrackRequest(server, response, connection, parts);
                    }
                    /* /thumbnail/<id> */
                    else if (parts.at(0) == fragment::thumbnail && parts.size() == 2) {
                        status = HandleThumbnailRequest(server, response, connection, parts);
                    }
                }
            }
        }
    }
    catch (...) {
    }

    if (response) {
#ifdef ENABLE_DEBUG
        server->context.debug->Info(TAG, str::Format("return http %d", status).c_str());
#endif

        ret = MHD_queue_response(connection, status, response);
        MHD_destroy_response(response);
    }

    return (MHD_Result) ret;
}

int HttpServer::HandleAudioTrackRequest(
    HttpServer* server,
    MHD_Response*& response,
    MHD_Connection *connection,
    std::vector<std::string>& pathParts)
{
    size_t bitrate = getUnsignedUrlParam(connection, "bitrate", 0);
    int maxActiveTranscoders = server->context.prefs->GetInt(
        prefs::transcoder_max_active_count.c_str(),
        defaults::transcoder_max_active_count);

    if (bitrate != 0 && Transcoder::GetActiveCount() >= maxActiveTranscoders) {
        response = MHD_create_response_from_buffer(0, nullptr, MHD_RESPMEM_PERSISTENT);
        return MHD_HTTP_TOO_MANY_REQUESTS;
    }

    int status = MHD_HTTP_OK;

    ITrack* track = nullptr;
    bool byExternalId = (pathParts.at(1) == fragment::external_id);

    if (byExternalId) {
        std::string externalId = urlDecode(pathParts.at(2));
        track = server->context.metadataProxy->QueryTrackByExternalId(externalId.c_str());
    }
    else if (pathParts.at(1) == fragment::id) {
        uint64_t id = std::stoull(urlDecode(pathParts.at(2)));
        track = server->context.metadataProxy->QueryTrackById(id);
    }

    if (track) {
        const std::string duration = GetMetadataString(track, key::duration);
        const std::string filename = GetMetadataString(track, key::filename);
        const std::string title = GetMetadataString(track, key::title, "");
        const std::string externalId = GetMetadataString(track, key::external_id, "");

        track->Release();

        std::string format;

        if (bitrate != 0) {
            format = getStringUrlParam(connection, "format", "mp3");
        }

        IDataStream* file = (bitrate == 0)
            ? server->context.environment->GetDataStream(filename.c_str(), OpenFlags::Read)
            : Transcoder::Transcode(server->context, filename, bitrate, format);

        const char* rangeVal = MHD_lookup_connection_value(
            connection, MHD_HEADER_KIND, "Range");

        Range* range = parseRange(file, rangeVal);

        /* ehh... */
        bool isOnDemandTranscoder = !!dynamic_cast<TranscodingAudioDataStream*>(file);

#ifdef ENABLE_DEBUG
        server->context.debug->Info(TAG, str::Format(
            "range request: %s, resolved range: %s, isOnDemandTranscoder=%s",
            rangeVal ? rangeVal : "[unspecified]",
            range ? range->HeaderValue().c_str() : "[unresolved]",
            isOnDemandTranscoder ? "true" : "false").c_str());
#endif

        /* gotta be careful with request ranges if we're transcoding. don't
        allow any custom ranges other than from 0 to end. */
        if (isOnDemandTranscoder && rangeVal && strlen(rangeVal)) {
            if (range->from != 0 || range->to != range->total - 1) {
                delete range;

#ifdef ENABLE_DEBUG
                server->context.debug->Info(TAG, "removing range header, seek requested with ondemand transcoder");
#endif

                if (HTTP_416_DISABLED) {
                    rangeVal = nullptr; /* ignore the header from here on out. */

                    /* lots of clients don't seem to be to deal with 416 properly;
                    instead, ignore the range header and return the whole file,
                    and a 200 (not 206) */
                    if (file) {
                        range = parseRange(file, nullptr);
                    }
                }
                else {
                    if (file) {
                        file->Release();
                        file = nullptr;
                    }

                    if (server->context.prefs->GetBool(
                        prefs::transcoder_synchronous_fallback.c_str(),
                        defaults::transcoder_synchronous_fallback))
                    {
                        /* if we're allowed, fall back to synchronous transcoding. we'll block
                        here until the entire file has been converted and cached */
                        file = Transcoder::TranscodeAndWait(server->context, nullptr, filename, bitrate, format);
                        range = parseRange(file, rangeVal);
                    }
                    else {
                        /* otherwise fail with a "range not satisfiable" status */
                        status = 416;
                        char empty[1];
                        response = MHD_create_response_from_buffer(0, empty, MHD_RESPMEM_PERSISTENT);
                    }
                }
            }
        }

        if (file) {
            size_t length = (range->to - range->from);

            response = MHD_create_response_from_callback(
                length == 0 ? MHD_SIZE_UNKNOWN : length + 1,
                4096,
                &fileReadCallback,
                range,
                &fileFreeCallback);

#ifdef ENABLE_DEBUG
            server->context.debug->Info(TAG, str::Format("response length=%d", ((length == 0) ? 0 : length + 1)).c_str());
#endif

            if (response) {
                /* 'format' will be valid if we're transcoding. otherwise, extract the extension
                from the filename. the client can use this as a hint when naming downloaded files */
                std::string extension = format.size() ? format : fileExtension(filename);
                MHD_add_response_header(response, "X-musikcube-File-Extension", extension.c_str());

                if (!isOnDemandTranscoder) {
                    MHD_add_response_header(response, "Accept-Ranges", "bytes");

                    if (std::fs::exists(std::fs::u8path(filename))) {
                        MHD_add_response_header(response, "X-musikcube-Filename-Override", externalId.c_str());
                    }
                }
                else {
                    MHD_add_response_header(response, "X-musikcube-Estimated-Content-Length", "true");
                }

                if (duration.size()) {
                    MHD_add_response_header(response, "X-Content-Duration", duration.c_str());
                    MHD_add_response_header(response, "Content-Duration", duration.c_str());
                }

                if (byExternalId) {
                    /* if we're using an on-demand transcoder, ensure the client does not cache the
                    result because we have to guess the content length. */
                    std::string value = isOnDemandTranscoder ? "no-cache" : "public, max-age=31536000";
                    MHD_add_response_header(response, "Cache-Control", value.c_str());
                }

                std::string type = (isOnDemandTranscoder || format.size())
                    ? contentType("." + format) : contentType(filename);

                MHD_add_response_header(response, "Content-Type", type.c_str());
                MHD_add_response_header(response, "Server", "musikcube server");

                if ((rangeVal && strlen(rangeVal)) || range->from > 0) {
                    if (range->total > 0) {
                        MHD_add_response_header(response, "Content-Range", range->HeaderValue().c_str());
                        status = MHD_HTTP_PARTIAL_CONTENT;
#ifdef ENABLE_DEBUG
                        if (rangeVal) {
                            server->context.debug->Info(TAG, str::Format("range header: %s", range->HeaderValue().c_str()).c_str());
                        }
#endif
                    }
                }
            }
            else {
                file->Release();
                file = nullptr;
            }
        }
        else {
            status = MHD_HTTP_NOT_FOUND;
        }
    }
    else {
        status = MHD_HTTP_NOT_FOUND;
    }

    return status;
}

int HttpServer::HandleThumbnailRequest(
    HttpServer* server,
    MHD_Response*& response,
    MHD_Connection* connection,
    std::vector<std::string>& pathParts)
{
    int status = MHD_HTTP_NOT_FOUND;

    char pathBuffer[4096];
    server->context.environment->GetPath(PathType::Library, pathBuffer, sizeof(pathBuffer));

    if (strlen(pathBuffer)) {
        std::string path = std::string(pathBuffer) + "thumbs/" + pathParts.at(1) + ".jpg";
        IDataStream* file = server->context.environment->GetDataStream(path.c_str(), OpenFlags::Read);

        if (file) {
            long length = file->Length();

            response = MHD_create_response_from_callback(
                length == 0 ? MHD_SIZE_UNKNOWN : length + 1,
                4096,
                &fileReadCallback,
                parseRange(file, nullptr),
                &fileFreeCallback);

            if (response) {
                MHD_add_response_header(response, "Cache-Control", "public, max-age=31536000");
                MHD_add_response_header(response, "Content-Type", contentType(path).c_str());
                MHD_add_response_header(response, "Server", "musikcube server");
                status = MHD_HTTP_OK;
            }
            else {
                file->Release();
            }
        }
    }

    return status;
}

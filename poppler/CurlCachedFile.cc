//========================================================================
//
// CurlCachedFile.cc
//
// This file is licensed under the GPLv2 or later
//
// Copyright 2009 Stefan Thomas <thomas@eload24.com>
// Copyright 2010, 2011 Hib Eris <hib@hiberis.nl>
// Copyright 2010, 2019, 2021 Albert Astals Cid <aacid@kde.org>
//
//========================================================================

#include <config.h>

#include "CurlCachedFile.h"

#include "goo/GooString.h"

//------------------------------------------------------------------------

CurlCachedFileLoader::CurlCachedFileLoader()
{
    url = nullptr;
    cachedFile = nullptr;
    curl = nullptr;
}

CurlCachedFileLoader::~CurlCachedFileLoader()
{
    curl_easy_cleanup(curl);
}

static size_t noop_cb(char *ptr, size_t size, size_t nmemb, void *ptr2)
{
    return size * nmemb;
}

size_t CurlCachedFileLoader::init(GooString *urlA, CachedFile *cachedFileA)
{
    double contentLength = -1;
    long code = 0;
    size_t size;

    url = urlA;
    cachedFile = cachedFileA;
    curl = curl_easy_init();

    curl_easy_setopt(curl, CURLOPT_URL, url->c_str());
    curl_easy_setopt(curl, CURLOPT_HEADER, 1);
    curl_easy_setopt(curl, CURLOPT_NOBODY, 1);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, &noop_cb);
    curl_easy_perform(curl);
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &code);
    if (code) {
        curl_easy_getinfo(curl, CURLINFO_CONTENT_LENGTH_DOWNLOAD, &contentLength);
        size = contentLength;
    } else {
        error(errInternal, -1, "Failed to get size of '{0:t}'.", url);
        size = -1;
    }
    curl_easy_reset(curl);

    return size;
}

static size_t load_cb(const char *ptr, size_t size, size_t nmemb, void *data)
{
    CachedFileWriter *writer = (CachedFileWriter *)data;
    return (writer->write)(ptr, size * nmemb);
}

int CurlCachedFileLoader::load(const std::vector<ByteRange> &ranges, CachedFileWriter *writer)
{
    CURLcode r = CURLE_OK;
    unsigned long long fromByte, toByte;
    for (const ByteRange &bRange : ranges) {

        fromByte = bRange.offset;
        toByte = fromByte + bRange.length - 1;
        GooString *range = GooString::format("{0:ulld}-{1:ulld}", fromByte, toByte);

        curl_easy_setopt(curl, CURLOPT_URL, url->c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, load_cb);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, writer);
        curl_easy_setopt(curl, CURLOPT_RANGE, range->c_str());
        r = curl_easy_perform(curl);
        curl_easy_reset(curl);

        delete range;

        if (r != CURLE_OK) {
            break;
        }
    }
    return r;
}

//------------------------------------------------------------------------

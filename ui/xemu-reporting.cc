/*
 * xemu Reporting
 *
 * Title compatibility and bug report submission.
 *
 * Copyright (C) 2020 Matt Borgerson
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <glib.h>
#include <glib/gi18n.h>
#include <stdio.h>
#include <curl/curl.h>

#include "xemu-reporting.h"
#include "json.hpp"
using json = nlohmann::json;

#define DEBUG_COMPAT_SERVICE 0

CompatibilityReport::CompatibilityReport()
{
}

CompatibilityReport::~CompatibilityReport()
{
}

const std::string &CompatibilityReport::GetSerializedReport()
{
	json report = {
		{"token", token},
		{"xemu_version", xemu_version},
		{"xemu_branch", xemu_branch},
		{"xemu_commit", xemu_commit},
		{"xemu_date", xemu_date},
		{"os_platform", os_platform},
		{"os_version", os_version},
		{"cpu", cpu},
		{"gl_vendor", gl_vendor},
		{"gl_renderer", gl_renderer},
		{"gl_version", gl_version},
		{"gl_shading_language_version", gl_shading_language_version},
		{"compat_rating", compat_rating},
		{"compat_comments", compat_comments},
		{"xbe_headers", xbe_headers},
	};
	serialized = report.dump(2); 
	return serialized;
}

/*
 * Makes POST request to file the report via libcurl.
 * Based on https://curl.haxx.se/libcurl/c/http-post.html.
 */
bool CompatibilityReport::Send()
{
	CURL *curl;
	CURLcode res = CURLE_FAILED_INIT;
	long http_res_code = 0;

	/* In windows, this will init the winsock stuff */ 
	curl_global_init(CURL_GLOBAL_ALL);

	curl = curl_easy_init();
	if (curl) {
#if DEBUG_COMPAT_SERVICE
		curl_easy_setopt(curl, CURLOPT_URL, "https://127.0.0.1/compatibility");
		curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0);
		curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0);
#else
		curl_easy_setopt(curl, CURLOPT_URL, "https://reports.xemu.app/compatibility");
#endif

		const std::string &s = GetSerializedReport();
		curl_easy_setopt(curl, CURLOPT_POSTFIELDS, s.c_str());
		res = curl_easy_perform(curl);
		if (res == CURLE_OK) {
			curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_res_code);
		}
		curl_easy_cleanup(curl);
	}

	curl_global_cleanup();

	if (res != CURLE_OK) {
		result_code = res;
		result_msg = curl_easy_strerror(res);
	    return false;
	}

	result_code = http_res_code;
	switch (http_res_code) {
	case 200:
		result_msg = "Ok";
		return true;
	case 400:
	case 411:
		result_msg = "Invalid request";
		return false;
	case 403:
		result_msg = "Invalid token";
		return false;
	case 413:
		result_msg = "Report too long";
		return false;
	default:
		result_msg = "Unknown error occured";
		return false;
	}
}

void CompatibilityReport::SetXbeData(struct xbe *xbe)
{
	assert(xbe != NULL);
	assert(xbe->headers != NULL);
	assert(xbe->headers_len > 0);

    // base64 encode all XBE headers to be sent with the report
    gchar *buf = g_base64_encode(xbe->headers, xbe->headers_len);
    xbe_headers = buf;
    g_free(buf);
}

#pragma once

/**
 * @file precious_runtime.hpp
 * @brief The Precious standard library, as C++ source text.
 *
 * A built-in like `ask` is a process -- several statements, some cleanup -- but
 * every place a built-in can appear in Precious is a slot that wants a single
 * *value*. The generated C++ used to close that gap with an inline lambda, which
 * worked but filled the generated file with noise nobody should have to read.
 *
 * So the body moves here instead. The compiler writes this file out as
 * `out/precious_runtime.h` and the generated program calls it:
 *
 *     std::string name = precious::ask("Who goes there? ");
 *
 * Everything in namespace `precious` is ordinary C++. To add a helper, add it
 * here and call it from the generator; there is no other place to change.
 *
 * ADDING A HELPER? Add it to the string below, then call it from a gen_xxx()
 * in the generator. Keep the body free of early returns once something has been
 * allocated -- there are no destructors in Precious to clean up for us.
 */

/// The contents of out/precious_runtime.h. A raw string literal, so the C++
/// below needs no escaping: '#' and '"' are written as themselves.
inline const char* precious_runtime_source() {
    return R"runtime(
// Written by the Precious compiler. Do not edit: it is replaced on every build.
//
// The standard library of a language that compiles to C++ -- the parts that are
// too big to inline into an expression.

#ifndef PRECIOUS_RUNTIME_HPP
#define PRECIOUS_RUNTIME_HPP

#include <iostream>
#include <limits>
#include <string>
#include <type_traits>

namespace precious {

// A `char` needs '\0' as its nothing; every other number needs 0.
template <typename T>
inline T zero() {
    if constexpr (std::is_same_v<T, char>) {
        return '\0';
    } else {
        return T(0);
    }
}

// Reads one value from stdin. A word takes a whole line, spaces and all;
// every other type takes a single value. The prompt is optional and may be any
// expression -- the caller only writes it when there is one.
template <typename T, typename P>
inline T read_from_stdin(const P& prompt) {
    // Flushing matters: without it the prompt would sit in the output buffer and
    // the typing would appear before it instead of after.
    if (!std::string(prompt).empty()) {
        std::cout << prompt << std::flush;
    }

    if constexpr (std::is_same_v<T, std::string>) {
        T result;
        std::getline(std::cin, result);
        return result;
    } else {
        T result = zero<T>();

        // Two details of std::cin are worth knowing:
        //  1. `>>` leaves the newline in the buffer, which would make the next
        //     getline() read an empty line. So drop the rest of the line.
        //  2. A failed read sets failbit, which turns every later read into a
        //     silent no-op. So clear the state. Bad input then becomes 0
        //     instead of breaking the rest of the program.
        if (!(std::cin >> result)) {
            result = zero<T>();
        }
        std::cin.clear();
        std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
        return result;
    }
}

// ask(...)        -- a word: a whole line
// ask_number(...) -- a number
// ask_decimal(...) -- a decimal
// ask_letter(...) -- a single character
//
// The destination type picks the reader, which is how `my x: number = ask(...)`
// reads a number and `my y = ask(...)` reads a line.
template <typename P = const char*>
inline std::string ask(const P& prompt = "") {
    return read_from_stdin<std::string>(prompt);
}

template <typename P = const char*>
inline long ask_number(const P& prompt = "") {
    return read_from_stdin<long>(prompt);
}

template <typename P = const char*>
inline double ask_decimal(const P& prompt = "") {
    return read_from_stdin<double>(prompt);
}

template <typename P = const char*>
inline char ask_letter(const P& prompt = "") {
    return read_from_stdin<char>(prompt);
}

}  // namespace precious

// ---------------------------------------------------------------------------
// Networking
// ---------------------------------------------------------------------------
//
// The whole of this section is behind a feature macro. The generated C++ only
// defines PRECIOUS_NETWORKING when the program wrote `import "networking"`, so
// a program that never imports it never sees <curl/curl.h> and never needs
// libcurl installed. That is the point of the macro: the dependency belongs to
// the program that asked for it, not to every Precious program on the machine.

#ifdef PRECIOUS_NETWORKING

#include <curl/curl.h>

namespace precious {

// curl hands the arriving bytes to a plain C function, which cannot capture
// anything -- so the string it is filling is passed in as a void*.
inline size_t curl_write(char* data, const size_t size, const size_t count, void* target) {
    static_cast<std::string*>(target)->append(data, size * count);
    return size * count;
}

// Does one request, and reports the HTTP status: 0 when there was no response
// at all, and the status (200, 404, ...) when there was.
//
// The cleanup is hand-ordered, because there are no destructors here to do it.
// `handle` is only allocated after the one early return, so the only way to
// skip the cleanup is to never allocate.
template <typename P>
inline long request(const P& url, const long timeout_seconds, std::string& body) {
    // curl needs a one-time global setup. `static` means "do this once, then
    // remember the answer", and it is thread-safe.
    static const bool ready = curl_global_init(CURL_GLOBAL_DEFAULT) == CURLE_OK;
    if (!ready) {
        return 0;
    }

    CURL* handle = curl_easy_init();
    if (handle == nullptr) {
        return 0;
    }

    body.clear();
    curl_easy_setopt(handle, CURLOPT_URL, std::string(url).c_str());
    curl_easy_setopt(handle, CURLOPT_WRITEFUNCTION, &curl_write);
    curl_easy_setopt(handle, CURLOPT_WRITEDATA, &body);
    curl_easy_setopt(handle, CURLOPT_TIMEOUT, timeout_seconds);
    curl_easy_setopt(handle, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(handle, CURLOPT_NOSIGNAL, 1L);

    const CURLcode result = curl_easy_perform(handle);

    long status = 0;
    if (result == CURLE_OK) {
        curl_easy_getinfo(handle, CURLINFO_RESPONSE_CODE, &status);
    }

    curl_easy_cleanup(handle); // reached on every path that allocated a handle

    if (result != CURLE_OK) {
        body.clear();
        return 0;
    }
    return status;
}

// http_get(url)   -- the body, or "" when the request failed
// http_code(url)  -- the status, or 0 when the request failed
//
// The second argument is a timeout in seconds. The default is 10: a host that
// never answers should not hang the program forever.
template <typename P>
inline std::string http_get(const P& url, const long timeout_seconds = 10) {
    std::string body;
    request(url, timeout_seconds, body);
    return body;
}

template <typename P>
inline long http_code(const P& url, const long timeout_seconds = 10) {
    std::string body;
    return request(url, timeout_seconds, body);
}

}  // namespace precious

#endif  // PRECIOUS_NETWORKING

#endif  // PRECIOUS_RUNTIME_HPP
)runtime";
}

#pragma once

#include <string>
#include <string_view>
#include <utility>

#include <cpl_error.h>
#include <libassert/assert.hpp>

namespace gdal {

// A failed GDAL call, with the category and message GDAL recorded for it.
class CplError {
public:
    explicit CplError(const CPLErrorNum code, std::string message) : _code(code), _message(std::move(message)) {
        DEBUG_ASSERT(code != CPLE_None);
    }

    // The error GDAL last recorded on this thread, or an application-defined one if it recorded none.
    static CplError from_last_error() {
        const CPLErrorNum code = CPLGetLastErrorNo();
        if (code == CPLE_None) {
            return CplError(CPLE_AppDefined, "GDAL recorded no error");
        }
        return CplError(code, CPLGetLastErrorMsg());
    }

    bool operator==(const CplError &) const = default;

    // The category GDAL assigned.
    CPLErrorNum code() const {
        return this->_code;
    }

    // The message GDAL recorded.
    const std::string &message() const {
        return this->_message;
    }

private:
    CPLErrorNum _code;
    std::string _message;
};

inline std::string_view format_as(const CplError &error) {
    return error.message();
}

} // namespace gdal

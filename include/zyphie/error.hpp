#pragma once
#include <stdexcept>
#include <string>
namespace zyphie {
enum class ErrorCode { invalid_argument, io, unsupported_feature, corrupt_archive,
    resource_limit, unsafe_path, destination_exists, source_changed, cancelled };
class Error : public std::runtime_error {
public:
    Error(ErrorCode code, const std::string& message) : std::runtime_error(message), code_(code) {}
    [[nodiscard]] ErrorCode code() const noexcept { return code_; }
private:
    ErrorCode code_;
};
inline void require(bool condition, ErrorCode code, const std::string& message) {
    if (!condition) { throw Error(code, message); }
}
}

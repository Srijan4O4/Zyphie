#include "zyphie/format.hpp"
extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data,std::size_t size) {
    try { zyphie::format::validate_path({reinterpret_cast<const char*>(data),size}); }
    catch(const zyphie::Error&) {}
    return 0;
}

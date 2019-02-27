#include <formats/json/types.hpp>

#include <utils/assert.hpp>

namespace formats {
namespace json {

Json::ValueType ToNativeType(Type type) {
  switch (type) {
    case Type::kNull:
      return Json::nullValue;
    case Type::kArray:
      return Json::arrayValue;
    case Type::kObject:
      return Json::objectValue;
    default:
      UASSERT_MSG(
          false,
          "No mapping from formats::json::Type to Json::ValueType found");
      return Json::nullValue;
  }
}

}  // namespace json
}  // namespace formats

#include <formats/bson/document.hpp>

#include <formats/bson/inline.hpp>
#include <formats/bson/value_impl.hpp>
#include <utils/assert.hpp>

namespace formats::bson {

Document::Document() : Value(MakeDoc()) {}

Document::Document(const Value& value) : Value(value.As<Document>()) {}

Document::Document(impl::BsonHolder bson)
    : Value(std::make_shared<impl::ValueImpl>(std::move(bson))) {}

}  // namespace formats::bson

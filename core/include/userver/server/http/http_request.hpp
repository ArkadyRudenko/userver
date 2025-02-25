#pragma once

/// @file userver/server/http/http_request.hpp
/// @brief @copybrief server::http::HttpRequest

#include <chrono>
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

#include <userver/engine/io/sockaddr.hpp>
#include <userver/http/header_map.hpp>
#include <userver/logging/log_helper_fwd.hpp>
#include <userver/server/http/form_data_arg.hpp>
#include <userver/server/http/http_method.hpp>
#include <userver/server/http/http_response.hpp>
#include <userver/utils/impl/projecting_view.hpp>
#include <userver/utils/str_icase.hpp>

USERVER_NAMESPACE_BEGIN

namespace engine::io {
class Socket;
class Sockaddr;
}  // namespace engine::io

namespace server::handlers {
class HttpHandlerBase;
}

/// Server parts of the HTTP protocol implementation.
namespace server::http {

class HttpRequestImpl;

/// @brief HTTP Request data
class HttpRequest final {
public:
    using HeadersMap = USERVER_NAMESPACE::http::headers::HeaderMap;

    using HeadersMapKeys = decltype(utils::impl::MakeKeysView(HeadersMap()));

    using CookiesMap = std::unordered_map<std::string, std::string, utils::StrCaseHash>;

    using CookiesMapKeys = decltype(utils::impl::MakeKeysView(CookiesMap()));

    /// @cond
    explicit HttpRequest(HttpRequestImpl& impl);
    ~HttpRequest() = default;

    request::ResponseBase& GetResponse() const;
    /// @endcond

    /// @brief Returns a container that should be filled with response data to
    /// this request.
    HttpResponse& GetHttpResponse() const;

    const HttpMethod& GetMethod() const;
    const std::string& GetMethodStr() const;

    /// @return Major version of HTTP. For example, for HTTP 1.0 it returns 1
    int GetHttpMajor() const;

    /// @return Minor version of HTTP. For example, for HTTP 1.0 it returns 0
    int GetHttpMinor() const;

    /// @return Request URL
    const std::string& GetUrl() const;

    /// @return Request path
    const std::string& GetRequestPath() const;

    /// @return Request path suffix, i.e. part of the path that remains after
    /// matching the path of a handler.
    const std::string& GetPathSuffix() const;

    std::chrono::duration<double> GetRequestTime() const;
    std::chrono::duration<double> GetResponseTime() const;

    /// @return Host from the URL.
    const std::string& GetHost() const;

    /// @return Request remote address
    const engine::io::Sockaddr& GetRemoteAddress() const;

    /// @return First argument value with name `arg_name` or an empty string if no
    /// such argument.
    /// Arguments are extracted from:
    /// - query part of the URL,
    /// - the HTTP body (only if `parse_args_from_body: true` for handler is set).
    ///
    /// In both cases, arg keys and values are url-decoded automatically when
    /// parsing into the HttpRequest.
    const std::string& GetArg(std::string_view arg_name) const;

    /// @return Argument values with name `arg_name` or an empty vector if no
    /// such argument.
    /// Arguments are extracted from:
    /// - query part of the URL,
    /// - the HTTP body (only if `parse_args_from_body: true` for handler is set).
    ///
    /// In both cases, arg keys and values are url-decoded automatically when
    /// parsing into the HttpRequest.
    const std::vector<std::string>& GetArgVector(std::string_view arg_name) const;

    /// @return true if argument with name arg_name exists, false otherwise.
    /// Arguments are extracted from:
    /// - query part of the URL,
    /// - the HTTP body (only if `parse_args_from_body: true` for handler is set).
    ///
    /// In both cases, arg keys and values are url-decoded automatically when
    /// parsing into the HttpRequest.
    bool HasArg(std::string_view arg_name) const;

    /// @return Count of arguments.
    /// Arguments are extracted from:
    /// - query part of the URL,
    /// - the HTTP body (only if `parse_args_from_body: true` for handler is set).
    size_t ArgCount() const;

    /// @return List of names of arguments.
    /// Arguments are extracted from:
    /// - query part of the URL,
    /// - the HTTP body (only if `parse_args_from_body: true` for handler is set).
    std::vector<std::string> ArgNames() const;

    /// @return First argument value with name arg_name from multipart/form-data
    /// request or an empty FormDataArg if no such argument.
    const FormDataArg& GetFormDataArg(std::string_view arg_name) const;

    /// @return Argument values with name arg_name from multipart/form-data
    /// request or an empty FormDataArg if no such argument.
    const std::vector<FormDataArg>& GetFormDataArgVector(std::string_view arg_name) const;

    /// @return true if argument with name arg_name exists in multipart/form-data
    /// request, false otherwise.
    bool HasFormDataArg(std::string_view arg_name) const;

    /// @return Count of multipart/form-data arguments.
    size_t FormDataArgCount() const;

    /// @return List of names of multipart/form-data arguments.
    std::vector<std::string> FormDataArgNames() const;

    /// @return Named argument from URL path with wildcards.
    /// @note Path args are currently NOT url-decoded automatically.
    const std::string& GetPathArg(std::string_view arg_name) const;

    /// @return Argument from URL path with wildcards by its 0-based index.
    /// @note Path args are currently NOT url-decoded automatically.
    const std::string& GetPathArg(size_t index) const;

    /// @return true if named argument from URL path with wildcards exists, false
    /// otherwise.
    bool HasPathArg(std::string_view arg_name) const;

    /// @return true if argument with index from URL path with wildcards exists,
    /// false otherwise.
    bool HasPathArg(size_t index) const;

    /// @return Number of wildcard arguments in URL path.
    size_t PathArgCount() const;

    /// @return Value of the header with case insensitive name header_name, or an
    /// empty string if no such header.
    const std::string& GetHeader(std::string_view header_name) const;
    /// @overload
    const std::string& GetHeader(const USERVER_NAMESPACE::http::headers::PredefinedHeader& header_name) const;
    const HeadersMap& GetHeaders() const;

    /// @return true if header with case insensitive name header_name exists,
    /// false otherwise.
    bool HasHeader(std::string_view header_name) const;
    /// @overload
    bool HasHeader(const USERVER_NAMESPACE::http::headers::PredefinedHeader& header_name) const;

    /// @return Number of headers.
    size_t HeaderCount() const;

    /// @return List of headers names.
    HeadersMapKeys GetHeaderNames() const;

    /// Removes the header with case insensitive name header_name.
    void RemoveHeader(std::string_view header_name);
    /// @overload
    void RemoveHeader(const USERVER_NAMESPACE::http::headers::PredefinedHeader& header_name);

    /// @return Value of the cookie with case sensitive name cookie_name, or an
    /// empty string if no such cookie exists.
    const std::string& GetCookie(const std::string& cookie_name) const;

    /// @return true if cookie with case sensitive name cookie_name exists, false
    /// otherwise.
    bool HasCookie(const std::string& cookie_name) const;

    /// @return Number of cookies.
    size_t CookieCount() const;

    /// @return List of cookies names.
    CookiesMapKeys GetCookieNames() const;

    /// @return HTTP body.
    const std::string& RequestBody() const;

    /// @return HTTP headers.
    const HeadersMap& RequestHeaders() const;

    /// @return HTTP cookies.
    const CookiesMap& RequestCookies() const;

    /// @cond
    void SetRequestBody(std::string body);
    void ParseArgsFromBody();

    std::chrono::steady_clock::time_point GetStartTime() const;
    /// @endcond

    /// @brief Set the response status code.
    ///
    /// Equivalent to this->GetHttpResponse().SetStatus(status).
    void SetResponseStatus(HttpStatus status) const;

    /// @return true if the body of the request is still compressed. In other
    /// words returns true if the static option `decompress_request` of a handler
    /// was set to `false` and this is a compressed request.
    bool IsBodyCompressed() const;

    /// @cond
    void SetUpgradeWebsocket(std::function<void(std::unique_ptr<engine::io::RwBase>&&, engine::io::Sockaddr&&)> cb
    ) const;
    void DoUpgrade(std::unique_ptr<engine::io::RwBase>&&, engine::io::Sockaddr&& peer_name) const;
    /// @endcond

private:
    friend class handlers::HttpHandlerBase;

    HttpRequestImpl& impl_;
};

}  // namespace server::http

USERVER_NAMESPACE_END

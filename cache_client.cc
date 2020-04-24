//Includes and some code: https://www.boost.org/doc/libs/1_72_0/libs/beast/example/http/client/sync/http_client_sync.cpp

#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/version.hpp>
#include <boost/asio/connect.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/algorithm/string.hpp>
#include <cstdlib>
#include <iostream>
#include <string>
#include <iostream>
#include "cache.hh"

namespace beast = boost::beast;     // from <boost/beast.hpp>
namespace http = beast::http;       // from <boost/beast/http.hpp>
namespace net = boost::asio;        // from <boost/asio.hpp>
using tcp = net::ip::tcp;           // from <boost/asio/ip/tcp.hpp>

class Cache::Impl {

public:
    std::string host_;
    std::string port_;

    net::io_context ioc_;
    tcp::resolver resolver_;
    mutable beast::tcp_stream stream_;
    boost::asio::ip::basic_resolver_results<tcp>  results_;

    unsigned HTTPVersion_ = 11;
    std::string get_val_;

    Impl(std::string host, std::string port):
        host_(host),
        port_(port),
        ioc_(),
        resolver_(ioc_),
        stream_(ioc_)
    {
        results_ = resolver_.resolve(host_, port_);
        stream_.connect(results_);
    }

    ~Impl() {
        std::cout << "Cache deconstructed\n";
        // The following check was suggested, but did not work,
        // so our deconstructor is merely a notice.
        /*
        if (ec && ec != beast::errc::not_connected) {
            throw beast::system_error{ ec };
        }
        */
    }

    void set(key_type key, val_type val, size_type size) {

        //std::cout << "\nBeginning set request...\n";

        //Set up a new ioc and stream
        /*
        net::io_context ioc;
        tcp::resolver resolver(ioc);
        auto const results_ = resolver.resolve(host_, port_);
        beast::tcp_stream stream_(ioc);
        stream_.connect(results_);
        */

        //std::cout << "\n" << key << "\n";
        //std::cout << val << "\n";
        //std::cout << size << "\n";

        // Set up an HTTP SET request message
        std::string requestBody = "/" + key + "/" + val + "/" + std::to_string(size);
        //std::cout << "The client asked (set): " << requestBody << "\n";
        http::request<http::string_body> req{ http::verb::put, requestBody, HTTPVersion_ };
        req.set(http::field::host, host_);
        req.set(http::field::user_agent, BOOST_BEAST_VERSION_STRING);
        // *Changed:
        req.set(http::field::content_type, "text/plain");
        req.body() = requestBody;
        //std::cout << "Attempted request body: " << req.body() << "\n\n";

        req.prepare_payload();
        // *End of changes
        // Send the HTTP request to the remote host
        http::write(stream_, req);

        // This buffer is used for reading and must be persisted
        beast::flat_buffer buffer;

        // Declare a container to hold the response
        http::response<http::string_body> res;

        // Receive the HTTP response
        http::read(stream_, buffer, res);

        // Gracefully close the connection
        beast::error_code ec;
        stream_.socket().shutdown(tcp::socket::shutdown_both, ec);
    }

    val_type get(key_type key, size_type& val_size) {

        std::cout << "\nBeginning get request...\n";

        //Set up a new ioc
        /*
        net::io_context ioc;
        tcp::resolver resolver(ioc);
        auto const results_ = resolver.resolve(host_, port_);
        beast::tcp_stream stream_(ioc);
        stream_.connect(results_);
        */

        // Set up an HTTP GET request message
        std::string requestBody = "/" + key;
        http::request<http::string_body> req{ http::verb::get, requestBody, HTTPVersion_ };
        req.set(http::field::host, host_);
        req.set(http::field::user_agent, BOOST_BEAST_VERSION_STRING);
        // *Changes:
        req.set(http::field::content_type, "text/plain");
        req.body() = requestBody;

        req.prepare_payload();
        // *End of changes
        // Send the HTTP request to the remote host
        http::write(stream_, req);

        // This buffer is used for reading and must be persisted
        beast::flat_buffer buffer;

        // Declare a container to hold the response
        http::response<http::string_body> res;

        // Receive the HTTP response
        http::read(stream_, buffer, res);

        // Gracefully close the connection
        beast::error_code ec;
        stream_.socket().shutdown(tcp::socket::shutdown_both, ec);

        std::vector<std::string> splitBody;
        //std::cout << "The client asked: " << requestBody << "\n";
        std::cout << "The server returned: " << res.body() << "\n";
        boost::split(splitBody, res.body(), boost::is_any_of(",:"));
        if (splitBody[0] == "NULL") {
            return nullptr;
        }
        get_val_ = "";
        get_val_ = splitBody[3].substr(2, splitBody[3].size() - 3); // Changed
        //std::cout << "Made it past the first substr, which held: " << get_val_ << "\n";

        val_type val = get_val_.c_str();  // "1000"
        //std::cout << "val: " << val << "\n";
        std::string val_size_string = splitBody[5].substr(2, splitBody[5].size() - 3); // Changed
        //std::cout << "val_size_string = " << val_size_string << "\n";
        val_size = std::stoi(val_size_string);
        return val;
    }

    bool del (key_type key) {
        std::cout << "\nBeginning del request...\n";

        //Set up a new ioc
        /*
        net::io_context ioc;
        tcp::resolver resolver(ioc);
        auto const results_ = resolver.resolve(host_, port_);
        beast::tcp_stream stream_(ioc);
        stream_.connect(results_);
        */

        // Set up an HTTP DELETE request message
        std::string requestBody = "/" + key;
        http::request<http::string_body> req{ http::verb::delete_, requestBody, HTTPVersion_ };
        req.set(http::field::host, host_);
        req.set(http::field::user_agent, BOOST_BEAST_VERSION_STRING);
        // *Changed:
        req.set(http::field::content_type, "text/plain");
        req.body() = requestBody;
        
        req.prepare_payload();
        // *End of changes
        // Send the HTTP request to the remote host
        http::write(stream_, req);

        // This buffer is used for reading and must be persisted
        beast::flat_buffer buffer;

        // Declare a container to hold the response
        http::response<http::string_body> res;

        // Receive the HTTP response
        http::read(stream_, buffer, res);

        // Gracefully close the connection
        beast::error_code ec;
        stream_.socket().shutdown(tcp::socket::shutdown_both, ec);

        std::string result = res.body();
        bool answer = true;
        if (result == "False") {
            answer = false;
        }
        return answer;
    }

    size_type space_used() {
        
        std::cout << "\nBeginning space_used request...\n";

        //Set up a new ioc
        /*
        net::io_context ioc;
        tcp::resolver resolver(ioc);
        auto const results_ = resolver.resolve(host_, port_);
        beast::tcp_stream stream_(ioc);
        stream_.connect(results_);
        */

        //Print is new
        std::cout << "Generating request...\n";
        // Set up an HTTP HEAD request message
        http::request<http::string_body> req{http::verb::head, "/", HTTPVersion_};
        req.set(http::field::host, host_);
        req.set(http::field::user_agent, BOOST_BEAST_VERSION_STRING);
        req.set(http::field::content_length, req.body().size());
        req.prepare_payload();
        //Print is new
        std::cout << "Writing...\n";
        // Send the HTTP request to the remote host
        http::write(stream_, req);

        // This buffer is used for reading and must be persisted
        beast::flat_buffer buffer;

        // Declare a container to hold the response
        http::response<http::string_body> res;

        std::cout << "Reading...\n";
        // Receive the HTTP response
        http::read(stream_, buffer, res);

        // Gracefully close the connection
        beast::error_code ec;
        stream_.socket().shutdown(tcp::socket::shutdown_both, ec);

        //Print is new
        std::cout << "Before string conversion: " << res["Space-Used"] << "\n";
        std::string space_used_string = res["Space-Used"].data();
        //Print is new
        std::cout << "After conversion: " << space_used_string << "\n";
        Cache::size_type space_used_return = std::stoi(space_used_string);
        return space_used_return;
    }

    void reset() {

        std::cout << "\nBeginning a reset request...\n";

        //Set up a new ioc
        /*
        net::io_context ioc;
        tcp::resolver resolver(ioc);
        auto const results_ = resolver.resolve(host_, port_);
        beast::tcp_stream stream_(ioc);
        stream_.connect(results_);
        */

        // NOTE: Reset still uses 'target' as its request body, so it'll likely fail a lot of the time
        // Set up an HTTP POST request message
        http::request<http::string_body> req{ http::verb::post, "/reset", HTTPVersion_ };
        req.set(http::field::host, host_);
        req.set(http::field::user_agent, BOOST_BEAST_VERSION_STRING);
        req.body() = "/reset";
        req.set(http::field::content_length, req.body().size());
        req.prepare_payload();

        // Send the HTTP request to the remote host
        http::write(stream_, req);

        // This buffer is used for reading and must be persisted
        beast::flat_buffer buffer;

        // Declare a container to hold the response
        http::response<http::empty_body> res;

        // Receive the HTTP response
        http::read(stream_, buffer, res);

        beast::error_code ec;
        stream_.socket().shutdown(tcp::socket::shutdown_both, ec);
    }
};


Cache::Cache(std::string host, std::string port):
pImpl_(new Impl(host, port))
{
    std::cout << "Cache constructed\n";
}

void Cache::set(key_type key, val_type val, size_type size) { pImpl_->set(key, val, size); }
Cache::val_type Cache::get(key_type key, size_type& val_size) const { return pImpl_->get(key, val_size); }
bool Cache::del(key_type key) { return pImpl_->del(key); }
Cache::size_type Cache::space_used() const { return pImpl_->space_used(); }
void Cache::reset() { pImpl_->reset(); }
Cache::~Cache() {} // Previously called pImpl_.reset(), but had to be removed due to unknown Seg Fault-ing
                   // Regardless, valgrind confirms that our cache leaks no memory
// Reference: https://www.boost.org/doc/libs/1_72_0/libs/beast/doc/html/index.html
// FFR: To download all necessary boost libraries, use 'sudo apt-get install libboost-all-dev'

#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/version.hpp>
#include <boost/asio/dispatch.hpp>
#include <boost/asio/strand.hpp>
#include <boost/config.hpp>
#include <boost/program_options.hpp>
#include <boost/algorithm/string.hpp>
#include <algorithm>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <vector>
#include <sstream>
#include <mutex>
#include "cache.hh"
#include "lru_evictor.hh"

namespace beast = boost::beast;         // from <boost/beast.hpp>
namespace http = beast::http;           // from <boost/beast/http.hpp>
namespace net = boost::asio;            // from <boost/asio.hpp>
namespace po = boost::program_options;
using tcp = boost::asio::ip::tcp;       // from <boost/asio/ip/tcp.hpp>

std::mutex cache_mutex;

//------------------------------------------------------------------------------

// This function produces an HTTP response for the given
// request. The type of the response object depends on the
// contents of the request, so the interface requires the
// caller to pass a generic lambda for receiving the response.
template<
    class Body, class Allocator,
    class Send>
    void
    handle_request(
        Cache* serverCache,
        http::request<Body, http::basic_fields<Allocator>>&& req,
        Send&& send)
{
    // Returns a bad request response
    auto const bad_request =
        [&req](beast::string_view why)
    {
        http::response<http::string_body> res{ http::status::bad_request, req.version() };
        res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
        res.set(http::field::content_type, "text/html");
        res.keep_alive(req.keep_alive());
        res.body() = std::string(why);
        res.prepare_payload();
        return res;
    };

    // Make sure we can handle the method
    if (req.method() != http::verb::get &&
        req.method() != http::verb::put &&
        req.method() != http::verb::delete_ &&
        req.method() != http::verb::post &&
        req.method() != http::verb::head)
        return send(bad_request("Unknown HTTP-method"));

    //********************************************************************************
        // Respond to HEAD request
    if (req.method() == http::verb::head)
    {
        std::cout << "Handling a HEAD request...\n";
        http::response<http::empty_body> res { http::status::ok, req.version() };
        res.insert("Space-Used", std::to_string(serverCache->space_used()));
        res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
        res.set(http::field::accept, "/k/v");
        res.set(http::field::content_type, "application/json");
        //auto const size = res.body().size();
        //res.content_length(size);
        res.keep_alive(req.keep_alive());
        res.prepare_payload();
        return send(std::move(res));
    }

    // Respond to GET /k request
    if (req.method() == http::verb::get) {
        std::cout << "Handling a GET request...\n";
        // http://www.martinbroadhurst.com/how-to-split-a-string-in-c.html, method 5
        std::vector<std::string> splitBody;
        boost::split(splitBody, req.body(), boost::is_any_of("/")); // Uses body now
        assert(splitBody.size() == 2 && "splitBody was the wrong size (get)\n");
        //
        Cache::size_type val_size;
        cache_mutex.lock();
        Cache::val_type result = serverCache->get(splitBody[1], val_size);
        cache_mutex.unlock();
        //std::cout << "Server thinks the key is: " << splitBody[1] << "\n";
        //std::cout << "Server thinks the data is: " << result << "\n";
        //std::cout << "Server thinks the val size is: " << val_size << "\n";
        http::response<http::string_body> res{ http::status::ok, req.version() };
        res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
        std::string bodyMessage;
        if (result == nullptr) {
            bodyMessage = std::string("NULL");
        }
        else {
            std::string gotValue = result;
            bodyMessage = std::string("\"key\": \"") + 
                          splitBody[1] + 
                          std::string("\", \"value\": \"") + 
                          gotValue + 
                          std::string("\", \"size\": \"") +
                          std::to_string(val_size) +
                          std::string("\"");
        }
        std::cout << "Sending this back: " << bodyMessage << "\n";
        res.body() = bodyMessage;
        auto const size = bodyMessage.size();
        res.content_length(size);
        res.keep_alive(req.keep_alive());
        res.prepare_payload();
        return send(std::move(res));
    }

    // Respond to PUT /k/v/s request
    if (req.method() == http::verb::put) {
        std::cout << "Handling a PUT request...\n";
        // http://www.martinbroadhurst.com/how-to-split-a-string-in-c.html, method 5
        std::vector<std::string> splitBody;
        // std::cout << "The server recieved this set request: " << req.body() << "\n";
        boost::split(splitBody, req.body(), boost::is_any_of("/")); // Uses body now
        assert(splitBody.size() == 4 && "splitBody was the wrong size (put)\n");
        Cache::size_type size;
        // std::cout << "Before conversion: " << splitBody[3] << "\n";
        std::stringstream ss(splitBody[3]);
        ss >> size;
        Cache::val_type val = splitBody[2].c_str();
        // std::cout << "Key: " << splitBody[1] << "\n";
        // std::cout << "Value: " << val << "\n";
        // std::cout << "Size: " << size << "\n";
        cache_mutex.lock();
        serverCache->set(splitBody[1], val, size);
        cache_mutex.unlock();

        // Test:
        Cache::size_type gotten_size;
        // std::cout << "Testing get in set function!\n";
        std::string gotten_data( serverCache->get(splitBody[1], gotten_size) );
        std::cout << "Got data! " << gotten_data << "\n";
        std::cout << "Got size! " << gotten_size << "\n";
        assert(gotten_data == splitBody[2].c_str() && gotten_size == size && "Set was bad!\n");
         
        http::response<http::empty_body> res{ http::status::ok, req.version() };
        res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
        res.keep_alive(req.keep_alive());
        res.prepare_payload();
        return send(std::move(res));
    }

    // Respond to DELETE /k request
    if (req.method() == http::verb::delete_) {
        std::cout << "Handling a DEL request...\n";
        // http://www.martinbroadhurst.com/how-to-split-a-string-in-c.html, method 5
        std::vector<std::string> splitBody;
        boost::split(splitBody, req.body(), boost::is_any_of("/"));
        assert(splitBody.size() == 2 && "splitBody was the wrong size (put)\n");
        //
        cache_mutex.lock();
        bool answer = serverCache->del(splitBody[1]);
        cache_mutex.unlock();
        http::response<http::string_body> res{ http::status::ok, req.version() };
        res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
        //Computer is unhappy with boolean concatenation.
        std::string confirmation;
        if (answer) {
            confirmation = "True";
        }
        else
        {
            confirmation = "False";
        }
        std::string bodyMessage = confirmation;
        res.body() = bodyMessage;
        res.content_length(bodyMessage.size());
        res.keep_alive(req.keep_alive());
        res.prepare_payload();
        return send(std::move(res));
    }

    // Respond to POST /reset request. POST /"anything else" should fail.
    if (req.method() == http::verb::post) {
        std::cout << "Handling a POST request...\n";
        // http://www.martinbroadhurst.com/how-to-split-a-string-in-c.html, method 5
        std::vector<std::string> splitBody;
        boost::split(splitBody, req.body(), boost::is_any_of("/"));
        
        http::response<http::empty_body> res{ http::status::ok, req.version() };
        assert(splitBody.size() == 2 && "splitBody was the wrong size (put)\n");
        if (splitBody[1] == "reset") {
            cache_mutex.lock();
            serverCache->reset();
            cache_mutex.unlock();
            assert(serverCache->space_used() == 0 && "Reset failed!\n");
        }
        else {
            res.result(http::status::not_found);
        }
        
        res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
        res.keep_alive(req.keep_alive());
        res.prepare_payload();
        return send(std::move(res));
    }
}
//********************************************************************************
//------------------------------------------------------------------------------

// Report a failure
void
fail(beast::error_code ec, char const* what)
{
    std::cerr << what << ": " << ec.message() << "\n";
}

// Handles an HTTP server connection
class session : public std::enable_shared_from_this<session>
{
    // This is the C++11 equivalent of a generic lambda.
    // The function object is used to send an HTTP message.
    struct send_lambda
    {
        session& self_;

        explicit
            send_lambda(session& self)
            : self_(self)
        {
        }

        template<bool isRequest, class Body, class Fields>
        void
            operator()(http::message<isRequest, Body, Fields>&& msg) const
        {
            // The lifetime of the message has to extend
            // for the duration of the async operation so
            // we use a shared_ptr to manage it.
            auto sp = std::make_shared<
                http::message<isRequest, Body, Fields>>(std::move(msg));

            // Store a type-erased version of the shared
            // pointer in the class to keep it alive.
            self_.res_ = sp;

            // Write the response
            http::async_write(
                self_.stream_,
                *sp,
                beast::bind_front_handler(
                    &session::on_write,
                    self_.shared_from_this(),
                    sp->need_eof()));
        }
    };

    beast::tcp_stream stream_;
    beast::flat_buffer buffer_;
    Cache* serverCache_;
    http::request<http::string_body> req_;
    std::shared_ptr<void> res_;
    send_lambda lambda_;

public:
    // Take ownership of the stream
    session(
        tcp::socket&& socket,
        Cache* serverCache)
        : stream_(std::move(socket))
        , serverCache_(serverCache)
        , lambda_(*this)
    {
    }

    // Start the asynchronous operation
    void
        run()
    {
        // We need to be executing within a strand to perform async operations
        // on the I/O objects in this session. Although not strictly necessary
        // for single-threaded contexts, this example code is written to be
        // thread-safe by default.

        net::dispatch(stream_.get_executor(),
            beast::bind_front_handler(
                &session::do_read,
                shared_from_this()));
    }

    void
        do_read()
    {
        // Make the request empty before reading,
        // otherwise the operation behavior is undefined.
        req_ = {};

        // Set the timeout.
        stream_.expires_after(std::chrono::seconds(30));

        // Read a request
        http::async_read(stream_, buffer_, req_,
            beast::bind_front_handler(
                &session::on_read,
                shared_from_this()));
    }

    void
        on_read(
            beast::error_code ec,
            std::size_t bytes_transferred)
    {
        boost::ignore_unused(bytes_transferred);
        // This means they closed the connection
        if (ec == http::error::end_of_stream)
            return do_close();


        if (ec)
            return fail(ec, "read");
        
        // Send the response
        handle_request(serverCache_, std::move(req_), lambda_);
    }

    void
        on_write(
            bool close,
            beast::error_code ec,
            std::size_t bytes_transferred)
    {
        boost::ignore_unused(bytes_transferred);

        if (ec)
            return fail(ec, "write");

        if (close)
        {
            // This means we should close the connection, usually because
            // the response indicated the "Connection: close" semantic.
            return do_close();
        }

        // We're done with the response so delete it
        res_ = nullptr;

        // Read another request
        do_read();
    }

    void
        do_close()
    {
        // Send a TCP shutdown
        beast::error_code ec;
        stream_.socket().shutdown(tcp::socket::shutdown_send, ec);

        // At this point the connection is closed gracefully
    }
};

//------------------------------------------------------------------------------

// Accepts incoming connections and launches the sessions
class listener : public std::enable_shared_from_this<listener>
{
    net::io_context& ioc_;
    tcp::acceptor acceptor_;
    Cache* serverCache_;

public:
    listener(
        net::io_context& ioc,
        tcp::endpoint endpoint,
        Cache* serverCache)
        : ioc_(ioc)
        , acceptor_(net::make_strand(ioc))
        , serverCache_(serverCache)
    {
        beast::error_code ec;

        // Open the acceptor
        acceptor_.open(endpoint.protocol(), ec);
        if (ec)
        {
            fail(ec, "open");
            return;
        }

        // Allow address reuse
        acceptor_.set_option(net::socket_base::reuse_address(true), ec);
        if (ec)
        {
            fail(ec, "set_option");
            return;
        }

        // Bind to the server address
        acceptor_.bind(endpoint, ec);
        if (ec)
        {
            fail(ec, "bind");
            return;
        }

        // Start listening for connections
        acceptor_.listen(
            net::socket_base::max_listen_connections, ec);
        if (ec)
        {
            fail(ec, "listen");
            return;
        }
    }

    // Start accepting incoming connections
    void
        run()
    {
        do_accept();
    }

private:
    void
        do_accept()
    {
        // The new connection gets its own strand
        acceptor_.async_accept(
            net::make_strand(ioc_),
            beast::bind_front_handler(
                &listener::on_accept,
                shared_from_this()));
    }

    void
        on_accept(beast::error_code ec, tcp::socket socket)
    {
        if (ec)
        {
            fail(ec, "accept");
        }
        else
        {
            // Create the session and run it
            std::make_shared<session>(
                std::move(socket), serverCache_)->run();
        }

        // Accept another connection
        do_accept();
    }
};

//------------------------------------------------------------------------------

int main(int argc, char** argv) {

    // Declare the supported options.
    po::options_description desc("Allowed options");
    desc.add_options()
        ("-s", po::value<std::string>()->default_value("127.0.0.1"), "define host server (default 127.0.0.1)")
        ("-p", po::value<unsigned short>()->default_value(3618), "define port number (default 3618)")
        ("-t", po::value<int>()->default_value(1), "define thread count (default 1)")
        ("-m", po::value<Cache::size_type>()->default_value(1024), "set maxmem (default 10)");

    po::variables_map vm;
    po::store(po::parse_command_line(argc, argv, desc), vm);
    po::notify(vm);
    
    net::ip::address const address = net::ip::make_address(vm["-s"].as<std::string>());
    unsigned short const port = vm["-p"].as<unsigned short>();
    auto const threads = vm["-t"].as<int>();
    auto const maxmem = vm["-m"].as<Cache::size_type>();
    std::cout << "Created cache of size " << maxmem << " with " << threads << " threads\n";
    std::cout << "Operating with address " << address << ", on port " << port << ".\n";

    // Evictor implementation is new
    // FIFO_Evictor f_evictor = FIFO_Evictor();
    LRU_Evictor lru_evictor = LRU_Evictor();

    Cache serverCache = Cache(maxmem, 0.75, &lru_evictor);
    Cache* s_cache = &serverCache;

    // The io_context is required for all I/O
    net::io_context ioc{ threads };

    // Create and launch a listening port
    std::make_shared<listener>(
        ioc,
        tcp::endpoint{ address, port }, s_cache)->run();

    // Run the I/O service on the requested number of threads
    std::vector<std::thread> v;
    v.reserve(threads - 1);
    for (auto i = threads - 1; i > 0; --i)
        v.emplace_back(
            [&ioc]
            {
                ioc.run();
            });
    ioc.run();

    return EXIT_SUCCESS;
}
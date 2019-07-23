#include <nano/lib/json_error_response.hpp>
#include <nano/rpc/rpc_connection_secure.hpp>
#include <nano/rpc/rpc_secure.hpp>

#include <boost/algorithm/string/predicate.hpp>
#include <boost/polymorphic_pointer_cast.hpp>

nano::rpc_connection_secure::rpc_connection_secure (nano::rpc_config const & rpc_config, boost::asio::io_context & io_ctx, nano::logger_mt & logger, nano::rpc_handler_interface & rpc_handler_interface, boost::asio::ssl::context & ssl_context) :
nano::rpc_connection (rpc_config, io_ctx, logger, rpc_handler_interface),
stream (socket, ssl_context)
{
}

void nano::rpc_connection_secure::parse_connection ()
{
	// Perform the SSL handshake
	auto this_l = std::static_pointer_cast<nano::rpc_connection_secure> (shared_from_this ());
	stream.async_handshake (boost::asio::ssl::stream_base::server,
	[this_l](auto & ec) {
		this_l->handle_handshake (ec);
	});
}

void nano::rpc_connection_secure::read ()
{
	auto this_l = std::static_pointer_cast<nano::rpc_connection_secure> (shared_from_this ());
	auto header_parser (std::make_shared<boost::beast::http::request_parser<boost::beast::http::empty_body>> ());
	header_parser->body_limit (rpc_config.max_request_size);
	boost::beast::http::async_read_header (stream, buffer, *header_parser, boost::asio::bind_executor (strand, [this_l, header_parser](boost::system::error_code const & ec, size_t bytes_transferred) {
		if (!ec)
		{
			if (boost::iequals (header_parser->get ()[boost::beast::http::field::expect], "100-continue"))
			{
				auto continue_response (std::make_shared<boost::beast::http::response<boost::beast::http::empty_body>> ());
				continue_response->version (11);
				continue_response->result (boost::beast::http::status::continue_);
				continue_response->set (boost::beast::http::field::server, "nano");
				boost::beast::http::async_write (this_l->stream, *continue_response, boost::asio::bind_executor (this_l->strand, [this_l, continue_response](boost::system::error_code const & ec, size_t bytes_transferred) {}));
			}

			this_l->parse_request (header_parser);
		}
		else
		{
			this_l->logger.always_log ("RPC header error: ", ec.message ());

			// Respond with the reason for the invalid header
			auto response_handler ([this_l](std::string const & tree_a) {
				this_l->write_result (tree_a, 11);
				boost::beast::http::async_write (this_l->stream, this_l->res, boost::asio::bind_executor (this_l->strand, [this_l](boost::system::error_code const & ec, size_t bytes_transferred) {
					this_l->write_completion_handler (this_l);
				}));
			});
			json_error_response (response_handler, std::string ("Invalid header: ") + ec.message ());
		}
	}));
}

void nano::rpc_connection_secure::parse_request (std::shared_ptr<boost::beast::http::request_parser<boost::beast::http::empty_body>> header_parser)
{
	auto this_l = std::static_pointer_cast<nano::rpc_connection_secure> (shared_from_this ());
	auto body_parser (std::make_shared<boost::beast::http::request_parser<boost::beast::http::string_body>> (std::move (*header_parser)));
	boost::beast::http::async_read (stream, buffer, *body_parser, boost::asio::bind_executor (strand, [this_l, body_parser](boost::system::error_code const & ec, size_t bytes_transferred) {
		if (!ec)
		{
			this_l->io_ctx.post ([this_l, body_parser]() {
				auto & req (body_parser->get ());
				auto start (std::chrono::steady_clock::now ());
				auto version (req.version ());
				std::stringstream ss;
				ss << std::hex << std::showbase << reinterpret_cast<uintptr_t> (this_l.get ());
				auto request_id = ss.str ();
				auto response_handler ([this_l, version, start, request_id](std::string const & tree_a) {
					auto body = tree_a;
					this_l->write_result (body, version);
					boost::beast::http::async_write (this_l->stream, this_l->res, boost::asio::bind_executor (this_l->strand, [this_l](boost::system::error_code const & ec, size_t bytes_transferred) {
						this_l->write_completion_handler (this_l);
					}));

					std::stringstream ss;
					ss << "RPC request " << request_id << " completed in: " << std::chrono::duration_cast<std::chrono::microseconds> (std::chrono::steady_clock::now () - start).count () << " microseconds";
					this_l->logger.always_log (ss.str ().c_str ());
				});
				auto method = req.method ();
				switch (method)
				{
					case boost::beast::http::verb::post:
					{
						auto handler (std::make_shared<nano::rpc_handler> (this_l->rpc_config, req.body (), request_id, response_handler, this_l->rpc_handler_interface, this_l->logger));
						handler->process_request ();
						break;
					}
					case boost::beast::http::verb::options:
					{
						this_l->prepare_head (version);
						this_l->res.prepare_payload ();
						boost::beast::http::async_write (this_l->stream, this_l->res, boost::asio::bind_executor (this_l->strand, [this_l](boost::system::error_code const & ec, size_t bytes_transferred) {
							this_l->write_completion_handler (this_l);
						}));
						break;
					}
					default:
					{
						json_error_response (response_handler, "Can only POST requests");
						break;
					}
				}
			});
		}
		else
		{
			this_l->logger.always_log ("RPC read error: ", ec.message ());
		}
	}));
}

void nano::rpc_connection_secure::on_shutdown (const boost::system::error_code & error)
{
	// No-op. We initiate the shutdown (since the RPC server kills the connection after each request)
	// and we'll thus get an expected EOF error. If the client disconnects, a short-read error will be expected.
}

void nano::rpc_connection_secure::handle_handshake (const boost::system::error_code & error)
{
	if (!error)
	{
		read ();
	}
	else
	{
		logger.always_log ("TLS: Handshake error: ", error.message ());
	}
}

void nano::rpc_connection_secure::write_completion_handler (std::shared_ptr<nano::rpc_connection> rpc)
{
	auto rpc_connection_secure = boost::polymorphic_pointer_downcast<nano::rpc_connection_secure> (rpc);
	rpc_connection_secure->stream.async_shutdown (boost::asio::bind_executor (rpc->strand, [rpc_connection_secure](auto const & ec_shutdown) {
		rpc_connection_secure->on_shutdown (ec_shutdown);
	}));
}

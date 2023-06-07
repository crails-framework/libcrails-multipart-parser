#include <crails/server/connection.hpp>
#include <crails/context.hpp>
#include <crails/logger.hpp>
#include "multipart_parser.hpp"

using namespace std;
using namespace Crails;

void RequestMultipartParser::operator()(Context& context, function<void(RequestParser::Status)> callback) const
{
  static const regex is_multipart("^multipart/form-data", regex_constants::extended);
  const HttpRequest& request = context.connection->get_request();

  if (request.method() != HttpVerb::get && content_type_matches(request, is_multipart))
  {
    parse_multipart(context, [callback]()
    {
      callback(RequestParser::Stop);
    });
  }
  else
    callback(RequestParser::Continue);
}

RequestMultipartParser::PendingBody::PendingBody(Context& c)
  :
  connection(*c.connection),
  params(c.params)
{
  multipart_parser.initialize(c);
}

void RequestMultipartParser::on_receive(shared_ptr<PendingBody> pending_body, Context& context, std::string_view chunk) const
{
  MultipartParser& multipart_parser = pending_body->multipart_parser;

  multipart_parser.total_read += chunk.length();
  copy(chunk.begin(), chunk.end(), back_inserter(multipart_parser.read_buffer));
  multipart_parser.parse(pending_body->params);
  if (multipart_parser.total_read >= multipart_parser.to_read)
    pending_body->finished_callback();
}

void RequestMultipartParser::parse_multipart(Context& context, function<void()> finished_callback) const
{
  shared_ptr<PendingBody> pending_body = make_shared<PendingBody>(context);
  auto connection = context.connection;
  const auto& request = connection->get_request();

  pending_body->finished_callback = finished_callback;
  connection->on_received_body_chunk([this, &context, pending_body](string_view chunk)
  {
    on_receive(pending_body, context, chunk);
  });
}

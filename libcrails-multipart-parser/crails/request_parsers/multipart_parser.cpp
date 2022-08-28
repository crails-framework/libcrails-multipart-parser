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
    parse_multipart(*context.connection, context.params, [callback]()
    {
      callback(RequestParser::Stop);
    });
  }
  else
    callback(RequestParser::Continue);
}

RequestMultipartParser::PendingBody::PendingBody(Connection& c, Params& p)
  : connection(c), params(p)
{
  multipart_parser.initialize(params);
}

void RequestMultipartParser::on_receive(shared_ptr<PendingBody> pending_body, Connection& connection) const
{
  MultipartParser& multipart_parser = pending_body->multipart_parser;
  const string& body = connection.get_request().body();
  unsigned int offset = multipart_parser.read_buffer.length();

  multipart_parser.total_read = body.length();
  for (unsigned int i = 0 ; i < body.length() - offset ; ++i)
    multipart_parser.read_buffer += body[i + offset];
  multipart_parser.parse(pending_body->params);
  if (multipart_parser.total_read < multipart_parser.to_read)
    throw std::runtime_error("RequestMutlipartParser: Asynchronous body reception not implemented");
  else
    pending_body->finished_callback();
}

void RequestMultipartParser::parse_multipart(Connection& connection, Params& params, function<void()> finished_callback) const
{
  shared_ptr<PendingBody> pending_body = make_shared<PendingBody>(connection, params);

  pending_body->finished_callback = finished_callback;
  on_receive(pending_body, connection);
}

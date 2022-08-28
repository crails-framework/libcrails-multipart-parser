#ifndef  CRAILS_MULTIPART_PARSER_HPP
# define CRAILS_MULTIPART_PARSER_HPP

# include <crails/request_parser.hpp>
# include "../multipart.hpp"

namespace Crails
{
  class RequestMultipartParser : public RequestParser
  {
  public:
    void operator()(Context&, std::function<void(RequestParser::Status)>) const override;
  private:
    void parse_multipart(Connection&, Params&, std::function<void()>) const;

    struct PendingBody
    {
      PendingBody(Connection&, Params&);

      Connection&            connection;
      Params&                params;
      MultipartParser        multipart_parser;
      std::function<void()>  finished_callback;
    };

    void on_receive(std::shared_ptr<PendingBody>, Connection&) const;
  };
}

#endif

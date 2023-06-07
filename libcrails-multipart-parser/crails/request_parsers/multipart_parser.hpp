#ifndef  CRAILS_MULTIPART_PARSER_HPP
# define CRAILS_MULTIPART_PARSER_HPP

# include <crails/request_parser.hpp>
# include "../multipart.hpp"

namespace Crails
{
  class Context;

  class RequestMultipartParser : public RequestParser
  {
  public:
    void operator()(Context&, std::function<void(RequestParser::Status)>) const override;
  private:
    void parse_multipart(Context&, std::function<void()>) const;

    struct PendingBody
    {
      PendingBody(Context&);

      Connection&            connection;
      Params&                params;
      MultipartParser        multipart_parser;
      std::function<void()>  finished_callback;
    };

    void on_receive(std::shared_ptr<PendingBody>, Context&, std::string_view chunk) const;
  };
}

#endif

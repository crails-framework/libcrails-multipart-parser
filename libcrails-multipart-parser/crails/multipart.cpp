#include <crails/server.hpp>
#include <crails/utils/string.hpp>
#include <crails/utils/random_string.hpp>
#include <crails/params.hpp>
#include <crails/logger.hpp>
#include <crails/utils/parse_cookie_values.hpp>
#include <boost/lexical_cast.hpp>
#include "multipart.hpp"
#include <fstream>
#include <regex>

using namespace std;
using namespace Crails;

void MultipartParser::parse(Params& params)
{
  bool blocked;

  do
  {
    blocked = true;
    if (parsed_state == 0)
    {
      regex reg_header(boundary + "\r\nContent-Disposition: ([^;]+);", regex_constants::ECMAScript);
      auto  matches = sregex_iterator(read_buffer.begin(), read_buffer.end(), reg_header);

      if (distance(matches, sregex_iterator()) > 0)
      {
        smatch       match    = *matches;
        unsigned int disp_so  = match.position(1);
        unsigned int disp_len = match.length(1);
        unsigned int form_end = disp_so + disp_len + 1;
        //unsigned int form_end = match.length(0);

        content_disposition = read_buffer.substr(disp_so, disp_len);
        read_buffer.erase(0, form_end);
        parsed_state++;
      }
    }
    if (parsed_state == 1)
    {
      static const string delimiter("\r\n");
      size_t form_len = read_buffer.find(delimiter);

      if (form_len != std::string::npos)
      {
        parse_cookie_values(read_buffer.substr(0, form_len), [this](const string& key, const string& val) -> bool
        {
          content_data[key] = val;
          return true;
        });
        content_data.as_data().each([this](Data data) -> bool
        {
          string str = data.as<std::string>();

          if (str.size() > 0)
            str.erase(0, 1);
          if (str.size() > 0)
            str.erase(str.size() - 1, str.size());
          data = str;
	  return true;
        });
        read_buffer.erase(0, form_len + delimiter.length());
        parsed_state++;
      }
    }
    if (parsed_state == 2)
    {
      if (read_buffer.substr(0, 14) == "Content-Type: ")
      {
        size_t end = read_buffer.find("\r\n");

        if (end != string::npos)
        {
          mimetype = read_buffer.substr(14, end - 14);
          read_buffer.erase(0, end + 4);
          parsed_state ++;
        }
      }
      else if (read_buffer.substr(0, 2) == "\r\n")
      {
        read_buffer.erase(0, 2);
        parsed_state++;
      }
    }
    if (parsed_state == 3)
    {
      size_t pos = read_buffer.find(boundary);

      if (mimetype == "")
      {
        if (pos != string::npos)
        {
          string field = content_data["name"];
          string value = read_buffer.substr(0, pos - 4);
          string ptree_key;

          // From CGI parameter to Ptree parameter
          for (size_t i = 0 ; i < field.length() ; ++i)
          {
            if (field[i] == '[')
              ptree_key += '.';
            else if (field[i] != ']')
              ptree_key += field[i];
          }
          if (*ptree_key.rbegin() == '.') // is array ?
          {
            ptree_key.pop_back();
            if (params[ptree_key].exists())
              params[ptree_key].push_back(value);
            else
              params[ptree_key].from_vector(std::vector<std::string>({value}));
          }
          else
            params[ptree_key] = value;
          // END From CGI parameter to Ptree parameter

          read_buffer.erase(0, pos);
          parsed_state = 0;
          blocked = false;
        }
      }
      else
      {
        unsigned int to_copy, to_erase;
        bool         to_close = false;

        if (!(file.is_open()))
        {
          content_data["filepath"] = Server::singleton::require().get_temporary_path() + '/' + Crails::generate_random_string("ABCDEF012345556789", 12);
          file.open(content_data["filepath"].as<std::string>());
          if (!(file.is_open()))
            logger << Logger::Info << "/!\\ Cannot open file " << content_data["filepath"].as<string>() << Logger::endl;
        }
        if (pos != string::npos)
        {
          Params::File to_push;

          to_push.key            = content_data["name"].as<std::string>();
          to_push.name           = content_data["filename"].as<std::string>();
          to_push.mimetype       = mimetype;
          to_push.temporary_path = content_data["filepath"].as<std::string>();
          params.files.push_back(to_push);
          to_erase               = pos;
          to_copy                = pos - 2;
          parsed_state           = 0;
          blocked                = read_buffer.size() <= pos + boundary.size();
          mimetype               = "";
          to_close               = true;
        }
        else
          to_copy = to_erase = read_buffer.size();
        file.write(read_buffer.c_str(), to_copy - 2);
        read_buffer.erase(0, to_erase - 2);
        if (to_close)
          file.close();
      }
    }
  } while (!blocked);
}

static string_view get_content_type(const HttpRequest& request)
{
  auto field = request.find(HttpHeader::content_type);

  if (field != request.end())
    return string_view(field->value().data(), field->value().length());
  return string_view();
}

static unsigned int get_content_length(const HttpRequest& request)
{
  auto field = request.find(HttpHeader::content_length);

  if (field != request.end())
    return boost::lexical_cast<unsigned int>(field->value());
  return 0;
}

void MultipartParser::initialize(Context& context)
{
  optional<uint64_t> content_length;
  const auto& request = context.connection->get_request();
  string      type(get_content_type(request));
  regex       get_boundary("boundary=(.*)", regex_constants::ECMAScript);
  auto        matches  = sregex_iterator(type.begin(), type.end(), get_boundary);
  smatch      match    = *matches;

  to_read      = get_content_length(request);
  total_read   = 0;
  boundary     = type.substr(match.position(1), match.length(1));
  parsed_state = 0;
}

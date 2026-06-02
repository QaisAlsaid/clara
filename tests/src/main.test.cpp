#include <clara/clara.hpp>
#include <iostream>

void print_cmd(const std::string&, const clara::parse::command& cmd);
void test_lexer(int argc, char** argv);
void test_parser(int argc, char** argv);

std::string opts[] { "lexer", "parser" };
std::string& opt = opts[1];

int main(int argc, char** argv)
{
    if(opt == std::string("lexer"))
    {
      std::cout << "lexer test...\n";
      test_lexer(argc, argv);
    }
    else if(opt == std::string("parser"))
    {
      std::cout << "parser test...\n";
      test_parser(argc, argv);
    }
}

void test_lexer(int argc, char** argv)
{
  auto args_str = clara::detail::join_args(argc, argv);
  clara::detail::lexer lx(args_str);

  auto tok = lx.advance();
  std::cout << "first token: type: " << clara::detail::token_type_to_string(tok.type) << " literal: " << tok.literal << "\n";

  while(tok.type != clara::detail::token_type::eof)
  {
    std::cout << "token: type: " << clara::detail::token_type_to_string(tok.type) << " literal: " << tok.literal << "\n";
    tok = lx.advance();
  }
}

void test_parser(int argc, char** argv)
{
  clara::parse::parser ps;
  ps.add_option("opt").requires_value().allow_multiple().set_alias("o");
  ps.add_flag("f");
  auto& parser = ps.add_subcommand("parser");

  //ps.add_option("opt");
  //ps.add_flag("f");
  //ps.add_flag("d");
  //ps.add_option("قيس");
  parser.add_option("opt").set_alias("o").set_alias_options(clara::parse::option_builder::alias_options::_default);
  //parser.add_option("قيس");
  //parser.add_flag("ق");
  auto pr = ps.parse(argc, argv);
  print_cmd("root", pr.root);
}

void print_cmd(const std::string& n, const clara::parse::command& cmd)
{
  std::cout << "cmd: [" << n << " raw data: " << cmd.get_raw() << "\n";
  for(const auto& scmd : cmd.get_subcommands())
    print_cmd(scmd.first, scmd.second);

  for(const auto& opt : cmd.get_options())
  {
    std::cout << "option: " << opt.first << " value: " << opt.second.get_raw() << "\n";
  }

  for(const auto& f : cmd.get_flags())
  {
    std::cout << "flag: " << f.first << "\n";
  }

  std::cout << "\n]\n"; 
}

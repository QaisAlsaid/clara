#include <clara/clara.hpp>

using namespace clara::parse;

class test_case
{
protected:
  parser p;
public:
  std::string name;
  virtual void set_up() = 0;
  virtual bool run() = 0;
};

std::vector<char*> make_argv(const std::vector<std::string>& i)
{
  std::vector<char*> v;
  for(auto x : i)
    v.push_back(const_cast<char*>(x.c_str()));
  return v;
}

void test_parser();

int main(int argc, char** argv)
{
  std::cout << "argc: " << argc << "\n";
  test_parser();
}

void test_parser()
{
  class test_single_flag : public test_case
  {
  public:
    void set_up() override
    {
      name = "test_single_flag";
      //p = parser{};
    }

    bool run() override
    {
      p.add_flag("x");
      p.add_flag("f");
      //char* arr[] = { "git", "-h" };
      //auto pr = p.parse(2, arr);
      //auto argv = make_argv({"git", "-h"});
      //auto pr = p.parse(argv.size() - 1, argv.data());
      auto pr = p.parse({ "./cmd", "-xf" });
      //for(auto f : pr.root.get_flags())
      //  std::cout << "flaggg: " << f.first << "\n";
      return pr.root.get_flag("x").has_value();
    }
  } t;
  t.set_up();
  std::cout << "==== starting test: '" << t.name << "' ====\n";
  auto res = t.run();
  std::cout << "==== test: '" << t.name << "' finished: result: " << (res ? "pass" : "fail") << "\n";
}

/*******************************************************************\

Module: C++ Language Module

Author: Daniel Kroening, kroening@cs.cmu.edu

\*******************************************************************/

#include <ansi-c/c_main.h>
#include <ansi-c/c_preprocess.h>
#include <ansi-c/gcc_builtin_headers.h>
#include <cpp/cpp_final.h>
#include <cpp/cpp_language.h>
#include <cpp/cpp_parser.h>
#include <cpp/cpp_typecheck.h>
#include <cpp/expr2cpp.h>
#include <cstring>
#include <fstream>
#include <sstream>
#include <util/c_link.h>
#include <util/config.h>
#include <util/replace_symbol.h>

bool cpp_languaget::preprocess(
  const std::string &path,
  std::ostream &outstream,
  message_handlert &message_handler)
{
  if(path=="")
    return c_preprocess("", outstream, true, message_handler);

  // check extension

  const char *ext=strrchr(path.c_str(), '.');
  if(ext!=nullptr && std::string(ext)==".ipp")
  {
    std::ifstream infile(path.c_str());

    char ch;

    while(infile.read(&ch, 1))
      outstream << ch;

    return false;
  }

  return c_preprocess(path, outstream, true, message_handler);
}

void cpp_languaget::internal_additions(std::ostream &out)
{
  out << "# 1 \"esbmc_intrinsics.h" << std::endl;

  out << "void *operator new(unsigned int size);" << std::endl;

  out << "extern \"C\" {" << std::endl;

  // assume/assert
  out << "void __ESBMC_assume(bool assumption);" << std::endl;
  out << "void assert(bool assertion);" << std::endl;
  out << "void __ESBMC_assert("
         "bool assertion, const char *description);" << std::endl;
  out << "bool __ESBMC_same_object(const void *, const void *);" << std::endl;

  // pointers
  out << "unsigned __ESBMC_POINTER_OBJECT(const void *p);" << std::endl;
  out << "signed __ESBMC_POINTER_OFFSET(const void *p);" << std::endl;

  out << "extern \"C\" int __ESBMC_rounding_mode = 0;" << std::endl;

  // __CPROVER namespace
  out << "namespace __CPROVER { }" << std::endl;

  // for dynamic objects
  out << "unsigned __CPROVER::constant_infinity_uint;" << std::endl;
  out << "bool __ESBMC_alloc[__CPROVER::constant_infinity_uint];" << std::endl;
  out << "unsigned __ESBMC_alloc_size[__CPROVER::constant_infinity_uint];" << std::endl;
  out << "bool __ESBMC_deallocated[__CPROVER::constant_infinity_uint];" << std::endl;
  out << "bool __ESBMC_is_dynamic[__CPROVER::constant_infinity_uint];" << std::endl;

  // float stuff
  out << "bool __ESBMC_isnan(double f);" << std::endl;
  out << "bool __ESBMC_isfinite(double f);" << std::endl;
  out << "bool __ESBMC_isinf(double f);" << std::endl;
  out << "bool __ESBMC_isnormal(double f);" << std::endl;
  out << "int __ESBMC_rounding_mode;" << std::endl;

  // absolute value
  out << "int __ESBMC_abs(int x);" << std::endl;
  out << "long int __ESBMC_labs(long int x);" << std::endl;
  out << "double __ESBMC_fabs(double x);" << std::endl;
  out << "long double __ESBMC_fabsl(long double x);" << std::endl;
  out << "float __ESBMC_fabsf(float x);" << std::endl;

  // Digital controllers code
  out << "void __ESBMC_generate_cascade_controllers(float * cden, int csize, float * cout, int coutsize, bool isDenominator);" << std::endl;
  out << "void __ESBMC_generate_delta_coefficients(float a[], double out[], float delta);" << std::endl;
  out << "bool __ESBMC_check_delta_stability(double dc[], double sample_time, int iwidth, int precision);" << std::endl;

  // Forward decs for pthread main thread begin/end hooks. Because they're
  // pulled in from the C library, they need to be declared prior to pulling
  // them in, for type checking.
  out << "void pthread_start_main_hook(void);" << std::endl;
  out << "void pthread_end_main_hook(void);" << std::endl;

  // GCC stuff
  out << GCC_BUILTIN_HEADERS;

  // Forward declarations for nondeterministic types.
  out << "int nondet_int();" << std::endl;
  out << "unsigned int nondet_uint();" << std::endl;
  out << "long nondet_long();" << std::endl;
  out << "unsigned long nondet_ulong();" << std::endl;
  out << "short nondet_short();" << std::endl;
  out << "unsigned short nondet_ushort();" << std::endl;
  out << "short nondet_short();" << std::endl;
  out << "unsigned short nondet_ushort();" << std::endl;
  out << "char nondet_char();" << std::endl;
  out << "unsigned char nondet_uchar();" << std::endl;
  out << "signed char nondet_schar();" << std::endl;

  // And again, for TACAS VERIFIER versions,
  out << "int __VERIFIER_nondet_int();" << std::endl;
  out << "unsigned int __VERIFIER_nondet_uint();" << std::endl;
  out << "long __VERIFIER_nondet_long();" << std::endl;
  out << "unsigned long __VERIFIER_nondet_ulong();" << std::endl;
  out << "short __VERIFIER_nondet_short();" << std::endl;
  out << "unsigned short __VERIFIER_nondet_ushort();" << std::endl;
  out << "short __VERIFIER_nondet_short();" << std::endl;
  out << "unsigned short __VERIFIER_nondet_ushort();" << std::endl;
  out << "char __VERIFIER_nondet_char();" << std::endl;
  out << "unsigned char __VERIFIER_nondet_uchar();" << std::endl;
  out << "signed char __VERIFIER_nondet_schar();" << std::endl;

  out << "const char *__PRETTY_FUNCTION__;" << std::endl;
  out << "const char *__FILE__ = \"\";" << std::endl;
  out << "unsigned int __LINE__ = 0;" << std::endl;

  out << "}" << std::endl;
}

bool cpp_languaget::parse(
  const std::string &path,
  message_handlert &message_handler)
{
  // store the path

  parse_path=path;

  // preprocessing

  std::ostringstream o_preprocessed;

  internal_additions(o_preprocessed);

  if(preprocess(path, o_preprocessed, message_handler))
    return true;

  std::istringstream i_preprocessed(o_preprocessed.str());

  // parsing

  cpp_parser.clear();
  cpp_parser.filename=path;
  cpp_parser.in=&i_preprocessed;
  cpp_parser.set_message_handler(&message_handler);
  cpp_parser.grammar=cpp_parsert::LANGUAGE;

  if(config.ansi_c.os==configt::ansi_ct::OS_WIN32)
    cpp_parser.mode=cpp_parsert::MSC;
  else
    cpp_parser.mode=cpp_parsert::GCC;

  cpp_scanner_init();

  bool result=cpp_parser.parse();

  // save result
  cpp_parse_tree.swap(cpp_parser.parse_tree);

  // save some memory
  cpp_parser.clear();

  return result;
}

bool cpp_languaget::typecheck(
  contextt &context,
  const std::string &module,
  message_handlert &message_handler)
{
  contextt new_context;

  if(cpp_typecheck(cpp_parse_tree, new_context, module, message_handler))
    return true;

  return c_link(context, new_context, message_handler, module);
}

bool cpp_languaget::final(
  contextt &context,
  message_handlert &message_handler)
{
  if(cpp_final(context, message_handler)) return true;
  if(c_main(context, "main", message_handler)) return true;

  return false;
}

void cpp_languaget::show_parse(std::ostream &out)
{
  for(cpp_parse_treet::itemst::const_iterator it=
      cpp_parse_tree.items.begin();
      it!=cpp_parse_tree.items.end();
      it++)
    show_parse(out, *it);
}

void cpp_languaget::show_parse(
  std::ostream &out,
  const cpp_itemt &item)
{
  if(item.is_linkage_spec())
  {
    const cpp_linkage_spect &linkage_spec=
      item.get_linkage_spec();

    for(const auto & it : linkage_spec.items())
      show_parse(out, it);

    out << std::endl;
  }
  else if(item.is_namespace_spec())
  {
    const cpp_namespace_spect &namespace_spec=
      item.get_namespace_spec();

    out << "NAMESPACE " << namespace_spec.get_namespace()
        << ":" << std::endl;

    for(const auto & it : namespace_spec.items())
      show_parse(out, it);

    out << std::endl;
  }
  else if(item.is_using())
  {
    const cpp_usingt &cpp_using=item.get_using();

    out << "USING ";
    if(cpp_using.get_namespace())
      out << "NAMESPACE ";
    out << cpp_using.name() << std::endl;
    out << std::endl;
  }
  else if(item.is_declaration())
  {
    item.get_declaration().output(out);
  }
  else
    out << "UNKNOWN: " << item << std::endl;
}

languaget *new_cpp_language()
{
  return new cpp_languaget;
}

bool cpp_languaget::from_expr(
  const exprt &expr,
  std::string &code,
  const namespacet &ns,
  bool fullname)
{
  code=expr2cpp(expr, ns, fullname);
  return false;
}

bool cpp_languaget::from_type(
  const typet &type,
  std::string &code,
  const namespacet &ns,
  bool fullname)
{
  code=type2cpp(type, ns, fullname);
  return false;
}

bool cpp_languaget::to_expr(
  const std::string &code,
  const std::string &module __attribute__((unused)),
  exprt &expr,
  message_handlert &message_handler,
  const namespacet &ns)
{
  expr.make_nil();

  // no preprocessing yet...

  std::istringstream i_preprocessed(code);

  // parsing

  cpp_parser.clear();
  cpp_parser.filename="";
  cpp_parser.in=&i_preprocessed;
  cpp_parser.set_message_handler(&message_handler);
  cpp_parser.grammar=cpp_parsert::EXPRESSION;
  cpp_scanner_init();

  bool result=cpp_parser.parse();

  if(cpp_parser.parse_tree.items.empty())
    result=true;
  else
  {
    // TODO
    //expr.swap(cpp_parser.parse_tree.declarations.front());

    // typecheck it
    result=cpp_typecheck(expr, message_handler, ns);
  }

  // save some memory
  cpp_parser.clear();

  return result;
}

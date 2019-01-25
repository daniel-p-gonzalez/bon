/*----------------------------------------------------------------------------*\
|*
|* Main entry point
|*
L*----------------------------------------------------------------------------*/
#include "bonAST.h"
#include "bonTokenizer.h"
#include "bonParser.h"
#include "bonLogger.h"
#include "bonDebugASTPass.h"
#include "bonScopeAnalysisPass.h"
#include "bonTypeAnalysisPass.h"
#include "bonCodeGenPass.h"
#include "bonLLVM.h"
#include "utils.h"
#include "auto_scope.h"
#include "term_colors.h"
#include "optionparser.h"

#include <iostream>
#include <sstream>
#include <cstdlib>

// this should be set by cmake build scripts
#ifndef BON_VERSION
  #define BON_VERSION "UNKNOWN_VERSION"
#endif

bool DUMP_ASM = false;
bool VERBOSE_OUTPUT = false;
int OPTIMIZATION_LEVEL = 3;


namespace bon {

// compilation state stored in obj for easier access in compile passes
ModuleState state_;
Parser parser_(state_);


void init_module_and_passes() {
  // create new module for current file
  state_.current_module = llvm::make_unique<Module>("bon", state_.llvm_context);
  state_.current_module->setDataLayout(
                          state_.JIT->getTargetMachine().createDataLayout());
  state_.current_module->setSourceFileName(bon::logger.get_current_file());

  // add pass manager to module for optimizations
  state_.function_pass_manager =
    llvm::make_unique<legacy::FunctionPassManager>(state_.current_module.get());

  // add optimization passes
  if (OPTIMIZATION_LEVEL >= 1) {
    state_.function_pass_manager->add(createSROAPass());

    // catch trivial redundancies
    state_.function_pass_manager->add(createEarlyCSEPass(false));

    // hoist scalars and load expressions
    state_.function_pass_manager->add(createGVNHoistPass());

    // eliminate common sub-expressions
    state_.function_pass_manager->add(createGVNPass());
    state_.function_pass_manager->add(createCFGSimplificationPass());

    state_.function_pass_manager->add(createSpeculativeExecutionPass());

    state_.function_pass_manager->add(createConstantPropagationPass());
    state_.function_pass_manager->add(createJumpThreadingPass());
    state_.function_pass_manager->add(createLibCallsShrinkWrapPass());
    state_.function_pass_manager->add(createCorrelatedValuePropagationPass());
    state_.function_pass_manager->add(createCFGSimplificationPass());
    state_.function_pass_manager->add(createInstructionCombiningPass());
    state_.function_pass_manager->add(createAggressiveDCEPass());
    state_.function_pass_manager->add(createDeadCodeEliminationPass());
    state_.function_pass_manager->add(createDeadStoreEliminationPass());
    state_.function_pass_manager->add(createMemCpyOptPass());

    state_.function_pass_manager->add(createPartiallyInlineLibCallsPass());

    // state_.function_pass_manager->add(createPartialInliningPass());
    // state_.function_pass_manager->add(createFunctionInliningPass(200));

    state_.function_pass_manager->add(createTailCallEliminationPass());

    state_.function_pass_manager->add(createReassociatePass());

    state_.function_pass_manager->add(createSLPVectorizerPass());

    // eliminate common sub-expressions
    state_.function_pass_manager->add(createGVNPass());
    // simplify control flow graph (e.g. deleting unreachable blocks)
    state_.function_pass_manager->add(createCFGSimplificationPass());
    state_.function_pass_manager->add(createPromoteMemoryToRegisterPass());
  }

  state_.function_pass_manager->doInitialization();
}

bool run_scope_analysis() {
  // typeclass type analysis
  ScopeAnalysisPass scope_analysis_pass(state_);
  for (auto &tclass_entry : state_.typeclasses) {
    auto &tclass = tclass_entry.second;
    tclass->run_pass(&scope_analysis_pass);
  }

  // function type analysis
  for (auto func_name : state_.function_names) {
    auto &funcAST = state_.all_functions[func_name];
    funcAST->run_pass(&scope_analysis_pass);
  }

  // top-level type analysis
  for (auto &funcAST : state_.toplevel_expressions) {
    funcAST->run_pass(&scope_analysis_pass);
  }

  if (logger.had_errors()) {
    logger.finalize();
    return false;
  }

  return true;
}

bool run_type_analysis() {
  // typeclass type analysis
  TypeAnalysisPass type_analysis_pass(state_);
  for (auto &tclass_entry : state_.typeclasses) {
    auto &tclass = tclass_entry.second;
    tclass->run_pass(&type_analysis_pass);
  }

  // function type analysis
  for (auto func_name : state_.function_names) {
    auto &funcAST = state_.all_functions[func_name];
    funcAST->run_pass(&type_analysis_pass);
  }

  // top-level type analysis
  for (auto &funcAST : state_.toplevel_expressions) {
    funcAST->run_pass(&type_analysis_pass);
  }

  if (logger.had_errors()) {
    logger.finalize();
    return false;
  }

  return true;
}

bool run_codegen() {
  // DebugASTPass debug_ast_pass;
  // for (auto &tclass_entry : state_.typeclasses) {
  //   auto &tclass = tclass_entry.second;
  //   tclass->run_pass(&debug_ast_pass);
  //   for (auto &impl : tclass->impls) {
  //     impl->run_pass(&debug_ast_pass);
  //   }
  // }

  CodeGenPass code_gen_pass(state_);

  for (auto func : state_.ordered_functions) {
    func->run_pass(&code_gen_pass);
    if (DUMP_ASM ||
        (VERBOSE_OUTPUT && verifyModule(*state_.current_module, &errs()))) {
      state_.current_module->dump();
    }
    state_.JIT->addModule(std::move(state_.current_module));
    init_module_and_passes();
  }

  // // typeclass impl code gen
  // for (auto &tclass_entry : state_.typeclasses) {
  //   auto &tclass = tclass_entry.second;
  //   tclass->run_pass(&code_gen_pass);
  // }

  // // function code gen
  // for (auto func_name : state_.function_names) {
  //   auto &funcAST = state_.all_functions[func_name];
  //   if (funcAST == nullptr) {
  //     continue;
  //   }
  //   funcAST->run_pass(&code_gen_pass);
  //   if (auto* function_ir = code_gen_pass.result()) {
  //     if (VERBOSE_OUTPUT && verifyModule(*state_.current_module, &errs())){
  //       state_.current_module->dump();
  //     }
  //     // TODO: module per function was only needed for REPL
  //     state_.JIT->addModule(std::move(state_.current_module));
  //     init_module_and_passes();
  //   }
  // }

  if (logger.had_errors()) {
    logger.finalize();
    return false;
  }

  logger.finalize();

  // top-level expression code gen
  for (auto &funcAST : state_.toplevel_expressions) {
    funcAST->run_pass(&code_gen_pass);
    if (auto* function_ir = code_gen_pass.result()) {
      if (DUMP_ASM ||
          (VERBOSE_OUTPUT && verifyModule(*state_.current_module, &errs()))) {
        state_.current_module->dump();
      }
      auto H = state_.JIT->addModule(std::move(state_.current_module));
      init_module_and_passes();

      // search the JIT for the top-level function we just generated
      auto func_symbol = state_.JIT->findSymbol("top-level");
      assert(func_symbol && "Function not found");

      // get the symbol's address and cast it to the right type (takes no
      // arguments, returns a double) so we can call it as a native function.
      double (*FP)() = (double (*)())(intptr_t)func_symbol.getAddress();
      FP();
      // delete top-level expression module from the JIT.
      // state_.JIT->removeModule(H);
    }
    else {
      return false;
    }
  }

  return true;
}

// TODO: put top-level expressions in single "top-level" function,
//       and run that function here
void run_module() {
}

bool compile_file(std::string filename, bool should_run_codegen) {
  bon::init_module_and_passes();
  parser_.parse_file(filename);
  if (!should_run_codegen) {
    return true;
  }
  if (!run_scope_analysis()) {
    return false;
  }
  if (!run_type_analysis()) {
    return false;
  }
  if (!run_codegen()) {
    return false;
  }
  run_module();
  return true;
}

} // namespace bon

int main(int argc, char* argv[]) {
enum  optionIndex { UNKNOWN, HELP, VERBOSE, VERSION, ASM, OPT_LEVEL };
  const option::Descriptor usage[] =
  {
    {UNKNOWN, 0, "", "",option::Arg::None,
      "USAGE: bon [options] [file]\n\n"
      "Options:" },

    {HELP, 0,"", "help",option::Arg::None,
      "  --help  \tPrint usage and exit." },

    {VERSION, 0,"","version",option::Arg::None,
      "  --version  \tDisplay version info." },

    {VERBOSE, 0,"v","verbose",option::Arg::None,
      "  --verbose, -v  \tEnable verbose output from compiler." },

    {ASM, 0,"","asm",option::Arg::None,
      "  --asm  \tDump llvm-asm output for compiled modules." },

    {OPT_LEVEL, 0, "O", "opt-level", option::Arg::Optional,
      " --opt-level, -O \tSet optimization level (0-3)"},

    {UNKNOWN, 0, "", "",option::Arg::None, "\nExamples:\n"
                                  "  bon hello.bon\n"
                                  "  bon --version\n"
                                  "  bon --asm -O3 examples/hello.bon\n" },
    {0,0,0,0,0,0}
  };

  // skip program name argv[0] if present
  argc-=(argc>0); argv+=(argc>0);
  option::Stats  stats(usage, argc, argv);
  std::vector<option::Option> options(stats.options_max);
  std::vector<option::Option> buffer(stats.buffer_max);
  option::Parser parse(usage, argc, argv, &options[0], &buffer[0]);

  if (parse.error()) {
    std::cout << "Option parse error" << std::endl;
    return 1;
  }

  if (options[HELP]) {
    option::printUsage(std::cout, usage);
    return 0;
  }

  if (options[VERSION]) {
    std::cout << "Bon " << BON_VERSION << std::endl;
    return 0;
  }

  VERBOSE_OUTPUT = options[VERBOSE] ? true : false;
  DUMP_ASM = options[ASM] ? true : false;
  OPTIMIZATION_LEVEL = options[OPT_LEVEL] ?
                       strtoul(options[OPT_LEVEL].arg, nullptr, 10)
                       : 2;

  bool unknown_options = false;
  for (option::Option* opt = options[UNKNOWN]; opt; opt = opt->next()) {
    std::cout << "Unknown option: "
              << std::string(opt->name,opt->namelen) << "\n";
    unknown_options = true;
  }

  if (unknown_options) {
    option::printUsage(std::cout, usage);
    return 1;
  }

  if (const char* stdlib_path = std::getenv("BON_STDLIB_PATH")) {
    BON_STDLIB_PATH = stdlib_path;
  }
  else {
    std::cout << "BON_STDLIB_PATH not set - have you run "
                 "\"source ~/.profile\" since installing?" << std::endl;
    return 1;
  }

  InitializeNativeTarget();
  InitializeNativeTargetAsmPrinter();
  InitializeNativeTargetAsmParser();

  bon::state_.JIT = llvm::make_unique<BonJIT>();

  bon::compile_file("prelude.bon", false);

  std::string filename = parse.nonOptionsCount() > 0 ? parse.nonOption(0) : "";

  if (filename != "") {
    bon::compile_file(filename, true);
  }
  else {
    option::printUsage(std::cout, usage);
    return 0;
  }

  return 0;
}

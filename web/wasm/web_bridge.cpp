// The engine bridge as a WebAssembly module for the static site: the same
// Session as kingbble_bridge, driven by kb_command (one text command in, one
// JSON object out) instead of stdin and stdout. One thread: a static site has
// no cross-origin isolation, so no shared memory.
//
//   kb_init(lexicon_path, leaves_path)  loads the word list and leaves (paths in
//                                        the module's file system); "" for no leaves
//   kb_command(line)                     any bridge command, see bridge.cpp
#include <emscripten/emscripten.h>

#define main kingbble_bridge_main
#include "../../engine/apps/bridge.cpp"
#undef main

namespace {
std::unique_ptr<Gaddag> g_gaddag;
std::unique_ptr<Leaves> g_leaves;
std::unique_ptr<Session> g_session;
std::string g_out;

std::string Run(const std::string& line) {
  if (!g_session) return Session::Error("the engine is not loaded");
  Session& session = *g_session;
  std::istringstream in(line);
  std::string cmd;
  in >> cmd;
  std::vector<std::string> args;
  for (std::string a; in >> a;) args.push_back(a);
  try {
    if (cmd == "new" && args.size() == 5)
      return session.New(std::stoull(args[0]), {args[1], args[2]},
                         {std::stod(args[3]), std::stod(args[4])});
    if (cmd == "state") return session.Ok();
    if (cmd == "play") return session.Play(args);
    if (cmd == "check") return session.Play(args, false);
    if (cmd == "exchange" && args.size() == 1) return session.Exchange(args[0]);
    if (cmd == "pass") return session.Pass();
    if (cmd == "bot") return session.BotMove();
    if (cmd == "hint") return session.Hint(args.empty() ? 10 : std::stoi(args[0]));
    if (cmd == "view" && args.size() == 1) return session.View(std::stoul(args[0]));
    if (cmd == "rewind" && args.size() == 1) return session.Rewind(std::stoul(args[0]));
    if (cmd == "undo") return session.Undo();
    if (cmd == "gcg") return session.Gcg();
    return Session::Error("unknown command: " + line);
  } catch (const std::exception& e) {
    return Session::Error(e.what());
  }
}
}  // namespace

extern "C" {

EMSCRIPTEN_KEEPALIVE const char* kb_init(const char* lexicon, const char* leaves) {
  try {
    const std::string lex = lexicon, lv = leaves;
    g_session.reset();
    g_gaddag = std::make_unique<Gaddag>(lex.ends_with(".kwg") ? Kwg::FromFile(lex).ToGaddag()
                                                              : Gaddag::FromFile(lex));
    g_leaves.reset();
    if (!lv.empty()) g_leaves = std::make_unique<Leaves>(Leaves::FromFile(lv));
    g_session = std::make_unique<Session>(*g_gaddag, g_leaves.get(), HumanBotOptions{}, 1);
    g_out = "{\"ok\":true,\"ready\":true,\"words\":" + std::to_string(g_gaddag->NumWords()) +
            ",\"leaves\":" + (g_leaves ? "true" : "false") + "}";
  } catch (const std::exception& e) {
    g_out = Session::Error(e.what());
  }
  return g_out.c_str();
}

EMSCRIPTEN_KEEPALIVE const char* kb_command(const char* line) {
  g_out = Run(line);
  return g_out.c_str();
}

}  // extern "C"

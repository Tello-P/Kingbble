// Quackle side of the Quackle match. Loads a Quackle lexicon and computer
// player, then answers requests on stdin: the game so far as GCG text, then a
// line "RACK <tiles>" with the rack of the player to move. Each answer is one
// line "MOVE <move>" with that player's move in GCG notation (Quackle may print
// other lines, which are ignored): "8D WORD" (blanks lowercase,
// played-through letters spelled out), "-ABC" for an exchange or "-" for a
// pass.
//
//   quackle_helper [--lexicon csw24] [--player "Championship Player"] [--threads N]
//   quackle_helper --lexicon csw24 --words   (prints the lexicon, one word per line)

#include <QString>
#include <QTextStream>
#include <iostream>
#include <string>

#include "alphabetparameters.h"
#include "board.h"
#include "boardparameters.h"
#include "computerplayer.h"
#include "computerplayercollection.h"
#include "datamanager.h"
#include "game.h"
#include "generator.h"
#include "lexiconparameters.h"
#include "quackleio/flexiblealphabet.h"
#include "quackleio/gcgio.h"
#include "quackleio/util.h"
#include "strategyparameters.h"

namespace {

Quackle::LetterString Encode(const std::string& s) {
  return QUACKLE_ALPHABET_PARAMETERS->encode(s);
}

std::string MoveText(const Quackle::Move& m) {
  switch (m.action) {
    case Quackle::Move::Place:
      return m.positionString() + " " + QUACKLE_ALPHABET_PARAMETERS->userVisible(m.prettyTiles());
    case Quackle::Move::Exchange:
    case Quackle::Move::BlindExchange:
      return "-" + QUACKLE_ALPHABET_PARAMETERS->userVisible(m.tiles());
    default:
      return "-";
  }
}

}  // namespace

int main(int argc, char** argv) {
  std::string lexicon = "csw24", player_name = "Championship Player";
  int threads = 0;
  bool words = false;
  for (int i = 1; i < argc; ++i) {
    const std::string a = argv[i];
    if (a == "--lexicon" && i + 1 < argc) {
      lexicon = argv[++i];
    } else if (a == "--player" && i + 1 < argc) {
      player_name = argv[++i];
    } else if (a == "--threads" && i + 1 < argc) {
      threads = std::stoi(argv[++i]);
    } else if (a == "--words") {
      words = true;
    } else {
      std::cerr << "usage: " << argv[0]
                << " [--lexicon NAME] [--player NAME] [--threads N] [--words]\n";
      return 2;
    }
  }

  Quackle::DataManager data;
  data.setComputerPlayers(Quackle::ComputerPlayerCollection::fullCollection());
  data.setAppDataDirectory(QUACKLE_DATA);
  data.setBackupLexicon("default_english");
  auto* alphabet = new QuackleIO::FlexibleAlphabetParameters;
  if (!alphabet->load(QuackleIO::Util::stdStringToQString(
          Quackle::AlphabetParameters::findAlphabetFile("english")))) {
    std::cerr << "cannot load the english alphabet\n";
    return 1;
  }
  data.setAlphabetParameters(alphabet);
  data.setBoardParameters(new Quackle::BoardParameters());
  data.lexiconParameters()->loadDawg(Quackle::LexiconParameters::findDictionaryFile(lexicon + ".dawg"));
  if (!data.lexiconParameters()->hasSomething()) {
    std::cerr << "cannot load lexicon " << lexicon << "\n";
    return 1;
  }
  data.strategyParameters()->initialize(lexicon);

  if (words) {
    Quackle::Generator gen;
    const int flags = Quackle::Generator::AnagramRearrange | Quackle::Generator::AddAnyLetters |
                      Quackle::Generator::ClearBlanknesses;
    for (const auto& w : gen.anagramLetters("", flags))
      std::cout << QUACKLE_ALPHABET_PARAMETERS->userVisible(w) << "\n";
    return 0;
  }

  bool found = false;
  const Quackle::Player& prototype = QUACKLE_COMPUTER_PLAYERS.playerForName(player_name, found);
  if (!found) {
    std::cerr << "no computer player named " << player_name << "\n";
    return 1;
  }
  Quackle::ComputerPlayer* player = prototype.computerPlayer()->clone();
  if (threads > 0) player->setThreadCount(threads, player->threadQoS());

  std::string line, gcg;
  while (std::getline(std::cin, line)) {
    if (line.rfind("RACK ", 0) != 0) {
      gcg += line + "\n";
      continue;
    }
    QString text = QuackleIO::Util::stdStringToQString(gcg);
    QTextStream in(&text);
    QuackleIO::GCGIO io;
    Quackle::Game* game = io.read(in, QuackleIO::Logania::MaintainBoardPreparation);
    gcg.clear();
    if (!game) {
      std::cerr << "cannot read the game\n";
      return 1;
    }
    if (!game->hasPositions()) game->addPosition();
    Quackle::GamePosition position = game->currentPosition();
    position.setCurrentPlayerRack(Quackle::Rack(Encode(line.substr(5))));
    player->setPosition(position);
    std::cout << "MOVE " << MoveText(player->move()) << std::endl;
    delete game;
  }
  delete player;
  return 0;
}

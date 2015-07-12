#ifndef JUCI_SINGLETONS_H_
#define JUCI_SINGLETONS_H_

#include "keybindings.h"
#include "source.h"
#include "directories.h"
#include "terminal.h"

namespace Singletons {
  class Config {
  public:
    static Source::Config *source() {
      return source_.get();
    }
  private:
    static std::unique_ptr<Source::Config> source_;
  };
}

#endif // JUCI_SINGLETONS_H_

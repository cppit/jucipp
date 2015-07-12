#include "singletons.h"

std::unique_ptr<Source::Config> Singletons::Config::source_=std::unique_ptr<Source::Config>(new Source::Config());

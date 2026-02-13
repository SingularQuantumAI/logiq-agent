#include "../config/Config.hpp"

namespace logiq::core {

class Agent {
public:
  explicit Agent(const logiq::config::Config &config);

  bool initialize();
  void run_once();
  void shutdown();

private:
  logiq::config::Config config_;
};

} // namespace logiq::core

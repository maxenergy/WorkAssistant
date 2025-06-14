#include "event_manager.h"

namespace work_assistant {

EventManager& EventManager::GetInstance() {
    static EventManager instance;
    return instance;
}

} // namespace work_assistant
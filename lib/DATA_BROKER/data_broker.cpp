#include "data_broker.h"

DataBroker& DataBroker::instance() {
    static DataBroker broker;
    return broker;
}

Blackboard DataBroker::snapshot() {
    std::lock_guard<std::mutex> lk(_mtx);
    return _board;
}

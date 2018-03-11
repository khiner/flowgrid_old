#include "Result.h"
#include <cassert>

using namespace NBase;

Result Result::NoError;

Result::Result()
        : success(true), error("success"), checked(true), child(nullptr) {}

Result::Result(const std::string &error)
        : success(false), error(error), checked(false), child(nullptr) {}

Result::Result(const std::ostringstream &error)
        : success(false), error(error.str()), checked(false), child(nullptr) {}

Result::Result(Result &cause, const std::string &error)
        : success(false), error(error), checked(false), child(new Result(cause)) {
    cause.checked = true;
    child->checked = true;
}

Result::Result(const Result &other) {
    success = other.success;
    error = other.error;
    checked = false;
    other.checked = true;
    child = other.child;
    other.child = nullptr;
}

Result::~Result() {
    assert(checked);
    delete (child);
}

Result &Result::operator=(const Result &other) {
    success = other.success;
    error = other.error;
    checked = false;
    other.checked = true;
    child = other.child;
    other.child = nullptr;
    return *this;
}

bool Result::failed() {
    checked = true;
    return !success;
}

bool Result::succeeded() {
    checked = true;
    return success;
}

std::string Result::getDescription() {
    std::string description = error;
    if (child) {
        description += "\n>>>> ";
        description += child->getDescription();
    }
    return description;
}

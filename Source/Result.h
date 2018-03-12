#pragma once

#include <string>
#include <sstream>

namespace NBase {
    class Result {
    public:
        explicit Result(const std::string &error);

        explicit Result(const std::ostringstream &error);

        Result(Result &cause, const std::string &error);

        ~Result();

        Result(const Result &other);

        bool failed();

        bool succeeded();

        std::string getDescription();

        Result &operator=(const Result &);

        static Result NoError;

        Result();

    private:
        bool success;
        std::string error;
        mutable bool checked;
        mutable Result *child;
    };
}

#define RETURN_IF_FAILED_MESSAGE(r, m) if ((r).failed()) { return NBase::Result(r,m) ; }
#define RETURN_IF_FAILED(r) if ((r).failed()) { return r ; }

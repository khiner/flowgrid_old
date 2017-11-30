#pragma once

#include <string>
#include <sstream>

namespace NBase
{
  class Result
  {
  public:

    Result(const std::string &error) ;
    Result(const std::ostringstream &error) ;
    Result(Result &cause,const std::string &error) ;
    ~Result() ;
    Result(const Result &other) ;

    bool Failed() ;
    bool Succeeded() ;
    std::string GetDescription() ;

    Result &operator=(const Result &) ;

    static Result NoError ;
  protected:
    Result() ;
  private:
    bool success_ ;
    std::string error_ ;
    mutable bool checked_ ;
    mutable Result *child_ ;
  };
}

#define RETURN_IF_FAILED_MESSAGE(r,m) if (r.Failed()) { return NBase::Result(r,m) ; }
#define RETURN_IF_FAILED(r) if (r.Failed()) { return r ; }
#define LOG_IF_FAILED(r,m) if (r.Failed()) { NBase::Result __combined(r,m); Trace::Error(__combined.GetDescription().c_str()) ; __combined.Failed();}


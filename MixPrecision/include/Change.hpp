#ifndef CHANGE_GUARD
#define CHANGE_GUARD 1

#include <llvm/IR/Type.h>
#include <llvm/IR/Module.h>
#include "utils.hpp"

using namespace llvm;
using namespace std;

typedef vector<PtrDep> Types;

class Change {
public:
  Change(Types, Value*);
  Change(Types, Value*, int);
  
  Value* const getValue()const;
  
  Types getType()const;
  
  int getField()const;

protected:
  Value* const value;
  const Types type;
  const int field;
};



class StrChange {
public:
  StrChange(string, string, int);
  ~StrChange(){}
  
  string getClassification() const;
  
  string getTypes() const;

  int getField() const;

protected:
  const string classification;
  const string types;
  const int field;
};


typedef vector<PtrDep> Types;

class FunctionChange : public Change {
  public:
    FunctionChange(Types, Value*, string);
    ~FunctionChange();

    string getSwitch() const;

  protected:
    const string swit;
};


class FuncStrChange : public StrChange {
public:
  FuncStrChange(string, string, int, string);
  ~FuncStrChange();

  string getSwitch() const;
  
protected:
  const string swit;
};


#endif

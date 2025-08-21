#include "../include/Change.hpp"

#include <llvm/IR/Type.h>
#include <llvm/IR/Module.h>

using namespace std;
using namespace llvm;

Change::Change(Types aType, Value* aValue) :type(aType),value(aValue),field(-1){}

Change::Change(Types aType, Value* aValue, int aField) :type(aType),value(aValue),field(aField){
}

Value * const  Change::getValue()const {
  return value;
}

Types Change::getType() const{
  return type;
}

int Change::getField()const {
  return field;
}


StrChange::StrChange(string aClassification, string aTypes, int aField):classification(aClassification),types(aTypes),field(aField) {
}

string StrChange::getClassification() const{
  return classification;
}

string StrChange::getTypes()const {
  return types;
}

int StrChange::getField()const {
  return field;
}


FunctionChange::FunctionChange(Types aType, Value* aValue, string aSwit) : Change(aType, aValue),swit(aSwit) {
}

string FunctionChange::getSwitch() const{
  return swit;
}


FuncStrChange::FuncStrChange(string aClassification, string aTypes, int aField, string aSwit) : StrChange(aClassification, aTypes, aField),swit(aSwit)  {
}

string FuncStrChange::getSwitch() const{
  return swit;
}

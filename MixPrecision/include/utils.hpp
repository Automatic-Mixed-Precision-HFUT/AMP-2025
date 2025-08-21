#ifndef MIXUTILS
#define MIXUTILS
#include <llvm/IR/GlobalVariable.h>
#include <llvm/IR/Instruction.h>
#include <llvm/IR/Value.h>
#include <llvm/IR/Type.h>
#include <llvm/IR/DebugInfo.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/Module.h>
#include <llvm/Support/CommandLine.h>
#include <llvm/Support/raw_ostream.h>
#include "llvm/IR/DerivedTypes.h"
#include "llvm/Support/Casting.h"
#include "llvm/Support/FileSystem.h"
#include <llvm/IR/ValueSymbolTable.h>
using llvm::Type,llvm::Value,llvm::Instruction,llvm::LoadInst,llvm::StoreInst,llvm::GetElementPtrInst,llvm::AllocaInst,llvm::GlobalVariable;
//解析指针类型
struct PtrDep {
    Type* ty;
    int dep;

    PtrDep(Type* t, int d) : ty(t), dep(d) {}
    
    static llvm::PointerType* getPoint(Type*ty,int dep){
        int i=0;
        while(dep>0){
            ty=llvm::PointerType::getUnqual(ty);
            --dep;
        }
        return llvm::dyn_cast<llvm::PointerType>(ty);
    }

    llvm::PointerType* getPoint(){
        int dep=this->dep;
        auto ty=this->ty;
        while(dep>0){
            ty=llvm::PointerType::getUnqual(ty);
            --dep;
        }
        return llvm::dyn_cast<llvm::PointerType>(ty);
    }
    
    PtrDep operator-(int i) const {
        return PtrDep(ty, dep - i);
    }

    PtrDep operator+(int i) const {
        return PtrDep(ty, dep + i);
    }

    PtrDep& operator+=(int i) {
        dep += i;
        return *this;
    }

    PtrDep& operator-=(int i) {
        dep -= i;
        return *this;
    }

    bool operator==(const PtrDep& other) const {
        return ty == other.ty && dep == other.dep;
    }

    bool operator!=(const PtrDep& other) const {
        return !(*this == other);
    }

    PtrDep& operator++() { // 前缀 ++
        ++dep;
        return *this;
    }

    PtrDep operator++(int) { // 后缀 ++
        PtrDep tmp = *this;
        ++dep;
        return tmp;
    }

    PtrDep& operator--() { // 前缀 --
        --dep;
        return *this;
    }

    PtrDep operator--(int) { // 后缀 --
        PtrDep tmp = *this;
        --dep;
        return tmp;
    }

    friend llvm::raw_ostream& operator<<(llvm::raw_ostream& os, const PtrDep& pd) {
        if (pd.ty) {
            pd.ty->print(os, /*IsForDebug=*/false);
        } else {
            os << "nullptr";
        }
        for (int i = 0; i < pd.dep; ++i) {
            os << "*";
        }
        return os;
    }
    inline std::string to_string() {
        std::string result;
        llvm::raw_string_ostream os(result);
        os << *this;
        return os.str();
    }
};
PtrDep resolvePointerElementType(Value *ptr);
PtrDep resolveTypeByUser(Value *val, int dep);
inline PtrDep resolveTypeByUser(Value *val){return resolveTypeByUser(val,0);}
#endif
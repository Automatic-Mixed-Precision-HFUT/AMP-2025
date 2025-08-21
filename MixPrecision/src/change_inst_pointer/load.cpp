#include "utils.hpp"

bool ChangeInstPointer::visitLoadInst(LoadInst &inst){

  LoadInst* loadInst = &inst;
  llvm::Align Alignment(alignment);

  LoadInst* newLoad;

    if(newTypeP.dep!=0){

     newLoad=new LoadInst(newTypeP.getPoint(),newTarget, "", false,Alignment, loadInst);


    }

    else{

      newLoad = new LoadInst(newTypeP.ty,newTarget, "", false,Alignment, loadInst);
   
    }



  if (newTypeP.dep!=0) {
    vector<Instruction*> erase;
    Value::use_iterator it = loadInst->use_begin();
    for(; it != loadInst->use_end(); it++) {
    
      if (CallInst* callInst = dyn_cast<CallInst>(it->getUser())) {
        if (callInst->getCalledFunction()->isIntrinsic() && callInst->getIntrinsicID() == Intrinsic::fmuladd){
     
          assert(callInst->getCalledFunction()->isIntrinsic() && callInst->getIntrinsicID() == Intrinsic::fmuladd);
        }else{
          
        BitCastInst *bitCast = new BitCastInst(newLoad, oldTypeP.getPoint(), "", callInst);
        for (unsigned i = 0; i < callInst->getNumOperands(); i++) {
          if (callInst->getOperand(i) == loadInst)
            callInst->setArgOperand(i, bitCast);
        }
        }
      } else {

        if (FPTruncInst *fp= dyn_cast<FPTruncInst>(it->getUser())){
          erase.push_back(fp);
          fp->replaceAllUsesWith(newLoad);
        }

        else{
       
          PtrDep newTypeP_ = PtrDep(newTypeP.ty, 0);
           PtrDep oldTypeP_ = PtrDep(oldTypeP.ty, 0);
          bool is_erased = ChangeVisitor::changePrecision(context,it, newLoad, loadInst, newTypeP_, oldTypeP_, alignment);

          if (!is_erased)
            erase.push_back(dyn_cast<Instruction>(it->getUser()));
        }

      }
    }



    for (unsigned int i = 0; i < erase.size(); i++) {
   
      erase[i]->eraseFromParent();
    }

  } 
  

  else {
    vector<Instruction*> erase;
    bool change=1;
    Value::use_iterator it = loadInst->use_begin();

    for(; it != loadInst->use_end(); it++){
   
      if (CallInst *pCallInst= dyn_cast<CallInst>(it->getUser())){
      
        Value *calledValue = pCallInst->getCalledOperand();
        
        Function *targetFunction = findTargetFunctionP(pCallInst->getCalledOperand());
      
      
        if (targetFunction->getName()=="isnan"||targetFunction->getName()=="isinf"){
          if (auto *CE = dyn_cast<ConstantExpr>(pCallInst->getCalledOperand())){
            if (CE->isCast()){
              change=0;


              Type *fty=targetFunction->getReturnType();

              FunctionType *newFuncType = FunctionType::get(Type::getInt32Ty(context), newTypeP.ty, false);
   
              std::vector<Value*> args;
              args.push_back(newLoad);

              auto *NewBitcast = new BitCastInst(CE->getOperand(0), PointerType::get(newFuncType, 0), "", pCallInst);

              auto *NewCall = CallInst::Create(newFuncType, NewBitcast,args, "", pCallInst);
              NewCall->setCallingConv(pCallInst->getCallingConv());

            
              pCallInst->replaceAllUsesWith(NewCall);
              erase.push_back(pCallInst);
            }
          }
        }
        else if (pCallInst->getCalledFunction()->isIntrinsic() && pCallInst->getIntrinsicID() == Intrinsic::fmuladd){
 
          if (pCallInst->getType()->isDoubleTy()){
           
            erase.push_back(pCallInst);
   
            std::vector<llvm::Value*> indices;
            for (unsigned i = 0; i < pCallInst->getNumOperands()-1; i++) {
              if (pCallInst->getOperand(i) == loadInst){
                pCallInst->setArgOperand(i, newLoad);
              }
       
              if (pCallInst->getOperand(i)->getType()->isDoubleTy()){
                FPTruncInst *truncInst1=new FPTruncInst(pCallInst->getOperand(i),Type::getFloatTy(context), "",pCallInst);
                indices.push_back(truncInst1);
              }
              else{
                indices.push_back(pCallInst->getOperand(i));
              }
            }

            ArrayRef<llvm::Value*> arrayRef(indices);
            Function *F=pCallInst->getFunction();
            auto M=F->getParent();
            Function *FmulAddF32 = Intrinsic::getDeclaration(M, Intrinsic::fmuladd, {Type::getFloatTy(context)});
            CallInst *NewCall=CallInst::Create(FmulAddF32,arrayRef,"",pCallInst);
   
            change=0;
            vector<Instruction*> erased1;
            Value::use_iterator it1 = pCallInst->use_begin();
            bool neednewstore=1;
            for(; it1 != pCallInst->use_end(); it1++){

              if (FPTruncInst *fp= dyn_cast<FPTruncInst>(it1->getUser())){    

                erased1.push_back(fp);
                fp->replaceAllUsesWith(NewCall);

                Value::use_iterator itfp = NewCall->use_begin();
                neednewstore=0;
          

              }
              else if(BinaryOperator *byop=dyn_cast<BinaryOperator>(it1->getUser())){
                if (byop->getOpcode()==Instruction::FDiv){
        
                  erased1.push_back(byop);
                  std::vector<llvm::Value*> arg;
                  for (unsigned i = 0; i < byop->getNumOperands(); i++){
                    if (byop->getOperand(i) == pCallInst){
                 
                      auto new_inst = transTyP(pCallInst->getType(), NewCall, pCallInst);
                      pCallInst->replaceAllUsesWith(new_inst);
                    }
             
                    if (byop->getOperand(i)->getType()->isDoubleTy()){
                      FPTruncInst *truncInst1=new FPTruncInst(byop->getOperand(i),Type::getFloatTy(context), "",byop);
                   
                      arg.push_back(truncInst1);
                    }
                    else{
                      arg.push_back(byop->getOperand(i));
                    }

                  }

                  BinaryOperator *newFdiv=BinaryOperator::Create(BinaryOperator::FDiv,arg[0],arg[1],"",byop);

                  vector<Instruction*> erased2;
                  Value::use_iterator itfdiv = byop->use_begin();
                  for(; itfdiv != byop->use_end(); itfdiv++){
            
                    bool is_erase=ChangeVisitor::changePrecision(
                        context, itfdiv, newFdiv, byop, getFloatPtrDep(context),
                        getDoublePtrDep(context), alignment);
                    if (!is_erase)
                      erased2.push_back(dyn_cast<Instruction>(itfdiv->getUser()));
                  }
                  for (unsigned int i = 0; i < erased2.size(); i++) {
                    erased2[i]->eraseFromParent();
                  }
                }
              }
              else {
                if (neednewstore){
                  bool is_erased = ChangeVisitor::changePrecision(
                      context, it1, NewCall, pCallInst, getFloatPtrDep(context),
                     getDoublePtrDep(context), alignment);
                  if (!is_erased)
                    erased1.push_back(dyn_cast<Instruction>(it1->getUser()));
                }else if(!neednewstore){
                  erased1.push_back(dyn_cast<Instruction>(it1->getUser()));
                }
              }
            }
            for (unsigned int i = 0; i < erased1.size(); i++) {
   
              erased1[i]->eraseFromParent();
            }
          

          }
  
         else if (pCallInst->getType()->isFloatTy()&&(newTypeP.ty->isHalfTy()|| (newTypeP.dep!=0 && newTypeP.ty->isHalfTy()))){      
         
       

            erase.push_back(dyn_cast<Instruction>(pCallInst));
           
            std::vector<llvm::Value*> indices;
            for (unsigned i = 0; i < pCallInst->getNumOperands()-1; i++) {

              if (pCallInst->getOperand(i) == loadInst){
                pCallInst->setArgOperand(i, newLoad);
              }
          
              if (pCallInst->getOperand(i)->getType()->isFloatTy()){
                FPTruncInst *truncInst1=new FPTruncInst(pCallInst->getOperand(i),Type::getHalfTy(context), "",pCallInst);
                indices.push_back(truncInst1);
              }else{
                indices.push_back(pCallInst->getOperand(i));
              }
            }
            ArrayRef<llvm::Value*> arrayRef(indices);
            Function *F=pCallInst->getFunction();
            auto M=F->getParent();
            Function *FmulAddF16 = Intrinsic::getDeclaration(M, Intrinsic::fmuladd, {Type::getHalfTy(context)});
            CallInst *NewCall=CallInst::Create(FmulAddF16,arrayRef,"",pCallInst);
            change=0;
            vector<Instruction*> erased1;
            Value::use_iterator it1 = pCallInst->use_begin();
            bool nonewst=1;
            for(; it1 != pCallInst->use_end(); it1++){
        

              if (FPTruncInst *fpTruncInst= dyn_cast<FPTruncInst>(it1->getUser())){
                erased1.push_back(fpTruncInst);
                Value::use_iterator itfpt = fpTruncInst->use_begin();
                for(; itfpt != fpTruncInst->use_end(); itfpt++){
            
                  if(dyn_cast<StoreInst>(itfpt->getUser())){
                    nonewst=0;
                  }
                }
                fpTruncInst->replaceAllUsesWith(NewCall);
              }else{
                if (nonewst){
                  bool is_erased = ChangeVisitor::changePrecision(context,it1, NewCall, pCallInst, getHalfPtrDep(context), getFloatPtrDep(context), alignment);
                  if (!is_erased){
                     erased1.push_back(dyn_cast<Instruction>(it1->getUser()));
                  }
                }else{
                  erased1.push_back(dyn_cast<Instruction>(it1->getUser()));
                }
              }

            }
            for (unsigned int i = 0; i < erased1.size(); i++) {
              erased1[i]->eraseFromParent();
            }
          }

        }
        else if (pCallInst->getCalledFunction()->isIntrinsic() && pCallInst->getIntrinsicID() == Intrinsic::fabs){
       
          change=0;
          erase.push_back(pCallInst);
          Function *F=pCallInst->getFunction();
          auto M=F->getParent();

          Function *Fabs = Intrinsic::getDeclaration(M, Intrinsic::fabs, newTypeP.ty);

          CallInst *NewCall=CallInst::Create(Fabs,newLoad,"",pCallInst);

          vector<Instruction *> erase2;
          Value::use_iterator it1 =pCallInst->use_begin();
          for (;it1!=pCallInst->use_end();it1++){
         
            if (FPTruncInst *fp= dyn_cast<FPTruncInst>(it1->getUser())){
              fp->replaceAllUsesWith(NewCall);
              erase2.push_back(fp);
            } else{
              bool is_erased = ChangeVisitor::changePrecision(context,it1, NewCall, pCallInst, newTypeP, oldTypeP, alignment);
              if (!is_erased)
                erase2.push_back(dyn_cast<Instruction>(it1->getUser()));
            }
          }
          for (unsigned int i = 0; i < erase2.size(); i++) {
            erase2[i]->eraseFromParent();
          }
        }else if(pCallInst->getCalledFunction()->getName()=="sqrt"||pCallInst->getCalledFunction()->getName()=="llvm.sqrt.f32"){
          change=0;
 
          erase.push_back(pCallInst);
          bool is_erased = ChangeVisitor::changePrecision(context,it, newLoad, newTarget, newTypeP, oldTypeP, alignment);
        }
      }
      else if (FPExtInst *inst1= dyn_cast<FPExtInst>(it->getUser())){
    
        if (newTypeP.ty->isHalfTy()||(newTypeP.dep!=0 && newTypeP.ty->isHalfTy())) {
      
      
          change=0;
          unsigned int changef16 = 0;
          Value::use_iterator it1 = inst1->use_begin();
          for (; it1 != inst1->use_end(); it1++) {
  
            if (FPTruncInst *fptrunc = dyn_cast<FPTruncInst>(it1->getUser())) {

              changef16 = 1;
            }else if(dyn_cast<CallInst>(it1->getUser())|| dyn_cast<FCmpInst>(it1->getUser())){
              changef16=2;
            }else if(BinaryOperator *tempbyop=dyn_cast<BinaryOperator>(it1->getUser())){
              changef16=3;
            }else if(StoreInst *st=dyn_cast<StoreInst>(it1->getUser())){
              changef16=4;
            }else if (ReturnInst *ret= dyn_cast<ReturnInst>(it1->getUser())){
              changef16=5;
            }
          }
          if (changef16==1) {
 
          FPExtInst *newfpext =
              new FPExtInst(newLoad, Type::getFloatTy(context), "", inst1);
          loadInst->replaceAllUsesWith(newfpext);
          vector<Instruction *> eraseed;
          Value::use_iterator ituse = inst1->use_begin();
          for (; ituse != inst1->use_end(); ituse++) {
              bool is_erased = ChangeVisitor::changePrecision(
                  context, ituse, inst1, inst1, newTypeP, oldTypeP, alignment);


          }

       

        }else if (changef16==2){

          FPExtInst *newfpext =
              new FPExtInst(newLoad, Type::getFloatTy(context), "", inst1);
          loadInst->replaceAllUsesWith(newfpext);
        }else if(changef16==3){
          FPExtInst *newfpext =
              new FPExtInst(newLoad, Type::getFloatTy(context), "", inst1);
          loadInst->replaceAllUsesWith(newfpext);
          vector<Instruction *> eraseed;
          Value::use_iterator ituse = inst1->use_begin();
          for (; ituse != inst1->use_end(); ituse++){
            bool is_erased = ChangeVisitor::changePrecision(
                context, ituse, inst1, inst1, newTypeP, oldTypeP, alignment);

   
          }
          }else if (changef16==4){
            FPExtInst *newfpext =
                new FPExtInst(newLoad, Type::getFloatTy(context), "", inst1);
            loadInst->replaceAllUsesWith(newfpext);
          }else if (changef16==5){
      
            FPExtInst *newfpext =
                new FPExtInst(newLoad, Type::getFloatTy(context), "", loadInst);
            loadInst->replaceAllUsesWith(newfpext);

          }
  
          else{
    FPExtInst *newfpext =
              new FPExtInst(newLoad, Type::getFloatTy(context), "", inst1);
          loadInst->replaceAllUsesWith(newfpext);
          }
        }
        }
     else if (BinaryOperator *byop= dyn_cast<BinaryOperator>(it->getUser())){
          Type *newTypetemp=NULL;
      
         newTypetemp=newTypeP.ty;
        if (((byop->getOpcode()==Instruction::FMul)||(byop->getOpcode()==Instruction::FDiv)||(byop->getOpcode()==Instruction::FAdd))&&byop->getType()->isDoubleTy()){
          std::vector<llvm::Value*> arg;
          change=0;
          for (unsigned i = 0; i < byop->getNumOperands(); i++){
            if (byop->getOperand(i) == loadInst){
              
       
              FPExtInst *ext = new FPExtInst(newLoad, oldTypeP.ty, "", loadInst);
              loadInst->replaceAllUsesWith(ext);
            }
       
         
            if (byop->getOperand(i)->getType()->isDoubleTy()){
              FPTruncInst *truncInst1=new FPTruncInst(byop->getOperand(i),Type::getFloatTy(context), "",byop);
       
              arg.push_back(truncInst1);
            }else{
              arg.push_back(byop->getOperand(i));
            }

          }
          BinaryOperator *newBinaryOp;
          if (byop->getOpcode()==Instruction::FMul){
            newBinaryOp=BinaryOperator::CreateFMul(arg[0],arg[1],"",byop);
          }else if(byop->getOpcode()==Instruction::FDiv){
            newBinaryOp=BinaryOperator::CreateFDiv(arg[0],arg[1],"",byop);
          }else{
            newBinaryOp=BinaryOperator::CreateFAdd(arg[0],arg[1],"",byop);
          }

 


          unsigned int align=4;
          Value::use_iterator fmul = byop->use_begin();
          vector<Instruction*> erasefmul;
          bool neederasestore=1;
          for (;fmul!=byop->use_end();fmul++){
       

            if (FPTruncInst *fpfmul=dyn_cast<FPTruncInst>(fmul->getUser())){
              erasefmul.push_back(fpfmul);
              fpfmul->replaceAllUsesWith(newBinaryOp);
              neederasestore=0;
            }else{
              if(neederasestore){
                bool is_erased = ChangeVisitor::changePrecision(
                    context, fmul, newBinaryOp, byop, newTypeP, oldTypeP, align);
                if (!is_erased)
                  erasefmul.push_back(dyn_cast<Instruction>(fmul->getUser()));
              }else{
                erasefmul.push_back(dyn_cast<Instruction>(fmul->getUser()));
              }
            }
          }
      
          for (unsigned int i = 0; i < erasefmul.size(); i++) {
     
            erasefmul[i]->eraseFromParent();
          }

        
          erase.push_back(byop);
        }
        else if((byop->getOpcode()==Instruction::FDiv||byop->getOpcode()==Instruction::FMul||byop->getOpcode()==Instruction::FAdd)&&newTypetemp->isHalfTy()){

     
          erase.push_back(byop);
          change=0;
          std::vector<llvm::Value*> arg;
          for (unsigned i = 0; i < byop->getNumOperands(); i++){
            if (byop->getOperand(i) == loadInst){

              FPExtInst *ext = new FPExtInst(newLoad, oldTypeP.ty, "", loadInst);
              loadInst->replaceAllUsesWith(ext);
          
            }
    
            if (byop->getOperand(i)->getType()->isFloatTy()){
              FPTruncInst *truncInst1=new FPTruncInst(byop->getOperand(i),Type::getHalfTy(context), "",byop);
           
              arg.push_back(truncInst1);
            }else{
              arg.push_back(byop->getOperand(i));
            }

          }
          BinaryOperator *newBinary=NULL;
          if (byop->getOpcode()==Instruction::FDiv){
            newBinary=BinaryOperator::Create(BinaryOperator::FDiv,arg[0],arg[1],"",byop);
          }else if (byop->getOpcode()==Instruction::FMul){
            newBinary=BinaryOperator::Create(BinaryOperator::FMul,arg[0],arg[1],"",byop);
          }else if (byop->getOpcode()==Instruction::FAdd){
            newBinary=BinaryOperator::Create(BinaryOperator::FAdd,arg[0],arg[1],"",byop);
          }



          vector<Instruction*> erased2;
          Value::use_iterator itfdiv = byop->use_begin();
          int changefdiv=1;
          for(; itfdiv != byop->use_end(); itfdiv++){

            if (FPTruncInst *fptrunc= dyn_cast<FPTruncInst>(itfdiv->getUser())){
              erased2.push_back(fptrunc);
              fptrunc->replaceAllUsesWith(newBinary);
              changefdiv=0;
            }else{
          if (changefdiv==1){
                bool is_erase=ChangeVisitor::changePrecision(
                    context, itfdiv, newBinary, byop, getHalfPtrDep(context),
                    getFloatPtrDep(context), alignment);
                if (!is_erase)
                  erased2.push_back(dyn_cast<Instruction>(itfdiv->getUser()));
              }else{
                erased2.push_back(dyn_cast<Instruction>(itfdiv->getUser()));
              }
            }
          }
          for (unsigned int i = 0; i < erased2.size(); i++) {
            erased2[i]->eraseFromParent();
          }
        }
      }
      else if (UnaryOperator *byop= dyn_cast<UnaryOperator>(it->getUser())){
        if (byop->getOpcode()==Instruction::FNeg&&byop->getType()->isDoubleTy()){
          UnaryOperator *newFneg=UnaryOperator::Create(UnaryOperator::FNeg,newLoad,"",byop);
          erase.push_back(byop);
        
          unsigned int align=4;
          change=0;
          Value::use_iterator fneg = byop->use_begin();
          std::vector<Instruction*> erasefneg;
          for (;fneg!=byop->use_end();fneg++){
      
            if(FPTruncInst *fp=dyn_cast<FPTruncInst>(fneg->getUser())){
              erasefneg.push_back(fp);
              fp->replaceAllUsesWith(newFneg);

            }else {
              bool is_erased = ChangeVisitor::changePrecision(
                  context, fneg, newFneg, byop, getFloatPtrDep(context),
                  getDoublePtrDep(context), align);
              if (!is_erased)
                erasefneg.push_back(dyn_cast<Instruction>(fneg->getUser()));
            }
          }
          for (unsigned int i = 0; i < erasefneg.size(); i++) {
            erasefneg[i]->eraseFromParent();
          }

        }
      }
      else if (FPTruncInst *fp=dyn_cast<FPTruncInst>(it->getUser())){

        erase.push_back(fp);
        fp->replaceAllUsesWith(newLoad);
        vector<Instruction*> erasedfp;
        change=0;
        Value::use_iterator itfp = newLoad->use_begin();
        for (;itfp!=newLoad->use_end();itfp++){
         
        
          if (dyn_cast<Instruction>(itfp->getUser())->getOperand(0)->getType()!=newTypeP.ty){
            bool is_erase=ChangeVisitor::changePrecision(
                context, itfp, newLoad, newLoad, newTypeP,
                oldTypeP, alignment);
          }

          }


      }else if(BinaryOperator *binnaryop=dyn_cast<BinaryOperator>(it->getUser())){
        Type *newTypetemp=NULL;
        Type *oldTypetemp=NULL;

        change=0;

        newTypetemp=newTypeP.ty;
        oldTypetemp=oldTypeP.ty;

        if (binnaryop->getOpcode()==Instruction::FDiv&&newTypetemp->isHalfTy()){

          erase.push_back(binnaryop);
          std::vector<llvm::Value*> arg;
          for (unsigned i = 0; i < binnaryop->getNumOperands(); i++){
            if (binnaryop->getOperand(i) == loadInst){
         
      
              FPExtInst *ext = new FPExtInst(newLoad, oldTypeP.ty, "", loadInst);
              loadInst->replaceAllUsesWith(ext);
            }
            
            if (binnaryop->getOperand(i)->getType()->isFloatTy()){
              FPTruncInst *truncInst1=new FPTruncInst(binnaryop->getOperand(i),Type::getHalfTy(context), "",binnaryop);
            
              arg.push_back(truncInst1);
            }else{
              arg.push_back(binnaryop->getOperand(i));
            }

          }

          BinaryOperator *newFdiv=BinaryOperator::Create(BinaryOperator::FDiv,arg[0],arg[1],"",binnaryop);
       
          vector<Instruction*> erased2;
          Value::use_iterator itfdiv = binnaryop->use_begin();
          for(; itfdiv != binnaryop->use_end(); itfdiv++){
            bool is_erase=ChangeVisitor::changePrecision(
                context, itfdiv, newFdiv, binnaryop, getFloatPtrDep(context),
                getDoublePtrDep(context), alignment);
            if (!is_erase)
              erased2.push_back(dyn_cast<Instruction>(itfdiv->getUser()));
          }
          for (unsigned int i = 0; i < erased2.size(); i++) {
            erased2[i]->eraseFromParent();
          }
        }


      }else if(FCmpInst *fcmp= dyn_cast<FCmpInst>(it->getUser())){
        Type *newTypetemp=NULL;
        Type *oldTypetemp=NULL;
     
        erase.push_back(fcmp);
        change=0;

        
        newTypetemp=newTypeP.ty;
        oldTypetemp=oldTypeP.ty;

        std::vector<llvm::Value*> arg;
        for (unsigned i = 0; i < fcmp->getNumOperands(); i++){
          if (fcmp->getOperand(i) == loadInst){
            arg.push_back(newLoad);
          }else if (fcmp->getOperand(i)->getType()==oldTypetemp){
            FPTruncInst *truncInst1=new FPTruncInst(fcmp->getOperand(i),newTypetemp, "",fcmp);
            arg.push_back(truncInst1);
          }
        }

        FCmpInst *fCmpInst=new FCmpInst(fcmp,fcmp->getPredicate(),arg[0],arg[1],"");
   
        fcmp->replaceAllUsesWith(fCmpInst);

      }else if (ReturnInst *ret= dyn_cast<ReturnInst>(it->getUser())){


        FPExtInst *newfpext=new FPExtInst(newLoad, ret->getReturnValue()->getType(), "", ret);
        loadInst->replaceAllUsesWith(newfpext);

      }
      else{
        erase.push_back(dyn_cast<Instruction>(it->getUser()));
      }
      }
      // erasing old instructions
    for (unsigned int i = 0; i < erase.size(); i++) {

        erase[i]->eraseFromParent();
    }
  
    if (change){
 
    if (newTypeP.ty->getTypeID() > oldTypeP.ty->getTypeID()) {

      FPTruncInst *trunc = new FPTruncInst(newLoad, oldTypeP.ty, "", loadInst);
      loadInst->replaceAllUsesWith(trunc);
    }
    else {

      FPExtInst *ext = new FPExtInst(newLoad, oldTypeP.ty, "", loadInst);
      loadInst->replaceAllUsesWith(ext);

    }
      }
  }
  
 
  return false;
}
//===- TraceUsePass.cpp - instrumenting pass: dynamic "User <- Operand" trace =//
//
// Инструментирующий LLVM-проход (new Pass Manager).
//
// Для каждой НЕ-phi инструкции I (это "User") он вставляет перед I вызов
//     traceUse(i8* <имя опкода I>, i8* <имя опкода операнда>)
// по одному на каждый операнд I, который сам является IR-инструкцией ("Operand").
// Во время выполнения каждый такой вызов дописывает в трассу строку
//     <User> <- <Operand>
// то есть мы собираем ДИНАМИЧЕСКУЮ трассу использования IR-инструкций:
// порядок и частота записей отражают реальное исполнение.
//
// Правила (из задания):
//   * User пропускается, если это phi.
//   * Операнды, не являющиеся инструкциями (константы, аргументы, глобалы,
//     метки блоков, Function в call) — не записываются: трасса про IR-инструкции.
//
// Задание со звёздочкой:
//   * Если операнд сам является phi, он разворачивается в инструкции,
//     которые phi использует как входы. Запись  shl <- phi  превращается
//     в  shl <- add  и  shl <- sub , если phi берёт операндами add и sub.
//
// Проход инструментирует все ОПРЕДЕЛЁННЫЕ функции модуля, поэтому компилируйте
// им только app.c (см. README) — тогда инструментируется лишь логический модуль.
//
//===----------------------------------------------------------------------===//

#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Verifier.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Support/raw_ostream.h"

#include <map>
#include <string>
#include <utility>
#include <vector>

using namespace llvm;

namespace {

struct TraceUsePass : public PassInfoMixin<TraceUsePass> {
  // Кэш глобальных строк с именами опкодов: одна глобаль на имя на модуль.
  std::map<std::string, Constant *> StrCache;

  Constant *getOpcodeStr(IRBuilder<> &B, StringRef Name) {
    auto It = StrCache.find(Name.str());
    if (It != StrCache.end())
      return It->second;
    // CreateGlobalStringPtr создаёт приватную глобаль-строку в модуле и
    // возвращает ptr-константу на неё (валидна в любой функции).
    Constant *S = B.CreateGlobalStringPtr(Name, "opc." + Name);
    StrCache.emplace(Name.str(), S);
    return S;
  }

  PreservedAnalyses run(Module &M, ModuleAnalysisManager &) {
    LLVMContext &Ctx = M.getContext();
    IRBuilder<> B(Ctx);

    Type *VoidTy = Type::getVoidTy(Ctx);
    PointerType *PtrTy = PointerType::getUnqual(Ctx); // opaque ptr (i8*)

    // void traceUse(ptr userName, ptr operandName);
    FunctionType *TraceTy = FunctionType::get(VoidTy, {PtrTy, PtrTy}, false);
    FunctionCallee TraceUse = M.getOrInsertFunction("traceUse", TraceTy);

    uint64_t NumInstrumented = 0, NumEdges = 0, NumPhiExpanded = 0;

    for (Function &F : M) {
      if (F.isDeclaration() || F.getName() == "traceUse")
        continue;

      // 1) Соберём цели заранее, чтобы не инструментировать собственные
      //    вставленные вызовы и не ломать итерацию по списку инструкций.
      std::vector<Instruction *> Targets;
      for (BasicBlock &BB : F)
        for (Instruction &I : BB) {
          if (isa<PHINode>(I))        // User == phi -> пропускаем
            continue;
          if (I.isDebugOrPseudoInst())
            continue;
          if (auto *CI = dyn_cast<CallInst>(&I))
            if (Function *Callee = CI->getCalledFunction())
              if (Callee->getName() == "traceUse")
                continue;             // не логируем сам логгер
          Targets.push_back(&I);
        }

      // 2) Для каждой цели вычислим рёбра User<-Operand и вставим вызовы.
      for (Instruction *I : Targets) {
        StringRef User = I->getOpcodeName();

        // Пары (имя User, имя Operand) в порядке операндов.
        std::vector<std::pair<StringRef, StringRef>> Edges;
        for (Use &U : I->operands()) {
          auto *OpI = dyn_cast<Instruction>(U.get());
          if (!OpI)                   // операнд не IR-инструкция -> пропуск
            continue;
          if (auto *Phi = dyn_cast<PHINode>(OpI)) {
            // Задание со звёздочкой: развернуть phi в его входные инструкции.
            for (Value *Inc : Phi->incoming_values())
              if (auto *IncI = dyn_cast<Instruction>(Inc)) {
                Edges.emplace_back(User, IncI->getOpcodeName());
                ++NumPhiExpanded;
              }
          } else {
            Edges.emplace_back(User, OpI->getOpcodeName());
          }
        }

        if (Edges.empty())
          continue;

        // Вставляем вызовы перед I: при каждом исполнении I в трассу попадут
        // все его рёбра в детерминированном порядке операндов.
        B.SetInsertPoint(I);
        for (auto &E : Edges) {
          Value *Args[] = {getOpcodeStr(B, E.first), getOpcodeStr(B, E.second)};
          B.CreateCall(TraceUse, Args);
          ++NumEdges;
        }
        ++NumInstrumented;
      }

      if (verifyFunction(F, &errs()))
        errs() << "[trace-use] WARNING: verify FAILED for " << F.getName()
               << '\n';
    }

    errs() << "[trace-use] module '" << M.getName() << "': instrumented "
           << NumInstrumented << " instrs, " << NumEdges << " use-edges ("
           << NumPhiExpanded << " from phi expansion)\n";

    return NumEdges ? PreservedAnalyses::none() : PreservedAnalyses::all();
  }
};

} // namespace

//===----------------------------------------------------------------------===//
// Регистрация плагина: и для `opt -passes=trace-use`, и для
// `clang -fpass-plugin=...` (вставка в самый конец конвейера -O2/-Os, чтобы
// инструментировать уже оптимизированный IR).
//===----------------------------------------------------------------------===//
PassPluginLibraryInfo getTraceUsePluginInfo() {
  return {LLVM_PLUGIN_API_VERSION, "TraceUse", "0.1", [](PassBuilder &PB) {
            // opt -load-pass-plugin ./libTraceUsePass.so -passes=trace-use
            PB.registerPipelineParsingCallback(
                [](StringRef Name, ModulePassManager &MPM,
                   ArrayRef<PassBuilder::PipelineElement>) {
                  if (Name == "trace-use") {
                    MPM.addPass(TraceUsePass{});
                    return true;
                  }
                  return false;
                });
            // clang -O2 -fpass-plugin=./libTraceUsePass.so ...  (после оптимизаций)
            PB.registerOptimizerLastEPCallback(
                [](ModulePassManager &MPM, OptimizationLevel) {
                  MPM.addPass(TraceUsePass{});
                });
          }};
}

extern "C" LLVM_ATTRIBUTE_WEAK PassPluginLibraryInfo llvmGetPassPluginInfo() {
  return getTraceUsePluginInfo();
}

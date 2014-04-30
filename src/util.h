#ifndef LLE_DISPLAY_H_H
#define LLE_DISPLAY_H_H

#include <list>
#include <vector>
#include <llvm/IR/Value.h>
#include <llvm/Support/raw_ostream.h>

#include <llvm/Analysis/MemoryDependenceAnalysis.h>

#define WALK_THROUGH_DEPTH 10

namespace lle
{
	typedef std::pair<llvm::MemDepResult,llvm::BasicBlock*> FindedDependenciesType;

	void pretty_print(llvm::Value* v,llvm::raw_ostream& o = llvm::outs());

	//walk through value depend tree to find func(V) == true
	//walk depth is under WALK_THROUTH_DEPTH
	bool walk_through_if(llvm::Value* V,std::function<bool(llvm::Value*)> func);

	//remove cast instruction for a value
	//because cast means the original value and the returned value is
	//semanticly equal
	llvm::Value* castoff(llvm::Value* v);

	void find_dependencies(llvm::Instruction*, const llvm::Pass*,
			llvm::SmallVectorImpl<FindedDependenciesType>&,
			llvm::NonLocalDepResult* NLDR = NULL);

}

inline 
llvm::raw_ostream& operator<<(llvm::raw_ostream& o,const llvm::MemDepResult& d){
	if(d.isClobber()) o<<"Clobber";
	else if(d.isDef()) o<<"Def";
	else if(d.isNonLocal()) o<<"NonLocal";
	else o<<"???";
	return o;
}
#endif

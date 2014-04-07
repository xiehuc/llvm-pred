#ifndef LLE_DISPLAY_H_H
#define LLE_DISPLAY_H_H

#include <vector>
#include <llvm/IR/Value.h>
#include <llvm/Support/raw_ostream.h>

#include <llvm/Analysis/MemoryDependenceAnalysis.h>

namespace lle
{
	typedef std::pair<llvm::MemDepResult,llvm::BasicBlock*> FindedDependenciesType;

	void pretty_print(llvm::Value* v,llvm::raw_ostream& o = llvm::outs());

	std::vector<llvm::Instruction*> resolve(llvm::Value*,std::vector<llvm::Value*>& resolved);

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
	else o<<"???";
	return o;
}
#endif

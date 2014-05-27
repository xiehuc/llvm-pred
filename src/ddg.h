#ifndef LLE_DDG_H_H
#define LLE_DDG_H_H

// Data Dependence Graph Representation

#include <list>
#include <unordered_set>
#include <unordered_map>
#include <llvm/IR/User.h>
#include <llvm/IR/Value.h>
#include <llvm/ADT/DenseMap.h>
#include <llvm/ADT/STLExtras.h>
#include <llvm/ADT/GraphTraits.h>

namespace lle{
   struct DDG;
   struct DDGNode;
}
namespace llvm{
   template<> struct GraphTraits<lle::DDG*>;
   template<> struct GraphTraits<lle::DDGNode*>;
}

struct lle::DDGNode: public llvm::DenseMap<llvm::Value*, std::vector<lle::DDGNode*> >::value_type
{
   typedef llvm::DenseMap<llvm::Value*, std::vector<lle::DDGNode*> > Super;
   typedef std::vector<lle::DDGNode*>::iterator iterator;

   DDGNode(llvm::Value* node){
      this->first = node;
   }

   iterator begin(){
      return this->second.begin();
   }

   iterator end(){
      return this->second.end();
   }

};

struct lle::DDG {
   typedef std::unordered_set<llvm::Value*> ResolvedListParam;
   typedef llvm::DenseMap<llvm::Value*, std::vector<lle::DDGNode*> > ResolvedList;
   typedef std::list<llvm::Value*> UnsolvedList;
   typedef std::unordered_map<llvm::Value*, llvm::Use*> ImplicityLinkType;

   ResolvedList resolved;
   UnsolvedList unsolved;
   DDGNode* root;
   DDG(ResolvedListParam& r,UnsolvedList& u,ImplicityLinkType& c);
   typedef ResolvedList::iterator iterator;

   iterator begin() { return resolved.begin(); }
   iterator end() { return resolved.end(); }

};

template<>
struct llvm::GraphTraits<lle::DDGNode*>
{
   typedef lle::DDGNode::iterator ChildIteratorType;

   static ChildIteratorType child_begin(lle::DDGNode* N){
      return N->begin();
   }

   static ChildIteratorType child_end(lle::DDGNode* N){
      return N->end();
   }
};

template<>
struct llvm::GraphTraits<lle::DDG*>:public llvm::GraphTraits<lle::DDGNode*>
{
   typedef lle::DDGNode NodeType;

   static NodeType* getEntryNode(lle::DDG* G){
      return G->root;
   }

   typedef lle::DDG::ResolvedList::value_type ValuePairTy;
   typedef std::pointer_to_unary_function<ValuePairTy&, NodeType*>
      DerefFun;
   typedef llvm::mapped_iterator<lle::DDG::iterator, DerefFun> nodes_iterator;

   static nodes_iterator nodes_begin(lle::DDG* G){
      return llvm::map_iterator(G->begin(), DerefFun(Deref));
   }
   static nodes_iterator nodes_end(lle::DDG* G){
      return llvm::map_iterator(G->end(), DerefFun(Deref));
   }
   static unsigned size(lle::DDG* G){
      return G->resolved.size();
   }

   static NodeType* Deref(ValuePairTy& V){
      return static_cast<NodeType*>(&V);
   }

};

#endif

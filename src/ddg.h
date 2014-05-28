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
   struct DDGraph;
   struct DDGNode;
}
namespace llvm{
   template<> struct GraphTraits<lle::DDGraph*>;
   template<> struct GraphTraits<lle::DDGNode*>;
}

struct lle::DDGNode: public llvm::DenseMap<llvm::Value*, std::vector<lle::DDGNode*> >::value_type
{
   typedef llvm::DenseMap<llvm::Value*, std::vector<lle::DDGNode*> > Container;
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


struct lle::DDGraph : public lle::DDGNode::Container
{
   typedef std::unordered_set<llvm::Value*> ResolvedListParam;
   typedef std::list<llvm::Value*> UnsolvedList;
   typedef std::unordered_map<llvm::Value*, llvm::Use*> ImplicityLinkType;

   UnsolvedList unsolved;
   DDGNode* root;
   DDGraph(ResolvedListParam& r,UnsolvedList& u,ImplicityLinkType& c,llvm::Value* root);
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
struct llvm::GraphTraits<lle::DDGraph*>:public llvm::GraphTraits<lle::DDGNode*>
{
   typedef lle::DDGNode NodeType;

   static NodeType* getEntryNode(lle::DDGraph* G){
      return G->root;
   }

   typedef lle::DDGraph::value_type ValuePairTy;
   typedef std::pointer_to_unary_function<ValuePairTy&, NodeType*>
      DerefFun;
   typedef llvm::mapped_iterator<lle::DDGraph::iterator, DerefFun> nodes_iterator;

   static nodes_iterator nodes_begin(lle::DDGraph* G){
      return llvm::map_iterator(G->begin(), DerefFun(Deref));
   }
   static nodes_iterator nodes_end(lle::DDGraph* G){
      return llvm::map_iterator(G->end(), DerefFun(Deref));
   }
   static unsigned size(lle::DDGraph* G){
      return G->size();
   }

   static NodeType* Deref(ValuePairTy& V){
      return static_cast<NodeType*>(&V);
   }

};

#endif

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
#include <llvm/Support/raw_ostream.h>
#include <llvm/Support/DOTGraphTraits.h>

#include "Resolver.h"

namespace lle{
   struct DDGraph;
   struct DDGNode;
   typedef llvm::DenseMap<llvm::Value*, std::pair<ulong, std::vector<DDGNode*> > > DDGValueType;
}
namespace llvm{
   template<> struct GraphTraits<lle::DDGraph*>;
   template<> struct GraphTraits<lle::DDGNode*>;
   template<> struct DOTGraphTraits<lle::DDGraph*>;
}

struct lle::DDGNode: 
   public DDGValueType::value_type
{
   typedef DDGValueType Container;
   typedef std::vector<lle::DDGNode*> Edges;
   typedef std::vector<lle::DDGNode*>::iterator iterator;
   enum Flags{
      NORMAL = 0,
      UNSOLVED = 1,
      IMPLICITY = 2,
   };

   DDGNode(llvm::Value* node, Flags flag){
      this->first = node;
      this->second.first = flag;
   }

   Flags& flags(){ return (Flags&)this->second.first; }
   iterator begin(){ return this->second.second.begin(); }
   iterator end(){ return this->second.second.end(); }
   Edges& edges(){ return this->second.second;}
};


/** never tring modify the data content
 *  never tring copy it
 */
struct lle::DDGraph : 
   public lle::DDGValueType
{
   DDGNode* root;
   DDGraph(lle::ResolveResult& RR, llvm::Value* root);
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

template<>
struct llvm::DOTGraphTraits<lle::DDGraph*> : public llvm::DefaultDOTGraphTraits
{
   DOTGraphTraits(bool isSimple = false) : DefaultDOTGraphTraits(isSimple) {}

   static std::string getGraphName(lle::DDGraph* G){ 
      return "Data Dependencies Graph";
   }

   static std::string getNodeAttributes(lle::DDGNode* N,lle::DDGraph* G){
      if(N->flags() == lle::DDGNode::UNSOLVED) return "style=dotted";
      else return "";
   }

   static std::string getEdgeAttributes(lle::DDGNode* S,lle::DDGNode::iterator T,lle::DDGraph* G){
      if(S->flags() == lle::DDGNode::IMPLICITY) return "color=red";
      else return "";
   }

   static bool renderGraphFromBottomUp(){ return true;}

   std::string getNodeLabel(lle::DDGNode* N,lle::DDGraph* G){
      std::string ret;
      llvm::raw_string_ostream os(ret);
      N->first->print(os);
      return ret;
   }
};

#endif

#ifndef LLE_DDG_H_H
#define LLE_DDG_H_H

// Data Dependence Graph Representation

#include <llvm/IR/User.h>
#include <llvm/IR/Value.h>
#include <llvm/IR/Function.h>
#include <llvm/ADT/DenseMap.h>
#include <llvm/ADT/STLExtras.h>
#include <llvm/ADT/SmallVector.h>
#include <llvm/ADT/GraphTraits.h>
#include <llvm/ADT/PointerUnion.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/Support/DOTGraphTraits.h>
#include <llvm/Support/CommandLine.h>

#include <list>
#include <deque>
#include <unordered_set>
#include <unordered_map>

#include "Resolver.h"
#include "util.h"

namespace lle{
extern llvm::cl::opt<bool> Ddg;
typedef llvm::PointerUnion<llvm::Value*, llvm::Use*> DDGraphKeyTy;
class DataDepGraph;
struct DataDepNode{
   typedef std::vector<DDGraphKeyTy> Impl;
   typedef Impl::iterator iterator;
   enum class Flags{
      SOLVED = 0,
      UNSOLVED = 1,
      IGNORED = 2
   };

   Impl childs_;     // the pointed childrens.
   Flags flags_;            // this node's type information
   DataDepGraph* parent_;

   DataDepNode():flags_(Flags::UNSOLVED) {};
   Flags flags(){return flags_;}
   iterator begin(){return childs_.begin();}
   iterator end(){return childs_.end();}
   DataDepGraph& parent(){ return *parent_;}
   Impl& impl(){return childs_;}
};
class DataDepGraph: public llvm::DenseMap<DDGraphKeyTy, DataDepNode>
{
   friend class lle::ResolveEngine;
   std::deque<llvm::Use*> unsolved;
   DDGraphKeyTy root; // the root for graph
   bool bottom_up;
   void isDependency(bool isDep){ bottom_up = isDep; }
   public:
   typedef llvm::DenseMap<DDGraphKeyTy, DataDepNode>::value_type value_type;
   // a helper function to get User from DDGValue
   static llvm::Value* get_user(value_type& v){
      if(llvm::Value* V = v.first.dyn_cast<llvm::Value*>()) return V;
      else if(llvm::Use* U = v.first.dyn_cast<llvm::Use*>()) return U->getUser();
      else return NULL;
   }

   DataDepGraph():bottom_up(false) {};
   void addUnsolved(llvm::Use& U){
      unsolved.push_back(&U);
   }
   void addUnsolved(llvm::Use*, llvm::Use*);
   // add a solved node.
   // @K is a solved Value* or Use*
   // @U is a unsolved Use, there a link K -> U
   void addSolved(DDGraphKeyTy K, llvm::Use& U){
      addSolved(K, &U, (&U)+1);
   }
   // add a solved node.
   // @F @T is a iterator from to.
   // which \forall V in [F,T) there a link K->V
   void addSolved(DDGraphKeyTy K, llvm::Use* F, llvm::Use* T);
   template<typename Ite>
   void addSolved(DDGraphKeyTy K, Ite F, Ite T)
   {
      for(Ite I = F; I!=T; ++I)
         addSolved(K, **I);
   }
   // add a solved constant value
   // @K isa solved Value* or Use*
   // @C isa constant, which naturally solved.
   void addSolved(DDGraphKeyTy K, llvm::Value* C);
   llvm::Use* popUnsolved(){
      if(unsolved.empty()) return NULL;
      llvm::Use* ret = unsolved.front();
      unsolved.pop_front();
      return ret;
   }
   void markIgnore(DDGraphKeyTy K){
      auto& N = (*this)[K];
      N.flags_ = DataDepNode::Flags::IGNORED;
   }

   void setRoot(DDGraphKeyTy K){ root = K; }
   DDGraphKeyTy getRootKey() { return root; }
   value_type& getRoot() {return *this->find(root);}
   bool isDenpendency() const {return bottom_up;}
};

}

namespace llvm{

template<> 
struct GraphTraits<lle::DataDepGraph::value_type*>
{
   typedef lle::DataDepGraph::value_type Self;
   typedef std::function<Self*(lle::DDGraphKeyTy&)> DerefFun;
   typedef mapped_iterator<lle::DataDepNode::iterator, DerefFun>
      ChildIteratorType;

   static ChildIteratorType child_begin(Self* N){
      using std::placeholders::_1;
      return map_iterator(N->second.begin(), 
            DerefFun(std::bind(Deref, _1, &N->second.parent())));
   }

   static ChildIteratorType child_end(Self* N){
      using std::placeholders::_1;
      return map_iterator(N->second.end(), 
            DerefFun(std::bind(Deref, _1, &N->second.parent())));
   }

   static Self* Deref(lle::DDGraphKeyTy V, lle::DataDepGraph* G){
      return &*G->find(V);
   }
};
template<>
struct GraphTraits<lle::DataDepGraph*>: public llvm::GraphTraits<lle::DataDepGraph::value_type*>
{
   typedef lle::DataDepGraph Self;
   typedef Self::value_type NodeType;

   static NodeType* getEntryNode(Self* G){
      return &G->getRoot();
   }

   typedef std::pointer_to_unary_function<NodeType&, NodeType*> DerefFun;
   typedef llvm::mapped_iterator<lle::DataDepGraph::iterator, DerefFun> nodes_iterator;

   static nodes_iterator nodes_begin(Self* G){
      return llvm::map_iterator(G->begin(), DerefFun(Deref));
   }
   static nodes_iterator nodes_end(Self* G){
      return llvm::map_iterator(G->end(), DerefFun(Deref));
   }
   static unsigned size(Self* G){
      return G->size();
   }

   static NodeType* Deref(NodeType& V){
      return static_cast<NodeType*>(&V);
   }

};
template<>
struct DOTGraphTraits<lle::DataDepGraph*> : public llvm::DefaultDOTGraphTraits
{
   DOTGraphTraits(bool isSimple = false) : DefaultDOTGraphTraits(isSimple) {}
   typedef lle::DataDepGraph Self;

   static std::string getGraphName(Self* G){ 
      return "Data Dependencies Graph";
   }

   static std::string getGraphProperties(const Self* G){
      std::string rankdir=G->isDenpendency()?"rankdir=\"BT\";":"";
      return "nodesep=1.5;\nnode [margin=\"0.5,0.055 \"];" + rankdir;
   }

   static std::string getNodeAttributes(Self::value_type* N,Self* G){
      auto F = N->second.flags();
      if(F == lle::DataDepNode::Flags::UNSOLVED)
         return "style=dashed";
      else if(F == lle::DataDepNode::Flags::IGNORED)
         return "style=dotted";
      else return "";
   }

   std::string getNodeLabel(Self::value_type* N, Self* G);
};

}

#endif

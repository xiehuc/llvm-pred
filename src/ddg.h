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
   struct DDGNode;
   typedef llvm::PointerUnion<llvm::Value*, llvm::Use*> DDGraphKeyTy;
   typedef llvm::DenseMap<DDGraphKeyTy, DDGNode > DDGraphImpl;
   typedef DDGraphImpl::value_type DDGValue;
   class DDGraph;
   extern llvm::cl::opt<bool> Ddg;
}
namespace llvm{
   template<> struct GraphTraits<lle::DDGraph*>;
   template<> struct GraphTraits<lle::DDGValue*>;
   template<> struct DOTGraphTraits<lle::DDGraph*>;
}

class lle::DDGNode{
   public:
   typedef std::string expr_type;
   typedef llvm::SmallVector<DDGValue*, 3> DDGNodeImpl;
   typedef DDGNodeImpl::iterator iterator;
   enum Flags{
      SOLVED = 0,
      UNSOLVED = 1,
      IMPLICITY = 2,
   };
   private:
   friend class DDGraph;
   DDGNodeImpl childs_;     // the pointed childrens.
   unsigned char prio;      // priority, 越小代表结合越紧密
   unsigned char ref_count; // the count other nodes pointed this
   uint ref_num_;           // associated number to output reference
   Flags flags_;            // this node's type information
   expr_type root;
#if 0
   llvm::Twine lhs,rhs,root,lbk,bk; /* the twines , the structure like this:
                                       a   +  b  ' '    (   root
                                       \   /  \   /      \   /
                                        lhs    rhs        lbk  )
                                          \    /           \  /
                                           root             bk
      because a twine can only hold two elements. plus bk means bracket.*/
#endif
   DDGValue* load_tg_;  /* when load's child is a call, this is the real
                           target(argument) for load. */

   public:
   DDGNode(Flags = UNSOLVED);
   std::string expr_buf; /* a buffer hold complex expression. because twine
                            doesn't hold memory target. */
   void set_expr(const expr_type& lhs,const expr_type& rhs = "", int prio = 0);
   const expr_type expr(int prio = 14); /* caculate expression with prio. when
      prio < self.prio return a bracket expression. because it means it would
      break the correct expression structure. */
   void ref(int R); // set reference number
   Flags flags(){return flags_;}
   iterator begin(){return childs_.begin();}
   iterator end(){return childs_.end();}
   DDGNodeImpl& impl(){return childs_;}
   DDGValue* load_inst(){return load_tg_;}
};

/** never tring modify the data content
 *  never tring copy it
 */
class lle::DDGraph : 
   public lle::DDGraphImpl
{
   // a helper function to make a DDGValue with initialed DDGNode structure
   DDGValue make_value(DDGraphKeyTy root, DDGNode::Flags flags);
   std::deque<llvm::Use*> unsolved;
   DDGValue* root; // the root for graph
   public:

   typedef DDGNode::expr_type expr_type;

   DDGraph(lle::ResolveResult& RR, llvm::Value* root);

   void setRoot(DDGraphKeyTy K){ root = &*this->find(K); }
   DDGValue* getRoot() {return root;}

   expr_type expr(); 
};

template<>
struct llvm::GraphTraits<lle::DDGValue*>
{
   typedef lle::DDGNode::iterator ChildIteratorType;

   static ChildIteratorType child_begin(lle::DDGValue* N){
      return N->second.begin();
   }

   static ChildIteratorType child_end(lle::DDGValue* N){
      return N->second.end();
   }
};

template<>
struct llvm::GraphTraits<lle::DDGraph*>:public llvm::GraphTraits<lle::DDGValue*>
{
   typedef lle::DDGValue NodeType;

   static NodeType* getEntryNode(lle::DDGraph* G){
      return G->getRoot();
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

   static std::string getNodeAttributes(lle::DDGValue* N,lle::DDGraph* G){
      if(N->second.flags() == lle::DDGNode::UNSOLVED) return "style=dotted";
      else return "";
   }

   static std::string getEdgeAttributes(lle::DDGValue* S,lle::DDGNode::iterator T,lle::DDGraph* G){
      if(S->second.flags() == lle::DDGNode::IMPLICITY) return "color=red";
      else return "";
   }

   static bool renderGraphFromBottomUp(){ return true;}

   std::string getNodeLabel(lle::DDGValue* N,lle::DDGraph* G);
};


namespace lle{
class DataDepGraph;
struct DataDepNode{
   typedef llvm::SmallVector<DDGraphKeyTy, 3> Impl;
   typedef Impl::iterator iterator;
   enum class Flags{
      SOLVED = 0,
      UNSOLVED = 1,
   };

   Impl childs_;     // the pointed childrens.
   Flags flags_;            // this node's type information
   DataDepGraph* parent_;

   DataDepNode(Flags = Flags::UNSOLVED);
   Flags flags(){return flags_;}
   iterator begin(){return childs_.begin();}
   iterator end(){return childs_.end();}
   DataDepGraph& parent(){ return *parent_;}
   Impl& impl(){return childs_;}
};
class DataDepGraph: public llvm::DenseMap<DDGraphKeyTy, DataDepNode>
{
   std::deque<llvm::Use*> unsolved;
   DDGraphKeyTy root; // the root for graph
   public:
   typedef llvm::DenseMap<DDGraphKeyTy, DataDepNode>::value_type value_type;
   // a helper function to get User from DDGValue
   static llvm::Value* get_user(value_type& v){
      if(llvm::Value* V = v.first.dyn_cast<llvm::Value*>()) return V;
      else if(llvm::Use* U = v.first.dyn_cast<llvm::Use*>()) return U->getUser();
      else return NULL;
   }

   DataDepGraph() {};
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

   void setRoot(DDGraphKeyTy K){ root = K; }
   value_type& getRoot() {return *this->find(root);}
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
            DerefFun(std::bind(Deref, _1, N->second.parent())));
   }

   static ChildIteratorType child_end(Self* N){
      using std::placeholders::_1;
      return map_iterator(N->second.end(), 
            DerefFun(std::bind(Deref, _1, N->second.parent())));
   }

   static Self* Deref(lle::DDGraphKeyTy V, lle::DataDepGraph& G){
      return &*G.find(V);
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

}

#endif

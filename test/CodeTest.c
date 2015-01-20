#include "../timing/libtiming.c"

#define REPEAT 1000
#define INSNUM 10000
/* use a template to generate instruction */
int inst_template(const char* templ, ...);
void test_ldAndSt(int ty){

   volatile int* var = malloc(sizeof(int));
   uint64_t t_res = timing_res();
   uint64_t t_err = timing_err();
   unsigned long beg = 0, end = 0, sum = 0, ref = 0;
   if(ty == 1){
      for(int i=0; i<REPEAT; ++i){
         beg = timing();
         //ref += load_template("load");
         inst_template("load",var);
         end = timing();
         sum += end-beg-t_err;
      }
      sum /= REPEAT;
      ref /= REPEAT;
      printf("load : %f cycles\n",(double)sum/INSNUM);
   }
   if(ty == 2){
      for(int i=0; i<REPEAT; ++i){
         beg = timing();
         //ref += load_template("load");
         inst_template("store",var);
         end = timing();
         sum += end-beg-t_err;
      }
      sum /= REPEAT;
      ref /= REPEAT;
      printf("store : %f cycles\n",(double)sum/INSNUM);
   }

}
void test_otherOp(int ty){
   uint64_t beg, end,ref, sum = 0;
   uint64_t t_err = timing_err();
   if(ty == 1){
      for(int i=0;i<REPEAT;++i){
         beg = timing();
         ref += inst_template("fix_add");
         end = timing();
         sum += end-beg-t_err;
      }
      sum /= REPEAT;
      ref /= REPEAT;
      printf("add : %f cycles\n",(double)sum/INSNUM);
   }
   if(ty == 2){
      for(int i=0;i<REPEAT;++i){
         beg = timing();
         ref += inst_template("float_add");
         end = timing();
         sum += end-beg-t_err;
      }
      sum /= REPEAT;
      ref /= REPEAT;
      printf("fadd : %f cycles\n",(double)sum/INSNUM);
   }
   if(ty == 3){
      for(int i=0;i<REPEAT;++i){
         beg = timing();
         ref += inst_template("fix_mul");
         end = timing();
         sum += end-beg-t_err;
      }
      sum /= REPEAT;
      ref /= REPEAT;
      printf("mul : %f cycles\n",(double)sum/INSNUM);
   }
   if(ty == 4){
      for(int i=0;i<REPEAT;++i){
         beg = timing();
         ref += inst_template("float_mul");
         end = timing();
         sum += end-beg-t_err;
      }
      sum /= REPEAT;
      ref /= REPEAT;
      printf("fmul : %f cycles\n",(double)sum/INSNUM);
   }
   if(ty == 5){
      for(int i=0;i<REPEAT;++i){
         beg = timing();
         ref += inst_template("fix_sub");
         end = timing();
         sum += end-beg-t_err;
      }
      sum /= REPEAT;
      ref /= REPEAT;
      printf("sub : %f cycles\n",(double)sum/INSNUM);
   }
   if(ty == 6){
      for(int i=0;i<REPEAT;++i){
         beg = timing();
         ref += inst_template("float_sub");
         end = timing();
         sum += end-beg-t_err;
      }
      sum /= REPEAT;
      ref /= REPEAT;
      printf("fsub : %f cycles\n",(double)sum/INSNUM);
   }
   if(ty == 7){
      for(int i=0;i<REPEAT;++i){
         beg = timing();
         ref += inst_template("u_div");
         end = timing();
         sum += end-beg-t_err;
      }
      sum /= REPEAT;
      ref /= REPEAT;
      printf("udiv : %f cycles\n",(double)sum/INSNUM);
   }
   if(ty == 8){
      for(int i=0;i<REPEAT;++i){
         beg = timing();
         ref += inst_template("s_div");
         end = timing();
         sum += end-beg-t_err;
      }
      sum /= REPEAT;
      ref /= REPEAT;
      printf("sdiv : %f cycles\n",(double)sum/INSNUM);
   }
   if(ty == 9){
      for(int i=0;i<REPEAT;++i){
         beg = timing();
         ref += inst_template("float_div");
         end = timing();
         sum += end-beg-t_err;
      }
      sum /= REPEAT;
      ref /= REPEAT;
      printf("fdiv : %f cycles\n",(double)sum/INSNUM);
   }
   if(ty == 10){
      for(int i=0;i<REPEAT;++i){
         beg = timing();
         ref += inst_template("u_rem");
         end = timing();
         sum += end-beg-t_err;
      }
      sum /= REPEAT;
      ref /= REPEAT;
      printf("urem : %f cycles\n",(double)sum/INSNUM);
   }
   if(ty == 11){
      for(int i=0;i<REPEAT;++i){
         beg = timing();
         ref += inst_template("s_rem");
         end = timing();
         sum += end-beg-t_err;
      }
      sum /= REPEAT;
      ref /= REPEAT;
      printf("srem : %f cycles\n",(double)sum/INSNUM);
   }
   if(ty == 12){
      for(int i=0;i<REPEAT;++i){
         beg = timing();
         ref += inst_template("float_rem");
         end = timing();
         sum += end-beg-t_err;
      }
      sum /= REPEAT;
      ref /= REPEAT;
      printf("frem : %f cycles\n",(double)sum/INSNUM);
   }
   if(ty == 13){
      for(int i=0;i<REPEAT;++i){
         beg = timing();
         ref += inst_template("shl");
         end = timing();
         sum += end-beg-t_err;
      }
      sum /= REPEAT;
      ref /= REPEAT;
      printf("shl : %f cycles\n",(double)sum/INSNUM);
   }
   if(ty == 14){
      for(int i=0;i<REPEAT;++i){
         beg = timing();
         ref += inst_template("lshr");
         end = timing();
         sum += end-beg-t_err;
      }
      sum /= REPEAT;
      ref /= REPEAT;
      printf("lshr : %f cycles\n",(double)sum/INSNUM);
   }
   if(ty == 15){
      for(int i=0;i<REPEAT;++i){
         beg = timing();
         ref += inst_template("ashr");
         end = timing();
         sum += end-beg-t_err;
      }
      sum /= REPEAT;
      ref /= REPEAT;
      printf("ashr : %f cycles\n",(double)sum/INSNUM);
   }
   if(ty == 16){
      for(int i=0;i<REPEAT;++i){
         beg = timing();
         ref += inst_template("and");
         end = timing();
         sum += end-beg-t_err;
      }
      sum /= REPEAT;
      ref /= REPEAT;
      printf("and : %f cycles\n",(double)sum/INSNUM);
   }
   if(ty == 17){
      for(int i=0;i<REPEAT;++i){
         beg = timing();
         ref += inst_template("or");
         end = timing();
         sum += end-beg-t_err;
      }
      sum /= REPEAT;
      ref /= REPEAT;
      printf("or : %f cycles\n",(double)sum/INSNUM);
   }
   if(ty == 18){
      for(int i=0;i<REPEAT;++i){
         beg = timing();
         ref += inst_template("xor");
         end = timing();
         sum += end-beg-t_err;
      }
      sum /= REPEAT;
      ref /= REPEAT;
      printf("xor : %f cycles\n",(double)sum/INSNUM);
   }
}
int main()
{
   test_ldAndSt(1);
   test_ldAndSt(2);
   test_otherOp(1);
   test_otherOp(2);
   test_otherOp(3);
   test_otherOp(4);
   test_otherOp(5);
   test_otherOp(6);
   test_otherOp(7);
   test_otherOp(8);
   test_otherOp(9);
   test_otherOp(10);
   test_otherOp(11);
   test_otherOp(12);
   test_otherOp(13);
   test_otherOp(14);
   test_otherOp(15);
   test_otherOp(16);
   test_otherOp(17);
   test_otherOp(18);
   return 0;
}


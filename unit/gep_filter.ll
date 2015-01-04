%ST = type { i32, double }
define i32* @main(){
  %ST* %s = alloca %ST;
  %i = getelementptr %ST* %s, i32 0, i32 0;
  store i32 1, %i
}

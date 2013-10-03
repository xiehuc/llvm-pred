
define i32 @inst_add(){
    %x = add i32 2,1
    ret i32 %x
}

define i32 @main(){
    call i32 @inst_add()
    ret i32 0
}


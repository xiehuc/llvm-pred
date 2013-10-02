define void @inst_add(){
    %x = add i32 2,1
    ret void
}

define i32 @main(){
    call void @inst_add()
    ret i32 0
}


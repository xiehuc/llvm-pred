define debug_reduce_call
break lle::ReduceCode::nousedOperator(llvm::Use&, llvm::Instruction*, lle::ConfigFlags)
  commands
    disable $bp_no
  end
set $bp_no = $bpnum
# kept memory
set $func_name = $arg0
disable $bp_no
break lle::ReduceCode::getAttribute(llvm::CallInst*) 
  commands
    d $bpnum
    break +1 if strcmp(Name.Data, $func_name)==0
      commands
        enable $bp_no
        c
      end
  end
end
document debug_reduce_call
  debug AttributeFlags ReduceCode::getAttribute(CallInst * CI)
  it would block on nousedOperator function
  usage: debug_reduce_call "mpi_irecv_"
end

#vim: filetype=gdb

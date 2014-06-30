; in this file, put need library function declare to attach useful information,
; to provide more accurate resolve result.

declare void @mpi_init_(i32* %err)
declare void @mpi_finalize_(i32* %err)
declare void @mpi_comm_size_(i32* noalias nocapture readonly %comm, i32* %size, i32* %err) "lle.arg.write"="1"
declare void @mpi_comm_rank_(i32* noalias nocapture readonly %comm, i32* %size, i32* %err) "lle.arg.write"="1"
; mpi_isend_
; mpi_irecv_
;declare void @mpi_allreduce_(
;    i8*  nocapture readonly %sendbuf, 
;    i8*                     %recvbuf,
;    i32* nocapture readonly %count, 
;    i32* nocapture readonly %datatype, 
;    i32* nocapture readonly %op, 
;    i32* nocapture readonly %comm,
;    i32*                    %err
;    )
; mpi_bcast_ IR中有两种格式, 
declare void @mpi_barrier_(i32* nocapture readonly %comm, i32* %err)
declare void @mpi_abort_(i32* nocapture readonly %comm, i32* %err)
;declare void @mpi_error_string_(
;    i32* nocapture readonly %errno,
;    i8*                     %buf,
;    i32*                    %err_len,
;    i32*                    %err,
;    i32  nocapture readonly %buf_len
;    )
declare void @mpi_waitall_(
    i32* nocapture readonly %count,
    i8*  nocapture readonly %request,
    i8*                     %status,
    i32*                    %err
    )


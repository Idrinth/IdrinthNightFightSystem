PUBLIC CalculateDamage_ThunkStub
EXTERN CalculateDamage_RealThunk:PROC

.code

CalculateDamage_ThunkStub PROC
    ; rcx currently holds RE::HitData* (per Windows x64 calling convention)
    push rbp
    mov  rbp, rsp
    and  rsp, -16          ; force 16-byte alignment
    sub  rsp, 20h          ; shadow space for the callee
    call CalculateDamage_RealThunk
    mov  rsp, rbp
    pop  rbp
    ret
CalculateDamage_ThunkStub ENDP

END
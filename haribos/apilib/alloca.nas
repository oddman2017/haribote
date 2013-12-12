[FORMAT "WCOFF"]
[INSTRSET "i486p"]
[BITS 32]
[FILE "alloca.nas"]

		GLOBAL	__alloca

[SECTION .text]

__alloca:
		ADD		EAX,-4
		SUB		ESP,EAX
		JMP		DWORD [ESP+EAX]		; RETの代わり


;=========================================================
;
; sub esp, eax
; add esp, 4
; jmp dword [esp+eax-4]             ; replace of ret
;
;=========================================================

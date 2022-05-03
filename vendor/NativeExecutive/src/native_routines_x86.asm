.model flat
.code

INCLUDE native_shared.inc

FiberStatus STRUCT
	callee DWORD	?
	termcb DWORD	?
	status DWORD	?
FiberStatus ENDS

Fiber STRUCT
	reg_ebx DWORD		?
	reg_edi DWORD		?
	reg_esi DWORD		?
	reg_esp DWORD		?
	reg_eip DWORD		?
	reg_ebp DWORD		?

	stack_base DWORD PTR	?
	stack_limit DWORD PTR	?
	except_info DWORD PTR	?

	stackSize DWORD		?
Fiber ENDS

; FIBER ROUTINES

; CDECL???
__fiber86_retHandler@4 proc
	; Set thread status to terminated
	mov eax,[esp+4]

	ASSUME eax:PTR FiberStatus

    mov [eax].status,FIBER_TERMINATED

    ; Cache the termination function
    mov edx,[eax].termcb

	mov ecx,eax

    ; Apply registers
    mov eax,[eax].callee

	ASSUME eax:PTR Fiber

    mov ebx,[eax].reg_ebx
    mov edi,[eax].reg_edi
    mov esi,[eax].reg_esi
    mov esp,[eax].reg_esp
    mov ebp,[eax].reg_ebp

    push [eax].reg_eip  ; The return address
	push ecx			; Push userdata as preparation

;ifdef _WIN32
    ; Save the termination function.
    push edx

    ASSUME fs:nothing

    ; Apply exception and stack info
    mov ecx,[eax].stack_base
    mov edx,[eax].stack_limit
    mov fs:[4],ecx
    mov fs:[8],edx
    mov ecx,[eax].except_info
    mov fs:[0],ecx

    ASSUME fs:error

    ; Pop the termination function.
    pop edx
;endif

	ASSUME eax:nothing

	; Terminate our thread
    call edx
    add esp,4

    ret
__fiber86_retHandler@4 endp

; CDECL
__fiber86_eswitch proc
    ; Save current environment
    mov eax,[esp+4]

    ASSUME eax:PTR Fiber

    mov [eax].reg_ebx,ebx
    mov [eax].reg_edi,edi
    mov [eax].reg_esi,esi
    add esp,4
    mov [eax].reg_esp,esp
    mov ebx,[esp-4]
    mov [eax].reg_eip,ebx
    mov [eax].reg_ebp,ebp

;ifdef _WIN32
    ASSUME fs:nothing

    ; Save exception and stack info
    mov ebx,fs:[0]
    mov ecx,fs:[4]
    mov edx,fs:[8]
    mov [eax].stack_base,ecx
    mov [eax].stack_limit,edx
    mov [eax].except_info,ebx

    ASSUME fs:error
;endif

    ; Apply registers
    mov eax,[esp+4]
    mov ebx,[eax].reg_ebx
    mov edi,[eax].reg_edi
    mov esi,[eax].reg_esi
    mov esp,[eax].reg_esp
    mov ebp,[eax].reg_ebp

;ifdef _WIN32
    ASSUME fs:nothing

    ; Apply exception and stack info
    mov ecx,[eax].stack_base
    mov edx,[eax].stack_limit
    mov fs:[4],ecx
    mov fs:[8],edx
    mov ecx,[eax].except_info
    mov fs:[0],ecx

    ASSUME fs:error
;endif

    jmp dword ptr[eax].reg_eip

    ASSUME eax:nothing
__fiber86_eswitch endp

; CDECL
__fiber86_qswitch proc
    ; Save current environment
    mov eax,[esp+4]         ; grab the "from" context

    ASSUME eax:PTR Fiber

    mov [eax].reg_ebx,ebx
    mov [eax].reg_edi,edi
    mov [eax].reg_esi,esi
    add esp,4
    mov [eax].reg_esp,esp
    mov ebx,[esp-4]
    mov [eax].reg_eip,ebx
    mov [eax].reg_ebp,ebp

;ifdef _WIN32
    ASSUME fs:nothing

    ; Save exception info
    mov ebx,fs:[0]
    mov [eax].except_info,ebx

    ASSUME fs:error
;endif

    ; Apply registers
    mov eax,[esp+4]         ; grab the "to" context 

    ASSUME eax:PTR Fiber

    mov ebx,[eax].reg_ebx
    mov edi,[eax].reg_edi
    mov esi,[eax].reg_esi
    mov esp,[eax].reg_esp
    mov ebp,[eax].reg_ebp

;ifdef _WIN32
    ASSUME fs:nothing

    ; Apply exception and stack info
    mov ecx,[eax].stack_base
    mov edx,[eax].stack_limit
    mov fs:[4],ecx
    mov fs:[8],edx
    mov ecx,[eax].except_info
    mov fs:[0],ecx

    ASSUME fs:error
;endif

    jmp dword ptr[eax].reg_eip

    ASSUME eax:nothing
__fiber86_qswitch endp

__fiber86_leave proc
    ; We assume that eax contains the fiber ptr.
    ; This is an optimized function to give up a context 
    ; and immediatly switch to a Fiber.

    ASSUME eax:PTR Fiber

    ; Apply registers
    mov ebx,[eax].reg_ebx
    mov edi,[eax].reg_edi
    mov esi,[eax].reg_esi
    mov esp,[eax].reg_esp
    mov ebp,[eax].reg_ebp

;ifdef _WIN32
    ASSUME fs:nothing

    ; Apply exception and stack info
    mov ecx,[eax].stack_base
    mov edx,[eax].stack_limit
    mov fs:[4],ecx
    mov fs:[8],edx
    mov ecx,[eax].except_info
    mov fs:[0],ecx

    ASSUME fs:error
;endif

    jmp dword ptr[eax].reg_eip

    ASSUME eax:PTR nothing
__fiber86_leave endp

; THREAD ROUTINES.

nativeThreadPlugin STRUCT
    terminationReturn DWORD     ?
nativeThreadPlugin ENDS

EXTERN _nativeThreadPluginInterface_ThreadProcCPP@4:PROC
EXTERN _nativeThreadPluginInterface_OnNativeThreadEnd@4:PROC

; STDCALL
__thread86_procNative@4 PROC
    ; Call the C++ thread runtime with our argument.
    mov ebx,[esp+4] ; nativeThreadPlugin type

    ASSUME ebx:PTR nativeThreadPlugin

    push ebx
    call _nativeThreadPluginInterface_ThreadProcCPP@4

    ; Push it in preparation for the function call.
    push ebx

    ; Check for a termination fiber.
    mov ebx,[ebx].terminationReturn

    ; Finished using thread, so notify the manager.
    call _nativeThreadPluginInterface_OnNativeThreadEnd@4

    ASSUME ebx:nothing

    test ebx,ebx
    jz NoTerminationReturn

    ; Since we have a termination return fiber, leave to it.
    mov eax,ebx
    jmp __fiber86_leave

NoTerminationReturn:
    ; We return to the WinNT thread dispatcher.
    mov eax,0
    ret
__thread86_procNative@4 ENDP

end
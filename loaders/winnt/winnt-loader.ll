; This is an LLVM file that implements code to hunt for the PEB in a running
; Windows process. From there, it locates the base address of kernel32.dll.
; It also provides an implementation of GetProcAddress.
; These are then used by all Win32 loader stubs to load their various symbols.

%struct.PEB = type { [4 x i8], [2 x i8*], %struct.PEB_LDR_DATA* }
define private %struct.PEB* @.WINNT.getPEB() readonly {
  %PEBPTR = call i8* asm "mov %fs:0x30, $0", "=r,~{dirflag},~{fpsr},~{flags}"()
  %PEB = bitcast i8* %PEBPTR to %struct.PEB*
  ret %struct.PEB* %PEB
}

%struct.PEB_LDR_DATA = type { [8 x i8], [3 x i8*], %struct.LIST_ENTRY* }
define private %struct.PEB_LDR_DATA* @.WINNT.getLDR() readonly {
  %PEB = call %struct.PEB* @.WINNT.getPEB()
  %LDRPTR = getelementptr inbounds %struct.PEB, %struct.PEB* %PEB, i32 0, i32 2
  %LDR = load %struct.PEB_LDR_DATA*, %struct.PEB_LDR_DATA** %LDRPTR
  ret %struct.PEB_LDR_DATA* %LDR
}

; Here's the function that actually tracks down the base of kernel32.dll
%struct.UNICODE_STRING = type { i16, i16, i16* }
%struct.LIST_ENTRY = type { [2 x %struct.LIST_ENTRY*], [2 x i8*], i8*, i8*, i8*, %struct.UNICODE_STRING }
define linkonce_odr i8* @.WINNT.FindKernel32() readonly {
  %LDR = call %struct.PEB_LDR_DATA* @.WINNT.getLDR()
  %first_entry_ptr = getelementptr inbounds %struct.PEB_LDR_DATA, %struct.PEB_LDR_DATA* %LDR, i32 0, i32 2
  %first_entry = load %struct.LIST_ENTRY*, %struct.LIST_ENTRY** %first_entry_ptr
  br label %search_loop

search_loop:
  %current_entry = phi %struct.LIST_ENTRY* [%first_entry, %0], [%next_entry, %load_next]
  %unicode_ptr = getelementptr inbounds %struct.LIST_ENTRY, %struct.LIST_ENTRY* %current_entry, i32 0, i32 5
  %is_kernel32 = call i1 @.WINNT.IsKernel32(%struct.UNICODE_STRING* %unicode_ptr)
  br i1 %is_kernel32, label %success, label %load_next

load_next:
  %next_entry_ptr = getelementptr inbounds %struct.LIST_ENTRY, %struct.LIST_ENTRY* %current_entry, i32 0, i32 0, i32 0
  %next_entry = load %struct.LIST_ENTRY*, %struct.LIST_ENTRY** %next_entry_ptr
  br label %search_loop

success:
  %kernel32_base_ptr = getelementptr inbounds %struct.LIST_ENTRY, %struct.LIST_ENTRY* %current_entry, i32 0, i32 2
  %kernel32_base = load i8*, i8** %kernel32_base_ptr
  ret i8* %kernel32_base
}

@.str.kernel32 = private constant [12 x i8] c"kernel32.dll"
define private i1 @.WINNT.IsKernel32(%struct.UNICODE_STRING* %unicode_ptr) readonly {
  %unicode_len_ptr = getelementptr inbounds %struct.UNICODE_STRING, %struct.UNICODE_STRING* %unicode_ptr, i32 0, i32 0
  %unicode_buf_ptr = getelementptr inbounds %struct.UNICODE_STRING, %struct.UNICODE_STRING* %unicode_ptr, i32 0, i32 2
  %unicode_len = load i16, i16* %unicode_len_ptr
  %unicode_buf_base = load i16*, i16** %unicode_buf_ptr
  %unicode_buf_base_8 = bitcast i16* %unicode_buf_base to i8*
  %unicode_start_offset = sub i16 %unicode_len, 24 ; UTF-16 bytes
  %unicode_buf_8 = getelementptr i8, i8* %unicode_buf_base_8, i16 %unicode_start_offset
  %unicode_buf = bitcast i8* %unicode_buf_8 to i16*
  %long_enough = icmp sge i16 %unicode_start_offset, 0
  br i1 %long_enough, label %loop, label %no

loop:
  %i = phi i8 [0, %0], [%i_plus_1, %loop]
  %prev_diff_mask = phi i16 [0, %0], [%diff_mask, %loop]

  %unicode_char_ptr = getelementptr i16, i16* %unicode_buf, i8 %i
  %unicode_char = load i16, i16* %unicode_char_ptr
  %kernel32_char_ptr = getelementptr [12 x i8], [12 x i8]* @.str.kernel32, i8 0, i8 %i 
  %kernel32_char = load i8, i8* %kernel32_char_ptr
  %kernel32_wchar = zext i8 %kernel32_char to i16

  %xor = xor i16 %kernel32_wchar, %unicode_char
  %diff_mask = or i16 %prev_diff_mask, %xor

  %i_plus_1 = add i8 %i, 1
  %should_loop = icmp ult i8 %i_plus_1, 12
  br i1 %should_loop, label %loop, label %loop_complete

loop_complete:
  %diff_mask_or_32 = or i16 %diff_mask, 32 ; 0x0020
  %match = icmp eq i16 %diff_mask_or_32, 32
  ret i1 %match

no:
  ret i1 0
}

; And this is an implementation of GetProcAddress
%struct.PE_HEADER_32 = type <{ 
  i32, i16, i16, i32, i32, i32, i16, i16, ; COFF stuff; 8 entries
  ; Optional header:
  i16, i8, i8, i32, i32, i32, i32, i32, ; signature-BaseOfCode
  i32, ; BaseOfData - only present in 32-bit
  i8*, i32, i32, i16, i16, i16, i16, i16, i16, i32, i32, i32, i32, ; ImageBase-CheckSum
  i16, i16, i8*, i8*, i8*, i8*, i32, i32, ; Subsystem-NumberOfRvaAndSizes
  i32 ; ExportTable (RVA)
}>
%struct.PE_HEADER_64 = type <{ 
  i32, i16, i16, i32, i32, i32, i16, i16, ; COFF stuff; 8 entries
  ; Optional header:
  i16, i8, i8, i32, i32, i32, i32, i32, ; signature-BaseOfCode
  i32, ; BaseOfData - only present in 32-bit
  i8*, i32, i32, i16, i16, i16, i16, i16, i16, i32, i32, i32, i32, ; ImageBase-CheckSum
  i16, i16, i8*, i8*, i8*, i8*, i32, i32, ; Subsystem-NumberOfRvaAndSizes
  i32 ; ExportTable (RVA)
}>
%struct.EDT = type <{
  i32, i32, i16, i16, i32, i32, i32, i32, i32, i32, i32
}>
define linkonce_odr i8* @.WINNT.GetProcAddress(i8* %hmodule, i8* %procname) {
  %pehdr_rva_ptr = getelementptr i8, i8* %hmodule, i8 60 ;0x3C
  %pehdr_rva_ptr_32 = bitcast i8* %pehdr_rva_ptr to i32*
  %pehdr_rva = load i32, i32* %pehdr_rva_ptr_32

  %pehdr_ptr_8 = getelementptr i8, i8* %hmodule, i32 %pehdr_rva
  %pehdr32_ptr = bitcast i8* %pehdr_ptr_8 to %struct.PE_HEADER_32*
  %pehdr64_ptr = bitcast i8* %pehdr_ptr_8 to %struct.PE_HEADER_64*

  %signature_ptr = getelementptr %struct.PE_HEADER_32, %struct.PE_HEADER_32* %pehdr32_ptr, i32 0, i32 8
  %signature = load i16, i16* %signature_ptr
  %is_32 = icmp eq i16 %signature, 267
  br i1 %is_32, label %read_32, label %read_64

read_32:
  %edt_rva_ptr_32 = getelementptr %struct.PE_HEADER_32, %struct.PE_HEADER_32* %pehdr32_ptr, i32 0, i32 38
  br label %read_edt

read_64:
  %edt_rva_ptr_64 = getelementptr %struct.PE_HEADER_64, %struct.PE_HEADER_64* %pehdr64_ptr, i32 0, i32 37
  br label %read_edt

read_edt:
  %edt_rva_ptr = phi i32* [%edt_rva_ptr_32, %read_32], [%edt_rva_ptr_64, %read_64]
  %edt_rva = load i32, i32* %edt_rva_ptr
  %edt_ptr_8 = getelementptr i8, i8* %hmodule, i32 %edt_rva
  %edt_ptr = bitcast i8* %edt_ptr_8 to %struct.EDT*

  %numnames_ptr = getelementptr %struct.EDT, %struct.EDT* %edt_ptr, i32 0, i32 7
  %functions_rva_ptr = getelementptr %struct.EDT, %struct.EDT* %edt_ptr, i32 0, i32 8
  %names_rva_ptr = getelementptr %struct.EDT, %struct.EDT* %edt_ptr, i32 0, i32 9
  %numnames = load i32, i32* %numnames_ptr
  %functions_rva = load i32, i32* %functions_rva_ptr
  %names_rva = load i32, i32* %names_rva_ptr
  %functions_ptr_8 = getelementptr i8, i8* %hmodule, i32 %functions_rva
  %names_ptr_8 = getelementptr i8, i8* %hmodule, i32 %names_rva
  %functions_ptr = bitcast i8* %functions_ptr_8 to i32*
  %names_ptr = bitcast i8* %names_ptr_8 to i32*

  br label %loop_header

loop_header:
  %names_left = phi i32 [%numnames, %read_edt], [%names_minus_1, %loop]
  %should_continue = icmp ne i32 %names_left, 0
  br i1 %should_continue, label %loop, label %failure

loop:
  %names_minus_1 = sub i32 %names_left, 1

  %name_rva_ptr = getelementptr i32, i32* %names_ptr, i32 %names_minus_1
  %function_rva_ptr = getelementptr i32, i32* %functions_ptr, i32 %names_minus_1

  %name_rva = load i32, i32* %name_rva_ptr
  %name_ptr = getelementptr i8, i8* %hmodule, i32 %name_rva

  %cmp = call i1 @.strcmp(i8* %procname, i8* %name_ptr)
  br i1 %cmp, label %success, label %loop_header

success:
  %function_rva = load i32, i32* %function_rva_ptr
  %function_ptr = getelementptr i8, i8* %hmodule, i32 %function_rva
  ret i8* %function_ptr

failure:
  ret i8* null
}

define private i1 @.strcmp(i8* %a, i8* %b) {
  br label %loop

loop:
  %pos = phi i32 [0, %0], [%pos_plus_1, %next_loop]

  %achar_ptr = getelementptr i8, i8* %a, i32 %pos
  %bchar_ptr = getelementptr i8, i8* %b, i32 %pos

  %achar = load i8, i8* %achar_ptr
  %bchar = load i8, i8* %bchar_ptr
  %match = icmp eq i8 %achar, %bchar
  %zero = icmp eq i8 %achar, 0

  br i1 %match, label %next_loop, label %no

next_loop:
  %pos_plus_1 = add i32 %pos, 1
  br i1 %zero, label %yes, label %loop

no:
  ret i1 0

yes:
  ret i1 1
}

; Here's what everyone's here for:
@.WINNT.kernel32 = linkonce global i8* undef
@.WINNT.kernel32.LoadLibraryA = linkonce global i8*(i8*)* undef

define private void @.WINNT.ctor.kernel32() {
  %kernel32 = call i8* @.WINNT.FindKernel32()
  store i8* %kernel32, i8** @.WINNT.kernel32
  ret void
}

@.str.LoadLibraryA = private constant [13 x i8] c"LoadLibraryA\00"
define private void @.WINNT.ctor.kernel32.LoadLibraryA() {
  %kernel32 = load i8*, i8** @.WINNT.kernel32
  %LoadLibraryA_8 = call i8* @.WINNT.GetProcAddress(i8* %kernel32, i8* getelementptr ([13 x i8], [13 x i8]* @.str.LoadLibraryA, i32 0, i32 0))
  %LoadLibraryA = bitcast i8* %LoadLibraryA_8 to i8*(i8*)*
  store i8*(i8*)* %LoadLibraryA, i8*(i8*)** @.WINNT.kernel32.LoadLibraryA
  ret void
}

%0 = type { i32, void ()*, i8* }
@llvm.global_ctors = appending global [2 x %0] [
  %0 { i32 0, void ()* @.WINNT.ctor.kernel32, i8* bitcast (i8** @.WINNT.kernel32 to i8*) },
  %0 { i32 1, void ()* @.WINNT.ctor.kernel32.LoadLibraryA, i8* bitcast (i8*(i8*)** @.WINNT.kernel32.LoadLibraryA to i8*) }
]

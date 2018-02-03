SheLLVM
=======

SheLLVM (pronounced either "shell-ell-ell-vee-em" or "shell-vee-em" but never
with a long e as in "she") is a collection of LLVM analysis and transform passes
to help developers compile lightly- to moderately-complex C(++) programs as
position-independent "load anywhere and jump to the beginning" machine code.

While this project started as a toolkit for writing shellcode in plain C, it
can really apply to any situation where a developer needs a program compiled in
a platform-independent and position-independent way.

Example usage
-------------

```
// main.c
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

extern void say_hello();

__attribute__((annotate("shellvm-main")))
void shellcode()
{
	while(1) {
		Sleep(1000);
		say_hello();
	}
}
```

```
// hello.c
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

void say_hello()
{
	MessageBox(NULL, "Hello, SheLLVM world!", "Hello", 0);
}
```

```
clang -target i686-w64-mingw32 -c -emit-llvm -o main.bc main.c
clang -target i686-w64-mingw32 -c -emit-llvm -o hello.bc hello.c
llvm-link -o linked.bc main.bc hello.bc shellvm-built/winnt-{user,kernel}32.bc
clang -load=shellvm-built/shellvm.so -O3 -shellvm -o shellcode.elf linked.bc
objcopy -O binary --only-section=.text shellcode.elf shellcode.bin
msfvenom -p - -a i386 --platform win32 -e x86/shikata_ga_nai < shellcode.bin > shellcode_encoded.bin
```

Features
--------

- **Portable**: SheLLVM makes no assumption about architecture. While this is
  most heavily tested and developed on x86/amd64, the passes involved run on
  pure LLVM IR and should work anywhere the LLVM ecosystem works.
- **Flexible**: While there are some guides to writing shellcode in C on the
  'net already, these usually impose a lot of restrictions on how the C code
  can be written; for example, by forbidding the use of string constants and
  global variables. SheLLVM attempts to provide a more "conventional" C
  environment while producing comparably self-contained code.
- **Platform-independent**: SheLLVM-generated code tries to keep to itself as
  much as possible. It does not rely on OS API calls or make assumptions about
  the memory layout of the target system. It does not need to unpack itself or
  allocate additional memory; all it requires is that it be loaded into a
  readable/executable segment and have the processor's stack initialized when
  execution begins. This makes SheLLVM suitable for use even in deeply embedded
  circumstances.
- **Compatible**: While this is primarily focused on C compatibility, there's
  no reason it can't work with other languages which use LLVM in the code
  generation pipeline, such as Swift or Rust. Patches for those languages very
  welcome!

Limitations
-----------

Before we talk about the limitations of SheLLVM, let's first talk about the
limitations of shellcode.

1. **It must be position-independent.** While there are some circumstances
   where shellcode might land at a fixed address in an injected program,
   most of the time there's no guarantee whatsoever. Shellcode must rely on
   relative addressing only.
2. **It must fit in a single segment.** Shellcode does not get the luxury of
   having a .text, .data, and .rdata segment. A single contiguous block must be
   loaded somewhere and the program must operate entirely in this environment.
3. **It does not get the benefit of the OS loader.** While this is the cause
   for #1 and #2 above, it also means relying on OS dynamic libraries is
   forbidden.
4. **It's usually loaded into a non-writeable segment.** Back in the 90s,
   before the importance of [W^X](https://en.wikipedia.org/wiki/W%5EX) was
   widely understood, shellcode could reasonably expect to land in RWX
   (readable, writeable, and executable) memory. Nowadays, shellcode may have
   to execute in R-X memory (and sometimes even --X memory).
5. **The only memory sure to be writeable is the stack.** This is a blessing
   and a curse. On one hand, the stack is always there and most architectures
   have a register pointing to it and everything. On the other hand, the stack
   can overflow if not used judiciously.

So, let's look at how these translate into limitations when using SheLLVM:

1. **All code must be compiled to LLVM bitcode.** These are LLVM passes. They
   can only work on LLVM IR. The code must be presented to SheLLVM as a single
   LLVM .bc file, which means heavy use of `llvm-link` and `clang -emit-llvm`.
2. **No linker.** Or, more precisely, no _object code_ linker. Linking
   functionality is still provided by LLVM, but this means all linking must be
   done with LLVM modules and not object files. Importantly, this means...
3. **No libraries.** Sorry, no `myfavoritelib.lib` or `libcoolthing.a`. If you
   want to use a third-party library, you're going to have to compile it as
   LLVM bitcode yourself and link it in with `llvm-link`. Most notably,
   however, this means you cannot rely on the Win32 API or C standard library.
   SheLLVM provides _loader stubs_ for dynamic libraries on certain platforms,
   which provide the standard platform API without needing to write
   symbol-hunting code by hand.
4. **You must have a single main function.** No linker/loader means no
   symbols. You can't export a collection of functions. The only function
   callable from the outside world is your main function.
5. **Your main function must have the longest lifetime.** Because the only
   writeable memory is the stack (as explained above), the main function ends
   up taking the responsibility for allocating all memory in its stack frame
   and freeing it when done. Similarly, the main function handles all
   constructors and destructors. As a consequence, the main function must be
   the first thing to run and the last thing to exit. (This is only really an
   issue for programs using threading and callbacks.)
6. **Your code has amnesia.** Since all state resides on the stack, this means
   your globals (and, by extension, static variables) have a lifetime only as
   long as your main function is running. This means that, while your globals
   (and static variables) will behave normally during a given run of your code,
   they will be reset to their default values during a subsequent run of your
   main function.

There are further limitations depending on which SheLLVM style you use.

SheLLVM Style 1 ("Megafunction Style")
======================================

In this style, SheLLVM functions as, essentially, a hyperaggressive inliner. It
attempts to reduce your entire program down to a single function, with no
callstack and no constants or globals on the heap.

Pros
----

- Works in --X memory.
- Compatible with most (all?) LLVM code generators.
- Highest code density.
- Does not use the stack for calls at all.

Cons
----

- All functions must be inlinable. In particular, this means no recursion is
  allowed (you **may** be able to get away with tail-call self-recursion, as
  LLVM's optimizer is pretty good at turning those into ordinary loops) and,
  perhaps more importantly, you must never take the address of any of your
  functions. **This means no callbacks or threads.**
- Everything resides on the stack, even large const arrays. This can be a
  problem for large programs, since read-only data is written to the stack
  instruction-by-instruction instead of being loaded into memory as-is.

~~SheLLVM Style 2 ("Concatenated Style")~~
==========================================

Note: This has not been implemented yet.

In this style, SheLLVM does not attempt to inline functions or restrict itself
to a single frame on the call stack. Instead, all global variables are placed
in a massive struct which lives in the stack frame of the main function. Every
other function's parameters list is modified to accept a pointer to this
struct, in order to provide global variable support.

Functions used as external callbacks must be annotated so as not to receive this
modification. In this case, it's the programmer's responsibility to restore the
pointer to the globals object (via `__shellvm_save()` and `__shellvm_restore()`
intrinsics).

Multiple functions (and constant heap variables) are concatenated together in
the output binary, and a small assembler stub at the entry point deduces from
the instruction pointer where it has been loaded in memory and computes, from
an offset table, the addresses of each function and constant in the program.


Pros
----

- Supports recursion and taking the addresses of functions.
- Supports threading and callbacks, when done carefully.
- Requires much less stack, due to constants being interspersed in the
  program data instead.

Cons
----

- Only works when the code segment is readable (R-X).
- Requires special handling at the assembly level.
- Much more complex.
- Callbacks from outside of LLVM code (e.g. due to spawning a thread) require
  the use of special SheLLVM intrinsics to save/restore the globals object.


Passes in SheLLVM
=================

These are the passes implemented in SheLLVM:

`-shellvm-prepare`
------------------

This pass makes sure exactly one function is marked as the main function (via
the `__attribute__((annotate("shellvm-main")))` annotation). It removes this
annotation, replaces it with an LLVM _attribute_, and marks all other functions
in the module as private.

`-shellvm-precheck`
-------------------

This just checks that all functions in the module are marked `norecurse`,
and that all functions are `unnamed_addr` (except for the main function, which
must be `local_unnamed_addr`).

`-mergecalls`
-------------

This merges call instructions which target the same function into the same
basic block, using a `switch` statement on the other end to branch back to
where the call left off.

Because this could be useful outside of SheLLVM, it does not have the
`-shellvm-` prefix.

`-shellvm-flatten`
------------------

This uses `-mergecalls` on the main function repeatedly, inlining each merged
callsite each time. It's responsible for taking a full function call graph and
flattening it down to a single function.

`-shellvm-global2stack`
-----------------------

This inlines all global variables (constant or not) which are used by only one
function into the stack of said function. Note that this can/will heavily break
non-SheLLVM programs if not used with care.

`-shellvm-inlinectors`
----------------------

This inlines LLVM ctors/dtors into the SheLLVM main function.

`-shellvm-postcheck`
--------------------

This ensures that the resultant module contains no globals, only one function,
no switch statements, etc.

It generally makes sure that the LLVM module is ready for code generation and
will behave as proper shellcode when lowered into machine instructions.

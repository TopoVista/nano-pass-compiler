; ModuleID = 'test'
source_filename = "test"

@0 = private unnamed_addr constant [4 x i8] c"%d\0A\00", align 1
@1 = private unnamed_addr constant [12 x i8] c"hello world\00", align 1
@2 = private unnamed_addr constant [4 x i8] c"%s\0A\00", align 1

define void @main() {
entry:
  %0 = call i32 (ptr, ...) @printf(ptr @0, i32 42)
  %1 = call i32 (ptr, ...) @printf(ptr @2, ptr @1)
  ret void
}

declare i32 @printf(ptr, ...)

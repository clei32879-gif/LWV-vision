/**
 * @file onnxrt_mingw_compat.h
 * @brief MinGW 编译 ONNX Runtime 头文件的 SAL 注解兼容层
 * (通过 -include 强制前置包含; MSVC 下不需要)
 */
#pragma once

#define _Frees_ptr_opt_
#define _In_
#define _In_opt_
#define _Inout_
#define _Inout_opt_
#define _Out_
#define _Outptr_
#define _Outptr_opt_
#define _Outptr_result_maybenull_
#define _Outptr_result_buffer_(x)
#define _In_reads_(x)
#define _In_reads_opt_(x)
#define _Out_writes_(x)
#define _Out_writes_opt_(x)
#define _Inout_updates_(x)
#define _Out_writes_bytes_(x)
#define _In_reads_bytes_(x)
#define _In_z_
#define _Out_z_
#define _Ret_maybenull_
#define _Ret_notnull_
#define _Check_return_
#define _Success_(x)
#define _Null_terminated_
#define _Post_ptr_invalid_
#define _At_(x)

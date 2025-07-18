; NOTE: Assertions have been autogenerated by utils/update_test_checks.py
; RUN: opt < %s -passes=vector-combine -S -mtriple=x86_64-- -mattr=sse2 | FileCheck %s
; RUN: opt < %s -passes=vector-combine -S -mtriple=x86_64-- -mattr=avx2 | FileCheck %s

target datalayout = "e-m:e-i64:64-f80:128-n8:16:32:64-S128"

define float @matching_fp_scalar(ptr align 16 dereferenceable(16) %p) {
; CHECK-LABEL: @matching_fp_scalar(
; CHECK-NEXT:    [[R:%.*]] = load float, ptr [[P:%.*]], align 16
; CHECK-NEXT:    ret float [[R]]
;
  %r = load float, ptr %p, align 16
  ret float %r
}

define float @matching_fp_scalar_volatile(ptr align 16 dereferenceable(16) %p) {
; CHECK-LABEL: @matching_fp_scalar_volatile(
; CHECK-NEXT:    [[R:%.*]] = load volatile float, ptr [[P:%.*]], align 16
; CHECK-NEXT:    ret float [[R]]
;
  %r = load volatile float, ptr %p, align 16
  ret float %r
}

define double @larger_fp_scalar(ptr align 16 dereferenceable(16) %p) {
; CHECK-LABEL: @larger_fp_scalar(
; CHECK-NEXT:    [[R:%.*]] = load double, ptr [[P:%.*]], align 16
; CHECK-NEXT:    ret double [[R]]
;
  %r = load double, ptr %p, align 16
  ret double %r
}

define float @smaller_fp_scalar(ptr align 16 dereferenceable(16) %p) {
; CHECK-LABEL: @smaller_fp_scalar(
; CHECK-NEXT:    [[R:%.*]] = load float, ptr [[P:%.*]], align 16
; CHECK-NEXT:    ret float [[R]]
;
  %r = load float, ptr %p, align 16
  ret float %r
}

define float @matching_fp_vector(ptr align 16 dereferenceable(16) %p) {
; CHECK-LABEL: @matching_fp_vector(
; CHECK-NEXT:    [[R:%.*]] = load float, ptr [[P:%.*]], align 16
; CHECK-NEXT:    ret float [[R]]
;
  %r = load float, ptr %p, align 16
  ret float %r
}

define float @matching_fp_vector_gep00(ptr align 16 dereferenceable(16) %p) {
; CHECK-LABEL: @matching_fp_vector_gep00(
; CHECK-NEXT:    [[R:%.*]] = load float, ptr [[P:%.*]], align 16
; CHECK-NEXT:    ret float [[R]]
;
  %r = load float, ptr %p, align 16
  ret float %r
}

define float @matching_fp_vector_gep01(ptr align 16 dereferenceable(20) %p) {
; CHECK-LABEL: @matching_fp_vector_gep01(
; CHECK-NEXT:    [[GEP:%.*]] = getelementptr inbounds <4 x float>, ptr [[P:%.*]], i64 0, i64 1
; CHECK-NEXT:    [[R:%.*]] = load float, ptr [[GEP]], align 4
; CHECK-NEXT:    ret float [[R]]
;
  %gep = getelementptr inbounds <4 x float>, ptr %p, i64 0, i64 1
  %r = load float, ptr %gep, align 4
  ret float %r
}

define float @matching_fp_vector_gep01_deref(ptr align 16 dereferenceable(19) %p) {
; CHECK-LABEL: @matching_fp_vector_gep01_deref(
; CHECK-NEXT:    [[GEP:%.*]] = getelementptr inbounds <4 x float>, ptr [[P:%.*]], i64 0, i64 1
; CHECK-NEXT:    [[R:%.*]] = load float, ptr [[GEP]], align 4
; CHECK-NEXT:    ret float [[R]]
;
  %gep = getelementptr inbounds <4 x float>, ptr %p, i64 0, i64 1
  %r = load float, ptr %gep, align 4
  ret float %r
}

define float @matching_fp_vector_gep10(ptr align 16 dereferenceable(32) %p) {
; CHECK-LABEL: @matching_fp_vector_gep10(
; CHECK-NEXT:    [[GEP:%.*]] = getelementptr inbounds <4 x float>, ptr [[P:%.*]], i64 1, i64 0
; CHECK-NEXT:    [[R:%.*]] = load float, ptr [[GEP]], align 16
; CHECK-NEXT:    ret float [[R]]
;
  %gep = getelementptr inbounds <4 x float>, ptr %p, i64 1, i64 0
  %r = load float, ptr %gep, align 16
  ret float %r
}

define float @matching_fp_vector_gep10_deref(ptr align 16 dereferenceable(31) %p) {
; CHECK-LABEL: @matching_fp_vector_gep10_deref(
; CHECK-NEXT:    [[GEP:%.*]] = getelementptr inbounds <4 x float>, ptr [[P:%.*]], i64 1, i64 0
; CHECK-NEXT:    [[R:%.*]] = load float, ptr [[GEP]], align 16
; CHECK-NEXT:    ret float [[R]]
;
  %gep = getelementptr inbounds <4 x float>, ptr %p, i64 1, i64 0
  %r = load float, ptr %gep, align 16
  ret float %r
}

define float @nonmatching_int_vector(ptr align 16 dereferenceable(16) %p) {
; CHECK-LABEL: @nonmatching_int_vector(
; CHECK-NEXT:    [[R:%.*]] = load float, ptr [[P:%.*]], align 16
; CHECK-NEXT:    ret float [[R]]
;
  %r = load float, ptr %p, align 16
  ret float %r
}

define double @less_aligned(ptr align 4 dereferenceable(16) %p) {
; CHECK-LABEL: @less_aligned(
; CHECK-NEXT:    [[R:%.*]] = load double, ptr [[P:%.*]], align 4
; CHECK-NEXT:    ret double [[R]]
;
  %r = load double, ptr %p, align 4
  ret double %r
}

define float @matching_fp_scalar_small_deref(ptr align 16 dereferenceable(15) %p) {
; CHECK-LABEL: @matching_fp_scalar_small_deref(
; CHECK-NEXT:    [[R:%.*]] = load float, ptr [[P:%.*]], align 16
; CHECK-NEXT:    ret float [[R]]
;
  %r = load float, ptr %p, align 16
  ret float %r
}

define i64 @larger_int_scalar(ptr align 16 dereferenceable(16) %p) {
; CHECK-LABEL: @larger_int_scalar(
; CHECK-NEXT:    [[R:%.*]] = load i64, ptr [[P:%.*]], align 16
; CHECK-NEXT:    ret i64 [[R]]
;
  %r = load i64, ptr %p, align 16
  ret i64 %r
}

define i8 @smaller_int_scalar(ptr align 16 dereferenceable(16) %p) {
; CHECK-LABEL: @smaller_int_scalar(
; CHECK-NEXT:    [[R:%.*]] = load i8, ptr [[P:%.*]], align 16
; CHECK-NEXT:    ret i8 [[R]]
;
  %r = load i8, ptr %p, align 16
  ret i8 %r
}

define double @larger_fp_scalar_256bit_vec(ptr align 32 dereferenceable(32) %p) {
; CHECK-LABEL: @larger_fp_scalar_256bit_vec(
; CHECK-NEXT:    [[R:%.*]] = load double, ptr [[P:%.*]], align 32
; CHECK-NEXT:    ret double [[R]]
;
  %r = load double, ptr %p, align 32
  ret double %r
}

define <4 x float> @load_f32_insert_v4f32(ptr align 16 dereferenceable(16) %p) nofree nosync {
; CHECK-LABEL: @load_f32_insert_v4f32(
; CHECK-NEXT:    [[TMP1:%.*]] = load <4 x float>, ptr [[P:%.*]], align 16
; CHECK-NEXT:    [[R:%.*]] = shufflevector <4 x float> [[TMP1]], <4 x float> poison, <4 x i32> <i32 0, i32 poison, i32 poison, i32 poison>
; CHECK-NEXT:    ret <4 x float> [[R]]
;
  %s = load float, ptr %p, align 4
  %r = insertelement <4 x float> poison, float %s, i32 0
  ret <4 x float> %r
}

define <4 x float> @casted_load_f32_insert_v4f32(ptr align 4 dereferenceable(16) %p) nofree nosync {
; CHECK-LABEL: @casted_load_f32_insert_v4f32(
; CHECK-NEXT:    [[TMP1:%.*]] = load <4 x float>, ptr [[P:%.*]], align 4
; CHECK-NEXT:    [[R:%.*]] = shufflevector <4 x float> [[TMP1]], <4 x float> poison, <4 x i32> <i32 0, i32 poison, i32 poison, i32 poison>
; CHECK-NEXT:    ret <4 x float> [[R]]
;
  %s = load float, ptr %p, align 4
  %r = insertelement <4 x float> poison, float %s, i32 0
  ret <4 x float> %r
}

; Element type does not change cost.

define <4 x i32> @load_i32_insert_v4i32(ptr align 16 dereferenceable(16) %p) nofree nosync {
; CHECK-LABEL: @load_i32_insert_v4i32(
; CHECK-NEXT:    [[TMP1:%.*]] = load <4 x i32>, ptr [[P:%.*]], align 16
; CHECK-NEXT:    [[R:%.*]] = shufflevector <4 x i32> [[TMP1]], <4 x i32> poison, <4 x i32> <i32 0, i32 poison, i32 poison, i32 poison>
; CHECK-NEXT:    ret <4 x i32> [[R]]
;
  %s = load i32, ptr %p, align 4
  %r = insertelement <4 x i32> poison, i32 %s, i32 0
  ret <4 x i32> %r
}

; Pointer type does not change cost.

define <4 x i32> @casted_load_i32_insert_v4i32(ptr align 4 dereferenceable(16) %p) nofree nosync {
; CHECK-LABEL: @casted_load_i32_insert_v4i32(
; CHECK-NEXT:    [[TMP1:%.*]] = load <4 x i32>, ptr [[P:%.*]], align 4
; CHECK-NEXT:    [[R:%.*]] = shufflevector <4 x i32> [[TMP1]], <4 x i32> poison, <4 x i32> <i32 0, i32 poison, i32 poison, i32 poison>
; CHECK-NEXT:    ret <4 x i32> [[R]]
;
  %s = load i32, ptr %p, align 4
  %r = insertelement <4 x i32> poison, i32 %s, i32 0
  ret <4 x i32> %r
}

; This is canonical form for vector element access.

define <4 x float> @gep00_load_f32_insert_v4f32(ptr align 16 dereferenceable(16) %p) nofree nosync {
; CHECK-LABEL: @gep00_load_f32_insert_v4f32(
; CHECK-NEXT:    [[TMP1:%.*]] = load <4 x float>, ptr [[P:%.*]], align 16
; CHECK-NEXT:    [[R:%.*]] = shufflevector <4 x float> [[TMP1]], <4 x float> poison, <4 x i32> <i32 0, i32 poison, i32 poison, i32 poison>
; CHECK-NEXT:    ret <4 x float> [[R]]
;
  %s = load float, ptr %p, align 16
  %r = insertelement <4 x float> poison, float %s, i64 0
  ret <4 x float> %r
}

; Should work with addrspace as well.

define <4 x float> @gep00_load_f32_insert_v4f32_addrspace(ptr addrspace(44) align 16 dereferenceable(16) %p) nofree nosync {
; CHECK-LABEL: @gep00_load_f32_insert_v4f32_addrspace(
; CHECK-NEXT:    [[TMP1:%.*]] = load <4 x float>, ptr addrspace(44) [[P:%.*]], align 16
; CHECK-NEXT:    [[R:%.*]] = shufflevector <4 x float> [[TMP1]], <4 x float> poison, <4 x i32> <i32 0, i32 poison, i32 poison, i32 poison>
; CHECK-NEXT:    ret <4 x float> [[R]]
;
  %s = load float, ptr addrspace(44) %p, align 16
  %r = insertelement <4 x float> poison, float %s, i64 0
  ret <4 x float> %r
}

; Should work with addrspace even when peeking past unsafe loads through geps

define <4 x i32> @unsafe_load_i32_insert_v4i32_addrspace(ptr align 16 dereferenceable(16) %v3) {
; CHECK-LABEL: @unsafe_load_i32_insert_v4i32_addrspace(
; CHECK-NEXT:    [[TMP1:%.*]] = addrspacecast ptr [[V3:%.*]] to ptr addrspace(42)
; CHECK-NEXT:    [[TMP2:%.*]] = load <4 x i32>, ptr addrspace(42) [[TMP1]], align 16
; CHECK-NEXT:    [[INSELT:%.*]] = shufflevector <4 x i32> [[TMP2]], <4 x i32> poison, <4 x i32> <i32 2, i32 poison, i32 poison, i32 poison>
; CHECK-NEXT:    ret <4 x i32> [[INSELT]]
;
  %t0 = getelementptr inbounds i32, ptr %v3, i32 1
  %t1 = addrspacecast ptr %t0 to ptr addrspace(42)
  %t2 = getelementptr inbounds i32, ptr addrspace(42) %t1, i64 1
  %val = load i32, ptr addrspace(42) %t2, align 4
  %inselt = insertelement <4 x i32> poison, i32 %val, i32 0
  ret <4 x i32> %inselt
}

; If there are enough dereferenceable bytes, we can offset the vector load.

define <8 x i16> @gep01_load_i16_insert_v8i16(ptr align 16 dereferenceable(18) %p) nofree nosync {
; CHECK-LABEL: @gep01_load_i16_insert_v8i16(
; CHECK-NEXT:    [[GEP:%.*]] = getelementptr inbounds <8 x i16>, ptr [[P:%.*]], i64 0, i64 1
; CHECK-NEXT:    [[TMP1:%.*]] = load <8 x i16>, ptr [[GEP]], align 2
; CHECK-NEXT:    [[R:%.*]] = shufflevector <8 x i16> [[TMP1]], <8 x i16> poison, <8 x i32> <i32 0, i32 poison, i32 poison, i32 poison, i32 poison, i32 poison, i32 poison, i32 poison>
; CHECK-NEXT:    ret <8 x i16> [[R]]
;
  %gep = getelementptr inbounds <8 x i16>, ptr %p, i64 0, i64 1
  %s = load i16, ptr %gep, align 2
  %r = insertelement <8 x i16> poison, i16 %s, i64 0
  ret <8 x i16> %r
}

; Can't safely load the offset vector, but can load+shuffle if it is profitable.

define <8 x i16> @gep01_load_i16_insert_v8i16_deref(ptr align 16 dereferenceable(17) %p) nofree nosync {
; CHECK-LABEL: @gep01_load_i16_insert_v8i16_deref(
; CHECK-NEXT:    [[TMP1:%.*]] = load <8 x i16>, ptr [[P:%.*]], align 16
; CHECK-NEXT:    [[R:%.*]] = shufflevector <8 x i16> [[TMP1]], <8 x i16> poison, <8 x i32> <i32 1, i32 poison, i32 poison, i32 poison, i32 poison, i32 poison, i32 poison, i32 poison>
; CHECK-NEXT:    ret <8 x i16> [[R]]
;
  %gep = getelementptr inbounds <8 x i16>, ptr %p, i64 0, i64 1
  %s = load i16, ptr %gep, align 2
  %r = insertelement <8 x i16> poison, i16 %s, i64 0
  ret <8 x i16> %r
}

; Verify that alignment of the new load is not over-specified.

define <8 x i16> @gep01_load_i16_insert_v8i16_deref_minalign(ptr align 2 dereferenceable(16) %p) nofree nosync {
; CHECK-LABEL: @gep01_load_i16_insert_v8i16_deref_minalign(
; CHECK-NEXT:    [[TMP1:%.*]] = load <8 x i16>, ptr [[P:%.*]], align 2
; CHECK-NEXT:    [[R:%.*]] = shufflevector <8 x i16> [[TMP1]], <8 x i16> poison, <8 x i32> <i32 1, i32 poison, i32 poison, i32 poison, i32 poison, i32 poison, i32 poison, i32 poison>
; CHECK-NEXT:    ret <8 x i16> [[R]]
;
  %gep = getelementptr inbounds <8 x i16>, ptr %p, i64 0, i64 1
  %s = load i16, ptr %gep, align 8
  %r = insertelement <8 x i16> poison, i16 %s, i64 0
  ret <8 x i16> %r
}

; Negative test - if we are shuffling a load from the base pointer, the address offset
; must be a multiple of element size.
; TODO: Could bitcast around this limitation.

define <4 x i32> @gep01_bitcast_load_i32_insert_v4i32(ptr align 1 dereferenceable(16) %p) nofree nosync {
; CHECK-LABEL: @gep01_bitcast_load_i32_insert_v4i32(
; CHECK-NEXT:    [[GEP:%.*]] = getelementptr inbounds <16 x i8>, ptr [[P:%.*]], i64 0, i64 1
; CHECK-NEXT:    [[S:%.*]] = load i32, ptr [[GEP]], align 1
; CHECK-NEXT:    [[R:%.*]] = insertelement <4 x i32> poison, i32 [[S]], i64 0
; CHECK-NEXT:    ret <4 x i32> [[R]]
;
  %gep = getelementptr inbounds <16 x i8>, ptr %p, i64 0, i64 1
  %s = load i32, ptr %gep, align 1
  %r = insertelement <4 x i32> poison, i32 %s, i64 0
  ret <4 x i32> %r
}

define <4 x i32> @gep012_bitcast_load_i32_insert_v4i32(ptr align 1 dereferenceable(20) %p) nofree nosync {
; CHECK-LABEL: @gep012_bitcast_load_i32_insert_v4i32(
; CHECK-NEXT:    [[TMP1:%.*]] = load <4 x i32>, ptr [[P:%.*]], align 1
; CHECK-NEXT:    [[R:%.*]] = shufflevector <4 x i32> [[TMP1]], <4 x i32> poison, <4 x i32> <i32 3, i32 poison, i32 poison, i32 poison>
; CHECK-NEXT:    ret <4 x i32> [[R]]
;
  %gep = getelementptr inbounds <16 x i8>, ptr %p, i64 0, i64 12
  %s = load i32, ptr %gep, align 1
  %r = insertelement <4 x i32> poison, i32 %s, i64 0
  ret <4 x i32> %r
}

; Negative test - if we are shuffling a load from the base pointer, the address offset
; must be a multiple of element size and the offset must be low enough to fit in the vector
; (bitcasting would not help this case).

define <4 x i32> @gep013_bitcast_load_i32_insert_v4i32(ptr align 1 dereferenceable(20) %p) nofree nosync {
; CHECK-LABEL: @gep013_bitcast_load_i32_insert_v4i32(
; CHECK-NEXT:    [[GEP:%.*]] = getelementptr inbounds <16 x i8>, ptr [[P:%.*]], i64 0, i64 13
; CHECK-NEXT:    [[S:%.*]] = load i32, ptr [[GEP]], align 1
; CHECK-NEXT:    [[R:%.*]] = insertelement <4 x i32> poison, i32 [[S]], i64 0
; CHECK-NEXT:    ret <4 x i32> [[R]]
;
  %gep = getelementptr inbounds <16 x i8>, ptr %p, i64 0, i64 13
  %s = load i32, ptr %gep, align 1
  %r = insertelement <4 x i32> poison, i32 %s, i64 0
  ret <4 x i32> %r
}

; If there are enough dereferenceable bytes, we can offset the vector load.

define <8 x i16> @gep10_load_i16_insert_v8i16(ptr align 16 dereferenceable(32) %p) nofree nosync {
; CHECK-LABEL: @gep10_load_i16_insert_v8i16(
; CHECK-NEXT:    [[GEP:%.*]] = getelementptr inbounds <8 x i16>, ptr [[P:%.*]], i64 1, i64 0
; CHECK-NEXT:    [[TMP1:%.*]] = load <8 x i16>, ptr [[GEP]], align 16
; CHECK-NEXT:    [[R:%.*]] = shufflevector <8 x i16> [[TMP1]], <8 x i16> poison, <8 x i32> <i32 0, i32 poison, i32 poison, i32 poison, i32 poison, i32 poison, i32 poison, i32 poison>
; CHECK-NEXT:    ret <8 x i16> [[R]]
;
  %gep = getelementptr inbounds <8 x i16>, ptr %p, i64 1, i64 0
  %s = load i16, ptr %gep, align 16
  %r = insertelement <8 x i16> poison, i16 %s, i64 0
  ret <8 x i16> %r
}

; Negative test - disable under asan because widened load can cause spurious
; use-after-poison issues when __asan_poison_memory_region is used.

define <8 x i16> @gep10_load_i16_insert_v8i16_asan(ptr align 16 dereferenceable(32) %p) sanitize_address {
; CHECK-LABEL: @gep10_load_i16_insert_v8i16_asan(
; CHECK-NEXT:    [[GEP:%.*]] = getelementptr inbounds <8 x i16>, ptr [[P:%.*]], i64 1, i64 0
; CHECK-NEXT:    [[S:%.*]] = load i16, ptr [[GEP]], align 16
; CHECK-NEXT:    [[R:%.*]] = insertelement <8 x i16> poison, i16 [[S]], i64 0
; CHECK-NEXT:    ret <8 x i16> [[R]]
;
  %gep = getelementptr inbounds <8 x i16>, ptr %p, i64 1, i64 0
  %s = load i16, ptr %gep, align 16
  %r = insertelement <8 x i16> poison, i16 %s, i64 0
  ret <8 x i16> %r
}

; hwasan and memtag should be similarly suppressed.

define <8 x i16> @gep10_load_i16_insert_v8i16_hwasan(ptr align 16 dereferenceable(32) %p) sanitize_hwaddress {
; CHECK-LABEL: @gep10_load_i16_insert_v8i16_hwasan(
; CHECK-NEXT:    [[GEP:%.*]] = getelementptr inbounds <8 x i16>, ptr [[P:%.*]], i64 1, i64 0
; CHECK-NEXT:    [[S:%.*]] = load i16, ptr [[GEP]], align 16
; CHECK-NEXT:    [[R:%.*]] = insertelement <8 x i16> poison, i16 [[S]], i64 0
; CHECK-NEXT:    ret <8 x i16> [[R]]
;
  %gep = getelementptr inbounds <8 x i16>, ptr %p, i64 1, i64 0
  %s = load i16, ptr %gep, align 16
  %r = insertelement <8 x i16> poison, i16 %s, i64 0
  ret <8 x i16> %r
}

define <8 x i16> @gep10_load_i16_insert_v8i16_memtag(ptr align 16 dereferenceable(32) %p) sanitize_memtag {
; CHECK-LABEL: @gep10_load_i16_insert_v8i16_memtag(
; CHECK-NEXT:    [[GEP:%.*]] = getelementptr inbounds <8 x i16>, ptr [[P:%.*]], i64 1, i64 0
; CHECK-NEXT:    [[S:%.*]] = load i16, ptr [[GEP]], align 16
; CHECK-NEXT:    [[R:%.*]] = insertelement <8 x i16> poison, i16 [[S]], i64 0
; CHECK-NEXT:    ret <8 x i16> [[R]]
;
  %gep = getelementptr inbounds <8 x i16>, ptr %p, i64 1, i64 0
  %s = load i16, ptr %gep, align 16
  %r = insertelement <8 x i16> poison, i16 %s, i64 0
  ret <8 x i16> %r
}

; Negative test - disable under tsan because widened load may overlap bytes
; being concurrently modified. tsan does not know that some bytes are undef.

define <8 x i16> @gep10_load_i16_insert_v8i16_tsan(ptr align 16 dereferenceable(32) %p) sanitize_thread {
; CHECK-LABEL: @gep10_load_i16_insert_v8i16_tsan(
; CHECK-NEXT:    [[GEP:%.*]] = getelementptr inbounds <8 x i16>, ptr [[P:%.*]], i64 1, i64 0
; CHECK-NEXT:    [[S:%.*]] = load i16, ptr [[GEP]], align 16
; CHECK-NEXT:    [[R:%.*]] = insertelement <8 x i16> poison, i16 [[S]], i64 0
; CHECK-NEXT:    ret <8 x i16> [[R]]
;
  %gep = getelementptr inbounds <8 x i16>, ptr %p, i64 1, i64 0
  %s = load i16, ptr %gep, align 16
  %r = insertelement <8 x i16> poison, i16 %s, i64 0
  ret <8 x i16> %r
}

; Negative test - can't safely load the offset vector, but could load+shuffle.

define <8 x i16> @gep10_load_i16_insert_v8i16_deref(ptr align 16 dereferenceable(31) %p) nofree nosync {
; CHECK-LABEL: @gep10_load_i16_insert_v8i16_deref(
; CHECK-NEXT:    [[GEP:%.*]] = getelementptr inbounds <8 x i16>, ptr [[P:%.*]], i64 1, i64 0
; CHECK-NEXT:    [[S:%.*]] = load i16, ptr [[GEP]], align 16
; CHECK-NEXT:    [[R:%.*]] = insertelement <8 x i16> poison, i16 [[S]], i64 0
; CHECK-NEXT:    ret <8 x i16> [[R]]
;
  %gep = getelementptr inbounds <8 x i16>, ptr %p, i64 1, i64 0
  %s = load i16, ptr %gep, align 16
  %r = insertelement <8 x i16> poison, i16 %s, i64 0
  ret <8 x i16> %r
}

; Negative test - do not alter volatile.

define <4 x float> @load_f32_insert_v4f32_volatile(ptr align 16 dereferenceable(16) %p) nofree nosync {
; CHECK-LABEL: @load_f32_insert_v4f32_volatile(
; CHECK-NEXT:    [[S:%.*]] = load volatile float, ptr [[P:%.*]], align 4
; CHECK-NEXT:    [[R:%.*]] = insertelement <4 x float> poison, float [[S]], i32 0
; CHECK-NEXT:    ret <4 x float> [[R]]
;
  %s = load volatile float, ptr %p, align 4
  %r = insertelement <4 x float> poison, float %s, i32 0
  ret <4 x float> %r
}

; Pointer is not as aligned as load, but that's ok.
; The new load uses the larger alignment value.

define <4 x float> @load_f32_insert_v4f32_align(ptr align 1 dereferenceable(16) %p) nofree nosync {
; CHECK-LABEL: @load_f32_insert_v4f32_align(
; CHECK-NEXT:    [[TMP1:%.*]] = load <4 x float>, ptr [[P:%.*]], align 4
; CHECK-NEXT:    [[R:%.*]] = shufflevector <4 x float> [[TMP1]], <4 x float> poison, <4 x i32> <i32 0, i32 poison, i32 poison, i32 poison>
; CHECK-NEXT:    ret <4 x float> [[R]]
;
  %s = load float, ptr %p, align 4
  %r = insertelement <4 x float> poison, float %s, i32 0
  ret <4 x float> %r
}

; Negative test - not enough bytes.

define <4 x float> @load_f32_insert_v4f32_deref(ptr align 4 dereferenceable(15) %p) nofree nosync {
; CHECK-LABEL: @load_f32_insert_v4f32_deref(
; CHECK-NEXT:    [[S:%.*]] = load float, ptr [[P:%.*]], align 4
; CHECK-NEXT:    [[R:%.*]] = insertelement <4 x float> poison, float [[S]], i32 0
; CHECK-NEXT:    ret <4 x float> [[R]]
;
  %s = load float, ptr %p, align 4
  %r = insertelement <4 x float> poison, float %s, i32 0
  ret <4 x float> %r
}

define <8 x i32> @load_i32_insert_v8i32(ptr align 16 dereferenceable(16) %p) nofree nosync {
; CHECK-LABEL: @load_i32_insert_v8i32(
; CHECK-NEXT:    [[TMP1:%.*]] = load <4 x i32>, ptr [[P:%.*]], align 16
; CHECK-NEXT:    [[R:%.*]] = shufflevector <4 x i32> [[TMP1]], <4 x i32> poison, <8 x i32> <i32 0, i32 poison, i32 poison, i32 poison, i32 poison, i32 poison, i32 poison, i32 poison>
; CHECK-NEXT:    ret <8 x i32> [[R]]
;
  %s = load i32, ptr %p, align 4
  %r = insertelement <8 x i32> poison, i32 %s, i32 0
  ret <8 x i32> %r
}

define <8 x i32> @casted_load_i32_insert_v8i32(ptr align 4 dereferenceable(16) %p) nofree nosync {
; CHECK-LABEL: @casted_load_i32_insert_v8i32(
; CHECK-NEXT:    [[TMP1:%.*]] = load <4 x i32>, ptr [[P:%.*]], align 4
; CHECK-NEXT:    [[R:%.*]] = shufflevector <4 x i32> [[TMP1]], <4 x i32> poison, <8 x i32> <i32 0, i32 poison, i32 poison, i32 poison, i32 poison, i32 poison, i32 poison, i32 poison>
; CHECK-NEXT:    ret <8 x i32> [[R]]
;
  %s = load i32, ptr %p, align 4
  %r = insertelement <8 x i32> poison, i32 %s, i32 0
  ret <8 x i32> %r
}

define <16 x float> @load_f32_insert_v16f32(ptr align 16 dereferenceable(16) %p) nofree nosync {
; CHECK-LABEL: @load_f32_insert_v16f32(
; CHECK-NEXT:    [[TMP1:%.*]] = load <4 x float>, ptr [[P:%.*]], align 16
; CHECK-NEXT:    [[R:%.*]] = shufflevector <4 x float> [[TMP1]], <4 x float> poison, <16 x i32> <i32 0, i32 poison, i32 poison, i32 poison, i32 poison, i32 poison, i32 poison, i32 poison, i32 poison, i32 poison, i32 poison, i32 poison, i32 poison, i32 poison, i32 poison, i32 poison>
; CHECK-NEXT:    ret <16 x float> [[R]]
;
  %s = load float, ptr %p, align 4
  %r = insertelement <16 x float> poison, float %s, i32 0
  ret <16 x float> %r
}

define <2 x float> @load_f32_insert_v2f32(ptr align 16 dereferenceable(16) %p) nofree nosync {
; CHECK-LABEL: @load_f32_insert_v2f32(
; CHECK-NEXT:    [[TMP1:%.*]] = load <4 x float>, ptr [[P:%.*]], align 16
; CHECK-NEXT:    [[R:%.*]] = shufflevector <4 x float> [[TMP1]], <4 x float> poison, <2 x i32> <i32 0, i32 poison>
; CHECK-NEXT:    ret <2 x float> [[R]]
;
  %s = load float, ptr %p, align 4
  %r = insertelement <2 x float> poison, float %s, i32 0
  ret <2 x float> %r
}

; Negative test - suppress load widening for asan/hwasan/memtag/tsan.

define <2 x float> @load_f32_insert_v2f32_asan(ptr align 16 dereferenceable(16) %p) sanitize_address {
; CHECK-LABEL: @load_f32_insert_v2f32_asan(
; CHECK-NEXT:    [[S:%.*]] = load float, ptr [[P:%.*]], align 4
; CHECK-NEXT:    [[R:%.*]] = insertelement <2 x float> poison, float [[S]], i32 0
; CHECK-NEXT:    ret <2 x float> [[R]]
;
  %s = load float, ptr %p, align 4
  %r = insertelement <2 x float> poison, float %s, i32 0
  ret <2 x float> %r
}

declare ptr @getscaleptr()
define void @PR47558_multiple_use_load(ptr nocapture nonnull %resultptr, ptr nocapture nonnull readonly %opptr) nofree nosync {
; CHECK-LABEL: @PR47558_multiple_use_load(
; CHECK-NEXT:    [[SCALEPTR:%.*]] = tail call nonnull align 16 dereferenceable(64) ptr @getscaleptr()
; CHECK-NEXT:    [[OP:%.*]] = load <2 x float>, ptr [[OPPTR:%.*]], align 4
; CHECK-NEXT:    [[SCALE:%.*]] = load float, ptr [[SCALEPTR]], align 16
; CHECK-NEXT:    [[T1:%.*]] = insertelement <2 x float> poison, float [[SCALE]], i32 0
; CHECK-NEXT:    [[T2:%.*]] = insertelement <2 x float> [[T1]], float [[SCALE]], i32 1
; CHECK-NEXT:    [[T3:%.*]] = fmul <2 x float> [[OP]], [[T2]]
; CHECK-NEXT:    store <2 x float> [[T3]], ptr [[RESULTPTR:%.*]], align 8
; CHECK-NEXT:    ret void
;
  %scaleptr = tail call nonnull align 16 dereferenceable(64) ptr @getscaleptr()
  %op = load <2 x float>, ptr %opptr, align 4
  %scale = load float, ptr %scaleptr, align 16
  %t1 = insertelement <2 x float> poison, float %scale, i32 0
  %t2 = insertelement <2 x float> %t1, float %scale, i32 1
  %t3 = fmul <2 x float> %op, %t2
  %t4 = extractelement <2 x float> %t3, i32 0
  %result0 = insertelement <2 x float> poison, float %t4, i32 0
  %t5 = extractelement <2 x float> %t3, i32 1
  %result1 = insertelement <2 x float> %result0, float %t5, i32 1
  store <2 x float> %result1, ptr %resultptr, align 8
  ret void
}

define <4 x float> @load_v2f32_extract_insert_v4f32(ptr align 16 dereferenceable(16) %p) nofree nosync {
; CHECK-LABEL: @load_v2f32_extract_insert_v4f32(
; CHECK-NEXT:    [[TMP1:%.*]] = load <4 x float>, ptr [[P:%.*]], align 16
; CHECK-NEXT:    [[R:%.*]] = shufflevector <4 x float> [[TMP1]], <4 x float> poison, <4 x i32> <i32 0, i32 poison, i32 poison, i32 poison>
; CHECK-NEXT:    ret <4 x float> [[R]]
;
  %l = load <2 x float>, ptr %p, align 4
  %s = extractelement <2 x float> %l, i32 0
  %r = insertelement <4 x float> poison, float %s, i32 0
  ret <4 x float> %r
}

define <4 x float> @load_v8f32_extract_insert_v4f32(ptr align 16 dereferenceable(16) %p) nofree nosync {
; CHECK-LABEL: @load_v8f32_extract_insert_v4f32(
; CHECK-NEXT:    [[TMP1:%.*]] = load <4 x float>, ptr [[P:%.*]], align 16
; CHECK-NEXT:    [[R:%.*]] = shufflevector <4 x float> [[TMP1]], <4 x float> poison, <4 x i32> <i32 0, i32 poison, i32 poison, i32 poison>
; CHECK-NEXT:    ret <4 x float> [[R]]
;
  %l = load <8 x float>, ptr %p, align 4
  %s = extractelement <8 x float> %l, i32 0
  %r = insertelement <4 x float> poison, float %s, i32 0
  ret <4 x float> %r
}

define <8 x i32> @load_v1i32_extract_insert_v8i32_extra_use(ptr align 16 dereferenceable(16) %p, ptr %store_ptr) nofree nosync {
; CHECK-LABEL: @load_v1i32_extract_insert_v8i32_extra_use(
; CHECK-NEXT:    [[L:%.*]] = load <1 x i32>, ptr [[P:%.*]], align 4
; CHECK-NEXT:    store <1 x i32> [[L]], ptr [[STORE_PTR:%.*]], align 4
; CHECK-NEXT:    [[TMP1:%.*]] = shufflevector <1 x i32> [[L]], <1 x i32> poison, <8 x i32> <i32 0, i32 poison, i32 poison, i32 poison, i32 poison, i32 poison, i32 poison, i32 poison>
; CHECK-NEXT:    ret <8 x i32> [[TMP1]]
;
  %l = load <1 x i32>, ptr %p, align 4
  store <1 x i32> %l, ptr %store_ptr
  %s = extractelement <1 x i32> %l, i32 0
  %r = insertelement <8 x i32> poison, i32 %s, i32 0
  ret <8 x i32> %r
}

; Can't safely load the offset vector, but can load+shuffle if it is profitable.

define <8 x i16> @gep1_load_v2i16_extract_insert_v8i16(ptr align 1 dereferenceable(16) %p) nofree nosync {
; CHECK-LABEL: @gep1_load_v2i16_extract_insert_v8i16(
; CHECK-NEXT:    [[TMP1:%.*]] = load <8 x i16>, ptr [[P:%.*]], align 4
; CHECK-NEXT:    [[R:%.*]] = shufflevector <8 x i16> [[TMP1]], <8 x i16> poison, <8 x i32> <i32 2, i32 poison, i32 poison, i32 poison, i32 poison, i32 poison, i32 poison, i32 poison>
; CHECK-NEXT:    ret <8 x i16> [[R]]
;
  %gep = getelementptr inbounds <2 x i16>, ptr %p, i64 1
  %l = load <2 x i16>, ptr %gep, align 8
  %s = extractelement <2 x i16> %l, i32 0
  %r = insertelement <8 x i16> poison, i16 %s, i64 0
  ret <8 x i16> %r
}

; Negative sanitizer tests.

define <4 x i32> @load_i32_insert_v4i32_asan(ptr align 16 dereferenceable(16) %p) nofree nosync sanitize_address  {
; CHECK-LABEL: @load_i32_insert_v4i32_asan(
; CHECK-NEXT:    [[S:%.*]] = load i32, ptr [[P:%.*]], align 4
; CHECK-NEXT:    [[R:%.*]] = insertelement <4 x i32> poison, i32 [[S]], i32 0
; CHECK-NEXT:    ret <4 x i32> [[R]]
;
  %s = load i32, ptr %p, align 4
  %r = insertelement <4 x i32> poison, i32 %s, i32 0
  ret <4 x i32> %r
}

define <4 x float> @load_v2f32_extract_insert_v4f32_hwasan(ptr align 16 dereferenceable(16) %p) nofree nosync sanitize_hwaddress  {
; CHECK-LABEL: @load_v2f32_extract_insert_v4f32_hwasan(
; CHECK-NEXT:    [[L:%.*]] = load <2 x float>, ptr [[P:%.*]], align 4
; CHECK-NEXT:    [[S:%.*]] = extractelement <2 x float> [[L]], i32 0
; CHECK-NEXT:    [[R:%.*]] = insertelement <4 x float> poison, float [[S]], i32 0
; CHECK-NEXT:    ret <4 x float> [[R]]
;
  %l = load <2 x float>, ptr %p, align 4
  %s = extractelement <2 x float> %l, i32 0
  %r = insertelement <4 x float> poison, float %s, i32 0
  ret <4 x float> %r
}

define <4 x float> @load_v2f32_extract_insert_v4f32_tsan(ptr align 16 dereferenceable(16) %p) nofree nosync sanitize_thread  {
; CHECK-LABEL: @load_v2f32_extract_insert_v4f32_tsan(
; CHECK-NEXT:    [[L:%.*]] = load <2 x float>, ptr [[P:%.*]], align 4
; CHECK-NEXT:    [[S:%.*]] = extractelement <2 x float> [[L]], i32 0
; CHECK-NEXT:    [[R:%.*]] = insertelement <4 x float> poison, float [[S]], i32 0
; CHECK-NEXT:    ret <4 x float> [[R]]
;
  %l = load <2 x float>, ptr %p, align 4
  %s = extractelement <2 x float> %l, i32 0
  %r = insertelement <4 x float> poison, float %s, i32 0
  ret <4 x float> %r
}

; Double negative msan tests, it's OK with the optimization.

define <2 x float> @load_f32_insert_v2f32_msan(ptr align 16 dereferenceable(16) %p) nofree nosync sanitize_memory  {
; CHECK-LABEL: @load_f32_insert_v2f32_msan(
; CHECK-NEXT:    [[TMP1:%.*]] = load <4 x float>, ptr [[P:%.*]], align 16
; CHECK-NEXT:    [[R:%.*]] = shufflevector <4 x float> [[TMP1]], <4 x float> poison, <2 x i32> <i32 0, i32 poison>
; CHECK-NEXT:    ret <2 x float> [[R]]
;
  %s = load float, ptr %p, align 4
  %r = insertelement <2 x float> poison, float %s, i32 0
  ret <2 x float> %r
}

; PR30986 - split vector loads for scalarized operations
define <2 x i64> @PR30986(ptr %0) {
; CHECK-LABEL: @PR30986(
; CHECK-NEXT:    [[TMP3:%.*]] = load i64, ptr [[TMP2:%.*]], align 16
; CHECK-NEXT:    [[TMP4:%.*]] = tail call i64 @llvm.ctpop.i64(i64 [[TMP3]])
; CHECK-NEXT:    [[TMP5:%.*]] = insertelement <2 x i64> poison, i64 [[TMP4]], i32 0
; CHECK-NEXT:    [[TMP6:%.*]] = getelementptr inbounds <2 x i64>, ptr [[TMP2]], i32 0, i32 1
; CHECK-NEXT:    [[TMP7:%.*]] = load i64, ptr [[TMP6]], align 8
; CHECK-NEXT:    [[TMP8:%.*]] = tail call i64 @llvm.ctpop.i64(i64 [[TMP7]])
; CHECK-NEXT:    [[TMP9:%.*]] = insertelement <2 x i64> [[TMP5]], i64 [[TMP8]], i32 1
; CHECK-NEXT:    ret <2 x i64> [[TMP9]]
;
  %2 = load <2 x i64>, ptr %0, align 16
  %3 = extractelement <2 x i64> %2, i32 0
  %4 = tail call i64 @llvm.ctpop.i64(i64 %3)
  %5 = insertelement <2 x i64> poison, i64 %4, i32 0
  %6 = extractelement <2 x i64> %2, i32 1
  %7 = tail call i64 @llvm.ctpop.i64(i64 %6)
  %8 = insertelement <2 x i64> %5, i64 %7, i32 1
  ret <2 x i64> %8
}
declare i64 @llvm.ctpop.i64(i64)

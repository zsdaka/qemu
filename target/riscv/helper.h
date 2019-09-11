/* Exceptions */
DEF_HELPER_2(raise_exception, noreturn, env, i32)

/* Floating Point - rounding mode */
DEF_HELPER_FLAGS_2(set_rounding_mode, TCG_CALL_NO_WG, void, env, i32)

/* Floating Point - fused */
DEF_HELPER_FLAGS_4(fmadd_s, TCG_CALL_NO_RWG, i64, env, i64, i64, i64)
DEF_HELPER_FLAGS_4(fmadd_d, TCG_CALL_NO_RWG, i64, env, i64, i64, i64)
DEF_HELPER_FLAGS_4(fmsub_s, TCG_CALL_NO_RWG, i64, env, i64, i64, i64)
DEF_HELPER_FLAGS_4(fmsub_d, TCG_CALL_NO_RWG, i64, env, i64, i64, i64)
DEF_HELPER_FLAGS_4(fnmsub_s, TCG_CALL_NO_RWG, i64, env, i64, i64, i64)
DEF_HELPER_FLAGS_4(fnmsub_d, TCG_CALL_NO_RWG, i64, env, i64, i64, i64)
DEF_HELPER_FLAGS_4(fnmadd_s, TCG_CALL_NO_RWG, i64, env, i64, i64, i64)
DEF_HELPER_FLAGS_4(fnmadd_d, TCG_CALL_NO_RWG, i64, env, i64, i64, i64)

/* Floating Point - Single Precision */
DEF_HELPER_FLAGS_3(fadd_s, TCG_CALL_NO_RWG, i64, env, i64, i64)
DEF_HELPER_FLAGS_3(fsub_s, TCG_CALL_NO_RWG, i64, env, i64, i64)
DEF_HELPER_FLAGS_3(fmul_s, TCG_CALL_NO_RWG, i64, env, i64, i64)
DEF_HELPER_FLAGS_3(fdiv_s, TCG_CALL_NO_RWG, i64, env, i64, i64)
DEF_HELPER_FLAGS_3(fmin_s, TCG_CALL_NO_RWG, i64, env, i64, i64)
DEF_HELPER_FLAGS_3(fmax_s, TCG_CALL_NO_RWG, i64, env, i64, i64)
DEF_HELPER_FLAGS_2(fsqrt_s, TCG_CALL_NO_RWG, i64, env, i64)
DEF_HELPER_FLAGS_3(fle_s, TCG_CALL_NO_RWG, tl, env, i64, i64)
DEF_HELPER_FLAGS_3(flt_s, TCG_CALL_NO_RWG, tl, env, i64, i64)
DEF_HELPER_FLAGS_3(feq_s, TCG_CALL_NO_RWG, tl, env, i64, i64)
DEF_HELPER_FLAGS_2(fcvt_w_s, TCG_CALL_NO_RWG, tl, env, i64)
DEF_HELPER_FLAGS_2(fcvt_wu_s, TCG_CALL_NO_RWG, tl, env, i64)
#if defined(TARGET_RISCV64)
DEF_HELPER_FLAGS_2(fcvt_l_s, TCG_CALL_NO_RWG, tl, env, i64)
DEF_HELPER_FLAGS_2(fcvt_lu_s, TCG_CALL_NO_RWG, tl, env, i64)
#endif
DEF_HELPER_FLAGS_2(fcvt_s_w, TCG_CALL_NO_RWG, i64, env, tl)
DEF_HELPER_FLAGS_2(fcvt_s_wu, TCG_CALL_NO_RWG, i64, env, tl)
#if defined(TARGET_RISCV64)
DEF_HELPER_FLAGS_2(fcvt_s_l, TCG_CALL_NO_RWG, i64, env, tl)
DEF_HELPER_FLAGS_2(fcvt_s_lu, TCG_CALL_NO_RWG, i64, env, tl)
#endif
DEF_HELPER_FLAGS_1(fclass_s, TCG_CALL_NO_RWG_SE, tl, i64)

/* Floating Point - Double Precision */
DEF_HELPER_FLAGS_3(fadd_d, TCG_CALL_NO_RWG, i64, env, i64, i64)
DEF_HELPER_FLAGS_3(fsub_d, TCG_CALL_NO_RWG, i64, env, i64, i64)
DEF_HELPER_FLAGS_3(fmul_d, TCG_CALL_NO_RWG, i64, env, i64, i64)
DEF_HELPER_FLAGS_3(fdiv_d, TCG_CALL_NO_RWG, i64, env, i64, i64)
DEF_HELPER_FLAGS_3(fmin_d, TCG_CALL_NO_RWG, i64, env, i64, i64)
DEF_HELPER_FLAGS_3(fmax_d, TCG_CALL_NO_RWG, i64, env, i64, i64)
DEF_HELPER_FLAGS_2(fcvt_s_d, TCG_CALL_NO_RWG, i64, env, i64)
DEF_HELPER_FLAGS_2(fcvt_d_s, TCG_CALL_NO_RWG, i64, env, i64)
DEF_HELPER_FLAGS_2(fsqrt_d, TCG_CALL_NO_RWG, i64, env, i64)
DEF_HELPER_FLAGS_3(fle_d, TCG_CALL_NO_RWG, tl, env, i64, i64)
DEF_HELPER_FLAGS_3(flt_d, TCG_CALL_NO_RWG, tl, env, i64, i64)
DEF_HELPER_FLAGS_3(feq_d, TCG_CALL_NO_RWG, tl, env, i64, i64)
DEF_HELPER_FLAGS_2(fcvt_w_d, TCG_CALL_NO_RWG, tl, env, i64)
DEF_HELPER_FLAGS_2(fcvt_wu_d, TCG_CALL_NO_RWG, tl, env, i64)
#if defined(TARGET_RISCV64)
DEF_HELPER_FLAGS_2(fcvt_l_d, TCG_CALL_NO_RWG, tl, env, i64)
DEF_HELPER_FLAGS_2(fcvt_lu_d, TCG_CALL_NO_RWG, tl, env, i64)
#endif
DEF_HELPER_FLAGS_2(fcvt_d_w, TCG_CALL_NO_RWG, i64, env, tl)
DEF_HELPER_FLAGS_2(fcvt_d_wu, TCG_CALL_NO_RWG, i64, env, tl)
#if defined(TARGET_RISCV64)
DEF_HELPER_FLAGS_2(fcvt_d_l, TCG_CALL_NO_RWG, i64, env, tl)
DEF_HELPER_FLAGS_2(fcvt_d_lu, TCG_CALL_NO_RWG, i64, env, tl)
#endif
DEF_HELPER_FLAGS_1(fclass_d, TCG_CALL_NO_RWG_SE, tl, i64)

/* Special functions */
DEF_HELPER_3(csrrw, tl, env, tl, tl)
DEF_HELPER_4(csrrs, tl, env, tl, tl, tl)
DEF_HELPER_4(csrrc, tl, env, tl, tl, tl)
#ifndef CONFIG_USER_ONLY
DEF_HELPER_2(sret, tl, env, tl)
DEF_HELPER_2(mret, tl, env, tl)
DEF_HELPER_1(wfi, void, env)
DEF_HELPER_1(tlb_flush, void, env)
#endif
/* Vector functions */
DEF_HELPER_5(vector_vlb_v, void, env, i32, i32, i32, i32)
DEF_HELPER_5(vector_vlh_v, void, env, i32, i32, i32, i32)
DEF_HELPER_5(vector_vlw_v, void, env, i32, i32, i32, i32)
DEF_HELPER_5(vector_vle_v, void, env, i32, i32, i32, i32)
DEF_HELPER_5(vector_vlbu_v, void, env, i32, i32, i32, i32)
DEF_HELPER_5(vector_vlhu_v, void, env, i32, i32, i32, i32)
DEF_HELPER_5(vector_vlwu_v, void, env, i32, i32, i32, i32)
DEF_HELPER_5(vector_vlbff_v, void, env, i32, i32, i32, i32)
DEF_HELPER_5(vector_vlhff_v, void, env, i32, i32, i32, i32)
DEF_HELPER_5(vector_vlwff_v, void, env, i32, i32, i32, i32)
DEF_HELPER_5(vector_vleff_v, void, env, i32, i32, i32, i32)
DEF_HELPER_5(vector_vlbuff_v, void, env, i32, i32, i32, i32)
DEF_HELPER_5(vector_vlhuff_v, void, env, i32, i32, i32, i32)
DEF_HELPER_5(vector_vlwuff_v, void, env, i32, i32, i32, i32)
DEF_HELPER_5(vector_vsb_v, void, env, i32, i32, i32, i32)
DEF_HELPER_5(vector_vsh_v, void, env, i32, i32, i32, i32)
DEF_HELPER_5(vector_vsw_v, void, env, i32, i32, i32, i32)
DEF_HELPER_5(vector_vse_v, void, env, i32, i32, i32, i32)
DEF_HELPER_6(vector_vlsb_v, void, env, i32, i32, i32, i32, i32)
DEF_HELPER_6(vector_vlsh_v, void, env, i32, i32, i32, i32, i32)
DEF_HELPER_6(vector_vlsw_v, void, env, i32, i32, i32, i32, i32)
DEF_HELPER_6(vector_vlse_v, void, env, i32, i32, i32, i32, i32)
DEF_HELPER_6(vector_vlsbu_v, void, env, i32, i32, i32, i32, i32)
DEF_HELPER_6(vector_vlshu_v, void, env, i32, i32, i32, i32, i32)
DEF_HELPER_6(vector_vlswu_v, void, env, i32, i32, i32, i32, i32)
DEF_HELPER_6(vector_vssb_v, void, env, i32, i32, i32, i32, i32)
DEF_HELPER_6(vector_vssh_v, void, env, i32, i32, i32, i32, i32)
DEF_HELPER_6(vector_vssw_v, void, env, i32, i32, i32, i32, i32)
DEF_HELPER_6(vector_vsse_v, void, env, i32, i32, i32, i32, i32)
DEF_HELPER_6(vector_vlxb_v, void, env, i32, i32, i32, i32, i32)
DEF_HELPER_6(vector_vlxh_v, void, env, i32, i32, i32, i32, i32)
DEF_HELPER_6(vector_vlxw_v, void, env, i32, i32, i32, i32, i32)
DEF_HELPER_6(vector_vlxe_v, void, env, i32, i32, i32, i32, i32)
DEF_HELPER_6(vector_vlxbu_v, void, env, i32, i32, i32, i32, i32)
DEF_HELPER_6(vector_vlxhu_v, void, env, i32, i32, i32, i32, i32)
DEF_HELPER_6(vector_vlxwu_v, void, env, i32, i32, i32, i32, i32)
DEF_HELPER_6(vector_vsxb_v, void, env, i32, i32, i32, i32, i32)
DEF_HELPER_6(vector_vsxh_v, void, env, i32, i32, i32, i32, i32)
DEF_HELPER_6(vector_vsxw_v, void, env, i32, i32, i32, i32, i32)
DEF_HELPER_6(vector_vsxe_v, void, env, i32, i32, i32, i32, i32)
DEF_HELPER_6(vector_vsuxb_v, void, env, i32, i32, i32, i32, i32)
DEF_HELPER_6(vector_vsuxh_v, void, env, i32, i32, i32, i32, i32)
DEF_HELPER_6(vector_vsuxw_v, void, env, i32, i32, i32, i32, i32)
DEF_HELPER_6(vector_vsuxe_v, void, env, i32, i32, i32, i32, i32)

DEF_HELPER_6(vector_vamoswapw_v, void, env, i32, i32, i32, i32, i32)
DEF_HELPER_6(vector_vamoswapd_v, void, env, i32, i32, i32, i32, i32)
DEF_HELPER_6(vector_vamoaddw_v, void, env, i32, i32, i32, i32, i32)
DEF_HELPER_6(vector_vamoaddd_v, void, env, i32, i32, i32, i32, i32)
DEF_HELPER_6(vector_vamoxorw_v, void, env, i32, i32, i32, i32, i32)
DEF_HELPER_6(vector_vamoxord_v, void, env, i32, i32, i32, i32, i32)
DEF_HELPER_6(vector_vamoandw_v, void, env, i32, i32, i32, i32, i32)
DEF_HELPER_6(vector_vamoandd_v, void, env, i32, i32, i32, i32, i32)
DEF_HELPER_6(vector_vamoorw_v, void, env, i32, i32, i32, i32, i32)
DEF_HELPER_6(vector_vamoord_v, void, env, i32, i32, i32, i32, i32)
DEF_HELPER_6(vector_vamominw_v, void, env, i32, i32, i32, i32, i32)
DEF_HELPER_6(vector_vamomind_v, void, env, i32, i32, i32, i32, i32)
DEF_HELPER_6(vector_vamomaxw_v, void, env, i32, i32, i32, i32, i32)
DEF_HELPER_6(vector_vamomaxd_v, void, env, i32, i32, i32, i32, i32)
DEF_HELPER_6(vector_vamominuw_v, void, env, i32, i32, i32, i32, i32)
DEF_HELPER_6(vector_vamominud_v, void, env, i32, i32, i32, i32, i32)
DEF_HELPER_6(vector_vamomaxuw_v, void, env, i32, i32, i32, i32, i32)
DEF_HELPER_6(vector_vamomaxud_v, void, env, i32, i32, i32, i32, i32)

DEF_HELPER_4(vector_vadc_vvm, void, env, i32, i32, i32)
DEF_HELPER_4(vector_vadc_vxm, void, env, i32, i32, i32)
DEF_HELPER_4(vector_vadc_vim, void, env, i32, i32, i32)
DEF_HELPER_4(vector_vmadc_vvm, void, env, i32, i32, i32)
DEF_HELPER_4(vector_vmadc_vxm, void, env, i32, i32, i32)
DEF_HELPER_4(vector_vmadc_vim, void, env, i32, i32, i32)
DEF_HELPER_4(vector_vsbc_vvm, void, env, i32, i32, i32)
DEF_HELPER_4(vector_vsbc_vxm, void, env, i32, i32, i32)
DEF_HELPER_4(vector_vmsbc_vvm, void, env, i32, i32, i32)
DEF_HELPER_4(vector_vmsbc_vxm, void, env, i32, i32, i32)
DEF_HELPER_5(vector_vadd_vv, void, env, i32, i32, i32, i32)
DEF_HELPER_5(vector_vadd_vx, void, env, i32, i32, i32, i32)
DEF_HELPER_5(vector_vadd_vi, void, env, i32, i32, i32, i32)
DEF_HELPER_5(vector_vsub_vv, void, env, i32, i32, i32, i32)
DEF_HELPER_5(vector_vsub_vx, void, env, i32, i32, i32, i32)
DEF_HELPER_5(vector_vrsub_vx, void, env, i32, i32, i32, i32)
DEF_HELPER_5(vector_vrsub_vi, void, env, i32, i32, i32, i32)
DEF_HELPER_5(vector_vwaddu_vv, void, env, i32, i32, i32, i32)
DEF_HELPER_5(vector_vwaddu_vx, void, env, i32, i32, i32, i32)
DEF_HELPER_5(vector_vwadd_vv, void, env, i32, i32, i32, i32)
DEF_HELPER_5(vector_vwadd_vx, void, env, i32, i32, i32, i32)
DEF_HELPER_5(vector_vwsubu_vv, void, env, i32, i32, i32, i32)
DEF_HELPER_5(vector_vwsubu_vx, void, env, i32, i32, i32, i32)
DEF_HELPER_5(vector_vwsub_vv, void, env, i32, i32, i32, i32)
DEF_HELPER_5(vector_vwsub_vx, void, env, i32, i32, i32, i32)
DEF_HELPER_5(vector_vwaddu_wv, void, env, i32, i32, i32, i32)
DEF_HELPER_5(vector_vwaddu_wx, void, env, i32, i32, i32, i32)
DEF_HELPER_5(vector_vwadd_wv, void, env, i32, i32, i32, i32)
DEF_HELPER_5(vector_vwadd_wx, void, env, i32, i32, i32, i32)
DEF_HELPER_5(vector_vwsubu_wv, void, env, i32, i32, i32, i32)
DEF_HELPER_5(vector_vwsubu_wx, void, env, i32, i32, i32, i32)
DEF_HELPER_5(vector_vwsub_wv, void, env, i32, i32, i32, i32)
DEF_HELPER_5(vector_vwsub_wx, void, env, i32, i32, i32, i32)

DEF_HELPER_5(vector_vand_vv, void, env, i32, i32, i32, i32)
DEF_HELPER_5(vector_vand_vx, void, env, i32, i32, i32, i32)
DEF_HELPER_5(vector_vand_vi, void, env, i32, i32, i32, i32)
DEF_HELPER_5(vector_vor_vv, void, env, i32, i32, i32, i32)
DEF_HELPER_5(vector_vor_vx, void, env, i32, i32, i32, i32)
DEF_HELPER_5(vector_vor_vi, void, env, i32, i32, i32, i32)
DEF_HELPER_5(vector_vxor_vv, void, env, i32, i32, i32, i32)
DEF_HELPER_5(vector_vxor_vx, void, env, i32, i32, i32, i32)
DEF_HELPER_5(vector_vxor_vi, void, env, i32, i32, i32, i32)
DEF_HELPER_5(vector_vsll_vv, void, env, i32, i32, i32, i32)
DEF_HELPER_5(vector_vsll_vx, void, env, i32, i32, i32, i32)
DEF_HELPER_5(vector_vsll_vi, void, env, i32, i32, i32, i32)
DEF_HELPER_5(vector_vsrl_vv, void, env, i32, i32, i32, i32)
DEF_HELPER_5(vector_vsrl_vx, void, env, i32, i32, i32, i32)
DEF_HELPER_5(vector_vsrl_vi, void, env, i32, i32, i32, i32)
DEF_HELPER_5(vector_vsra_vv, void, env, i32, i32, i32, i32)
DEF_HELPER_5(vector_vsra_vx, void, env, i32, i32, i32, i32)
DEF_HELPER_5(vector_vsra_vi, void, env, i32, i32, i32, i32)
DEF_HELPER_5(vector_vnsrl_vv, void, env, i32, i32, i32, i32)
DEF_HELPER_5(vector_vnsrl_vx, void, env, i32, i32, i32, i32)
DEF_HELPER_5(vector_vnsrl_vi, void, env, i32, i32, i32, i32)
DEF_HELPER_5(vector_vnsra_vv, void, env, i32, i32, i32, i32)
DEF_HELPER_5(vector_vnsra_vx, void, env, i32, i32, i32, i32)
DEF_HELPER_5(vector_vnsra_vi, void, env, i32, i32, i32, i32)

DEF_HELPER_5(vector_vminu_vv, void, env, i32, i32, i32, i32)
DEF_HELPER_5(vector_vminu_vx, void, env, i32, i32, i32, i32)
DEF_HELPER_5(vector_vmin_vv, void, env, i32, i32, i32, i32)
DEF_HELPER_5(vector_vmin_vx, void, env, i32, i32, i32, i32)
DEF_HELPER_5(vector_vmaxu_vv, void, env, i32, i32, i32, i32)
DEF_HELPER_5(vector_vmaxu_vx, void, env, i32, i32, i32, i32)
DEF_HELPER_5(vector_vmax_vv, void, env, i32, i32, i32, i32)
DEF_HELPER_5(vector_vmax_vx, void, env, i32, i32, i32, i32)
DEF_HELPER_5(vector_vmseq_vv, void, env, i32, i32, i32, i32)
DEF_HELPER_5(vector_vmseq_vx, void, env, i32, i32, i32, i32)
DEF_HELPER_5(vector_vmseq_vi, void, env, i32, i32, i32, i32)
DEF_HELPER_5(vector_vmsne_vv, void, env, i32, i32, i32, i32)
DEF_HELPER_5(vector_vmsne_vx, void, env, i32, i32, i32, i32)
DEF_HELPER_5(vector_vmsne_vi, void, env, i32, i32, i32, i32)
DEF_HELPER_5(vector_vmsltu_vv, void, env, i32, i32, i32, i32)
DEF_HELPER_5(vector_vmsltu_vx, void, env, i32, i32, i32, i32)
DEF_HELPER_5(vector_vmslt_vv, void, env, i32, i32, i32, i32)
DEF_HELPER_5(vector_vmslt_vx, void, env, i32, i32, i32, i32)
DEF_HELPER_5(vector_vmsleu_vv, void, env, i32, i32, i32, i32)
DEF_HELPER_5(vector_vmsleu_vx, void, env, i32, i32, i32, i32)
DEF_HELPER_5(vector_vmsleu_vi, void, env, i32, i32, i32, i32)
DEF_HELPER_5(vector_vmsle_vv, void, env, i32, i32, i32, i32)
DEF_HELPER_5(vector_vmsle_vx, void, env, i32, i32, i32, i32)
DEF_HELPER_5(vector_vmsle_vi, void, env, i32, i32, i32, i32)
DEF_HELPER_5(vector_vmsgtu_vx, void, env, i32, i32, i32, i32)
DEF_HELPER_5(vector_vmsgtu_vi, void, env, i32, i32, i32, i32)
DEF_HELPER_5(vector_vmsgt_vx, void, env, i32, i32, i32, i32)
DEF_HELPER_5(vector_vmsgt_vi, void, env, i32, i32, i32, i32)

DEF_HELPER_5(vector_vmul_vv, void, env, i32, i32, i32, i32)
DEF_HELPER_5(vector_vmul_vx, void, env, i32, i32, i32, i32)
DEF_HELPER_5(vector_vmulhsu_vv, void, env, i32, i32, i32, i32)
DEF_HELPER_5(vector_vmulhsu_vx, void, env, i32, i32, i32, i32)
DEF_HELPER_5(vector_vmulh_vv, void, env, i32, i32, i32, i32)
DEF_HELPER_5(vector_vmulh_vx, void, env, i32, i32, i32, i32)
DEF_HELPER_5(vector_vdivu_vv, void, env, i32, i32, i32, i32)
DEF_HELPER_5(vector_vdivu_vx, void, env, i32, i32, i32, i32)
DEF_HELPER_5(vector_vdiv_vv, void, env, i32, i32, i32, i32)
DEF_HELPER_5(vector_vdiv_vx, void, env, i32, i32, i32, i32)
DEF_HELPER_5(vector_vremu_vv, void, env, i32, i32, i32, i32)
DEF_HELPER_5(vector_vremu_vx, void, env, i32, i32, i32, i32)
DEF_HELPER_5(vector_vrem_vv, void, env, i32, i32, i32, i32)
DEF_HELPER_5(vector_vrem_vx, void, env, i32, i32, i32, i32)
DEF_HELPER_5(vector_vmulhu_vv, void, env, i32, i32, i32, i32)
DEF_HELPER_5(vector_vmulhu_vx, void, env, i32, i32, i32, i32)
DEF_HELPER_5(vector_vmadd_vv, void, env, i32, i32, i32, i32)
DEF_HELPER_5(vector_vmadd_vx, void, env, i32, i32, i32, i32)
DEF_HELPER_5(vector_vnmsub_vv, void, env, i32, i32, i32, i32)
DEF_HELPER_5(vector_vnmsub_vx, void, env, i32, i32, i32, i32)
DEF_HELPER_5(vector_vmacc_vv, void, env, i32, i32, i32, i32)
DEF_HELPER_5(vector_vmacc_vx, void, env, i32, i32, i32, i32)
DEF_HELPER_5(vector_vnmsac_vv, void, env, i32, i32, i32, i32)
DEF_HELPER_5(vector_vnmsac_vx, void, env, i32, i32, i32, i32)
DEF_HELPER_5(vector_vwmulu_vv, void, env, i32, i32, i32, i32)
DEF_HELPER_5(vector_vwmulu_vx, void, env, i32, i32, i32, i32)
DEF_HELPER_5(vector_vwmulsu_vv, void, env, i32, i32, i32, i32)
DEF_HELPER_5(vector_vwmulsu_vx, void, env, i32, i32, i32, i32)
DEF_HELPER_5(vector_vwmul_vv, void, env, i32, i32, i32, i32)
DEF_HELPER_5(vector_vwmul_vx, void, env, i32, i32, i32, i32)
DEF_HELPER_5(vector_vwmaccu_vv, void, env, i32, i32, i32, i32)
DEF_HELPER_5(vector_vwmaccu_vx, void, env, i32, i32, i32, i32)
DEF_HELPER_5(vector_vwmacc_vv, void, env, i32, i32, i32, i32)
DEF_HELPER_5(vector_vwmacc_vx, void, env, i32, i32, i32, i32)
DEF_HELPER_5(vector_vwmaccsu_vv, void, env, i32, i32, i32, i32)
DEF_HELPER_5(vector_vwmaccsu_vx, void, env, i32, i32, i32, i32)
DEF_HELPER_5(vector_vwmaccus_vx, void, env, i32, i32, i32, i32)
DEF_HELPER_5(vector_vmerge_vvm, void, env, i32, i32, i32, i32)
DEF_HELPER_5(vector_vmerge_vxm, void, env, i32, i32, i32, i32)
DEF_HELPER_5(vector_vmerge_vim, void, env, i32, i32, i32, i32)

DEF_HELPER_5(vector_vsaddu_vv, void, env, i32, i32, i32, i32)
DEF_HELPER_5(vector_vsaddu_vx, void, env, i32, i32, i32, i32)
DEF_HELPER_5(vector_vsaddu_vi, void, env, i32, i32, i32, i32)
DEF_HELPER_5(vector_vsadd_vv, void, env, i32, i32, i32, i32)
DEF_HELPER_5(vector_vsadd_vx, void, env, i32, i32, i32, i32)
DEF_HELPER_5(vector_vsadd_vi, void, env, i32, i32, i32, i32)
DEF_HELPER_5(vector_vssubu_vv, void, env, i32, i32, i32, i32)
DEF_HELPER_5(vector_vssubu_vx, void, env, i32, i32, i32, i32)
DEF_HELPER_5(vector_vssub_vv, void, env, i32, i32, i32, i32)
DEF_HELPER_5(vector_vssub_vx, void, env, i32, i32, i32, i32)
DEF_HELPER_5(vector_vaadd_vv, void, env, i32, i32, i32, i32)
DEF_HELPER_5(vector_vaadd_vx, void, env, i32, i32, i32, i32)
DEF_HELPER_5(vector_vaadd_vi, void, env, i32, i32, i32, i32)
DEF_HELPER_5(vector_vasub_vv, void, env, i32, i32, i32, i32)
DEF_HELPER_5(vector_vasub_vx, void, env, i32, i32, i32, i32)
DEF_HELPER_5(vector_vsmul_vv, void, env, i32, i32, i32, i32)
DEF_HELPER_5(vector_vsmul_vx, void, env, i32, i32, i32, i32)
DEF_HELPER_5(vector_vwsmaccu_vv, void, env, i32, i32, i32, i32)
DEF_HELPER_5(vector_vwsmaccu_vx, void, env, i32, i32, i32, i32)
DEF_HELPER_5(vector_vwsmacc_vv, void, env, i32, i32, i32, i32)
DEF_HELPER_5(vector_vwsmacc_vx, void, env, i32, i32, i32, i32)
DEF_HELPER_5(vector_vwsmaccsu_vv, void, env, i32, i32, i32, i32)
DEF_HELPER_5(vector_vwsmaccsu_vx, void, env, i32, i32, i32, i32)
DEF_HELPER_5(vector_vwsmaccus_vx, void, env, i32, i32, i32, i32)
DEF_HELPER_5(vector_vssrl_vv, void, env, i32, i32, i32, i32)
DEF_HELPER_5(vector_vssrl_vx, void, env, i32, i32, i32, i32)
DEF_HELPER_5(vector_vssrl_vi, void, env, i32, i32, i32, i32)
DEF_HELPER_5(vector_vssra_vv, void, env, i32, i32, i32, i32)
DEF_HELPER_5(vector_vssra_vx, void, env, i32, i32, i32, i32)
DEF_HELPER_5(vector_vssra_vi, void, env, i32, i32, i32, i32)
DEF_HELPER_5(vector_vnclipu_vv, void, env, i32, i32, i32, i32)
DEF_HELPER_5(vector_vnclipu_vx, void, env, i32, i32, i32, i32)
DEF_HELPER_5(vector_vnclipu_vi, void, env, i32, i32, i32, i32)
DEF_HELPER_5(vector_vnclip_vv, void, env, i32, i32, i32, i32)
DEF_HELPER_5(vector_vnclip_vx, void, env, i32, i32, i32, i32)
DEF_HELPER_5(vector_vnclip_vi, void, env, i32, i32, i32, i32)

DEF_HELPER_4(vector_vsetvli, void, env, i32, i32, i32)
DEF_HELPER_4(vector_vsetvl, void, env, i32, i32, i32)

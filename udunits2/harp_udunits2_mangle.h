#ifndef HARP_UDUNITS2_MANGLE_H
#define HARP_UDUNITS2_MANGLE_H

/*
 * This header file mangles all symbols exported from the udunits2 library.
 * This is needed on some platforms because of nameresolving conflicts when
 * HARP is used as a module in an application that has its own version of udunits2.
 * Even though name mangling is not needed for every platform or HARP
 * interface, we always perform the mangling for consitency reasons.
 *
 * The following command was used to obtain the symbol list:
 * nm .libs/libudunits2.a | grep " [TR] " | grep -v " _harp_"
 *
 * Note that the symbols of the expat library that udunits2 links against are
 * name mangled separately.
 */

#ifdef HARP_UDUNITS2_NAME_MANGLE

#define coreFreeSystem harp_coreFreeSystem
#define cv_combine harp_cv_combine
#define cv_convert_double harp_cv_convert_double
#define cv_convert_doubles harp_cv_convert_doubles
#define cv_convert_float harp_cv_convert_float
#define cv_convert_floats harp_cv_convert_floats
#define cv_free harp_cv_free
#define cv_get_expression harp_cv_get_expression
#define cv_get_galilean harp_cv_get_galilean
#define cv_get_inverse harp_cv_get_inverse
#define cv_get_log harp_cv_get_log
#define cv_get_offset harp_cv_get_offset
#define cv_get_pow harp_cv_get_pow
#define cv_get_scale harp_cv_get_scale
#define cv_get_trivial harp_cv_get_trivial
#define itumFreeSystem harp_itumFreeSystem
#define smFind harp_smFind
#define smFree harp_smFree
#define smNew harp_smNew
#define smRemove harp_smRemove
#define smSearch harp_smSearch
#define uaiFree harp_uaiFree
#define uaiNew harp_uaiNew
#define ut_accept_visitor harp_ut_accept_visitor
#define ut_add_name_prefix harp_ut_add_name_prefix
#define ut_add_symbol_prefix harp_ut_add_symbol_prefix
#define ut_are_convertible harp_ut_are_convertible
#define ut_clone harp_ut_clone
#define ut_compare harp_ut_compare
#define ut_decode_time harp_ut_decode_time
#define ut_delete_buffer harp_ut_delete_buffer
#define ut_divide harp_ut_divide
#define ut_encode_clock harp_ut_encode_clock
#define ut_encode_date harp_ut_encode_date
#define ut_encode_time harp_ut_encode_time
#define ut_form_plural harp_ut_form_plural
#define ut_format harp_ut_format
#define ut_free harp_ut_free
#define ut_free_system harp_ut_free_system
#define ut_get_bufferpos harp_ut_get_bufferpos
#define ut_get_converter harp_ut_get_converter
#define ut_get_dimensionless_unit_one harp_ut_get_dimensionless_unit_one
#define ut_get_name harp_ut_get_name
#define ut_get_path_xml harp_ut_get_path_xml
#define ut_get_status harp_ut_get_status
#define ut_get_symbol harp_ut_get_symbol
#define ut_get_system harp_ut_get_system
#define ut_get_unit_by_name harp_ut_get_unit_by_name
#define ut_get_unit_by_symbol harp_ut_get_unit_by_symbol
#define ut_handle_error_message harp_ut_handle_error_message
#define ut_ignore harp_ut_ignore
#define ut_invert harp_ut_invert
#define ut_is_dimensionless harp_ut_is_dimensionless
#define ut_log harp_ut_log
#define ut_map_name_to_unit harp_ut_map_name_to_unit
#define ut_map_symbol_to_unit harp_ut_map_symbol_to_unit
#define ut_map_unit_to_name harp_ut_map_unit_to_name
#define ut_map_unit_to_symbol harp_ut_map_unit_to_symbol
#define ut_multiply harp_ut_multiply
#define ut_new_base_unit harp_ut_new_base_unit
#define ut_new_dimensionless_unit harp_ut_new_dimensionless_unit
#define ut_new_system harp_ut_new_system
#define ut_offset harp_ut_offset
#define ut_offset_by_time harp_ut_offset_by_time
#define ut_parse harp_ut_parse
#define ut_raise harp_ut_raise
#define ut_read_xml harp_ut_read_xml
#define ut_root harp_ut_root
#define ut_same_system harp_ut_same_system
#define ut_scale harp_ut_scale
#define ut_scan_buffer harp_ut_scan_buffer
#define ut_scan_bytes harp_ut_scan_bytes
#define ut_scan_string harp_ut_scan_string
#define ut_set_error_message_handler harp_ut_set_error_message_handler
#define ut_set_second harp_ut_set_second
#define ut_set_status harp_ut_set_status
#define ut_trim harp_ut_trim
#define ut_unmap_name_to_unit harp_ut_unmap_name_to_unit
#define ut_unmap_symbol_to_unit harp_ut_unmap_symbol_to_unit
#define ut_unmap_unit_to_name harp_ut_unmap_unit_to_name
#define ut_unmap_unit_to_symbol harp_ut_unmap_unit_to_symbol
#define ut_write_to_stderr harp_ut_write_to_stderr
#define uterror harp_uterror
#define utget_debug harp_utget_debug
#define utget_in harp_utget_in
#define utget_leng harp_utget_leng
#define utget_lineno harp_utget_lineno
#define utget_out harp_utget_out
#define utget_text harp_utget_text
#define utGetPrefixByName harp_utGetPrefixByName
#define utGetPrefixBySymbol harp_utGetPrefixBySymbol
#define utimFreeSystem harp_utimFreeSystem
#define utlex_destroy harp_utlex_destroy
#define utparse harp_utparse
#define utpop_buffer_state harp_utpop_buffer_state
#define utpush_buffer_state harp_utpush_buffer_state
#define utset_debug harp_utset_debug
#define utset_in harp_utset_in
#define utset_lineno harp_utset_lineno
#define utset_out harp_utset_out

#endif

#endif

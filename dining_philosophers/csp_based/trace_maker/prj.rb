require 'mxx_ru/cpp'

MxxRu::Cpp::lib_target {

	required_prj 'so_5/prj_s.rb'
	required_prj 'fmt_mxxru/prj.rb'

	target 'lib/csp_based.trace_maker'

	cpp_source 'all.cpp'
}

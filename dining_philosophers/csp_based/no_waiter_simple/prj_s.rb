require 'mxx_ru/cpp'

MxxRu::Cpp::exe_target {

	required_prj 'so_5/prj_s.rb'
	required_prj 'fmt_mxxru/prj.rb'

	required_prj 'dining_philosophers/csp_based/trace_maker/prj.rb'

	target 'csp_no_waiter_simple'

	cpp_source 'main.cpp'
}

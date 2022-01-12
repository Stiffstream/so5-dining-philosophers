MxxRu::arch_externals :so5 do |e|
  e.url 'https://github.com/Stiffstream/sobjectizer/releases/download/v.5.7.3/so-5.7.3.tar.bz2'

  e.map_dir 'dev/so_5' => 'dev'
end

MxxRu::arch_externals :so5extra do |e|
  e.url 'https://github.com/Stiffstream/so5extra/releases/download/v.1.5.0/so5extra-1.5.0.tar.bz2'

  e.map_dir 'dev/so_5_extra' => 'dev'
end

MxxRu::arch_externals :fmt do |e|
  e.url 'https://github.com/fmtlib/fmt/releases/download/8.1.1/fmt-8.1.1.zip'

  e.map_dir 'include' => 'dev/fmt'
  e.map_dir 'src' => 'dev/fmt'
  e.map_dir 'support' => 'dev/fmt'
  e.map_file 'CMakeLists.txt' => 'dev/fmt/*'
  e.map_file 'README.rst' => 'dev/fmt/*'
  e.map_file 'ChangeLog.rst' => 'dev/fmt/*'
end

MxxRu::arch_externals :fmtlib_mxxru do |e|
  e.url 'https://github.com/Stiffstream/fmtlib_mxxru/archive/refs/tags/fmt-5.0.0-1.tar.gz'

  e.map_dir 'dev/fmt_mxxru' => 'dev'
end


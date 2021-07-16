local t = ...
local strDistId, strDistVersion, strCpuArch = t:get_platform()
local tResult

if strDistId=='@JONCHKI_PLATFORM_DIST_ID@' and strDistVersion=='@JONCHKI_PLATFORM_DIST_VERSION@' and strCpuArch=='@JONCHKI_PLATFORM_CPU_ARCH@' then
  t:install_dev('include',     '${install_dev_include}/')
  t:install('bin/openocd.so',  '${install_lua_cpath}/openocd/')
  tResult = true
end

return tResult

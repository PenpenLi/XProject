@echo off



echo ���ɿͻ��˱�======
pushd Client
call Xls2Js.bat
popd



echo ���ɷ���˱�======
pushd Server
call Xls2Lua.bat
popd

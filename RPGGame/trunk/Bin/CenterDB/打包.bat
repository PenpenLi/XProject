@echo off

echo ɾ���ɰ�
del globaldb.zip /Q

echo ���ȫ�����ݿ�
mkdir globaldb\
xcopy /y/f *.sh globaldb\
xcopy /y/f SSDBServer globaldb\

echo ������ݿ�ṹ
mkdir globaldb\DB\Center
xcopy /y/f DB\Center\ssdb.conf globaldb\DB\Center


7z a -tzip GlobalDB.zip globaldb
rd /s/q globaldb
pause


@echo off

echo ������RPGGame
xcopy Libs ..\..\RPGGame\trunk\Libs /e /y
xcopy Source\Include ..\..\RPGGame\trunk\Src\Include /e /y

echo ������Game
xcopy Libs ..\..\Game\trunk\Libs /e /y
xcopy Source\Include ..\..\Game\trunk\Src\Include /e /y

echo ������PandaRC
xcopy Libs ..\..\PandaRC\Libs /e /y
xcopy Source\Include ..\..\PandaRC\Src\Include /e /y
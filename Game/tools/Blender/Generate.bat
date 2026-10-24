@echo off
setlocal
if not defined BLENDER set "BLENDER=F:\Program Files\Blender Foundation\Blender 4.5\blender.exe"
set "ASSETS=%~dp0..\..\assets\Original"
"%BLENDER%" --background --factory-startup --python-exit-code 1 --python "%~dp0obsidian_cluster.py" -- --out "%ASSETS%\ObsidianCluster" || exit /b 1
"%BLENDER%" --background --factory-startup --python-exit-code 1 --python "%~dp0fountain.py" -- --out "%ASSETS%\Fountain" || exit /b 1
"%BLENDER%" --background --factory-startup --python-exit-code 1 --python "%~dp0ground_materials.py" -- --out "%ASSETS%\Materials" || exit /b 1
"%BLENDER%" --background --factory-startup --python-exit-code 1 --python "%~dp0critters.py" -- --out "%ASSETS%\Critters" || exit /b 1
"%BLENDER%" --background --factory-startup --python-exit-code 1 --python "%~dp0hubprops.py" -- --out "%ASSETS%\HubProps" || exit /b 1
"%BLENDER%" --background --factory-startup --python-exit-code 1 --python "%~dp0stations.py" -- --out "%ASSETS%\Stations" || exit /b 1
"%BLENDER%" --background --factory-startup --python-exit-code 1 --python "%~dp0arcana.py" -- --out "%ASSETS%\Arcana" || exit /b 1
"%BLENDER%" --background --factory-startup --python-exit-code 1 --python "%~dp0nature.py" -- --out "%ASSETS%\Nature" || exit /b 1
"%BLENDER%" --background --factory-startup --python-exit-code 1 --python "%~dp0emberhold.py" -- --out "%ASSETS%\Emberhold" || exit /b 1
"%BLENDER%" --background --factory-startup --python-exit-code 1 --python "%~dp0market.py" -- --out "%ASSETS%\Market" || exit /b 1
"%BLENDER%" --background --factory-startup --python-exit-code 1 --python "%~dp0chess.py" -- --out "%ASSETS%\Chess" || exit /b 1
"%BLENDER%" --background --factory-startup --python-exit-code 1 --python "%~dp0hero.py" -- --out "%ASSETS%\Hero" || exit /b 1
"%BLENDER%" --background --factory-startup --python-exit-code 1 --python "%~dp0armory.py" -- --out "%ASSETS%\Armory" || exit /b 1
"%BLENDER%" --background --factory-startup --python-exit-code 1 --python "%~dp0glacier.py" -- --out "%ASSETS%\Glacier" || exit /b 1

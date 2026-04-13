@echo off
echo ========================================
echo  CONSTRUCTOR DE SISTEMA DE ALARMA
echo ========================================

:: Cambiar al directorio del script automáticamente
cd /d "%~dp0"

echo Directorio actual: %CD%
echo.

:: Verificar que existe el archivo principal
if not exist "alarm_system.py" (
    echo ERROR: No se encuentra alarm_system.py
    echo Asegúrate de estar en el directorio correcto.
    pause
    exit /b 1
)

echo Limpiando builds anteriores...
if exist "build" rmdir /s /q build
if exist "dist" rmdir /s /q dist
if exist "SistemaAlarma.spec" del SistemaAlarma.spec

echo.
echo Construyendo ejecutable...
echo Esto puede tardar varios minutos...
echo.

:: Construir ejecutable con más imports ocultos
pyinstaller ^
  --onefile ^
  --windowed ^
  --name "SistemaAlarma" ^
  --icon=alarm_icon.ico ^
  --add-data "alarm_icon.ico;." ^
  --hidden-import "tkinter" ^
  --hidden-import "cryptography" ^
  --hidden-import "csv" ^
  --hidden-import "json" ^
  --hidden-import "hashlib" ^
  --hidden-import "queue" ^
  --hidden-import "threading" ^
  --hidden-import "logging" ^
  --hidden-import "datetime" ^
  --hidden-import "socket" ^
  --hidden-import "os" ^
  --hidden-import "sys" ^
  --hidden-import "pathlib" ^
  --hidden-import "enum" ^
  --hidden-import "time" ^
  --hidden-import "threading" ^
  --hidden-import "collections" ^
  --hidden-import "collections.abc" ^
  --hidden-import "encodings" ^
  --hidden-import "encodings.utf_8" ^
  --clean ^
  --noconfirm ^
  alarm_system.py

echo.
if exist "dist\SistemaAlarma.exe" (
    echo ¡EJECUTABLE CREADO EXITOSAMENTE!
    echo ========================================
    echo Archivo: dist\SistemaAlarma.exe
    echo Tamaño: 
    for %%F in ("dist\SistemaAlarma.exe") do echo   %%~zF bytes
    echo.
    
    :: Copiar ícono si existe
    if exist "alarm_icon.ico" (
        copy "alarm_icon.ico" "dist\" > nul
        echo Ícono copiado a la carpeta dist\
    ) else (
        echo ADVERTENCIA: No se encontró alarm_icon.ico
        echo La aplicación usará el ícono por defecto.
    )
    
    echo.
    echo Para distribuir, copie toda la carpeta "dist"
    echo Incluyendo los archivos de configuración que se crean.
    
    :: Abrir carpeta dist en explorador
    echo.
    set /p open="¿Abrir carpeta dist en el explorador? (S/N): "
    if /i "%open%"=="S" explorer "dist"
) else (
    echo ERROR: No se pudo crear el ejecutable
    echo Revise los mensajes de error anteriores.
)

echo.
pause
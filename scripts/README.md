# Служебные скрипты

| Файл | Что делает |
|---|---|
| `install.bat` | Устанавливает модификацию в клиент игры (копируется в `dist/` при сборке). |
| `uninstall.bat` | Удаляет модификацию из клиента игры (копируется в `dist/`). |
| `check-symbols.sh` / `check-symbols.py` | Офлайн-проверка `src/core/symbols.def.h` по `factorio.pdb` нужной версии игры (WSL, нужен `llvm-pdbutil`). |
| `gen-winmm-proxy.py` | Генерирует заглушки экспорта прокси `winmm.dll` по системной библиотеке. |

## Как bat-файлы определяют язык

1. `locale=` из `%APPDATA%\Factorio\config\config.ini`.
2. Если там `auto` или настройки нет, берётся язык интерфейса Windows из реестра: `HKCU\Control Panel\Desktop\PreferredUILanguages`, затем первый язык из `HKCU\Control Panel\International\User Profile\Languages`, затем `HKCU\Control Panel\Desktop\MuiCached\MachinePreferredUILanguages`.
3. `ru…` даёт русский, всё остальное — английский.

PowerShell (`Get-UICulture`) для этого **не подходит**: при кодовой странице консоли, которая не умеет показывать язык интерфейса (437, 65001), он возвращает `en-US`.

Вывод идёт в UTF-8 (`chcp 65001`), сами файлы сохранены в UTF-8 без BOM с переводами строк CRLF. С LF cmd разбирает такие файлы неверно.

## Поиск игры в Steam

Без аргумента скрипты запускают через `-EncodedCommand` (UTF-16LE, base64) такой PowerShell-код:

```powershell
$s = (Get-ItemProperty 'HKCU:\Software\Valve\Steam' -ErrorAction SilentlyContinue).SteamPath
if (-not $s) { exit 1 }
$libs = @($s)
$vdf = Join-Path $s 'steamapps\libraryfolders.vdf'
if (Test-Path $vdf) {
  foreach ($m in [regex]::Matches((Get-Content -Raw $vdf), '"path"\s+"([^"]+)"')) { $libs += $m.Groups[1].Value.Replace('\\', '\') }
}
foreach ($l in $libs) {
  $p = Join-Path $l 'steamapps\common\Factorio'
  if (Test-Path (Join-Path $p 'bin\x64\factorio.exe')) { (Resolve-Path $p).Path; exit 0 }
}
exit 1
```

Base64 в `for /f (...)` обязательно стоит в кавычках: без них cmd заменяет `=` на пробел и портит строку. Результат принимается, только если в найденной папке есть `bin\x64\factorio.exe`.

Перегенерировать base64 после правки кода:

```bash
python3 -c "import base64,sys; print(base64.b64encode(sys.stdin.read().encode('utf-16-le')).decode())" < search.ps1
```

## Проверка скриптов без настоящей игры

`MODPACKS_NOPAUSE=1` отключает `pause` в конце. Достаточно поддельной папки с `bin\x64\factorio.exe` и `factorio.pdb` (любое содержимое) и `dist\files` с любыми `winmm.dll`, `modpacks.dll`, `VERSION`. Запущенную игру можно изобразить копией `ping.exe`, переименованной в `factorio.exe`.

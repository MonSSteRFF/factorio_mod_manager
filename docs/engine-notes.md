# Заметки о движке Factorio

Исследовано на **Factorio 2.1.21** (build 87673, win64, Steam) по `bin/x64/factorio.pdb` с помощью `llvm-pdbutil` (публичные символы и типы) и `llvm-objdump` (дизассемблирование). Адреса ниже — RVA этой сборки, только для ориентира. Код модификации их **не использует**: всё берётся по именам (см. `src/core/symbols.def.h`).

Общие факты:
- PDB содержит публичные символы и полную информацию о типах: поля с именами и смещениями, базовые классы, слоты vtable виртуальных методов, значения enum.
- `factorio.exe` собран MSVC со статическим UCRT. `operator new` → `malloc` → `HeapAlloc(__acrt_heap)`, то есть стандартная куча процесса (`GetProcessHeap`). Наша DLL тоже собирается со статическим UCRT и пользуется **той же кучей**. Поэтому строки, `std::function` и виджеты можно создавать у нас и отдавать движку, а движок может их освобождать. Для виджетов всё равно используется `operator new` движка, чтобы размер и выделение были как в штатном коде.
- Соглашение о вызовах — обычное MSVC x64: `this` в `rcx`. Классы по значению (`std::function`) передаются указателем на временный объект, который уничтожает вызываемая функция. Это получается само, если вызывать через указатель на функцию с точными C++-типами параметров.

## 2.1 Стили виджетов

Стили хранятся в `GuiStyle`, доступном через глобальный указатель:

```
GlobalContext* global                                   (символ `global`)
  global->style : UniquePointer<GuiStyle>               (GlobalContext::style, +0x258)
    GuiStyle::styles : std::map<std::string, IntegratedStyleBase*, std::less<void>>   (+0x1a8)
```

- Ключ словаря `styles` — имя стиля из прототипов (`data.raw["gui-style"]["default"]`): `button`, `tool_button`, `red_button`, `dropdown`, `textbox`, `label`, `bold_red_label`, `horizontal_flow`, `vertical_flow`, `empty_widget`, `deep_frame_in_shallow_frame`, `scroll_pane` и т.д. Все используемые имена есть в `data/core/prototypes/style.lua` версии 2.1.21. Это публичные имена, на которые опираются Lua-моды, поэтому они стабильнее внутренних полей.
- Из `IntegratedStyleBase*` получаем указатель на agui-стиль через виртуальный `IntegratedStyleBase::getAguiStyle()`. Это слот **vtbl+24** (`setup` +8, `apply` +16, `getAguiStyle` +24, `getStyleSpecification` +32). Результат приводится к нужному типу: `agui::ButtonStyle const*`, `agui::DropDownStyle const*`, `agui::TextBoxStyle const*` и т.д.
- Штатный код обращается к стилям через именованные поля `GuiStyle` (`_deepFrameInShallowFrame`, `_modsScrollPane`, `_tableWithSelection`, `buttonStyle`, `dropDownStyle`, `textBoxStyle`…) и поле `IntegratedStyle<Spec>::agui`. Например, `ManageModsPane` вызывает `FrameWithSubheader(global->style->_deepFrameInShallowFrame->agui, false, true)`. Мы используем словарь по имени, это эквивалентно и не зависит от набора полей.
- Обход `std::map` выполняется по раскладке MSVC STL (`_Myhead`, узлы `_Left/_Parent/_Right/_Color/_Isnil`, затем `pair<const string, T>`). Она стабильна для всего семейства v14x.

## 2.2 События agui

Подписка на события — методы виджета, принимающие «цель» и `std::function`:

| Событие | Метод |
|---|---|
| клик | `agui::Widget::onClick(agui::GenericTargetable*, std::function<void()>)` |
| выбор в выпадающем списке | `agui::DropDown::onItemSelect(agui::GenericTargetable*, std::function<void(int)>)` |
| смена галочки | `agui::ToggleButton::onCheckChange(agui::GenericTargetable*, std::function<void()>)` |
| смена вкладки | `agui::TabbedPane::onSelectedTabChange(agui::GenericTargetable*, std::function<void()>)` |
| поиск | конструктор `SearchBar(agui::Frame*, std::function<void(std::string const&)>, NamedBool<InSubheaderTag>)` |

- `GenericTargetable` — база `agui::Widget` по смещению **+8** (из базовых классов в PDB). Штатный код передаёт `&widget->GenericTargetable` своего же виджета (`lea rdx, [r14+8]`). Обработчик живёт, пока жива цель. Мы передаём базу `GenericTargetable` того виджета, к которому относится обработчик.
- `onClick` возвращает `agui::Widget&` (для цепочек), остальные — `void`.
- Текстового события у `TextField` нет. Текст читаем по подтверждению через виртуальный `agui::TextBox::getText()`.

### Дерево виджетов и владение

- `agui::Widget`: `parentWidget` (+0x50), `children` (`std::vector<Widget*>`, +0x58), `usageBitMask` (+0x88).
- Добавление и вставка: виртуальные `Widget::add(Widget*)` (vtbl+0x100 в этой сборке) и `Widget::insert(Widget*, unsigned)`, позиция — `Widget::getIndexInParent()`.
- **Владение.** Штатный код создаёт динамические виджеты через `operator new`, затем выставляет `usageBitMask |= 0x200` и добавляет в родителя (`ManageModsPane::ManageModsPane`, создание заголовков таблицы). `Widget::clearParentWidget(KeepWidgetAlive=false)` проверяет `test dword [this+usageBitMask], 0x200` и уничтожает такой виджет. Виджеты-поля (по значению) этот флаг не имеют. Константа `0x200` в PDB не названа, поэтому модификация **читает её из кода** `Widget::clearParentWidget`: ищет инструкцию `test dword [reg+usageBitMask], imm32` в начале функции. Если не нашла, модификация не активируется.

## 2.3 Галочки модов и `getChangedCount`

```
ManageModsPane
  modsData             +0xde0   std::vector<std::unique_ptr<ModAndName>>
  modWidgets           +0xdf8   std::vector<std::unique_ptr<ModWidgets>>
  filteredModWidgets   +0xe20   std::vector<ModWidgets*>
  enabledMaster        +0xe38   agui::CheckBox
  searchBar            +0x1088  SearchBar
  (база FrameWithSubheader::subheader +0x6c0 — agui::Frame верхней панели)

ManageModsPane::ModWidgets
  stateData   +0x0    ModAndName&
  enabled     +0x8    agui::CheckBox
  computed    +0x6c8  std::optional<ModDependency::Info>   (кэш проверки зависимостей)

ModAndName:  mod +0x0 (Mod*), toBeDeleted +0xe, localisedName +0x10
Mod:         name +0x48 (std::string), title +0x68
agui::ToggleButton::checkedState +0x134 : enum CheckedState { UNCHECKED=0, CHECKED=1, INTERMEDIATE=2 }
```

Как штатный `ManageModsPane::toggleMasterEnabled()` включает все моды:
1. для каждой строки из `filteredModWidgets`: `row->enabled.checkedState = CHECKED`, `row->stateData.toBeDeleted = false`, `enableAllRequiredDependenciesForMod(*row, false)`;
2. для **всех** строк `modWidgets`: сбрасывает кэш `computed` (`optional` без значения);
3. для каждой строки из `filteredModWidgets`: `updateStyleForModWidgets(*row)`;
4. обновляет инфо-панель выбранного мода.

`getChangedCount()` проходит `modWidgets` и сравнивает `checkedState` каждой строки с текущим состоянием мода в `ModManager`. Поэтому прямой записи `checkedState` достаточно, чтобы штатное подтверждение увидело изменения.

**Последовательность применения пакета (D6):**
1. для каждой строки `modWidgets`, кроме `base`: `checkedState = (mod в пакете) ? CHECKED : UNCHECKED`; если `CHECKED`, то `toBeDeleted = false`;
2. для всех строк сбросить `computed`;
3. для всех строк вызвать `updateStyleForModWidgets`;
4. вызвать `updateMasterEnabledStatus()` и `refreshView()`.

`enableAllRequiredDependenciesForMod` **не вызывается**: пакет даёт ровно свой состав, о недостающих зависимостях сообщит штатная проверка при подтверждении (решение D6).

Верхняя панель вкладки (`subheader`) после конструктора выглядит так: `[HorizontalPusher] [searchBar]`. Конструктор делает `subheader << HorizontalPusher` и `subheader.add(&searchBar)`. Галочка `enabledMaster` в панель **не входит**: это заголовок первого столбца таблицы `modsTable` (добавляется через виртуальный метод таблицы по `vtbl+0x568` вместе с заголовками «Название» и «Версия»). Панель пакетов вставляется в начало: `subheader.insert(panel, 0)`. Перед вставкой проверяется, что `searchBar.parentWidget == &subheader`.

## 2.3b Черновик галочек и моменты его записи

Обработчик клика по галочке строки — анонимная лямбда в конструкторе `ManageModsPane`, в публичных символах PDB её нет. Поэтому модификация не ловит сам клик, а сравнивает галочки со снимком (baseline) в моменты записи:

| Момент | Где перехвачено |
|---|---|
| «Подтвердить» и согласие в диалоге при выходе | `ManageModsPane::apply()` (`?apply@ManageModsPane@@QEAA_NXZ`, необязательный). Единственный вызов — из `ModsGui::onDialogButtonPressed(bool)`. Хук вызывает оригинал и пишет черновик, только если тот вернул `true` |
| смена вкладки окна | `ModsGui::updateTabContentsVisibility()` — обработчик `tabs.onSelectedTabChange`; хук ставится при наличии символа, независимо от инфо-панели |
| смена пакета в списке | наш обработчик выпадающего списка |

Пометка `*` у списка пакетов обновляется сразу после клика через хук на `ManageModsPane::updateStyleForModWidgets(ModWidgets&)`: в 2.1.21 лямбда клика вызывает её для строки после смены галочки. Её же вызывают `enableAllRequiredDependenciesForMod`, `toggleMasterEnabled`, `refreshView` и наш `RefreshRows`, поэтому хук только сравнивает галочку строки с последним виденным состоянием. Если лямбда перестанет её вызывать, пропадёт лишь мгновенность пометки: данные пишутся по снимкам.

Выделение строки в наших таблицах — `agui::TableWithSelection::editSelectedIndex(unsigned row, int)` (необязательный). Строка заголовка считается нулевой, `0xFFFFFFFF` снимает выделение, одинаковый индекс игнорируется. Функция вызывает `dispatchUserChangedSelection`, а он — подписчиков `onTableSelectionChange`, в том числе наш обработчик, поэтому на время программного выделения он заглушён.

## 2.3a Вкладки окна модов

```
ModsGui
  tabs                 +0xd28   agui::TabbedPane
  manageTab            +0x1800  agui::Tab
  manageTabContents    +0x1a50  ManageModsPane
  exploreTab / exploreTabContents (указатель), updatesTab / updatesTabContents (указатель)
```

- В конструкторе `ModsGui::ModsGui()`: `Tab(const std::string& caption, TabStyle const* = nullptr)`, затем `tabs.addTab(Tab*, Widget* content, Tab* after = nullptr)` для каждой вкладки, затем `tabs.onSelectedTabChange(..., updateTabContentsVisibility)`.
- Содержимое вкладок переключает `TabbedPane`. `updateTabContentsVisibility()` только показывает или прячет **боковые инфо-панели** трёх штатных вкладок, сравнивая выбранную вкладку с `manageTab`/`exploreTab`/`updatesTab`. Когда выбрана наша вкладка, все три сравнения ложны и все инфо-панели скрыты, что нам и нужно. Хук на эту функцию не обязателен, но установлен как страховка видимости (D4).
- Наша вкладка: `new Tab(caption)` и `new FrameWithSubheader(style("deep_frame_in_shallow_frame"), false, true)` как содержимое (так же устроена `ManageModsPane`). Оба с флагом владения, `tabs.addTab(tab, content, nullptr)` после оригинального конструктора.
- `SearchBar` сам себя в фрейм не добавляет: `ManageModsPane` делает `subheader << HorizontalPusher` и `subheader.add(&searchBar)`. Для нашей вкладки повторяем это: распорка — `EmptyWidget` со `stretchHorizontally()`.

## 2.4 Локаль и выделение памяти

- Текущий язык: `global->localeProvider` (`GlobalContext::localeProvider`, +0x148) → `LocaleProvider::currentLocale` (+0x10) → `LocaleProvider::Locale::language` (`std::string`, +0x0), например `"ru"`, `"en"`. В главном меню `GlobalContext::getActiveLocaleProvider()` возвращает именно его.
- Запасной путь: `GlobalContext::localeSetting` (+0x3b8, значение из `config.ini`) и `GlobalContext::recommendedAutoLocale` (+0x398).
- Выделение: `void* operator new(unsigned __int64)` (`??2@YAPEAX_K@Z`), освобождение: `void operator delete(void*)` (`??3@YAXPEAX@Z`). Размеры классов (`agui::TextButton` 528, `agui::DropDown` 3552, `agui::TextField` 2680, `SearchBar` 5576, `FrameWithSubheader` 3464, `agui::Tab`…) берутся из PDB во время выполнения.

## Чек-лист ручной проверки после обновления игры

Перед запуском: `scripts/check-symbols.sh <путь к bin\x64>` и `build\Release\modpacks_symtool.exe <factorio.exe>`. Обе проверки должны сообщить, что все обязательные записи найдены.

После запуска в `%APPDATA%\Factorio\modpacks\modpacks.log` должны быть строки `mod active` и пять `hook installed` (`ModsGui::ModsGui`, `~ManageModsPane`, `updateStyleForModWidgets`, `updateTabContentsVisibility`, `ManageModsPane::apply`). При открытии окна «Моды» — `mods window extended`.

**Вкладка «Управление»**
1. Слева от поиска: список пакетов и кнопки «Создать», «Переименовать», «Удалить», панель не налезает на поиск.
2. Без пакетов: список неактивен с текстом «Нет пакетов», «Переименовать» и «Удалить» неактивны.
3. «Создать» → название → «OK»: пакет создан, выбран, в `mod-packs.json` включённые моды без `base`.
4. Пустое название и дубликат в другом регистре дают красную ошибку. После успешного создания ошибка исчезает.
5. Выбор пакета в списке меняет галочки. При «Назад» игра спрашивает об отмене изменений («N модов изменены»), после отмены `mod-list.json` не изменился.
6. Выбор пакета не сбрасывает выделенный мод и инфо-панель справа.
7. «Переименовать» (поле заполнено текущим названием) и «Удалить» с «Да» / «Нет».
8. «Подтвердить» после выбора пакета перезапускает игру с модами пакета. После перезапуска выбран тот же пакет.
8a. При выбранном пакете клик по галочке показывает `*` справа от списка и пишет в лог `checkbox: <мод> on/off`; повторный клик убирает `*`.
8b. «Подтвердить», смена пакета и смена вкладки пишут правки в выбранный пакет: в логе `draft written to <пакет> on confirm / pack switch / tab switch`, `*` пропадает. Выход с отменой изменений пакет не меняет.
8c. После выхода без подтверждения и повторного открытия выбран пакет, совпадающий с включёнными модами, или «Не выбран» (в логе `pack on open`).

**Вкладка «Пакеты модов»**
9. Вкладка правее «Обновлений», переключение между четырьмя вкладками работает.
10. Две таблицы как на «Управлении»: в одну строку название, версия справа, квадратная кнопка `>` / `<` 16×16, высота строк как на «Управлении». Ширина 50/50, таблицы на всю ширину, в том числе пустые.
11. Счётчики «Все моды (N)» и «В пакете (N)» верные.
12. `>` / `<` переносят мод, он встаёт на место по алфавиту, файл сохраняется сразу.
13. Стрелка «Название» переворачивает порядок только своей таблицы.
14. Поиск фильтрует обе таблицы.
15. Щелчок по строке любой таблицы показывает справа информацию о моде и подсвечивает строку; подсветка остаётся после сортировки, поиска и переноса мода `>` / `<`, а в другой таблице снимается. После перехода на «Управление» там снова мод, выбранный на «Управлении».
16. Без пакета: подсказка «Создайте или выберите пакет», кнопки `>` неактивны. Выбор пакета на этой вкладке не меняет галочки на «Управлении».
17. Правка только пакетов не вызывает вопроса о несохранённых изменениях при закрытии окна.

**Общее**
18. 10+ открытий и закрытий окна «Моды» подряд без падения, в логе нет `error`.
19. Тексты мода на языке игры (RU) или на английском.

Автоматизировать клики для такой проверки можно скриптами, как в этой сессии: скриншот окна через `System.Drawing`, клики через `SetCursorPos` и `mouse_event`. Текст в игру проходит только через `SendInput` с `KEYEVENTF_UNICODE`, клавиши Backspace и Enter — через `SendInput` со скан-кодом. Обычный `keybd_event` игра не принимает.

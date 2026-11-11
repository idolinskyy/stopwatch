# StopWatch

Секундомір і таймер. Консоль і вікно на Qt рахують час одним ядром.

Пауза зупиняє відлік: поки вона триває, час не йде.

## Що вміє

Секундомір: старт, пауза, скидання, кола (спліт, загальний час, різниця з попереднім), найкраще, найгірше і середнє коло, примітка, експорт у CSV і Markdown.

Таймер: інтервал `25m`, `5m30s`, `1h 15m`, `90s`, `10:00`, `01:02:03.5`, `2хв 10с`. Є пресети, зсув на 30 секунд і автоповтор. Після нуля йде перебіг, у вікні тоді грає сигнал.

Табло: зелене, бурштинове, блакитне, червоне. У вікні три варіанти цифр, типовий — ElectroBoard 7. Тема, шрифт, розмір вікна і останній інтервал запам'ятовуються.

## Залежності

-   CMake 3.25 або новіший
-   компілятор C++23 (GCC або Clang)
-   Ninja або Make
-   Qt 6 Widgets, якщо потрібне вікно

```bash
sudo apt install build-essential cmake ninja-build qt6-base-dev
```

Якщо в дистрибутиві немає Qt 6, вікно збирається зі встановленого окремо, наприклад `$HOME/Qt/6.12.0/gcc_64`. Консоль від Qt не залежить: `-DBUILD_GUI=OFF`.

Звук у вікні береться з системної теми: потрібні `paplay` і пакет `sound-theme-freedesktop`. Якщо їх немає, програма грає власний сигнал через `aplay`.

## Збірка

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

Зі свого каталогу Qt:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_PREFIX_PATH="$HOME/Qt/6.12.0/gcc_64"
cmake --build build -j
```

| Опція                            |                                     |
| -------------------------------- | ----------------------------------- |
| `-DBUILD_GUI=OFF`                | без вікна                           |
| `-DBUILD_APPS=OFF`               | без програм                         |
| `-DBUILD_TESTS=OFF`              | без тестів                          |
| `-DENABLE_SANITIZERS=ON`         | AddressSanitizer і UBSan            |
| `-DENABLE_WARNINGS_AS_ERRORS=ON` | попередження компілятора як помилки |

Якщо в системі є `clang-format`, є цілі `format` і `format-check`.

Після збірки:

-   `build/apps/cli/StopWatch`
-   `build/apps/gui/StopWatch-gui`

Вікно, зібране з окремого Qt, шукає його бібліотеки за шляхом зі збірки. Той каталог Qt має лишатися на місці.

## Встановлення

Префікс задається під час конфігурації: ярлик меню записує повний шлях до програми. Для одного користувача це `~/.local`.

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX="$HOME/.local" \
  -DCMAKE_PREFIX_PATH="$HOME/Qt/6.12.0/gcc_64"
cmake --build build -j
cmake --install build
```

`CMAKE_PREFIX_PATH` не потрібен, якщо CMake сам знайшов Qt 6. Інший префікс — ще раз запустити `cmake` з новим `-DCMAKE_INSTALL_PREFIX`. `cmake --install --prefix` шлях у ярлику не змінює.

| Файл                                                       |            |
| ---------------------------------------------------------- | ---------- |
| `~/.local/bin/StopWatch`                                   | консоль    |
| `~/.local/bin/StopWatch-gui`                               | вікно      |
| `~/.local/share/applications/stopwatch.desktop`            | пункт меню |
| `~/.local/share/icons/hicolor/scalable/apps/stopwatch.svg` | іконка     |
| `~/.local/share/doc/stopwatch/LICENSE`                     | GPL-3      |
| `~/.local/share/doc/stopwatch/NOTICE`                      | Qt і решта |

Якщо пункт меню не з'явився:

```bash
update-desktop-database "$HOME/.local/share/applications"
```

Для всіх користувачів той самий порядок, з `-DCMAKE_INSTALL_PREFIX=/usr/local` і правами на запис у цей каталог. Запуск тоді `/usr/local/bin/StopWatch` і `/usr/local/bin/StopWatch-gui`, якщо цього каталогу немає в `PATH`.

## Запуск

```bash
./build/apps/cli/StopWatch
./build/apps/gui/StopWatch-gui
```

Після встановлення в `~/.local`, якщо `~/.local/bin` є в `PATH`:

```bash
StopWatch
StopWatch-gui
```

Інакше повний шлях: `"$HOME/.local/bin/StopWatch"`.

Консолі потрібен термінал з Unicode.

```bash
StopWatch --help
StopWatch --stopwatch --auto
StopWatch --timer 5m30s
StopWatch --headless-timer 90s
```

`--headless-timer` друкує один рядок. `--timer` робить те саме, якщо stdin не термінал.

```bash
cmake --build build --target unit_tests
ctest --test-dir build --output-on-failure
QT_QPA_PLATFORM=offscreen ./build/apps/gui/StopWatch-gui --self-check
```

## Видалення

Список файлів лежить у `build/install_manifest.txt`.

```bash
xargs rm -f < build/install_manifest.txt
update-desktop-database "$HOME/.local/share/applications"
```

`rm` запускайте тим самим користувачем, який ставив програму. Для `/usr/local` перед `xargs` потрібен `sudo`.

## Клавіші

Консоль:

| Клавіша     |                                            |
| ----------- | ------------------------------------------ |
| Пробіл      | старт / пауза                              |
| L           | коло або автоповтор                        |
| R           | скинути                                    |
| 1 / 2 / Tab | секундомір / таймер                        |
| S або T     | ввести інтервал                            |
| P           | наступний пресет                           |
| + / −       | ±30 секунд                                 |
| N           | примітка до останнього кола                |
| E           | `stopwatch_laps.csv` і `stopwatch_laps.md` |
| C           | наступна тема                              |
| Q           | вихід                                      |

У вікні ті самі клавіші, крім Tab і T. Tab переходить між елементами. E відкриває діалог для CSV, Markdown — окрема кнопка. S ставить курсор у поле інтервалу. Ctrl+Q закриває вікно. Поки курсор у полі вводу, літери йдуть у текст. Примітку кола міняють подвійним клацанням або клавішею N.

## Ліцензія

Код, шрифт ElectroBoard 7 і іконка — GNU GPL версії 3 або будь-якої пізнішої. Текст у файлі `LICENSE`.

Вікно лінкується з Qt 6 динамічно. Qt лишається під LGPL 3 (або GPL, якщо так зібрано той Qt) і в репозиторій не входить. Тести можуть завантажити Catch2 3.5.2, ліцензія Boost Software License 1.0. Подробиці у `NOTICE`.

У вікні те саме: Довідка → Про StopWatch і Про Qt…

## Каталоги

```text
libs/core    час, секундомір, таймер
apps/cli     консоль
apps/gui     вікно
packaging    ярлик і іконка
tests        тести ядра
```

`libs/core` без Qt. Час іде від `std::chrono::steady_clock`.

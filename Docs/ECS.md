# ECS игровой логики

Игрок, генерация лабиринта, прохождение и состояние сессии используют Unreal Mass Entity. `UMazeECSSubsystem` создаётся отдельно для каждого игрового мира/PIE. Системы работают синхронно на игровом потоке; автоматический граф `UMassProcessor` и параллельные задачи сейчас не используются.

| Сущность | Компоненты Mass | Обработка |
| --- | --- | --- |
| Игрок | Vitals, PlayerInput, PlayerPose, Locomotion, PlayerCommand, Progress, Exploration, Items | Управление, скорость, прыжок, урон, восстановление, определение выхода, исследование, экипированные предметы |
| Лабиринт | Generation | Seed, размеры, топология, поверхность стен, положение пола, старта и выходов |
| Сессия | Session | Активный лабиринт, открытое меню/настройки, начало игры, видимость карты |

`MazeECSFragments.h` содержит компоненты. `MazeGameplaySystems.h` содержит систему управления и генерации. `MazeVitalsSystem.h` содержит правила показателей. `MazeECSSubsystem` организует вызовы систем, запросы Mass, обработку сессии и прогресса.

## Порядок выполнения

1. GameMode создаёт адаптер лабиринта и инициализирует его до размещения PlayerStart. Адаптер запрашивает сущность ECS, система генерирует данные синхронно. Повторный BeginPlay не меняет seed и не строит тот же лабиринт заново.
2. `FMazeGenerationSystem` использует детерминированные вычислительные помощники `FMazeLayout` и `FMazeSurface`. Actor не запускает эти алгоритмы и не владеет топологией. Mass query обрабатывает фрагменты, ожидающие генерации.
3. `AMazeWorld` читает неизменяемый результат и передаёт его в ProceduralMesh, создаёт коллизии и надписи. Seed в Actor — зеркало для репликации Unreal; полученный seed поступает обратно в ECS клиента.
4. BeginPlay персонажа создаёт сущность игрока. Обработчики клавиш записывают фрагмент ввода. Каждый тик адаптер передаёт наблюдения физики в `ResolvePlayer`; система рассчитывает команды движения и обновляет прогресс. Адаптер исполняет команды через CharacterMovement и контроллер камеры. Jump запускается по фронту нажатия; стоимость списывается по подтверждению движка.
5. World Subsystem раз за кадр обновляет показатели запросом Mass по сущностям с Vitals/Locomotion. При паузе обновление выключено. Открытие/закрытие меню очищает ввод ECS.
6. HUD читает показатели, готовую топологию и результат прохождения. Скрытие карты/HUD не отключает проверку выхода. Новая ревизия лабиринта сбрасывает связанный прогресс.
7. EndPlay адаптеров удаляет сущности. Данные генерации живут в неизменяемом shared payload фрагмента; потребители могут удерживать снимок во время перегенерации. При уничтожении мира освобождаются его менеджер, запросы и сущность сессии.

## Граница с движком

Actor/Character, PlayerController, GameMode и HUD служат адаптерами Unreal: создают камеру и освещение, принимают ввод, вызывают решатель физики, загружают уровни, сохраняют настройки и рисуют UI. Игровые данные и правила находятся в ECS. Один лабиринт представлен одной сущностью с общим результатом генерации; отдельная сущность на каждую стену не создаётся.

Текущая игра рассчитана на одну локальную сессию и один активный лабиринт на мир. Состояние меню общее для сессии; split-screen и репликация ECS-состояния игрока не реализованы. Репликация seed сохранена.

Новые механики добавляются фрагментами и системами. Правило закреплено в `AGENTS.md`. Не возвращать отдельные копии показателей, генератор или проверку победы в Actor/HUD.

## Применение

Нужна полная сборка `labyEditor` с закрытым редактором: переименован World Subsystem и изменена структура отражаемых типов. После сборки открыть проект и запустить новую игру. Тесты и Play не запускались по просьбе пользователя.

## Предметы и налобный фонарик

- `FMazeItemsFragment` сущности игрока — единственный владелец списка экземпляров предметов. Каждый экземпляр содержит тип, уникальный в пределах игрока ID и слот экипировки (`None` означает хранение без экипировки). `NextInstanceId` принадлежит тому же фрагменту. Отдельных Actor или Mass-сущностей для носимых предметов пока нет.
- При `CreatePlayer` сервер/standalone вызывает `FMazeItemSystem::InitializeLoadout` один раз после создания сущности: выдаётся постоянный фонарик в слоте `Head`. Система определяет наличие работающего фонарика из экипировки. Заряда, переключения, выбрасывания, подбора, сохранения между мирами и интерфейса инвентаря пока нет; это основа данных для последующего расширения.
- `ReadItems` возвращает снимок по значению, `ReadHeadlampEnabled` — результат системы. Actor не получает изменяемые ссылки. Его `ReplicatedItems` — только транспортное зеркало: обновляется из ECS на сервере, реплицируется всем релевантным клиентам, а `OnRep_Items` передаёт снимок в клиентский ECS через `ReceiveItems`. Этот метод принимает данные только в клиентском мире. Клиенты сами предметы не выдают.
- `AMazeCharacter` создаёт только движковый ресурс `USpotLightComponent`. Каждый тик, включая удалённых персонажей, он читает разрешение света из ECS и выставляет направление через `GetBaseAimRotation` (с реплицируемым pitch). Источник закреплён чуть выше верхушки капсулы; наклон взгляда поворачивает луч, не перемещая источник внутрь тела. Параметры света централизованы в `FMazeHeadlampDefinition`. На dedicated server свет выключен.
- Предметы не зависят от меню, смерти или simulation delta: фонарик постоянно включён, пока экипирован; таймеров и автоматических Mass-процессоров нет. Обновление представления выполняется до ранних выходов из тика персонажа. При `EndPlay` удаляется сущность со списком предметов, выключается свет и очищается транспортное зеркало; компоненты уничтожает Unreal. При новом персонаже выдаётся новый стартовый набор.
- Для применения этого изменения нужна полная сборка с закрытым редактором: добавлены отражаемые типы предметов, реплицируемое поле и default subobject света. Live Coding не используется для замены конструкции уже существующих персонажей. Сборка, тесты и Play в рамках изменения не запускались.

## Exploration maps

- The player entity owns `FMazeExplorationFragment`: discovered cells, maze handle/revision and last pose samples. Data resets on entity destruction or maze regeneration; there is no disk persistence or shared team discovery.
- `ResolvePlayer` calls `FMazeExplorationSystem::Update` after storing the pose and resolving input availability, before movement commands. The current cell and visible cell centers within six cells and a 120-degree forward sector are discovered. Grid-edge ray traversal stops at walls and exact diagonal corners. Disabled input, death and positions outside maze height suspend discovery. Position/direction changes refresh visibility.
- Discovery is local navigation data. Clients compute their own mask from the replicated topology and local pose; server health and exit rules remain independent. `ReadExploration` validates the entity, maze handle and revision and returns const data for immediate painting only. Widgets never retain fragment pointers.
- `Session.bMapOpen` is local interface state, like the existing menu (split-screen is unsupported). `SetMapOpen` invokes the system transition; the controller clears only its own character input, changes focus and blocks its movement/look. Other server players remain unaffected. Zoom and pan belong to widget presentation only.
- `WBP_ExplorationMap` owns the new minimap slot. The editor module creates the missing Blueprint on startup. `UMazeExplorationMapWidget` renders discovered cells in the bottom-right corner and uses the same mask in fullscreen mode. M/Esc close, drag pans, wheel zooms, Home recenters. EndPlay removes the widget. A native layout fallback handles an absent asset.
- The existing `UMazeMinimapWidget` remains the complete DEVELOPMENT MAP, toggled with F7. Shipping/Test hide it and disable its native rendering. It does not use exploration data.

## Потолок и освещение лабиринта

- Размер клетки в `FMazeGenerationFragment` — 462,5 см при толщине стены 50 см: чистая ширина коридора уменьшена с 825 до 412,5 см. Общий масштаб сетки также уменьшает физические размеры комнат; их ширина в топологии остаётся 2–4 клетки, стартовой — 2 клетки. Геометрия, точки появления и карта используют размер клетки из ECS; новый масштаб применяется при следующей генерации.

- `FMazeGenerationSystem` формирует `CeilingTransform` в неизменяемом `FMazeGeneratedData` вместе со стенами и полом. Нижняя грань потолка совпадает с `WallHeight` (320 см), толщина равна `WallThickness`; плита закрывает весь лабиринт, включая комнаты и провалы пола, до внешних краёв стен.
- `AMazeWorld` применяет готовый transform к компоненту потолка с материалом стен и коллизией `BlockAll`. Геометрия обновляется вместе с новой ревизией лабиринта; компонент живёт и уничтожается вместе с Actor. Сервер и клиенты получают одинаковую геометрию из реплицируемого seed, отдельного изменяемого состояния потолка нет.
- Внешнее освещение, купол с текстурой неба/луны и ночная дымка больше не создаются. Освещение обеспечивают налобные фонарики; фиксированная экспозиция сохранена. Скрипт `create_night_environment.py` теперь создаёт только материал земли и его текстуры; старые ассеты неба больше не используются.
- Для применения потолка нужна сборка с закрытым редактором и повторное открытие: добавлен default subobject `Ceiling` и отражаемое поле Actor. Сборка, тесты и Play при изменении не запускались.

# ECS игровой логики

Игрок, генерация лабиринта, прохождение и состояние сессии используют Unreal Mass Entity. `UMazeECSSubsystem` создаётся отдельно для каждого игрового мира/PIE. Системы работают синхронно на игровом потоке; автоматический граф `UMassProcessor` и параллельные задачи сейчас не используются.

| Сущность | Компоненты Mass | Обработка |
| --- | --- | --- |
| Игрок | Vitals, PlayerInput, PlayerPose, Locomotion, PlayerCommand, Progress | Управление, скорость, прыжок, урон, восстановление, определение выхода |
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

## Exploration maps

- The player entity owns `FMazeExplorationFragment`: discovered cells, maze handle/revision and last pose samples. Data resets on entity destruction or maze regeneration; there is no disk persistence or shared team discovery.
- `ResolvePlayer` calls `FMazeExplorationSystem::Update` after storing the pose and resolving input availability, before movement commands. The current cell and visible cell centers within six cells and a 120-degree forward sector are discovered. Grid-edge ray traversal stops at walls and exact diagonal corners. Disabled input, death and positions outside maze height suspend discovery. Position/direction changes refresh visibility.
- Discovery is local navigation data. Clients compute their own mask from the replicated topology and local pose; server health and exit rules remain independent. `ReadExploration` validates the entity, maze handle and revision and returns const data for immediate painting only. Widgets never retain fragment pointers.
- `Session.bMapOpen` is local interface state, like the existing menu (split-screen is unsupported). `SetMapOpen` invokes the system transition; the controller clears only its own character input, changes focus and blocks its movement/look. Other server players remain unaffected. Zoom and pan belong to widget presentation only.
- `WBP_ExplorationMap` owns the new minimap slot. The editor module creates the missing Blueprint on startup. `UMazeExplorationMapWidget` renders discovered cells in the bottom-right corner and uses the same mask in fullscreen mode. M/Esc close, drag pans, wheel zooms, Home recenters. EndPlay removes the widget. A native layout fallback handles an absent asset.
- The existing `UMazeMinimapWidget` remains the complete DEVELOPMENT MAP, toggled with F7. Shipping/Test hide it and disable its native rendering. It does not use exploration data.

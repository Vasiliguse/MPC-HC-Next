# MPC-HC Next

MPC-HC Next — Windows-аудио- и видеоплеер с современной оболочкой, аппаратным воспроизведением и развитием собственного D3D11 renderer pipeline.

Проект является модифицированной и активно развиваемой версией кода Media Player Classic. Пользовательское имя проекта — **MPC-HC Next**. GPLv3, исходная информация об авторах и требования к сторонним компонентам сохраняются.

## Основные направления

- современный интерфейс MPC-HC Next / N Play;
- DirectShow playback core;
- FFmpeg;
- DXVA2 и аппаратное декодирование;
- native D3D11 renderer;
- D3D11 video processor;
- HDR10 signalling и HDR metadata;
- 10/12-bit GPU surfaces;
- ASS/SSA и Unicode subtitle infrastructure;
- D3D11 subtitle overlay;
- Windows 10/11 и x64;
- дальнейшая работа над HLG, HDR subtitle composition, scaling/shaders, dithering и полной playback validation.

## Состояние

Это development tree. Отдельные возможности находятся в разработке и не должны считаться полностью готовыми только на основании наличия соответствующего кода.

## Сборка

Используются Visual Studio и Windows SDK. Доступны Win32 и x64 конфигурации Debug/Release. Автоматическая проверка выполняется через GitHub Actions.

## Структура

- `src/apps` — приложение и интерфейс;
- `src/filters` — DirectShow filters, decoders, parsers и renderers;
- `src/SubPic` — subtitle rendering;
- `src/Subtitles` — subtitle processing;
- `src/ExtLib` — external/support libraries;
- `distrib` — installer/distribution;
- `docs` — документация.

## Лицензия и происхождение

Проект распространяется под GPLv3. MPC-HC Next является изменённой работой на базе существующего Media Player Classic codebase; upstream copyright/attribution notices не удаляются.

## Репозиторий

https://github.com/Vasiliguse/MPC-HC-Next

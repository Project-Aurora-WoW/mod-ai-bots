/*
 * mod-ai-bots — модуль AI-ботов для AzerothCore
 * Файл: src/BotConsoleAppender.h
 *
 * Консольный аппендер с честным UTF-8/Unicode: на Windows строка
 * выводится через WriteConsoleW (кириллица, «ё», символы вне CP866),
 * при перенаправленном выводе — как UTF-8 в stdout/stderr.
 */

#ifndef AI_BOTS_BOT_CONSOLE_APPENDER_H
#define AI_BOTS_BOT_CONSOLE_APPENDER_H

#include "Appender.h"
#include "LogCommon.h"
#include "LogMessage.h"
#include "StringConvert.h"
#include "Tokenize.h"
#include "Util.h"

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#endif

#include <cstdio>
#include <string>
#include <string_view>
#include <vector>

class BotConsoleAppender : public Appender
{
public:
    // Свободный слот после APPENDER_NONE/CONSOLE/FILE/DB (0..3).
    static constexpr uint8 type = 4;

    BotConsoleAppender(uint8 id, std::string const& name, LogLevel level, AppenderFlags flags,
                       std::vector<std::string_view> const& args)
        : Appender(id, name, level, flags)
    {
        // Аргументы как у штатного консольного аппендера: type, level, flags, "цвета".
        if (args.size() <= 3 || args[3].empty())
            return;

        std::vector<std::string_view> colors = Acore::Tokenize(args[3], ' ', false);
        if (colors.size() != 6)
            return;

        for (size_t i = 0; i < 6; ++i)
        {
            Optional<uint8> color = Acore::StringTo<uint8>(colors[i]);
            if (!color)
                return;
            _colors[i] = *color;
        }

        _colored = true;
    }

    AppenderType getType() const override { return static_cast<AppenderType>(type); }

private:
    void _write(LogMessage const* message) override
    {
        if (!message)
            return;

        bool const stdoutStream = !(message->level == LOG_LEVEL_ERROR || message->level == LOG_LEVEL_FATAL);
        FILE* stream = stdoutStream ? stdout : stderr;

#ifdef _WIN32
        HANDLE const handle = GetStdHandle(stdoutStream ? STD_OUTPUT_HANDLE : STD_ERROR_HANDLE);
        if (handle && handle != INVALID_HANDLE_VALUE)
        {
            std::string const line = message->prefix + message->text;
            std::wstring wide;
            if (Utf8toWStr(line, wide))
            {
                if (_colored)
                    SetColor(stdoutStream, _colors[ColorIndexForLevel(message->level)]);

                wide += L"\r\n";

                DWORD written = 0;
                if (WriteConsoleW(handle, wide.c_str(), static_cast<DWORD>(wide.size()), &written, nullptr))
                {
                    if (_colored)
                        ResetColor(stdoutStream);
                    return;
                }
            }
        }
#endif

        // Консоли нет (вывод в файл или пайп) — пишем UTF-8 как есть.
        std::fwrite(message->prefix.data(), 1, message->prefix.size(), stream);
        std::fwrite(message->text.data(), 1, message->text.size(), stream);
        std::fputc('\n', stream);
        std::fflush(stream);
    }

    static uint8 ColorIndexForLevel(LogLevel level)
    {
        switch (level)
        {
            case LOG_LEVEL_FATAL: return 0;
            case LOG_LEVEL_ERROR: return 1;
            case LOG_LEVEL_WARN:  return 2;
            case LOG_LEVEL_INFO:  return 3;
            case LOG_LEVEL_DEBUG: return 4;
            default:              return 5; // TRACE
        }
    }

#ifdef _WIN32
    static void SetColor(bool stdoutStream, uint8 color)
    {
        static WORD const ColorTable[] =
        {
            0,
            FOREGROUND_RED,
            FOREGROUND_GREEN,
            FOREGROUND_RED | FOREGROUND_GREEN,
            FOREGROUND_BLUE,
            FOREGROUND_RED | FOREGROUND_BLUE,
            FOREGROUND_GREEN | FOREGROUND_BLUE,
            FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE,
            FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_INTENSITY,
            FOREGROUND_RED | FOREGROUND_INTENSITY,
            FOREGROUND_GREEN | FOREGROUND_INTENSITY,
            FOREGROUND_BLUE | FOREGROUND_INTENSITY,
            FOREGROUND_RED | FOREGROUND_BLUE | FOREGROUND_INTENSITY,
            FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_INTENSITY,
            FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_INTENSITY
        };

        if (color < sizeof(ColorTable) / sizeof(ColorTable[0]))
            SetConsoleTextAttribute(GetStdHandle(stdoutStream ? STD_OUTPUT_HANDLE : STD_ERROR_HANDLE), ColorTable[color]);
    }

    static void ResetColor(bool stdoutStream)
    {
        SetConsoleTextAttribute(GetStdHandle(stdoutStream ? STD_OUTPUT_HANDLE : STD_ERROR_HANDLE),
            FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);
    }
#else
    static void SetColor(bool, uint8) { }
    static void ResetColor(bool) { }
#endif

    bool _colored = false;
    uint8 _colors[6] = { 0, 0, 0, 0, 0, 0 };
};

#endif // AI_BOTS_BOT_CONSOLE_APPENDER_H

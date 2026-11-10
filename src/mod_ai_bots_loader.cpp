/*
 * mod-ai-bots — AI-боты для AzerothCore
 * Файл: src/mod_ai_bots_loader.cpp
 *
 * Точка входа модуля. Ядро AzerothCore генерирует вызов
 * Addmod_ai_botsScripts() из имени папки модуля (mod-ai-bots -> mod_ai_bots),
 * поэтому имя функции менять нельзя.
 */

void AddAiBotsScripts();

// cf. naming convention: replace all '-' in the module folder name with '_'
void Addmod_ai_botsScripts()
{
    AddAiBotsScripts();
}

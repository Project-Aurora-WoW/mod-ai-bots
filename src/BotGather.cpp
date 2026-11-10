/*
 * mod-ai-bots — модуль AI-ботов для AzerothCore
 * Файл: src/BotGather.cpp
 *
 * Сбор ресурсов: бот с травничеством или горным делом находит
 * ближайшую ноду и забирает добычу. Движение ботов пока не работает,
 * поэтому к ноде бот приближается near-телепортом.
 */

#include "BotGather.h"
#include "Cell.h"
#include "CellImpl.h"
#include "GameObject.h"
#include "GridNotifiers.h"
#include "GridNotifiersImpl.h"
#include "Log.h"
#include "LootMgr.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "SharedDefines.h"

#include <algorithm>
#include <list>
#include <set>
#include <string>
#include <unordered_map>

namespace
{
    std::unordered_map<uint32, uint32> s_gatherTimers;

    bool IsOreNode(GameObject const* go)
    {
        std::string const& name = go->GetGOInfo()->name;
        return name.find("Vein") != std::string::npos || name.find("Deposit") != std::string::npos;
    }

    bool IsHerbNode(GameObject const* go)
    {
        static std::set<std::string> const herbs =
        {
            "Peacebloom", "Silverleaf", "Earthroot", "Mageroyal", "Briarthorn",
            "Bruiseweed", "Stranglekelp", "Liferoot", "Kingsblood", "Fadeleaf",
            "Goldthorn", "Khadgar's Whisker", "Wintersbite", "Firebloom",
            "Purple Lotus", "Arthas' Tears", "Sungrass", "Blindweed", "Gromsblood",
            "Ghost Mushroom", "Golden Sansam", "Dreamfoil", "Mountain Silversage",
            "Plaguebloom", "Icecap", "Black Lotus"
        };
        return herbs.find(go->GetGOInfo()->name) != herbs.end();
    }

    bool IsGatherableNode(GameObject const* go, uint32 skillId)
    {
        if (!go || go->GetGoType() != GAMEOBJECT_TYPE_CHEST)
            return false;

        if (go->GetGoState() != GO_STATE_READY)
            return false;

        GameObjectTemplate const* info = go->GetGOInfo();
        if (!info || !info->chest.lootId)
            return false;

        if (skillId == SKILL_MINING)
            return IsOreNode(go);

        if (skillId == SKILL_HERBALISM)
            return IsHerbNode(go);

        return false;
    }

    class GatherableNodeCheck
    {
    public:
        GatherableNodeCheck(WorldObject const* obj, float range, uint32 skillId)
            : _obj(obj), _range(range), _skillId(skillId) {}

        bool operator()(GameObject* go) const
        {
            return IsGatherableNode(go, _skillId) && _obj->IsWithinDistInMap(go, _range);
        }

    private:
        WorldObject const* _obj;
        float _range;
        uint32 _skillId;
    };
}

void AiBotsGather::Update(Player* bot, uint32 diff)
{
    if (!bot || !bot->IsAlive() || !bot->IsInWorld() || bot->IsInCombat())
        return;

    uint32 skillId = 0;
    if (bot->HasSkill(SKILL_HERBALISM))
        skillId = SKILL_HERBALISM;
    else if (bot->HasSkill(SKILL_MINING))
        skillId = SKILL_MINING;
    else
        return;

    uint32& timer = s_gatherTimers[bot->GetGUID().GetCounter()];
    timer += diff;
    if (timer < 15000)
        return;
    timer = 0;

    std::list<GameObject*> nodes;
    GatherableNodeCheck check(bot, 200.0f, skillId);
    Acore::GameObjectListSearcher<GatherableNodeCheck> searcher(bot, nodes, check);
    Cell::VisitObjects(bot, searcher, 200.0f);

    if (nodes.empty())
        return;

    GameObject* node = nullptr;
    for (GameObject* candidate : nodes)
        if (!node || bot->GetDistance(candidate) < bot->GetDistance(node))
            node = candidate;

    if (!node)
        return;

    // Движение ботов пока не работает — подходим вплотную телепортом.
    if (bot->GetDistance(node) > 5.0f)
    {
        Position dest(node->GetPositionX() + 1.5f, node->GetPositionY() + 1.5f,
                      node->GetPositionZ(), bot->GetOrientation());
        bot->NearTeleportTo(dest.GetPositionX(), dest.GetPositionY(), dest.GetPositionZ(), dest.GetOrientation());
        bot->SetSemaphoreTeleportNear(0);
        bot->UpdatePosition(dest, true);
    }

    Loot loot;
    loot.FillLoot(node->GetGOInfo()->chest.lootId, LootTemplates_Gameobject, bot, false, true,
                  LOOT_MODE_DEFAULT, node);

    uint32 gathered = 0;
    for (LootItem& li : loot.items)
    {
        if (li.is_looted || !li.itemid)
            continue;

        uint32 count = std::max<uint32>(li.count, 1);
        if (!sObjectMgr->GetItemTemplate(li.itemid))
            continue;

        if (!bot->AddItem(li.itemid, count))
            continue;

        li.is_looted = true;
        if (loot.unlootedCount > 0)
            --loot.unlootedCount;
        ++gathered;
    }

    node->SetLootState(GO_JUST_DEACTIVATED);

    uint32 skillValue = bot->GetSkillValue(skillId);
    if (skillValue)
        bot->UpdateGatherSkill(skillId, skillValue, 1);

    if (gathered)
        LOG_INFO("server.loading", ">> mod-ai-bots: {} собрал ресурсы с ноды {} ({}) — {} шт.",
                 bot->GetName(), node->GetEntry(), node->GetGOInfo()->name, gathered);
}

#include "../pchdef.h"
#include "playerbot.h"
#include "PlayerbotFactory.h"
#include "../SQLStorages.h"
#include "../ItemPrototype.h"
#include "PlayerbotAIConfig.h"
#include "../AccountMgr.h"

using namespace ai;
using namespace std;

void PlayerbotFactory::Randomize()
{
    bot->SetLevel(level);
    bot->SetFlag(PLAYER_FLAGS, PLAYER_FLAGS_HIDE_HELM);
    bot->SetFlag(PLAYER_FLAGS, PLAYER_FLAGS_HIDE_CLOAK);

    InitTalents();
    InitQuests();
    InitSpells();
    InitSkills();
    InitEquipment();
    InitPet();
}

void PlayerbotFactory::RandomizeForZone(uint32 mapId)
{
    QueryResult *results = WorldDatabase.PQuery("SELECT avg(t.minlevel) minlevel, avg(t.maxlevel) maxlevel FROM creature c "
            "inner join creature_template t on c.guid = t.entry "
            "where c.map = '%u'", mapId);
    if (results)
    {
        Field* fields = results->Fetch();
        uint32 minLevel = fields[0].GetUInt32();
        uint32 maxLevel = fields[1].GetUInt32();
        this->level = urand(minLevel, maxLevel);
        delete results;
    }

    Randomize();
}

void PlayerbotFactory::InitPet()
{
    Pet* pet = bot->GetPet();
    if (!pet)
        return;

    for (PetSpellMap::const_iterator itr = pet->m_spells.begin(); itr != pet->m_spells.end(); ++itr)
    {
        if(itr->second.state == PETSPELL_REMOVED)
            continue;

        uint32 spellId = itr->first;
        if(IsPassiveSpell(spellId))
            continue;

        pet->ToggleAutocast(spellId, true);
    }
}

void PlayerbotFactory::InitSpells()
{
    list<uint32> spells;
    for(PlayerSpellMap::iterator itr = bot->GetSpellMap().begin(); itr != bot->GetSpellMap().end(); ++itr)
    {
        uint32 spellId = itr->first;
        if(itr->second.state == PLAYERSPELL_REMOVED || itr->second.disabled || IsPassiveSpell(spellId))
            continue;

        spells.push_back(spellId);
    }

    for (list<uint32>::iterator i = spells.begin(); i != spells.end(); ++i)
    {
        bot->removeSpell(*i);
    }

    for (int i = 0; i < 15; i++)
        InitAvailableSpells();
}

void PlayerbotFactory::InitTalents()
{
    bot->resetTalents(true, true);
    uint32 specNo = urand(0, 2);
    for (int i = 0; i < 5; i++)
        InitTalents(specNo);
}

void PlayerbotFactory::InitEquipment()
{
    for(uint8 slot = 0; slot < EQUIPMENT_SLOT_END; ++slot)
    {
        if (slot == EQUIPMENT_SLOT_TABARD || slot == EQUIPMENT_SLOT_BODY)
            continue;

        for (uint32 desiredQuality = sPlayerbotAIConfig.randomGearQuality; desiredQuality >= ITEM_QUALITY_NORMAL; desiredQuality--)
        {
            if (EquipItem(slot, desiredQuality))
                break;
        }
    }
}

bool PlayerbotFactory::EquipItem(uint8 slot, uint32 desiredQuality)
{
    vector<uint32> items;
    for (uint32 itemId = 0; itemId < sItemStorage.MaxEntry; ++itemId)
    {
        ItemPrototype const* proto = sObjectMgr.GetItemPrototype(itemId);
        if (!proto)
            continue;

        if (proto->Class != ITEM_CLASS_WEAPON &&
            proto->Class != ITEM_CLASS_ARMOR &&
            proto->Class != ITEM_CLASS_CONTAINER &&
            proto->Class != ITEM_CLASS_PROJECTILE)
            continue;

        if (proto->Duration & 0x80000000)
            continue;

        if (proto->Quality != desiredQuality)
            continue;

        if (proto->Class == ITEM_CLASS_ARMOR && (
            slot == EQUIPMENT_SLOT_HEAD ||
            slot == EQUIPMENT_SLOT_SHOULDERS ||
            slot == EQUIPMENT_SLOT_CHEST ||
            slot == EQUIPMENT_SLOT_WAIST ||
            slot == EQUIPMENT_SLOT_LEGS ||
            slot == EQUIPMENT_SLOT_FEET ||
            slot == EQUIPMENT_SLOT_WRISTS ||
            slot == EQUIPMENT_SLOT_HANDS))
        {
            if (bot->HasSkill(SKILL_PLATE_MAIL))
            {
                if (proto->SubClass != ITEM_SUBCLASS_ARMOR_PLATE)
                    continue;
            }
            else if (bot->HasSkill(SKILL_MAIL))
            {
                if (proto->SubClass != ITEM_SUBCLASS_ARMOR_MAIL)
                    continue;
            }
            else if (bot->HasSkill(SKILL_LEATHER))
            {
                if (proto->SubClass != ITEM_SUBCLASS_ARMOR_LEATHER)
                    continue;
            }
        }

        uint32 requiredLevel = proto->RequiredLevel;
        if (!requiredLevel || (requiredLevel > level || requiredLevel < level - 5))
            continue;

        if (proto->Quality > ITEM_QUALITY_NORMAL)
        {
            uint8 sp = 0, ap = 0, tank = 0;
            for (int j = 0; j < MAX_ITEM_PROTO_STATS; ++j)
            {
                // for ItemStatValue != 0
                if(!proto->ItemStat[j].ItemStatValue)
                    continue;

                switch (proto->ItemStat[j].ItemStatType)
                {
                case ITEM_MOD_MANA:
                case ITEM_MOD_INTELLECT:
                case ITEM_MOD_SPIRIT:
                case ITEM_MOD_MANA_REGENERATION:
                case ITEM_MOD_SPELL_POWER:
                case ITEM_MOD_SPELL_PENETRATION:
                    sp++;
                    break;

                case ITEM_MOD_HEALTH:
                case ITEM_MOD_STAMINA:
                case ITEM_MOD_HEALTH_REGEN:
                case ITEM_MOD_DEFENSE_SKILL_RATING:
                case ITEM_MOD_DODGE_RATING:
                case ITEM_MOD_PARRY_RATING:
                case ITEM_MOD_BLOCK_RATING:
                case ITEM_MOD_HIT_TAKEN_MELEE_RATING:
                case ITEM_MOD_HIT_TAKEN_RANGED_RATING:
                case ITEM_MOD_HIT_TAKEN_SPELL_RATING:
                case ITEM_MOD_CRIT_TAKEN_MELEE_RATING:
                case ITEM_MOD_CRIT_TAKEN_RANGED_RATING:
                case ITEM_MOD_CRIT_TAKEN_SPELL_RATING:
                case ITEM_MOD_HIT_TAKEN_RATING:
                case ITEM_MOD_CRIT_TAKEN_RATING:
                case ITEM_MOD_RESILIENCE_RATING:
                case ITEM_MOD_BLOCK_VALUE:
                    tank++;
                    break;

                case ITEM_MOD_AGILITY:
                case ITEM_MOD_STRENGTH:
                case ITEM_MOD_HIT_MELEE_RATING:
                case ITEM_MOD_HIT_RANGED_RATING:
                case ITEM_MOD_HIT_SPELL_RATING:
                case ITEM_MOD_CRIT_MELEE_RATING:
                case ITEM_MOD_CRIT_RANGED_RATING:
                case ITEM_MOD_CRIT_SPELL_RATING:
                case ITEM_MOD_HASTE_MELEE_RATING:
                case ITEM_MOD_HASTE_RANGED_RATING:
                case ITEM_MOD_HASTE_SPELL_RATING:
                case ITEM_MOD_HIT_RATING:
                case ITEM_MOD_CRIT_RATING:
                case ITEM_MOD_HASTE_RATING:
                case ITEM_MOD_EXPERTISE_RATING:
                case ITEM_MOD_ATTACK_POWER:
                case ITEM_MOD_RANGED_ATTACK_POWER:
                case ITEM_MOD_ARMOR_PENETRATION_RATING:
                    ap++;
                    break;
                }
            }


            switch (bot->getClass())
            {
            case CLASS_PRIEST:
            case CLASS_MAGE:
            case CLASS_WARLOCK:
                if (!sp || sp < ap)
                    continue;
                break;
            case CLASS_PALADIN:
            case CLASS_WARRIOR:
                if (!tank || tank < sp)
                    continue;
                break;
            case CLASS_HUNTER:
            case CLASS_ROGUE:
            case CLASS_DRUID:
                if (!ap || ap < sp)
                    continue;
                break;
            }
        }

        if (proto->Class == ITEM_CLASS_WEAPON)
        {
            switch (bot->getClass())
            {
            case CLASS_PRIEST:
                if (proto->SubClass != ITEM_SUBCLASS_WEAPON_STAFF &&
                        proto->SubClass != ITEM_SUBCLASS_WEAPON_WAND &&
                        proto->SubClass != ITEM_SUBCLASS_WEAPON_MACE)
                    continue;
                break;
            case CLASS_MAGE:
            case CLASS_WARLOCK:
                if (proto->SubClass != ITEM_SUBCLASS_WEAPON_STAFF &&
                        proto->SubClass != ITEM_SUBCLASS_WEAPON_WAND &&
                        proto->SubClass != ITEM_SUBCLASS_WEAPON_SWORD)
                    continue;
                break;
            case CLASS_WARRIOR:
            case CLASS_PALADIN:
                if (proto->SubClass != ITEM_SUBCLASS_WEAPON_MACE2 &&
                        proto->SubClass != ITEM_SUBCLASS_WEAPON_SWORD2 &&
                        proto->SubClass != ITEM_SUBCLASS_WEAPON_MACE &&
                        proto->SubClass != ITEM_SUBCLASS_WEAPON_SWORD)
                    continue;
                break;
            case CLASS_SHAMAN:
                if (proto->SubClass != ITEM_SUBCLASS_WEAPON_MACE)
                    continue;
                break;
            case CLASS_DRUID:
                if (proto->SubClass != ITEM_SUBCLASS_WEAPON_MACE2 &&
                        proto->SubClass != ITEM_SUBCLASS_WEAPON_STAFF)
                    continue;
                break;
            case CLASS_HUNTER:
                if (proto->SubClass != ITEM_SUBCLASS_WEAPON_AXE2 &&
                        proto->SubClass != ITEM_SUBCLASS_WEAPON_SWORD2)
                    continue;
                break;
            case CLASS_ROGUE:
                if (proto->SubClass != ITEM_SUBCLASS_WEAPON_DAGGER)
                    continue;
                break;
            }
        }

        uint16 dest = 0;
        if (CanEquipUnseenItem(slot, dest, itemId))
            items.push_back(itemId);
    }

    if (items.empty())
        return false;

    for (int attempts = 0; attempts < 100; attempts++)
    {
        uint32 index = urand(0, items.size() - 1);
        uint32 newItemId = items[index];

        Item* const oldItem = bot->GetItemByPos(INVENTORY_SLOT_BAG_0, slot);
        if (oldItem)
        {
            bot->ConvertItem(oldItem, newItemId);
            return true;
        }
        else
        {
            uint16 dest;
            if (!CanEquipUnseenItem(slot, dest, newItemId))
                continue;

            Item* newItem = bot->EquipNewItem(dest, newItemId, true);
            if (newItem)
            {
                bot->AutoUnequipOffhandIfNeed();
                return true;
            }
        }
    }
    return false;
}

bool PlayerbotFactory::CanEquipUnseenItem(uint8 slot, uint16 &dest, uint32 item)
{
    dest = 0;
    Item *pItem = Item::CreateItem(item, 1, bot);
    if (pItem)
    {
        InventoryResult result = bot->CanEquipItem(slot, dest, pItem, true );
        pItem->RemoveFromUpdateQueueOf(bot);
        delete pItem;
        return result == EQUIP_ERR_OK;
    }

    return false;
}


void PlayerbotFactory::InitSkills()
{
    uint32 maxValue = level * 5;
    SetRandomSkill(SKILL_DEFENSE);
    SetRandomSkill(SKILL_SWORDS);
    SetRandomSkill(SKILL_AXES);
    SetRandomSkill(SKILL_BOWS);
    SetRandomSkill(SKILL_GUNS);
    SetRandomSkill(SKILL_MACES);
    SetRandomSkill(SKILL_2H_SWORDS);
    SetRandomSkill(SKILL_STAVES);
    SetRandomSkill(SKILL_2H_MACES);
    SetRandomSkill(SKILL_2H_AXES);
    SetRandomSkill(SKILL_DAGGERS);
    SetRandomSkill(SKILL_THROWN);
    SetRandomSkill(SKILL_CROSSBOWS);
    SetRandomSkill(SKILL_WANDS);
    SetRandomSkill(SKILL_POLEARMS);
    SetRandomSkill(SKILL_FIST_WEAPONS);

    if (bot->getLevel() >= 70)
        bot->SetSkill(SKILL_RIDING, 300, 300);
    else if (bot->getLevel() >= 60)
        bot->SetSkill(SKILL_RIDING, 225, 225);
    else if (bot->getLevel() >= 40)
        bot->SetSkill(SKILL_RIDING, 150, 150);
    else if (bot->getLevel() >= 20)
        bot->SetSkill(SKILL_RIDING, 75, 75);
    else
        bot->SetSkill(SKILL_RIDING, 0, 0);

    uint32 skillLevel = bot->getLevel() < 40 ? 0 : 1;
    switch (bot->getClass())
    {
    case CLASS_DEATH_KNIGHT:
    case CLASS_WARRIOR:
    case CLASS_PALADIN:
        bot->SetSkill(SKILL_PLATE_MAIL, skillLevel, skillLevel);
        break;
    case CLASS_SHAMAN:
    case CLASS_HUNTER:
        bot->SetSkill(SKILL_MAIL, skillLevel, skillLevel);
    }
}

void PlayerbotFactory::SetRandomSkill(uint16 id)
{
    uint32 maxValue = level * 5;
    uint32 curValue = urand(maxValue - level, maxValue);
    bot->SetSkill(id, curValue, maxValue);

}

void PlayerbotFactory::InitAvailableSpells()
{
    bot->learnDefaultSpells();

    for (uint32 id = 0; id < sCreatureStorage.MaxEntry; ++id)
    {
        CreatureInfo const* co = sCreatureStorage.LookupEntry<CreatureInfo>(id);
        if (!co ||co->trainer_type != TRAINER_TYPE_CLASS || co->trainer_class != bot->getClass())
            continue;

		TrainerSpellData const* trainer_spells = sObjectMgr.GetNpcTrainerTemplateSpells(co->trainerId);
        if (!trainer_spells)
            continue;

        for (TrainerSpellMap::const_iterator itr =  trainer_spells->spellList.begin(); itr !=  trainer_spells->spellList.end(); ++itr)
        {
            TrainerSpell const* tSpell = &itr->second;

            if (!tSpell)
                continue;

            uint32 reqLevel = 0;
            if (!tSpell->learnedSpell && !bot->IsSpellFitByClassAndRace(tSpell->learnedSpell, &reqLevel))
                continue;

            reqLevel = tSpell->isProvidedReqLevel ? tSpell->reqLevel : std::max(reqLevel, tSpell->reqLevel);
            TrainerSpellState state = bot->GetTrainerSpellState(tSpell, reqLevel);
            if (state != TRAINER_SPELL_GREEN)
                continue;

            if (tSpell->learnedSpell)
                bot->learnSpell(tSpell->learnedSpell, false);
        }
    }
}

void PlayerbotFactory::InitTalents(uint32 specNo)
{
    uint32 classMask = bot->getClassMask();

    uint32 lastRow = 999;
    for (uint32 i = 0; i < sTalentStore.GetNumRows() && bot->GetFreeTalentPoints(); ++i)
    {
        TalentEntry const *talentInfo = sTalentStore.LookupEntry(i);
        if(!talentInfo)
            continue;

        TalentTabEntry const *talentTabInfo = sTalentTabStore.LookupEntry( talentInfo->TalentTab );
        if(!talentTabInfo || talentTabInfo->tabpage != specNo)
            continue;

        if( (classMask & talentTabInfo->ClassMask) == 0 )
            continue;

        // search highest talent rank
        uint32 spellid = 0;

        for(int rank = MAX_TALENT_RANK-1; rank >= 0; --rank)
        {
            if(talentInfo->RankID[rank]!=0)
            {
                spellid = talentInfo->RankID[rank];
                break;
            }
        }

        if(!spellid)                                        // ??? none spells in talent
            continue;

        if (talentInfo->Row == lastRow)
            continue;

        SpellEntry const* spellInfo = sSpellStore.LookupEntry(spellid);
        if(!spellInfo || !SpellMgr::IsSpellValid(spellInfo, bot, false))
            continue;

        if (bot->HasSpell(spellid))
            continue;

        bot->learnSpellHighRank(spellid);
        lastRow = talentInfo->Row;
    }
}

ObjectGuid PlayerbotFactory::GetRandomBot()
{
    vector<ObjectGuid> guids;
    for (list<uint32>::iterator i = sPlayerbotAIConfig.randomBotAccounts.begin(); i != sPlayerbotAIConfig.randomBotAccounts.end(); i++)
    {
        uint32 accountId = *i;
        if (!sAccountMgr.GetCharactersCount(accountId))
            continue;

        QueryResult *result = CharacterDatabase.PQuery("SELECT guid FROM characters WHERE account = '%u'", accountId);
        if (!result)
            continue;

        do
        {
            Field* fields = result->Fetch();
            ObjectGuid guid = ObjectGuid(fields[0].GetUInt64());
            if (!sObjectMgr.GetPlayer(guid))
                guids.push_back(guid);
        } while (result->NextRow());

        delete result;
    }

    if (guids.empty())
        return ObjectGuid();

    int index = urand(0, guids.size() - 1);
    return guids[index];
}

void PlayerbotFactory::InitQuests()
{
    QueryResult *results = WorldDatabase.PQuery("SELECT entry FROM udb.quest_template where QuestLevel = -1 and RequiredClasses = '%u' and MinLevel <= '%u'",
            bot->getClass(), bot->getLevel());
    if (!results)
        return;

    do
    {
        Field* fields = results->Fetch();
        uint32 questId = fields[0].GetUInt32();
        Quest const *quest = sObjectMgr.GetQuestTemplate(questId);

        if (bot->GetQuestStatus(questId) == QUEST_STATUS_COMPLETE)
            continue;

        bot->SetQuestStatus(questId, QUEST_STATUS_NONE);

        if (!bot->SatisfyQuestClass(quest, false) ||
                !bot->SatisfyQuestRace(quest, false) ||
                !bot->SatisfyQuestStatus(quest, false))
            continue;

        bot->SetQuestStatus(questId, QUEST_STATUS_COMPLETE);

    } while (results->NextRow());

    delete results;
}
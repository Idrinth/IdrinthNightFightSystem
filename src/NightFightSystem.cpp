#include "PCH.h"
#include "logger.h"
#include "ryml/ryml.hpp"
#include "c4/substr.hpp"
#include "c4/std/string.hpp"
#include <utility>
#include <map>

std::map<std::string, spdlog::level::level_enum> logLevelMap = {
    {"trace", spdlog::level::level_enum::trace},
    {"debug", spdlog::level::level_enum::debug},
    {"info", spdlog::level::level_enum::info},
    {"warn", spdlog::level::level_enum::warn},
    {"warning", spdlog::level::level_enum::warn},
    {"err", spdlog::level::level_enum::err},
    {"error", spdlog::level::level_enum::err},
    {"critical", spdlog::level::level_enum::critical},
    {"crit", spdlog::level::level_enum::critical},
    {"off", spdlog::level::level_enum::off},
};
std::map<spdlog::level::level_enum, std::string> logLevelList = {
    {spdlog::level::level_enum::trace, "trace"},
    {spdlog::level::level_enum::debug, "debug"},
    {spdlog::level::level_enum::info, "info"},
    {spdlog::level::level_enum::warn, "warning"},
    {spdlog::level::level_enum::err, "error"},
    {spdlog::level::level_enum::critical, "critical"},
    {spdlog::level::level_enum::off, "off"}
};

std::string yamlNodeToString(const c4::yml::NodeRef& node) {
    auto buf = node.val();
    return std::string{buf.data(), buf.size()};
}
std::string yamlNodeToString(const c4::yml::ConstNodeRef& node) {
    auto buf = node.val();
    return std::string{buf.data(), buf.size()};
}

class Config {
    public:
        [[nodiscard]] spdlog::level::level_enum getLogLevel() const {
            return logLevel;
        }
        static Config getSingleton() noexcept {
            static Config instance;

            static std::atomic_bool initialized;
            if (!initialized.exchange(true)) {
                std::ifstream inputFile(R"(Data\SKSE\Plugins\IdrinthNightFightSystem.yaml)", std::ios::in);
                if (!inputFile.good()) {
                    return instance;
                }
                std::string data;
                for (std::string line; std::getline(inputFile, line); ) {
                    data.append(line);
                    data.append("\n");
                }
                inputFile.close();

                c4::substr sus = c4::to_substr(data);
                c4::yml::Tree tree = ryml::parse_in_place(sus);
                if (const c4::yml::NodeRef node = tree["logLevel"]; !node.invalid() && !node.val_is_null()) {
                    if (const std::string out = yamlNodeToString(node); logLevelMap.contains(out)) {
                        instance.logLevel = logLevelMap[out];
                    }
                }
            }

            return instance;
        }
    private:
        spdlog::level::level_enum logLevel = spdlog::level::level_enum::err;
};
struct CalculateDamage {
    static float work(RE::HitData* hitData) {
        if (!hitData) {
            logger::trace("No hit data.");
            return 1.0f;
        }
        const auto actorT = hitData->target.get();
        const auto actorS = hitData->aggressor.get();
        if (!actorS || !actorT) {
            logger::trace("Actors not existing.");
            return 1.0f;
        }
        const auto processT = actorT->GetHighProcess();
        const auto processS = actorS->GetHighProcess();
        float targetLightLevel = 100;
        float sourceLightLevel = 100;
        if (processT) {
            targetLightLevel = processT->lightLevel;
        }
        if (processS) {
            sourceLightLevel = processS->lightLevel;
        }
        float deltaLightLevel = targetLightLevel - sourceLightLevel;
        float factorRanged = 1;
        float factorMelee = 1;
        logger::debug("Light: {} {} {}", targetLightLevel, sourceLightLevel, deltaLightLevel);
        if (targetLightLevel >= 100 && deltaLightLevel >= 50) {
            factorMelee = 0.8f;
            factorRanged = 0.7f;
        } else if (targetLightLevel >= 100 && deltaLightLevel >= 25) {
            factorMelee = 0.9f;
            factorRanged = 0.8f;
        } else if (targetLightLevel == 0) {
            factorMelee = 0.7f;
            factorRanged = 0.5f;
        } else if (targetLightLevel < 25) {
            factorMelee = 0.85f;
            factorRanged = 0.7f;
        } else if (targetLightLevel < 50) {
            factorMelee = 0.95f;
            factorRanged = 0.85f;
        } else if (targetLightLevel < 75) {
            factorRanged = 0.95f;
        }
        logger::debug("Melee: {}, Ranged: {}", factorMelee, factorRanged);
        if (hitData->weapon) {
            logger::trace("Weapon attack");
            if (hitData->weapon->IsMelee()) {
                return factorMelee;
            }
            return factorRanged;
        }
        if (hitData->attackDataSpell) {
            logger::trace("Spell attack");
            bool hasArea = false;
            for (const auto effect : hitData->attackDataSpell->effects) {
                if (effect->GetArea() > 0) {
                    hasArea = true;
                }
            }
            if (hitData->attackDataSpell->data.delivery == RE::MagicSystem::Delivery::kSelf) {
                return 1.0f;
            }
            if (hitData->attackDataSpell->data.delivery == RE::MagicSystem::Delivery::kTouch) {
                if (hasArea) {
                    return (1 + factorMelee) / 2;
                }
                return factorMelee;
            }
            if (hitData->attackDataSpell->data.delivery == RE::MagicSystem::Delivery::kTargetActor) {
                if (hasArea) {
                    return (1 + factorRanged) / 2;
                }
                return factorRanged;
            }
            if (hitData->attackDataSpell->data.delivery == RE::MagicSystem::Delivery::kAimed) {
                return factorRanged;
            }
            return (1 + factorRanged) / 2;
        }
        return 1.0f;
    }
    static void thunk(RE::HitData* hitData) {
        logger::trace("Got hit data");
        func(hitData);
        const float factor = work(hitData);
        logger::debug("Factor: {}", factor);
        if (factor == 1.0f) {
            logger::trace("Nothing to adjust");
            return;
        }
        logger::trace("Adjusting damages");
        hitData->totalDamage *= factor;
        hitData->physicalDamage *= factor;
        hitData->resistedTypedDamage *= factor;
        hitData->resistedPhysicalDamage *= factor;
        hitData->reflectedDamage *= factor;
        logger::trace("Adjusted damages");
    }
    static inline REL::Relocation<decltype(thunk)> func;
};
extern "C" void CalculateDamage_RealThunk(RE::HitData* hitData) {
    CalculateDamage::thunk(hitData);
}

extern "C" void CalculateDamage_ThunkStub(RE::HitData* hitData);

SKSEPluginLoad(const SKSE::LoadInterface* skse) {
    SKSE::Init(skse);

    const auto &config = Config::getSingleton();
    SetupLogger(config.getLogLevel());

    logger::info("Setup with LogLevel {}", logLevelList[config.getLogLevel()]);

    SKSE::AllocTrampoline(14);
    const REL::Relocation<std::uintptr_t> target{ RELOCATION_ID(42842, 44001) };//Damage Calculations
    auto& trampoline = SKSE::GetTrampoline();
    CalculateDamage::func = trampoline.write_call<5>(
        target.address() + 0x358,
        reinterpret_cast<std::uintptr_t>(CalculateDamage_ThunkStub)
    );
    return true;
}
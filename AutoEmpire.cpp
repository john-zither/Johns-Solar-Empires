#include "stdafx.h"
#include "AutoEmpire.h"

using namespace Simulator;

AutoEmpire* AutoEmpire::sInstance = nullptr;
constexpr float AutoEmpire::kTerraformStep;
constexpr float AutoEmpire::kColonizeSearchRadius;

AutoEmpire::AutoEmpire()
    : mIncomeAccum(0)
    , mTerraAccum(0)
    , mColonizeAccum(0)
    , mDevelopAccum(0)
{}

void AutoEmpire::Initialize() { sInstance = this; }
void AutoEmpire::Dispose()    { sInstance = nullptr; }

bool AutoEmpire::Write(ISerializerStream*)  { return true; }
bool AutoEmpire::Read(ISerializerStream*)   { return true; }
bool AutoEmpire::WriteToXML(XmlSerializer*) { return false; }

AutoEmpire* AutoEmpire::Get() { return sInstance; }

// ── Main loop ─────────────────────────────────────────────────────────────────

void AutoEmpire::Update(int deltaTime, int /*deltaGameTime*/)
{
    if (!IsSpaceGame()) return;
    cEmpire* empire = GetPlayerEmpire();
    if (!empire) return;

    TickIncome(deltaTime);
    TickTerraform(deltaTime);
    TickColonize(deltaTime);
    TickDevelop(deltaTime);
}

// ── Income ────────────────────────────────────────────────────────────────────
//
// Income per colony = spice_production * kSpiceIncomeRate.
// Planets whose assigned spice is flagged "rare" in SpaceTrading earn a 3x bonus.

void AutoEmpire::TickIncome(int deltaTime)
{
    mIncomeAccum += deltaTime;
    if (mIncomeAccum < kIncomeInterval) return;
    mIncomeAccum -= kIncomeInterval;

    cEmpire* empire = GetPlayerEmpire();
    if (!empire) return;

    int totalIncome = 0;

    for (auto& starPtr : empire->mStars)
    {
        auto& planets = starPtr->GetPlanetRecords();
        for (auto& planetPtr : planets)
        {
            cPlanetRecord* planet = planetPtr.get();
            if (planet->mTechLevel < TechLevel::Empire) continue;

            int production = cPlanetRecord::CalculateSpiceProduction(planet);
            if (production <= 0) continue;

            int income = production * kSpiceIncomeRate;

            // Planets assigned a rare-category spice earn a bonus.
            if (planet->mSpiceGen.instanceID != 0 && SpaceTrading.IsRare(planet->mSpiceGen))
                income *= kRareSpiceMultiplier;

            totalIncome += income;
        }
    }

    if (totalIncome > 0)
    {
        empire->mEmpireMoney += totalIncome;
        App::ConsolePrintF("AutoEmpire: +%d Sporebucks from colony spice production", totalIncome);
    }
}

// ── Terraforming ──────────────────────────────────────────────────────────────
//
// For each player-owned uncolonized planet: nudge atmosphere and temperature
// toward 0 (ideal), and seed plants/animals from any planet in the same system
// that already has an ecosystem. Falls back to other player-owned planets if the
// local system has no species to donate.

void AutoEmpire::TickTerraform(int deltaTime)
{
    mTerraAccum += deltaTime;
    if (mTerraAccum < kTerraInterval) return;
    mTerraAccum -= kTerraInterval;

    cEmpire* empire = GetPlayerEmpire();
    if (!empire) return;

    int terraformed = 0;

    for (auto& starPtr : empire->mStars)
    {
        auto& planets = starPtr->GetPlanetRecords();
        for (auto& planetPtr : planets)
        {
            cPlanetRecord* planet = planetPtr.get();
            if (planet->IsMoon() || planet->IsDestroyed())          continue;
            if (planet->mTechLevel >= TechLevel::Empire)            continue; // already colonised

            // ── Physical scores ──────────────────────────────────────────────
            float& atm = planet->mAtmosphereScore;
            if      (atm >  kTerraformStep) atm -= kTerraformStep;
            else if (atm < -kTerraformStep) atm += kTerraformStep;
            else                             atm  = 0.0f;

            float& tmp = planet->mTemperatureScore;
            if      (tmp >  kTerraformStep) tmp -= kTerraformStep;
            else if (tmp < -kTerraformStep) tmp += kTerraformStep;
            else                             tmp  = 0.0f;

            // ── Ecosystem seeding ────────────────────────────────────────────
            // First look at other planets in the same star system for donors.
            bool needPlants  = (int)planet->mPlantSpecies.size()  < kMaxPlantSlots;
            bool needAnimals = (int)planet->mAnimalSpecies.size() < kMaxAnimalSlots;

            if (needPlants || needAnimals)
            {
                cStarRecord* star = planet->GetStarRecord();
                if (star)
                {
                    for (auto& srcPtr : star->GetPlanetRecords())
                    {
                        cPlanetRecord* src = srcPtr.get();
                        if (src == planet) continue;

                        if (needPlants && !src->mPlantSpecies.empty())
                        {
                            planet->mPlantSpecies.push_back(src->mPlantSpecies[0]);
                            needPlants = ((int)planet->mPlantSpecies.size() < kMaxPlantSlots);
                        }
                        if (needAnimals && !src->mAnimalSpecies.empty())
                        {
                            planet->mAnimalSpecies.push_back(src->mAnimalSpecies[0]);
                            needAnimals = ((int)planet->mAnimalSpecies.size() < kMaxAnimalSlots);
                        }
                    }
                }
            }

            // If the system had no donors, borrow one species from any player planet.
            if (needPlants || needAnimals)
            {
                for (auto& srcStarPtr : empire->mStars)
                {
                    for (auto& srcPlanetPtr : srcStarPtr->GetPlanetRecords())
                    {
                        cPlanetRecord* src = srcPlanetPtr.get();
                        if (src == planet) continue;

                        if (needPlants && !src->mPlantSpecies.empty())
                        {
                            planet->mPlantSpecies.push_back(src->mPlantSpecies[0]);
                            needPlants = false;
                        }
                        if (needAnimals && !src->mAnimalSpecies.empty())
                        {
                            planet->mAnimalSpecies.push_back(src->mAnimalSpecies[0]);
                            needAnimals = false;
                        }
                        if (!needPlants && !needAnimals) goto eco_done;
                    }
                }
                eco_done:;
            }

            ++terraformed;
        }
    }

    if (terraformed > 0)
        App::ConsolePrintF("AutoEmpire: Terraformed %d planets", terraformed);
}

// ── Colonisation ──────────────────────────────────────────────────────────────
//
// Priority 1: uncolonised planets in already-owned star systems (no T-score filter,
//             no AddStarOwnership needed since the star is already ours).
// Priority 2: nearest unclaimed star within kColonizeSearchRadius that has any
//             habitable (non-moon, non-destroyed, TechLevel <= Creature) planet.

void AutoEmpire::TickColonize(int deltaTime)
{
    mColonizeAccum += deltaTime;
    if (mColonizeAccum < kColonizeInterval) return;
    mColonizeAccum -= kColonizeInterval;

    cEmpire* empire = GetPlayerEmpire();
    if (!empire) return;
    if (empire->mEmpireMoney < kColonizeCost) return;

    // ── Priority 1: in-system planet ─────────────────────────────────────────
    for (auto& starPtr : empire->mStars)
    {
        for (auto& planetPtr : starPtr->GetPlanetRecords())
        {
            cPlanetRecord* planet = planetPtr.get();
            if (planet->IsMoon() || planet->IsDestroyed())  continue;
            if (planet->mTechLevel > TechLevel::Creature)   continue; // don't displace existing civs

            cPlanetRecord::FillPlanetDataForTechLevel(planet, TechLevel::Empire);
            SpaceTrading.AssignPlanetSpice(planet);
            empire->mEmpireMoney -= kColonizeCost;
            App::ConsolePrintF(
                "AutoEmpire: Colonized in-system world. Treasury: %d Sporebucks",
                empire->mEmpireMoney);
            return;
        }
    }

    // ── Priority 2: nearest unclaimed star within search radius ───────────────
    cStarRecord* homeStar = empire->GetHomeStarRecord();
    if (!homeStar) return;

    StarRequestFilter filter;
    filter.starTypes   = 0x1FFF;
    filter.techLevels  = 0x3F;
    filter.maxDistance = kColonizeSearchRadius;

    eastl::vector<cStarRecordPtr> candidates;
    StarManager.FindStars(homeStar->mPosition, filter, candidates);

    cStarRecord*   bestStar   = nullptr;
    cPlanetRecord* bestPlanet = nullptr;
    float          bestDist   = 1e10f;

    for (auto& starPtr : candidates)
    {
        cStarRecord* star = starPtr.get();
        if (star->mEmpireID != static_cast<uint32_t>(-1)) continue; // already owned

        for (auto& planetPtr : star->GetPlanetRecords())
        {
            cPlanetRecord* planet = planetPtr.get();
            if (planet->IsMoon() || planet->IsDestroyed()) continue;
            if (planet->mTechLevel > TechLevel::Creature)  continue;

            float dist = (star->mPosition - homeStar->mPosition).Length();
            if (dist < bestDist)
            {
                bestDist   = dist;
                bestStar   = star;
                bestPlanet = planet;
            }
        }
    }

    if (bestStar && bestPlanet)
    {
        empire->AddStarOwnership(bestStar);
        cPlanetRecord::FillPlanetDataForTechLevel(bestPlanet, TechLevel::Empire);
        SpaceTrading.AssignPlanetSpice(bestPlanet);
        empire->mEmpireMoney -= kColonizeCost;
        App::ConsolePrintF(
            "AutoEmpire: Colonized new system! Treasury: %d Sporebucks",
            empire->mEmpireMoney);
    }
}

// ── Colony development ────────────────────────────────────────────────────────
//
// If a colony has no city data yet (edge case), initialize it once.
// Otherwise, grow each city one size step toward its maximum. This avoids
// calling FillPlanetDataForTechLevel on existing colonies, which would
// regenerate all city data from scratch and wipe existing buildings.

void AutoEmpire::TickDevelop(int deltaTime)
{
    mDevelopAccum += deltaTime;
    if (mDevelopAccum < kDevelopInterval) return;
    mDevelopAccum -= kDevelopInterval;

    cEmpire* empire = GetPlayerEmpire();
    if (!empire) return;

    cPlanetRecord* homeWorld = GetPlayerHomePlanet();
    int developed = 0;

    for (auto& starPtr : empire->mStars)
    {
        for (auto& planetPtr : starPtr->GetPlanetRecords())
        {
            cPlanetRecord* planet = planetPtr.get();
            if (planet == homeWorld)                     continue;
            if (planet->mTechLevel != TechLevel::Empire) continue;

            if (planet->mCivData.empty())
            {
                // Colony has no city data yet; initialize it once.
                cPlanetRecord::FillPlanetDataForTechLevel(planet, TechLevel::Empire);
                ++developed;
                continue;
            }

            // Grow existing cities one size step toward their maximum.
            for (cCivData* civ : planet->mCivData)
            {
                if (!civ) continue;
                for (cCityData* city : civ->mCities)
                {
                    if (!city) continue;
                    if (city->mSize < city->mMaxSize)
                    {
                        city->mSize++;
                        ++developed;
                    }
                }
            }
        }
    }

    if (developed > 0)
        App::ConsolePrintF("AutoEmpire: Grew %d city slots", developed);
}

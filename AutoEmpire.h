#pragma once
#include "stdafx.h"
#include <Spore/Simulator/SubSystem/cStrategy.h>
#include <Spore/Simulator/cPlanetRecord.h>
#include <Spore/Simulator/cStarRecord.h>
#include <Spore/Simulator/cEmpire.h>
#include <Spore/Simulator/SubSystem/SpacePlayerData.h>
#include <Spore/Simulator/SubSystem/StarManager.h>
#include <Spore/Simulator/SubSystem/TerraformingManager.h>
#include <Spore/Simulator/SubSystem/SpaceTrading.h>

class AutoEmpire : public Simulator::cStrategy
{
public:
    static const uint32_t TYPE    = 0x7A3C1E2D;
    static const uint32_t NOUN_ID = TYPE;

    AutoEmpire();

    void        Initialize() override;
    void        Dispose()    override;
    const char* GetName() const override { return "JohnsSolarEmpires::AutoEmpire"; }

    bool Write(Simulator::ISerializerStream* stream)  override;
    bool Read(Simulator::ISerializerStream* stream)   override;
    bool WriteToXML(Simulator::XmlSerializer* stream) override;

    void Update(int deltaTime, int deltaGameTime) override;

    static AutoEmpire* Get();

private:
    void TickIncome(int deltaTime);
    void TickTerraform(int deltaTime);
    void TickColonize(int deltaTime);
    void TickDevelop(int deltaTime);

    int mIncomeAccum;
    int mTerraAccum;
    int mColonizeAccum;
    int mDevelopAccum;

    static const int kIncomeInterval      = 60000;   // 60 s
    static const int kTerraInterval       = 120000;  // 2 min
    static const int kColonizeInterval    = 300000;  // 5 min
    static const int kDevelopInterval     = 180000;  // 3 min

    // Income: Sporebucks = spice_production * rate (rare spice earns a bonus)
    static const int kSpiceIncomeRate     = 50;
    static const int kRareSpiceMultiplier = 3;

    // Terraform: how far to nudge scores per tick; ecosystem slot limits
    static constexpr float kTerraformStep = 0.05f;
    static const int kMaxPlantSlots       = 6;
    static const int kMaxAnimalSlots      = 9;

    // Colonize: in-system planets first, then nearest unclaimed (any T-score)
    static const int   kColonizeCost            = 10000;
    static constexpr float kColonizeSearchRadius = 25.0f;

    static AutoEmpire* sInstance;
};

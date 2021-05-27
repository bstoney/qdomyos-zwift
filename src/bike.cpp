#include <QDebug>
#include <QSettings>
#include "bike.h"

bike::bike()
{

}

void bike::changeResistance(int8_t resistance) { if(autoResistanceEnable) {requestResistance = resistance * m_difficult; emit resistanceChanged(requestResistance);} RequestedResistance = resistance * m_difficult; }
void bike::changeRequestedPelotonResistance(int8_t resistance) { RequestedPelotonResistance = resistance; }
void bike::changeCadence(int16_t cadence) { RequestedCadence = cadence; }
void bike::changePower(int32_t power)
{
    RequestedPower = power;
    QSettings settings;
    bool force_resistance = settings.value("virtualbike_forceresistance", true).toBool();
    bool erg_mode = settings.value("zwift_erg", false).toBool();
    double erg_filter_upper = settings.value("zwift_erg_filter", 0.0).toDouble();
    double erg_filter_lower = settings.value("zwift_erg_filter_down", 0.0).toDouble();

    double deltaDown = wattsMetric().value() - ((double)power);
    double deltaUp = ((double)power) - wattsMetric().value();
    qDebug() << "filter  " + QString::number(deltaUp) + " " + QString::number(deltaDown) + " " +QString::number(erg_filter_upper) + " " +QString::number(erg_filter_lower);
    if(force_resistance /*&& erg_mode*/ && (deltaUp > erg_filter_upper || deltaDown > erg_filter_lower))
        changeResistance((int8_t)resistanceFromPowerRequest(power)); // resistance start from 1
}
double bike::currentCrankRevolutions() { return CrankRevs;}
uint16_t bike::lastCrankEventTime() { return LastCrankEventTime;}
metric bike::lastRequestedResistance() { return RequestedResistance; }
metric bike::lastRequestedPelotonResistance() { return RequestedPelotonResistance; }
metric bike::lastRequestedCadence() { return RequestedCadence; }
metric bike::lastRequestedPower() { return RequestedPower; }
metric bike::currentResistance() { return Resistance;}
metric bike::currentCadence() { return Cadence;}
uint8_t bike::fanSpeed() { return FanSpeed; }
bool bike::connected() { return false; }
uint16_t bike::watts() { return 0; }
metric bike::pelotonResistance() { return m_pelotonResistance; }
int bike::pelotonToBikeResistance(int pelotonResistance) {return pelotonResistance;}
uint8_t bike::resistanceFromPowerRequest(uint16_t power) {return power / 10;} // in order to have something
void bike::cadenceSensor(uint8_t cadence) { Cadence.setValue(cadence); }

bluetoothdevice::BLUETOOTH_TYPE bike::deviceType() { return bluetoothdevice::BIKE; }

void bike::clearStats()
{
    moving.clear(true);
    elapsed.clear(true);
    Speed.clear(false);
    KCal.clear(true);
    Distance.clear(true);
    Heart.clear(false);
    m_jouls.clear(true);
    elevationAcc = 0;
    m_watt.clear(false);
    WeightLoss.clear(false);

    RequestedPelotonResistance.clear(false);
    RequestedResistance.clear(false);
    RequestedCadence.clear(false);
    RequestedPower.clear(false);
    m_pelotonResistance.clear(false);
    Cadence.clear(false);
    Resistance.clear(false);
}

void bike::setPaused(bool p)
{
    paused = p;
    moving.setPaused(p);
    elapsed.setPaused(p);
    Speed.setPaused(p);
    KCal.setPaused(p);
    Distance.setPaused(p);
    Heart.setPaused(p);
    m_jouls.setPaused(p);
    m_watt.setPaused(p);
    WeightLoss.setPaused(p);
    m_pelotonResistance.setPaused(p);
    Cadence.setPaused(p);
    Resistance.setPaused(p);
    RequestedPelotonResistance.setPaused(p);
    RequestedResistance.setPaused(p);
    RequestedCadence.setPaused(p);
    RequestedPower.setPaused(p);
}

void bike::setLap()
{
    moving.setLap(true);
    elapsed.setLap(true);
    Speed.setLap(false);
    KCal.setLap(true);
    Distance.setLap(true);
    Heart.setLap(false);
    m_jouls.setLap(true);
    m_watt.setLap(false);
    WeightLoss.setLap(false);

    RequestedPelotonResistance.setLap(false);
    RequestedResistance.setLap(false);
    RequestedCadence.setLap(false);
    RequestedPower.setLap(false);
    m_pelotonResistance.setLap(false);
    Cadence.setLap(false);
    Resistance.setLap(false);
}


#include "bike.h"
#include "qdebugfixup.h"
#include <QSettings>

bike::bike() {}


void bike::changeResistance(int8_t resistance) {
    if (autoResistanceEnable) {
        requestResistance = (resistance * m_difficult) + gears();
        emit resistanceChanged(requestResistance);
    }
    RequestedResistance = resistance * m_difficult;
}


void bike::changeRequestedPelotonResistance(int8_t resistance) { RequestedPelotonResistance = resistance; }
void bike::changeCadence(int16_t cadence) { RequestedCadence = cadence; }
void bike::changePower(int32_t power) {

    RequestedPower = power;
    QSettings settings;
    bool force_resistance = settings.value(QStringLiteral("virtualbike_forceresistance"), true).toBool();
    // bool erg_mode = settings.value(QStringLiteral("zwift_erg"), false).toBool(); //Not used anywhere in code
    double erg_filter_upper = settings.value(QStringLiteral("zwift_erg_filter"), 0.0).toDouble();
    double erg_filter_lower = settings.value(QStringLiteral("zwift_erg_filter_down"), 0.0).toDouble();

    double deltaDown = wattsMetric().value() - ((double)power);
    double deltaUp = ((double)power) - wattsMetric().value();
    qDebug() << QStringLiteral("filter  ") + QString::number(deltaUp) + " " + QString::number(deltaDown) + " " +
                    QString::number(erg_filter_upper) + " " + QString::number(erg_filter_lower);
    if (force_resistance /*&& erg_mode*/ && (deltaUp > erg_filter_upper || deltaDown > erg_filter_lower))
        changeResistance((int8_t)resistanceFromPowerRequest(power)); // resistance start from 1
}
int8_t bike::gears() { return m_gears; }
void bike::setGears(int8_t gears) { m_gears = gears; }
double bike::currentCrankRevolutions() { return CrankRevs; }
uint16_t bike::lastCrankEventTime() { return LastCrankEventTime; }
metric bike::lastRequestedResistance() { return RequestedResistance; }
metric bike::lastRequestedPelotonResistance() { return RequestedPelotonResistance; }
metric bike::lastRequestedCadence() { return RequestedCadence; }
metric bike::lastRequestedPower() { return RequestedPower; }
metric bike::currentResistance() { return Resistance; }
metric bike::currentCadence() { return Cadence; }
uint8_t bike::fanSpeed() { return FanSpeed; }
bool bike::connected() { return false; }
uint16_t bike::watts() { return 0; }
metric bike::pelotonResistance() { return m_pelotonResistance; }
int bike::pelotonToBikeResistance(int pelotonResistance) { return pelotonResistance; }
uint8_t bike::resistanceFromPowerRequest(uint16_t power) { return power / 10; } // in order to have something
void bike::cadenceSensor(uint8_t cadence) { Cadence.setValue(cadence); }

bluetoothdevice::BLUETOOTH_TYPE bike::deviceType() { return bluetoothdevice::BIKE; }

void bike::clearStats() {

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
    WattKg.clear(false);
}

void bike::setPaused(bool p) {

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
    WattKg.setPaused(p);
}

void bike::setLap() {

    moving.setLap(true);
    elapsed.setLap(true);
    Speed.setLap(false);
    KCal.setLap(true);
    Distance.setLap(true);
    Heart.setLap(false);
    m_jouls.setLap(true);
    m_watt.setLap(false);
    WeightLoss.setLap(false);
    WattKg.setLap(false);

    RequestedPelotonResistance.setLap(false);
    RequestedResistance.setLap(false);
    RequestedCadence.setLap(false);
    RequestedPower.setLap(false);
    m_pelotonResistance.setLap(false);
    Cadence.setLap(false);
    Resistance.setLap(false);
}

uint8_t bike::metrics_override_heartrate() {

    QSettings settings;
    QString setting =
        settings.value(QStringLiteral("peloton_heartrate_metric"), QStringLiteral("Heart Rate")).toString();
    if (!setting.compare(QStringLiteral("Heart Rate"))) {
        return currentHeart().value();
    } else if (!setting.compare(QStringLiteral("Speed"))) {

        return currentSpeed().value();
    } else if (!setting.compare(QStringLiteral("Inclination"))) {

        return 0;
    } else if (!setting.compare(QStringLiteral("Cadence"))) {

        return Cadence.value();
    } else if (!setting.compare(QStringLiteral("Elevation"))) {

        return elevationGain();
    } else if (!setting.compare(QStringLiteral("Calories"))) {

        return calories();
    } else if (!setting.compare(QStringLiteral("Odometer"))) {

        return odometer();
    } else if (!setting.compare(QStringLiteral("Pace"))) {

        return currentPace().second();
    } else if (!setting.compare(QStringLiteral("Resistance"))) {

        return Resistance.value();
    } else if (!setting.compare(QStringLiteral("Watt"))) {

        return wattsMetric().value();
    } else if (!setting.compare(QStringLiteral("Weight Loss"))) {

        return weightLoss();
    } else if (!setting.compare(QStringLiteral("AVG Watt"))) {

        return wattsMetric().average();
    } else if (!setting.compare(QStringLiteral("FTP"))) {

        return 0;
    } else if (!setting.compare(QStringLiteral("Fan"))) {

        return FanSpeed;
    } else if (!setting.compare(QStringLiteral("Jouls"))) {

        return jouls().value();
    } else if (!setting.compare(QStringLiteral("Lap Elapsed"))) {

        return lapElapsedTime().second();
    } else if (!setting.compare(QStringLiteral("Elapsed"))) {

        return elapsed.value();
    } else if (!setting.compare(QStringLiteral("Moving Time"))) {

        return movingTime().second();
    } else if (!setting.compare(QStringLiteral("Peloton Offset"))) {

        return 0;
    } else if (!setting.compare(QStringLiteral("Peloton Resistance"))) {

        return pelotonResistance().value();
    } else if (!setting.compare(QStringLiteral("Date Time"))) {

        return 0;
    } else if (!setting.compare(QStringLiteral("Target Resistance"))) {

        return RequestedResistance.value();
    } else if (!setting.compare(QStringLiteral("Target Peloton Resistance"))) {

        return RequestedPelotonResistance.value();
    } else if (!setting.compare(QStringLiteral("Target Power"))) {

        return RequestedPower.value();
    }
    else if(!setting.compare("Watt/Kg"))
    {
        return wattKg().value();
    }
    return currentHeart().value();
}
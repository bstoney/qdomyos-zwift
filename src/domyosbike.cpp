#include "domyosbike.h"
#include "ios/lockscreen.h"
#include "keepawakehelper.h"
#include "virtualbike.h"
#include <QBluetoothLocalDevice>
#include <QDateTime>
#include <QFile>
#include <QMetaEnum>
#include <QSettings>
#include <chrono>
#include <math.h>

using namespace std::chrono_literals;

domyosbike::domyosbike(bool noWriteResistance, bool noHeartService, bool testResistance, uint8_t bikeResistanceOffset,
                       double bikeResistanceGain) {
    m_watt.setType(metric::METRIC_WATT);
    Speed.setType(metric::METRIC_SPEED);
    refresh = new QTimer(this);

    this->testResistance = testResistance;
    this->noWriteResistance = noWriteResistance;
    this->noHeartService = noHeartService;
    this->bikeResistanceGain = bikeResistanceGain;
    this->bikeResistanceOffset = bikeResistanceOffset;

    initDone = false;
    connect(refresh, &QTimer::timeout, this, &domyosbike::update);
    refresh->start(300ms);
}

domyosbike::~domyosbike() {
    qDebug() << QStringLiteral("~domyosbike()") << virtualBike;
    if (virtualBike) {
        delete virtualBike;
    }
}

void domyosbike::writeCharacteristic(uint8_t *data, uint8_t data_len, const QString &info, bool disable_log,
                                     bool wait_for_response) {
    QEventLoop loop;
    QTimer timeout;

    if (wait_for_response) {
        connect(this, &domyosbike::packetReceived, &loop, &QEventLoop::quit);
        timeout.singleShot(300ms, &loop, &QEventLoop::quit);
    } else {
        connect(gattCommunicationChannelService, &QLowEnergyService::characteristicWritten, &loop, &QEventLoop::quit);
        timeout.singleShot(300ms, &loop, &QEventLoop::quit);
    }

    if (gattCommunicationChannelService->state() != QLowEnergyService::ServiceState::ServiceDiscovered ||
        m_control->state() == QLowEnergyController::UnconnectedState) {
        qDebug() << QStringLiteral("writeCharacteristic error because the connection is closed");
        return;
    }

    gattCommunicationChannelService->writeCharacteristic(gattWriteCharacteristic,
                                                         QByteArray((const char *)data, data_len));

    if (!disable_log) {
        qDebug() << QStringLiteral(" >> ") + QByteArray((const char *)data, data_len).toHex(' ') +
                        QStringLiteral(" // ") + info;
    }

    loop.exec();

    if (timeout.isActive() == false) {
        qDebug() << QStringLiteral(" exit for timeout");
    }
}

void domyosbike::updateDisplay(uint16_t elapsed) {
    uint16_t multiplier = 1;
    if (bike_type == TELINK) {
        multiplier = 10;
    }

    QSettings settings;
    bool distance = settings.value(QStringLiteral("domyos_treadmill_distance_display"), true).toBool();

    // if(bike_type == CHANG_YOW)
    if (distance) {
        uint8_t display2[] = {0xf0, 0xcd, 0x01, 0x00, 0x00, 0x01, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
                              0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x00};

        display2[3] = ((((uint16_t)(odometer() * 10))) >> 8) & 0xFF;
        display2[4] = (((uint16_t)(odometer() * 10))) & 0xFF;

        for (uint8_t i = 0; i < sizeof(display2) - 1; i++) {
            display2[26] += display2[i]; // the last byte is a sort of a checksum
        }

        writeCharacteristic(display2, 20, QStringLiteral("updateDisplay2"), false, false);
        writeCharacteristic(&display2[20], sizeof(display2) - 20, QStringLiteral("updateDisplay2"), false, true);
    }

    uint8_t display[] = {0xf0, 0xcb, 0x03, 0x00, 0x00, 0xff, 0x01, 0x00, 0x00, 0x02, 0x01, 0x00, 0x00, 0x00,
                         0x01, 0x00, 0x00, 0x01, 0x01, 0x00, 0x00, 0x01, 0xff, 0xff, 0xff, 0xff, 0x00};

    display[3] = (elapsed / 60) & 0xFF; // high byte for elapsed time (in seconds)
    display[4] = (elapsed % 60 & 0xFF); // low byte for elasped time (in seconds)

    display[7] = ((uint8_t)((uint16_t)(currentSpeed().value() * multiplier) >> 8)) & 0xFF;
    display[8] = (uint8_t)(currentSpeed().value() * multiplier) & 0xFF;

    display[12] = (uint8_t)currentHeart().value();

    // display[13] = ((((uint8_t)calories())) >> 8) & 0xFF;
    // display[14] = (((uint8_t)calories())) & 0xFF;

    if (bike_type == TELINK) {
        display[15] = ((((uint16_t)currentCadence().value()) * multiplier) >> 8) & 0xFF;
        display[16] = (((uint16_t)currentCadence().value()) * multiplier) & 0xFF;
    } else {
        display[16] = ((uint8_t)(currentCadence().value() * multiplier));
    }

    display[19] = ((((uint16_t)calories()) * multiplier) >> 8) & 0xFF;
    display[20] = (((uint16_t)calories()) * multiplier) & 0xFF;

    for (uint8_t i = 0; i < sizeof(display) - 1; i++) {
        display[26] += display[i]; // the last byte is a sort of a checksum
    }

    writeCharacteristic(display, 20, QStringLiteral("updateDisplay elapsed=") + QString::number(elapsed), false, false);
    writeCharacteristic(&display[20], sizeof(display) - 20,
                        QStringLiteral("updateDisplay elapsed=") + QString::number(elapsed), false, true);
}

void domyosbike::forceResistance(int8_t requestResistance) {
    uint8_t write[] = {0xf0, 0xad, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
                       0xff, 0xff, 0xff, 0xff, 0xff, 0x00, 0x01, 0xff, 0xff, 0xff, 0x00};

    write[10] = requestResistance;

    for (uint8_t i = 0; i < sizeof(write) - 1; i++) {
        write[22] += write[i]; // the last byte is a sort of a checksum
    }

    writeCharacteristic(write, 20, QStringLiteral("forceResistance ") + QString::number(requestResistance));
    writeCharacteristic(&write[20], sizeof(write) - 20,
                        QStringLiteral("forceResistance ") + QString::number(requestResistance));
}

void domyosbike::update() {
    uint8_t noOpData[] = {0xf0, 0xac, 0x9c};

    // stop tape
    // uint8_t initDataF0C800B8[] = {0xf0, 0xc8, 0x00, 0xb8}; //unused code

    if (m_control->state() == QLowEnergyController::UnconnectedState) {
        emit disconnected();
        return;
    }

    if (initRequest) {
        initRequest = false;
        // if(bike_type == CHANG_YOW)
        btinit_changyow(false);
        // else
        //    btinit_telink(false);
    } else if (/*bluetoothDevice.isValid() &&*/
               m_control->state() == QLowEnergyController::DiscoveredState && gattCommunicationChannelService &&
               gattWriteCharacteristic.isValid() && gattNotifyCharacteristic.isValid() && initDone) {
        update_metrics(true, watts());

        // ******************************************* virtual bike init *************************************
        if (!firstStateChanged && !virtualBike
#ifdef Q_OS_IOS
#ifndef IO_UNDER_QT
            && !h
#endif
#endif
        ) {
            QSettings settings;
            bool virtual_device_enabled = settings.value(QStringLiteral("virtual_device_enabled"), true).toBool();
#ifdef Q_OS_IOS
#ifndef IO_UNDER_QT
            bool cadence = settings.value("bike_cadence_sensor", false).toBool();
            bool ios_peloton_workaround = settings.value("ios_peloton_workaround", true).toBool();
            if (ios_peloton_workaround && cadence) {
                qDebug() << "ios_peloton_workaround activated!";
                h = new lockscreen();
                h->virtualbike_ios();
            } else
#endif
#endif
                if (virtual_device_enabled) {
                qDebug() << QStringLiteral("creating virtual bike interface...");
                virtualBike =
                    new virtualbike(this, noWriteResistance, noHeartService, bikeResistanceOffset, bikeResistanceGain);
                // connect(virtualBike,&virtualbike::debug ,this,&schwinnic4bike::debug);
            }
        }
        firstStateChanged = 1;
        // ********************************************************************************************************

        // updating the treadmill console every second
        if (sec1Update++ == (1000 / refresh->interval())) {
            sec1Update = 0;
            if (incompletePackets == false) {
                updateDisplay(elapsed.value());
            }
        } else {
            if (incompletePackets == false) {
                writeCharacteristic(noOpData, sizeof(noOpData), QStringLiteral("noOp"), true, true);
            }
        }

        if (incompletePackets == false) {
            if (testResistance) {
                if ((((int)elapsed.value()) % 5) == 0) {
                    uint8_t new_res = currentResistance().value() + 1;
                    if (new_res > 15) {
                        new_res = 1;
                    }
                    forceResistance(new_res);
                }
            }

            if (requestResistance != -1) {
                if (requestResistance > max_resistance) {
                    requestResistance = max_resistance;
                } else if (requestResistance < 1) {
                    requestResistance = 1;
                }

                if (requestResistance != currentResistance().value()) {
                    qDebug() << QStringLiteral("writing resistance ") + QString::number(requestResistance);
                    forceResistance(requestResistance);
                }
                requestResistance = -1;
            }
            if (requestStart != -1) {
                qDebug() << QStringLiteral("starting...");

                // if(bike_type == CHANG_YOW)
                btinit_changyow(true);
                // else
                //    btinit_telink(true);

                requestStart = -1;
                emit bikeStarted();
            }
            if (requestStop != -1) {
                qDebug() << QStringLiteral("stopping...");
                // writeCharacteristic(initDataF0C800B8, sizeof(initDataF0C800B8), "stop tape");
                requestStop = -1;
            }
        }
    }
}

void domyosbike::serviceDiscovered(const QBluetoothUuid &gatt) {
    qDebug() << QStringLiteral("serviceDiscovered ") + gatt.toString();
}

void domyosbike::characteristicChanged(const QLowEnergyCharacteristic &characteristic, const QByteArray &newValue) {
    // qDebug() << "characteristicChanged" << characteristic.uuid() << newValue << newValue.length();
    Q_UNUSED(characteristic);
    QSettings settings;
    QString heartRateBeltName =
        settings.value(QStringLiteral("heart_rate_belt_name"), QStringLiteral("Disabled")).toString();
    QByteArray value = newValue;

    qDebug() << QStringLiteral(" << ") + QString::number(value.length()) + QStringLiteral(" ") + value.toHex(' ');

    // for the init packets, the lenght is always less than 20
    // for the display and status packets, the lenght is always grater then 20 and there are 2 cases:
    // - intense run: it always send more than 20 bytes in one packets, so the lenght will be always != 20
    // - t900: it splits packets with lenght grater than 20 in two distinct packets, so the first one it has lenght of
    // 20,
    //         and the second one with the remained byte
    // so this simply condition will match all the cases, excluding the 20byte packet of the T900.
    if (newValue.length() != 20) {
        qDebug() << QStringLiteral("packetReceived!");
        emit packetReceived();
    }

    QByteArray startBytes;
    startBytes.append(0xf0);
    startBytes.append(0xbc);

    QByteArray startBytes2;
    startBytes2.append(0xf0);
    startBytes2.append(0xdb);

    QByteArray startBytes3;
    startBytes3.append(0xf0);
    startBytes3.append(0xdd);

    // on some treadmills, the 26bytes has splitted in 2 packets
    if ((lastPacket.length() == 20 && lastPacket.startsWith(startBytes) && value.length() == 6) ||
        (lastPacket.length() == 20 && lastPacket.startsWith(startBytes2) && value.length() == 7) ||
        (lastPacket.length() == 20 && lastPacket.startsWith(startBytes3) && value.length() == 7)) {
        incompletePackets = false;
        qDebug() << QStringLiteral("...final bytes received");
        lastPacket.append(value);
        value = lastPacket;
    }

    lastPacket = value;

    if (value.length() != 26) {
        // semaphore for any writing packets (for example, update display)
        if (value.length() == 20 &&
            (value.startsWith(startBytes) || value.startsWith(startBytes2) || value.startsWith(startBytes3))) {
            qDebug() << QStringLiteral("waiting for other bytes...");
            incompletePackets = true;
        }

        qDebug() << QStringLiteral("packet ignored");
        return;
    }

    if (value.at(22) == 0x06) {
        qDebug() << QStringLiteral("start button pressed!");
        requestStart = 1;
    } else if (value.at(22) == 0x07) {
        qDebug() << QStringLiteral("stop button pressed!");
        requestStop = 1;
    }

    /*if ((uint8_t)value.at(1) != 0xbc && value.at(2) != 0x04)  // intense run, these are the bytes for the inclination
       and speed status return;*/

    double speed = GetSpeedFromPacket(value);
    double kcal = GetKcalFromPacket(value);
    double distance = GetDistanceFromPacket(value);

    double ucadence = ((uint8_t)value.at(9));
    double cadenceFilter = settings.value(QStringLiteral("domyos_bike_cadence_filter"), 0).toDouble();
    if (settings.value(QStringLiteral("cadence_sensor_name"), QStringLiteral("Disabled"))
            .toString()
            .startsWith(QStringLiteral("Disabled"))) {
        if (cadenceFilter == 0 || cadenceFilter > ucadence) {
            Cadence = ucadence;
        } else {
            qDebug() << QStringLiteral("cadence filter out ") << ucadence << cadenceFilter;
        }
    }

    Resistance = value.at(14);
    if (Resistance.value() < 1) {
        qDebug() << QStringLiteral("invalid resistance value ") + QString::number(Resistance.value()) +
                        QStringLiteral(" putting to default");
        Resistance = 1;
    }
    emit resistanceRead(Resistance.value());
    m_pelotonResistance = (Resistance.value() * 100) / max_resistance;

#ifdef Q_OS_ANDROID
    if (settings.value("ant_heart", false).toBool())
        Heart = (uint8_t)KeepAwakeHelper::heart();
    else
#endif
    {
        if (heartRateBeltName.startsWith(QStringLiteral("Disabled"))) {
            uint8_t heart = ((uint8_t)value.at(18));
            if (heart == 0) {
#ifdef Q_OS_IOS
#ifndef IO_UNDER_QT
                lockscreen h;
                long appleWatchHeartRate = h.heartRate();
                h.setKcal(KCal.value());
                h.setDistance(Distance.value());
                Heart = appleWatchHeartRate;
                qDebug() << "Current Heart from Apple Watch: " + QString::number(appleWatchHeartRate);
#endif
#endif
            } else
                Heart = heart;
        }
    }

    if (Cadence.value() > 0) {
        CrankRevs++;
        LastCrankEventTime += (uint16_t)(1024.0 / (((double)(Cadence.value())) / 60.0));
    }
    lastRefreshCharacteristicChanged = QDateTime::currentDateTime();

#ifdef Q_OS_IOS
#ifndef IO_UNDER_QT
    bool cadence = settings.value("bike_cadence_sensor", false).toBool();
    bool ios_peloton_workaround = settings.value("ios_peloton_workaround", true).toBool();
    if (ios_peloton_workaround && cadence && h && firstStateChanged) {
        h->virtualbike_setCadence(currentCrankRevolutions(), lastCrankEventTime());
        h->virtualbike_setHeartRate((uint8_t)metrics_override_heartrate());
    }
#endif
#endif

    qDebug() << QStringLiteral("Current speed: ") + QString::number(speed);
    qDebug() << QStringLiteral("Current cadence: ") + QString::number(Cadence.value());
    qDebug() << QStringLiteral("Current resistance: ") + QString::number(Resistance.value());
    qDebug() << QStringLiteral("Current heart: ") + QString::number(Heart.value());
    qDebug() << QStringLiteral("Current KCal: ") + QString::number(kcal);
    qDebug() << QStringLiteral("Current Distance: ") + QString::number(distance);
    qDebug() << QStringLiteral("Current CrankRevs: ") + QString::number(CrankRevs);
    qDebug() << QStringLiteral("Last CrankEventTime: ") + QString::number(LastCrankEventTime);
    qDebug() << QStringLiteral("Current Watt: ") + QString::number(watts());

    if (m_control->error() != QLowEnergyController::NoError) {
        qDebug() << "QLowEnergyController ERROR!!" << m_control->errorString();
    }

    if (!settings.value(QStringLiteral("speed_power_based"), false).toBool()) {
        Speed = speed;
    } else {
        Speed = metric::calculateSpeedFromPower(m_watt.value());
    }
    KCal = kcal;
    Distance = distance;
}

double domyosbike::GetSpeedFromPacket(const QByteArray &packet) {
    uint16_t convertedData = (packet.at(6) << 8) | ((uint8_t)packet.at(7));
    double data = (double)convertedData / 10.0f;
    return data;
}

double domyosbike::GetKcalFromPacket(const QByteArray &packet) {
    uint16_t convertedData = (packet.at(10) << 8) | ((uint8_t)packet.at(11));
    return (double)convertedData;
}

double domyosbike::GetDistanceFromPacket(const QByteArray &packet) {
    uint16_t convertedData = (packet.at(12) << 8) | packet.at(13);
    double data = ((double)convertedData) / 10.0f;
    return data;
}

void domyosbike::btinit_changyow(bool startTape) {
    // set speed and incline to 0
    uint8_t initData1[] = {0xf0, 0xc8, 0x01, 0xb9};
    uint8_t initData2[] = {0xf0, 0xc9, 0xb9};

    // main startup sequence
    uint8_t initDataStart[] = {0xf0, 0xa3, 0x93};
    uint8_t initDataStart2[] = {0xf0, 0xa4, 0x94};
    uint8_t initDataStart3[] = {0xf0, 0xa5, 0x95};
    uint8_t initDataStart4[] = {0xf0, 0xab, 0x9b};
    uint8_t initDataStart5[] = {0xf0, 0xc4, 0x03, 0xb7};
    uint8_t initDataStart6[] = {0xf0, 0xad, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
                                0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x01, 0xff};
    uint8_t initDataStart7[] = {0xff, 0xff, 0x8b}; // power on bt icon
    uint8_t initDataStart8[] = {0xf0, 0xcb, 0x02, 0x00, 0x08, 0xff, 0xff, 0xff, 0xff, 0xff,
                                0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x01, 0x00};
    uint8_t initDataStart9[] = {0x00, 0x01, 0xff, 0xff, 0xff, 0xff, 0xb6}; // power on bt word
    uint8_t initDataStart10[] = {0xf0, 0xad, 0xff, 0xff, 0x00, 0x05, 0xff, 0xff, 0xff, 0xff,
                                 0xff, 0xff, 0xff, 0x00, 0x00, 0xff, 0xff, 0xff, 0x01, 0xff};
    uint8_t initDataStart11[] = {0xff, 0xff, 0x94}; // start tape
    uint8_t initDataStart12[] = {0xf0, 0xcb, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
                                 0xff, 0xff, 0xff, 0xff, 0x01, 0x00, 0x14, 0x01, 0xff, 0xff};
    uint8_t initDataStart13[] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xbd};

    writeCharacteristic(initData1, sizeof(initData1), QStringLiteral("init"), false, true);
    writeCharacteristic(initData2, sizeof(initData2), QStringLiteral("init"), false, true);
    writeCharacteristic(initDataStart, sizeof(initDataStart), QStringLiteral("init"), false, true);
    writeCharacteristic(initDataStart2, sizeof(initDataStart2), QStringLiteral("init"), false, true);
    writeCharacteristic(initDataStart3, sizeof(initDataStart3), QStringLiteral("init"), false, true);
    writeCharacteristic(initDataStart4, sizeof(initDataStart4), QStringLiteral("init"), false, true);
    writeCharacteristic(initDataStart5, sizeof(initDataStart5), QStringLiteral("init"), false, true);
    writeCharacteristic(initDataStart6, sizeof(initDataStart6), QStringLiteral("init"), false, false);
    writeCharacteristic(initDataStart7, sizeof(initDataStart7), QStringLiteral("init"), false, true);
    writeCharacteristic(initDataStart8, sizeof(initDataStart8), QStringLiteral("init"), false, false);
    writeCharacteristic(initDataStart9, sizeof(initDataStart9), QStringLiteral("init"), false, true);
    writeCharacteristic(initDataStart10, sizeof(initDataStart10), QStringLiteral("init"), false, false);
    writeCharacteristic(initDataStart11, sizeof(initDataStart11), QStringLiteral("init"), false, true);
    if (startTape) {
        writeCharacteristic(initDataStart12, sizeof(initDataStart12), QStringLiteral("init"), false, false);
        writeCharacteristic(initDataStart13, sizeof(initDataStart13), QStringLiteral("init"), false, true);
    }

    initDone = true;
}

void domyosbike::btinit_telink(bool startTape) {
    Q_UNUSED(startTape)

    // set speed and incline to 0
    uint8_t initData1[] = {0xf0, 0xc8, 0x01, 0xb9};
    uint8_t initData2[] = {0xf0, 0xc9, 0xb9};
    uint8_t noOpData[] = {0xf0, 0xac, 0x9c};

    // main startup sequence
    uint8_t initDataStart[] = {0xf0, 0xcc, 0xff, 0xff, 0xff, 0xff, 0x01, 0xff, 0xb8};

    writeCharacteristic(initData1, sizeof(initData1), QStringLiteral("init"));
    writeCharacteristic(initData2, sizeof(initData2), QStringLiteral("init"));
    writeCharacteristic(noOpData, sizeof(noOpData), QStringLiteral("noOp"));
    writeCharacteristic(initDataStart, sizeof(initDataStart), QStringLiteral("init"));
    updateDisplay(0);

    initDone = true;
}

void domyosbike::stateChanged(QLowEnergyService::ServiceState state) {
    QBluetoothUuid _gattWriteCharacteristicId(QStringLiteral("49535343-8841-43f4-a8d4-ecbe34729bb3"));
    QBluetoothUuid _gattNotifyCharacteristicId(QStringLiteral("49535343-1e4d-4bd9-ba61-23c647249616"));

    QMetaEnum metaEnum = QMetaEnum::fromType<QLowEnergyService::ServiceState>();
    qDebug() << QStringLiteral("BTLE stateChanged ") + QString::fromLocal8Bit(metaEnum.valueToKey(state));

    if (state == QLowEnergyService::ServiceDiscovered) {
        // qDebug() << gattCommunicationChannelService->characteristics();

        gattWriteCharacteristic = gattCommunicationChannelService->characteristic(_gattWriteCharacteristicId);
        gattNotifyCharacteristic = gattCommunicationChannelService->characteristic(_gattNotifyCharacteristicId);
        Q_ASSERT(gattWriteCharacteristic.isValid());
        Q_ASSERT(gattNotifyCharacteristic.isValid());

        // establish hook into notifications
        connect(gattCommunicationChannelService, &QLowEnergyService::characteristicChanged, this,
                &domyosbike::characteristicChanged);
        connect(gattCommunicationChannelService, &QLowEnergyService::characteristicWritten, this,
                &domyosbike::characteristicWritten);
        connect(gattCommunicationChannelService,
                static_cast<void (QLowEnergyService::*)(QLowEnergyService::ServiceError)>(&QLowEnergyService::error),
                this, &domyosbike::errorService);
        connect(gattCommunicationChannelService, &QLowEnergyService::descriptorWritten, this,
                &domyosbike::descriptorWritten);

        QByteArray descriptor;
        descriptor.append((char)0x01);
        descriptor.append((char)0x00);
        gattCommunicationChannelService->writeDescriptor(
            gattNotifyCharacteristic.descriptor(QBluetoothUuid::ClientCharacteristicConfiguration), descriptor);
    }
}

void domyosbike::searchingStop() { searchStopped = true; }

void domyosbike::descriptorWritten(const QLowEnergyDescriptor &descriptor, const QByteArray &newValue) {
    qDebug() << QStringLiteral("descriptorWritten ") + descriptor.name() + QStringLiteral(" ") + newValue.toHex(' ');

    initRequest = true;
    emit connectedAndDiscovered();
}

void domyosbike::characteristicWritten(const QLowEnergyCharacteristic &characteristic, const QByteArray &newValue) {
    Q_UNUSED(characteristic);
    qDebug() << QStringLiteral("characteristicWritten ") + newValue.toHex(' ');
}

void domyosbike::serviceScanDone(void) {
    qDebug() << QStringLiteral("serviceScanDone");

    QBluetoothUuid _gattCommunicationChannelServiceId(QStringLiteral("49535343-fe7d-4ae5-8fa9-9fafd205e455"));

    gattCommunicationChannelService = m_control->createServiceObject(_gattCommunicationChannelServiceId);
    connect(gattCommunicationChannelService, &QLowEnergyService::stateChanged, this, &domyosbike::stateChanged);
    gattCommunicationChannelService->discoverDetails();
}

void domyosbike::errorService(QLowEnergyService::ServiceError err) {
    QMetaEnum metaEnum = QMetaEnum::fromType<QLowEnergyService::ServiceError>();
    qDebug() << QStringLiteral("domyosbike::errorService") + QString::fromLocal8Bit(metaEnum.valueToKey(err)) +
                    m_control->errorString();
}

void domyosbike::error(QLowEnergyController::Error err) {
    QMetaEnum metaEnum = QMetaEnum::fromType<QLowEnergyController::Error>();
    qDebug() << QStringLiteral("domyosbike::error") + QString::fromLocal8Bit(metaEnum.valueToKey(err)) +
                    m_control->errorString();
}

void domyosbike::deviceDiscovered(const QBluetoothDeviceInfo &device) {
    qDebug() << QStringLiteral("Found new device: ") + device.name() + QStringLiteral(" (") +
                    device.address().toString() + ')';
    if (device.name().startsWith(QStringLiteral("Domyos-Bike")) &&
        !device.name().startsWith(QStringLiteral("DomyosBridge"))) {
        bluetoothDevice = device;

        if (device.address().toString().startsWith(QStringLiteral("57"))) {
            qDebug() << QStringLiteral("domyos telink bike found");
            bike_type = TELINK;
        } else {
            qDebug() << QStringLiteral("domyos changyow bike found");
            bike_type = CHANG_YOW;
        }

        m_control = QLowEnergyController::createCentral(bluetoothDevice, this);
        connect(m_control, &QLowEnergyController::serviceDiscovered, this, &domyosbike::serviceDiscovered);
        connect(m_control, &QLowEnergyController::discoveryFinished, this, &domyosbike::serviceScanDone);
        connect(m_control,
                static_cast<void (QLowEnergyController::*)(QLowEnergyController::Error)>(&QLowEnergyController::error),
                this, &domyosbike::error);
        connect(m_control, &QLowEnergyController::stateChanged, this, &domyosbike::controllerStateChanged);

        connect(m_control,
                static_cast<void (QLowEnergyController::*)(QLowEnergyController::Error)>(&QLowEnergyController::error),
                this, [this](QLowEnergyController::Error error) {
                    Q_UNUSED(error);
                    Q_UNUSED(this);
                    qDebug() << QStringLiteral("Cannot connect to remote device.");
                    searchStopped = false;
                    emit disconnected();
                });
        connect(m_control, &QLowEnergyController::connected, this, [this]() {
            Q_UNUSED(this);
            qDebug() << QStringLiteral("Controller connected. Search services...");
            m_control->discoverServices();
        });
        connect(m_control, &QLowEnergyController::disconnected, this, [this]() {
            Q_UNUSED(this);
            qDebug() << QStringLiteral("LowEnergy controller disconnected");
            searchStopped = false;
            emit disconnected();
        });

        // Connect
        m_control->connectToDevice();
        return;
    }
}

bool domyosbike::connected() {
    if (!m_control) {
        return false;
    }
    return m_control->state() == QLowEnergyController::DiscoveredState;
}

void *domyosbike::VirtualBike() { return virtualBike; }

void *domyosbike::VirtualDevice() { return VirtualBike(); }

int domyosbike::pelotonToBikeResistance(int pelotonResistance) { return (pelotonResistance * max_resistance) / 100; }

uint8_t domyosbike::resistanceFromPowerRequest(uint16_t power) {
    qDebug() << QStringLiteral("resistanceFromPowerRequest") << currentCadence().value();

    for (int i = 1; i < max_resistance - 1; i++) {
        if (wattsFromResistance(i) <= power && wattsFromResistance(i + 1) >= power) {
            return i;
        }
    }
    return Resistance.value();
}

uint16_t domyosbike::wattsFromResistance(double resistance) {
    return ((10.39 + 1.45 * (resistance - 1.0)) * (exp(0.028 * (currentCadence().value()))));
}

uint16_t domyosbike::watts() {
    double v = 0;
    // const uint8_t max_resistance = 15;
    // ref
    // https://translate.google.com/translate?hl=it&sl=en&u=https://support.wattbike.com/hc/en-us/articles/115001881825-Power-Resistance-and-Cadence-Tables&prev=search&pto=aue

    if (currentSpeed().value() <= 0) {
        return 0;
    }
    v = wattsFromResistance(currentResistance().value());
    return v;
}

void domyosbike::controllerStateChanged(QLowEnergyController::ControllerState state) {
    qDebug() << QStringLiteral("controllerStateChanged") << state;
    if (state == QLowEnergyController::UnconnectedState && m_control) {
        qDebug() << QStringLiteral("trying to connect back again...");
        initDone = false;
        m_control->connectToDevice();
    }
}

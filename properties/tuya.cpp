#include <QtEndian>
#include <QtMath>
#include "tuya.h"

void PropertiesTUYA::Data::parseCommand(quint8 commandId, const QByteArray &payload)
{
    const tuyaHeaderStruct *header = reinterpret_cast <const tuyaHeaderStruct*> (payload.constData());
    QVariant data;

    if (commandId != 0x01 && commandId != 0x02)
        return;

    data = parseData(header, payload.mid(sizeof(tuyaHeaderStruct)));

    if (!data.isValid())
        return;

    update(header->dataPoint, data);
}

QVariant PropertiesTUYA::Data::parseData(const tuyaHeaderStruct *header, const QByteArray &data)
{
    switch (header->dataType)
    {
        case TUYA_TYPE_RAW:
            return data.mid(0, header->length);

        case TUYA_TYPE_BOOL:

            if (header->length == 1)
                return data.at(0) ? true : false;

            break;

        case TUYA_TYPE_VALUE:

            if (header->length == 4)
            {
                quint32 value;
                memcpy(&value, data.constData(), header->length);
                return qFromBigEndian(value);
            }

            break;

        case TUYA_TYPE_ENUM:

            if (header->length == 1)
                return static_cast <quint8> (data.at(0));

            break;
    }

    return QVariant();
}

void PropertiesTUYA::DataPoints::update(quint8 dataPoint, const QVariant &data)
{
    QMap <QString, QVariant> map = m_value.toMap();
    QList <QVariant> list = option().toMap().value(QString::number(dataPoint)).toList();
    QList <QString> types = {"bool", "value", "enum"};

    for (int i = 0; i < list.count(); i++)
    {
        QMap <QString, QVariant> item = list.at(i).toMap();
        QString name = item.value("name").toString();

        if(name.isEmpty())
            continue;

        switch (types.indexOf(item.value("type").toString()))
        {
            case 0: // bool
            {
                bool check = item.value("invert").toBool() ? !data.toBool() : data.toBool();
                QString value = option(name).toStringList().value(check ? 1 : 0);

                if (value.isEmpty())
                    map.insert(name, check);
                else
                    map.insert(name, value);

                break;
            }

            case 1: // value
            {
                double value = data.toInt() / item.value("divider", 1).toDouble();

                if (item.value("round").toBool())
                    value = round(value);

                // TODO: add offsets
                map.insert(name, value);
                break;
            }

            case 2: // enum
            {
                QString value = option(name).toStringList().value(data.toInt());

                if (!value.isEmpty())
                    map.insert(name, value);

                break;
            }

            default:
            {
                QVariant value = item.value("value");

                if (value.isValid())
                    map.insert(name, value);

                break;
            }
        }
    }

    m_value = map.isEmpty() ? QVariant() : map;
}

void PropertiesTUYA::ElectricityMeter::update(quint8 dataPoint, const QVariant &data)
{
    QList <QString> list = {"_TZE200_byzdayie", "_TZE200_ewxhg6o9", "_TZE200_fsb6zw01"};
    QMap <QString, QVariant> map = m_value.toMap();

    if (list.contains(manufacturerName()))
    {
        switch (dataPoint)
        {
            case 0x01: map.insert("status", data.toBool() ? "on" : "off"); break;
            case 0x11: map.insert("energy", data.toInt() / 100.0); break;
            case 0x12: map.insert("current", data.toInt() / 1000.0 + option("currentOffset").toDouble()); break;
            case 0x13: map.insert("power", data.toInt() / 10.0 + option("powerOffset").toDouble()); break;
            case 0x14: map.insert("voltage", data.toInt() / 10.0 + option("voltageOffset").toDouble()); break;
        }
    }
    else if (manufacturerName() == "_TZE200_lsanae15")
    {
        switch (dataPoint)
        {
            case 0x01: map.insert("energy", data.toInt() / 100.0); break;

            case 0x06:
            {
                QByteArray payload = data.toByteArray();
                quint16 value;

                memcpy(&value, payload.constData(), sizeof(value));
                map.insert("voltage", qFromBigEndian(value) / 10.0 + option("voltageOffset").toDouble());

                memcpy(&value, payload.constData() + 3, sizeof(value));
                map.insert("current", qFromBigEndian(value) / 1000.0 + option("currentOffset").toDouble());

                memcpy(&value, payload.constData() + 6, sizeof(value));
                map.insert("power", qFromBigEndian(value) + option("powerOffset").toDouble());

                break;
            }

            case 0x10: map.insert("status", data.toBool() ? "on" : "off"); break;
        }
    }
    else
    {
        switch (dataPoint)
        {
            case 0x01: map.insert("energy", data.toInt() / 100.0); break;

            case 0x06:
            {
                QByteArray payload = data.toByteArray();
                quint16 value;

                memcpy(&value, payload.constData() + 11, sizeof(value));
                map.insert("current", qFromBigEndian(value) / 1000.0 + option("currentOffset").toDouble());

                memcpy(&value, payload.constData() + 13, sizeof(value));
                map.insert("voltage", qFromBigEndian(value) / 10.0 + option("voltageOffset").toDouble());

                break;
            }

            case 0x10: map.insert("status", data.toBool() ? "on" : "off"); break;
            case 0x67: map.insert("power", data.toInt() / 100.0 + option("powerOffset").toDouble()); break;
            case 0x69: map.insert("frequency", data.toInt() / 100.0); break;
            case 0x6F: map.insert("powerFactor", data.toInt() / 10.0); break;
        }
    }

    m_value = map.isEmpty() ? QVariant() : map;
}

void PropertiesTUYA::WeekdayThermostatProgram::update(quint8 dataPoint, const QVariant &data)
{
    QMap <QString, QVariant> map = m_value.toMap();

    if (dataPoint == 0x70)
    {
        QList <QString> names = {"Hour", "Minute", "Temperature"};
        QByteArray program = data.toByteArray();

        for (int i = 0; i < 18; i++)
        {
            quint8 value = static_cast <quint8> (program.at(i));
            map.insert(QString("weekdayP%1%2").arg(i / 3 + 1).arg(names.value(i % 3)), value);
        }

        m_meta.insert("received", true);
    }

    m_value = map.isEmpty() ? QVariant() : map;
}

void PropertiesTUYA::HolidayThermostatProgram::update(quint8 dataPoint, const QVariant &data)
{
    QMap <QString, QVariant> map = m_value.toMap();

    if (dataPoint == 0x71)
    {
        QList <QString> names = {"Hour", "Minute", "Temperature"};
        QByteArray program = data.toByteArray();

        for (int i = 0; i < 18; i++)
        {
            quint8 value = static_cast <quint8> (program.at(i));
            map.insert(QString("holidayP%1%2").arg(i / 3 + 1).arg(names.value(i % 3)), value);
        }

        m_meta.insert("received", true);
    }

    m_value = map.isEmpty() ? QVariant() : map;
}

void PropertiesTUYA::MoesThermostatProgram::update(quint8 dataPoint, const QVariant &data)
{
    QMap <QString, QVariant> map = m_value.toMap();

    if (dataPoint == 0x65)
    {
        QList <QString> types = {"weekday", "saturday", "sunday"}, names = {"Hour", "Minute", "Temperature"};
        QByteArray program = data.toByteArray();

        for (int i = 0; i < 36; i++)
        {
            double value = static_cast <double> (program.at(i));
            map.insert(QString("%1P%2%3").arg(types.value(i / 12)).arg(i / 3 % 4 + 1).arg(names.value(i % 3)), (i + 1) % 3 ? value : value / 2);
        }

        m_meta.insert("received", true);
    }

    m_value = map.isEmpty() ? QVariant() : map;
}

void PropertiesTUYA::NeoSiren::update(quint8 dataPoint, const QVariant &data)
{
    QMap <QString, QVariant> map = m_value.toMap();

    switch (dataPoint)
    {
        case 0x05:
        {
            switch (data.toInt())
            {
                case 0: map.insert("volume", "low"); break;
                case 1: map.insert("volume", "medium"); break;
                case 2: map.insert("volume", "high"); break;
            }

            break;
        }

        case 0x07: map.insert("duration", data.toInt()); break;
        case 0x0D: map.insert("alarm", data.toBool()); break;
        case 0x0F: map.insert("battery", data.toInt()); break;
        case 0x15: map.insert("melody", data.toInt()); break;
    }

    m_value = map.isEmpty() ? QVariant() : map;
}

void PropertiesTUYA::PresenceSensor::update(quint8 dataPoint, const QVariant &data)
{
    QMap <QString, QVariant> map = m_value.toMap();

    switch (dataPoint)
    {
        case 0x01: map.insert("occupancy", data.toBool()); break;
        case 0x02: map.insert("sensitivity", data.toInt()); break;
        case 0x03: map.insert("distanceMin", data.toDouble() / 100); break;
        case 0x04: map.insert("distanceMax", data.toDouble() / 100); break;
        case 0x09: map.insert("targetDistance", data.toDouble() / 100); break;
        case 0x65: map.insert("detectionDelay", data.toDouble() / 10); break;
        case 0x66: map.insert("fadingTime", data.toInt() / 10); break;
        case 0x68: map.insert("illuminance", data.toInt() + option("illuminanceOffset").toDouble()); break;
    }

    m_value = map.isEmpty() ? QVariant() : map;
}

void PropertiesTUYA::RadarSensor::update(quint8 dataPoint, const QVariant &data)
{
    QMap <QString, QVariant> map = m_value.toMap();

    switch (dataPoint)
    {
        case 0x01: map.insert("occupancy", data.toBool()); break;
        case 0x02: map.insert("radarSensitivity", data.toInt()); break;
        case 0x66: map.insert("motion", data.toInt() != 0x01 ? true : false); break;
        case 0x67: map.insert("illuminance", data.toInt() + option("illuminanceOffset").toDouble()); break;
        case 0x69: map.insert("tumbleSwitch", data.toBool()); break;
        case 0x6A: map.insert("tumbleAlarmTime", data.toInt() + 1); break;

        case 0x70:
        {
            switch (data.toInt())
            {
                case 0: map.insert("radarScene", "default"); break;
                case 1: map.insert("radarScene", "area"); break;
                case 2: map.insert("radarScene", "toilet"); break;
                case 3: map.insert("radarScene", "bedroom"); break;
                case 4: map.insert("radarScene", "parlour"); break;
                case 5: map.insert("radarScene", "office"); break;
                case 6: map.insert("radarScene", "hotel"); break;
            }

            break;
        }

        case 0x72:
        {
            switch (data.toInt())
            {
                case 0: map.insert("motionDirection", "standingStill"); break;
                case 1: map.insert("motionDirection", "movingForward"); break;
                case 2: map.insert("motionDirection", "movingBackward"); break;
            }

            break;
        }

        case 0x73: map.insert("motionSpeed", data.toInt()); break;

        case 0x74:
        {
            switch (data.toInt())
            {
                case 0: map.insert("fallDown", "none"); break;
                case 1: map.insert("fallDown", "maybe"); break;
                case 2: map.insert("fallDown", "fall"); break;
            }

            break;
        }

        case 0x75: map.insert("staticDwellAlarm", data.toInt()); break;
        case 0x76: map.insert("fallSensitivity", data.toInt()); break;
    }

    m_value = map.isEmpty() ? QVariant() : map;
}

void PropertiesTUYA::CoverMotor::update(quint8 dataPoint, const QVariant &data)
{
    QMap <QString, QVariant> map = m_value.toMap();

    switch (dataPoint)
    {
        case 0x02:
        case 0x03:
        {
            quint8 value = static_cast <quint8> (option("invertCover").toBool() ? 100 - data.toInt() : data.toInt());

            map.insert("cover", value ? "open" : "closed");
            map.insert("position", static_cast <quint8> (value));

            break;
        }

        case 0x05:  map.insert("reverse", data.toBool()); break;
        case 0x69:  map.insert("speed", data.toInt()); break;
    }

    m_value = map.isEmpty() ? QVariant() : map;
}

void PropertiesTUYA::CoverSwitch::parseAttribte(quint16 attributeId, const QByteArray &data)
{
    QMap <QString, QVariant> map = m_value.toMap();

    switch (attributeId)
    {
        case 0xF000:

            switch (data.at(0))
            {
                case 0: map.insert("event", option("invertCover").toBool() ? "close" : "open"); break;
                case 1: map.insert("event", "stop"); break;
                case 2: map.insert("event", option("invertCover").toBool() ? "open" : "close"); break;
            }

            break;

        case 0xF001: map.insert("calibration", data.at(0) ? false : true); break;
        case 0xF002: map.insert("reverse", data.at(0) ? true : false); break;
    }

    m_value = map.isEmpty() ? QVariant() : map;
}

void PropertiesTUYA::ChildLock::parseAttribte(quint16 attributeId, const QByteArray &data)
{
    if (attributeId != 0x8000)
        return;

    m_value = data.at(0) ? true : false;
}

void PropertiesTUYA::OperationMode::parseAttribte(quint16 attributeId, const QByteArray &data)
{
    if (attributeId != 0x8004)
        return;

    switch (static_cast <quint8> (data.at(0)))
    {
        case 0x00: m_value = "command"; break;
        case 0x01: m_value = "event"; break;
    }
}

void PropertiesTUYA::IndicatorMode::parseAttribte(quint16 attributeId, const QByteArray &data)
{
    if (attributeId != 0x8001)
        return;

    switch (static_cast <quint8> (data.at(0)))
    {
        case 0x00: m_value = "off"; break;
        case 0x01: m_value = "default"; break;
        case 0x02: m_value = "inverted"; break;
        case 0x03: m_value = "on"; break;
    }
}

void PropertiesTUYA::SwitchType::parseAttribte(quint16 attributeId, const QByteArray &data)
{
    if (attributeId != 0xD030)
        return;

    switch (static_cast <quint8> (data.at(0)))
    {
        case 0x00: m_value = "toggle"; break;
        case 0x01: m_value = "static"; break;
        case 0x02: m_value = "momentary"; break;
    }
}

void PropertiesTUYA::PowerOnStatus::parseAttribte(quint16 attributeId, const QByteArray &data)
{
    if (attributeId != 0x8002)
        return;

    switch (static_cast <quint8> (data.at(0)))
    {
        case 0x00: m_value = "off"; break;
        case 0x01: m_value = "on"; break;
        case 0x02: m_value = "previous"; break;
    }
}

void PropertiesTUYA::ButtonAction::parseCommand(quint8 commandId, const QByteArray &payload)
{
    switch (commandId)
    {
        case 0xFC:

            switch (payload.at(0))
            {
                case 0x00: m_value = "rotateRight"; break;
                case 0x01: m_value = "rotateLeft"; break;
            }

            break;

        case 0xFD:

            switch (payload.at(0))
            {
                case 0x00: m_value = "singleClick"; break;
                case 0x01: m_value = "doubleClick"; break;
                case 0x02: m_value = "hold"; break;
            }

            break;
    }
}
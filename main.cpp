#include <QCoreApplication>
#include <QUdpSocket>
#include <QNetworkDatagram>
#include <QHostAddress>
#include <QDebug>
#include <QDataStream>
#include <QDateTime>
#include <QByteArray>

// CRC-16/CCITT-FALSE
quint16 calculateCRC16(const QByteArray &data)
{
    quint16 crc = 0xFFFF;

    for (char byte : data) {
        crc ^= static_cast<quint16>(
                   static_cast<quint8>(byte)) << 8;

        for (int bit = 0; bit < 8; ++bit) {
            if (crc & 0x8000) {
                crc = static_cast<quint16>(
                    (crc << 1) ^ 0x1021);
            } else {
                crc = static_cast<quint16>(crc << 1);
            }
        }
    }

    return crc;
}

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    QUdpSocket socket;

    // ONE COMMON SEQUENCE COUNTER FOR PACKET A AND PACKET B
    quint8 previousSequence = 0;
    bool sequenceStarted = false;

    quint64 packetCount = 0;
    QDateTime lastPacketTime;

    // Listen on UDP port 5000
    if (!socket.bind(QHostAddress::AnyIPv4, 5000)) {
        qDebug() << "Could not bind to port 5000:"
                 << socket.errorString();
        return 1;
    }

    qDebug() << "Receiver is listening on UDP port 5000 for Packets A and B...";

    QObject::connect(
        &socket,
        &QUdpSocket::readyRead,
        [&socket, &previousSequence,
         &sequenceStarted, &packetCount, &lastPacketTime]() {

            while (socket.hasPendingDatagrams()) {

                QNetworkDatagram datagram =
                    socket.receiveDatagram();

                QByteArray packet = datagram.data();

                QDateTime receiveTime =
                    QDateTime::currentDateTime();

                ++packetCount;

                // ==========================================
                // PACKET RECEIVED INFORMATION
                // ==========================================

                qDebug() << "\n========== PACKET RECEIVED ==========";

                qDebug() << "Timestamp             :"
                         << receiveTime.toString(
                                "yyyy-MM-dd hh:mm:ss.zzz");

                if (lastPacketTime.isValid()) {
                    qint64 diffMs =
                        lastPacketTime.msecsTo(receiveTime);

                    qDebug() << "Time since prev packet:"
                             << diffMs << "ms ("
                             << diffMs / 1000.0
                             << "seconds)";
                } else {
                    qDebug() << "Time since prev packet: First packet received.";
                }

                lastPacketTime = receiveTime;

                qDebug() << "Packet size           :"
                         << packet.size() << "bytes";

                qDebug() << "Raw data (hex)        :"
                         << packet.toHex(' ');

                // ==========================================
                // PACKET FORMAT VALIDATION
                // ==========================================

                bool validFormat =
                    packet.size() == 10 &&
                    static_cast<quint8>(packet[0]) == 0xAA;

                bool serializationValid = false;

                quint8 header = 0;
                quint8 identification = 0;
                quint8 currentSequence = 0;
                quint8 footer = 0;

                QByteArray payload;
                QByteArray receivedCrcBytes;

                quint8 expectedSequence = 1;

                if (validFormat) {

                    header =
                        static_cast<quint8>(packet[0]);

                    identification =
                        static_cast<quint8>(packet[1]);

                    currentSequence =
                        static_cast<quint8>(packet[2]);

                    payload = packet.mid(3, 4);
                    receivedCrcBytes = packet.mid(7, 2);

                    footer =
                        static_cast<quint8>(packet[9]);

                    // ======================================
                    // COMMON SERIALIZATION VALIDATION
                    //
                    // Both Packet A (ID 0x01) and
                    // Packet B (ID 0x02) use the SAME
                    // sequence.
                    //
                    // Sequence: 1, 2, 3 ... 255, 1, 2 ...
                    //
                    // First valid sequence must be 1.
                    // ======================================

                    bool recognizedPacket =
                        (identification == 0x01 ||
                         identification == 0x02);

                    if (recognizedPacket) {

                        if (!sequenceStarted) {

                            expectedSequence = 1;

                        } else {

                            expectedSequence =
                                (previousSequence == 255)
                                    ? 1
                                    : static_cast<quint8>(
                                          previousSequence + 1);
                        }

                        serializationValid =
                            (currentSequence == expectedSequence);

                        // Advance the shared counter only when
                        // the received sequence is valid.
                        //
                        // An invalid packet does not change the
                        // expected sequence. The receiver can
                        // recover when the expected number arrives.

                        if (serializationValid) {
                            previousSequence = currentSequence;
                            sequenceStarted = true;
                        }

                        qDebug() << "Serialization Status  :"
                                 << (serializationValid
                                         ? "VALID [PASS]"
                                         : "INVALID [FAIL]");

                        // qDebug() << "Expected sequence     :"
                        //          << static_cast<int>(
                        //                 expectedSequence);

                        // qDebug() << "Received sequence     :"
                        //          << static_cast<int>(
                        //                 currentSequence);

                    } else {

                        qDebug() << "Serialization Status  : Unknown packet ID";
                    }

                    qDebug() << "Received packet number:"
                             << packetCount;

                    // ==========================================
                    // CRC VALIDATION
                    // CRC covers bytes 0 through 6
                    // ==========================================

                    QByteArray dataForCrc = packet.left(7);

                    quint16 calculatedCrc =
                        calculateCRC16(dataForCrc);

                    quint16 receivedCrc = 0;

                    QDataStream crcDs(receivedCrcBytes);
                    crcDs.setByteOrder(QDataStream::BigEndian);
                    crcDs >> receivedCrc;

                    bool crcValid =
                        (calculatedCrc == receivedCrc);

                    // ==========================================
                    // PARSED PACKET INFORMATION
                    // Display only if serialization is valid.
                    // ==========================================

                    if (serializationValid) {

                        qDebug() << "\n--- Parsed Packet Information ---";

                        qDebug() << "Header                :"
                                 << QString("0x%1").arg(
                                        header, 2, 16, QChar('0'));

                        qDebug() << "Identification        :"
                                 << QString("0x%1").arg(
                                        identification, 2, 16, QChar('0'));

                        qDebug() << "Serialization Flag    :"
                                 << QString("0x%1").arg(
                                        currentSequence, 2, 16, QChar('0'));

                        // ======================================
                        // PACKET A: SPEED AND BEARING
                        // ID = 0x01
                        // ======================================

                        if (identification == 0x01) {

                            qDebug() << "Packet Type           : Packet A (Speed & Bearing)";

                            qDebug() << "Payload (hex)         :"
                                     << payload.toHex(' ');

                            quint16 rawSpeed = 0;
                            quint16 rawBearing = 0;

                            QDataStream ds(payload);
                            ds.setByteOrder(QDataStream::BigEndian);
                            ds >> rawSpeed >> rawBearing;

                            float speed =
                                static_cast<float>(rawSpeed) / 10.0f;

                            float bearing =
                                static_cast<float>(rawBearing) / 100.0f;

                            qDebug() << "  -> Speed            :"
                                     << speed << "m/s";

                            qDebug() << "  -> Bearing          :"
                                     << bearing << "deg";
                        }

                        // ======================================
                        // PACKET B: TEMPERATURE, STATUS, BATTERY
                        // ID = 0x02
                        // ======================================

                        else if (identification == 0x02) {

                            qDebug() << "Packet Type           : Packet B (Temperature, Status, Battery)";

                            qDebug() << "Payload (hex)         :"
                                     << payload.toHex(' ');

                            quint32 payloadVal = 0;

                            QDataStream ds(payload);
                            ds.setByteOrder(QDataStream::BigEndian);
                            ds >> payloadVal;

                            quint16 rawTemp =
                                (payloadVal >> 20) & 0x0FFF;

                            quint8 eqStatus =
                                (payloadVal >> 12) & 0x00FF;

                            quint8 statusCode =
                                (payloadVal >> 8) & 0x000F;

                            quint8 rawBattery =
                                payloadVal & 0x00FF;

                            float temperature =
                                static_cast<float>(rawTemp) / 10.0f;

                            float batteryVoltage =
                                static_cast<float>(rawBattery) / 10.0f;

                            qDebug() << "  -> Temperature      :"
                                     << temperature << "°C";

                            qDebug() << "  -> Eq Status        :"
                                     << QString("0x%1").arg(
                                            eqStatus, 2, 16, QChar('0'));

                            qDebug() << "  -> Status Code      :"
                                     << statusCode;

                            qDebug() << "  -> Battery Volt     :"
                                     << batteryVoltage << "V";
                        }
                    }

                    // ==========================================
                    // CRC AND FOOTER INFORMATION
                    // Uncomment these lines if needed.
                    // ==========================================

                    // qDebug() << "CRC (Received)        :"
                    //          << receivedCrcBytes.toHex(' ');

                    // qDebug() << "CRC (Calculated)      :"
                    //          << QString("0x%1").arg(
                    //                 calculatedCrc, 4, 16, QChar('0'));

                    // qDebug() << "CRC Status            :"
                    //          << (crcValid
                    //                  ? "VALID [PASS]"
                    //                  : "INVALID [FAIL]");

                    // qDebug() << "Footer                :"
                    //          << QString("0x%1").arg(
                    //                 footer, 2, 16, QChar('0'));

                } else {

                    // ==========================================
                    // INVALID PACKET FORMAT
                    // ==========================================

                    qDebug() << "\n--- Unrecognized Format ---";

                    qDebug() << "Serialization Status  : Skipped";

                    qDebug() << "Received packet number:"
                             << packetCount;

                    qDebug() << "Raw Data Only         :"
                             << packet.toHex(' ');
                }

                // ==========================================
                // SENDER INFORMATION
                // ==========================================

                qDebug() << "\nSender IP             :"
                         << datagram.senderAddress().toString();

                qDebug() << "Sender port           :"
                         << datagram.senderPort();

                qDebug() << "=====================================";
            }
        });

    return app.exec();
}
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QByteArray>
#include <QFile>
#include <QString>
#include <QtEndian>

/// Writes a silent mono 16-bit PCM WAV file of the given length.
inline bool writeSilentWav(const QString &path, int milliseconds)
{
    constexpr quint32 rate = 8000;
    const quint32 samples = rate * quint32(milliseconds) / 1000;
    const quint32 dataSize = samples * 2;
    QByteArray wav;
    const auto u32 = [&wav](quint32 value) {
        char bytes[4];
        qToLittleEndian(value, bytes);
        wav.append(bytes, 4);
    };
    const auto u16 = [&wav](quint16 value) {
        char bytes[2];
        qToLittleEndian(value, bytes);
        wav.append(bytes, 2);
    };
    wav.append("RIFF");
    u32(36 + dataSize);
    wav.append("WAVEfmt ");
    u32(16);
    u16(1);
    u16(1);
    u32(rate);
    u32(rate * 2);
    u16(2);
    u16(16);
    wav.append("data");
    u32(dataSize);
    wav.append(QByteArray(int(dataSize), '\0'));
    QFile file(path);
    return file.open(QIODevice::WriteOnly) && file.write(wav) == wav.size();
}

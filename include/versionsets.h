/*****************************************************************************
*
*  PROJECT:     Magic.TXD
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        include/versionsets.h
*  PURPOSE:     Editor game and magic-rw version management.
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*  Project location: https://osdn.net/projects/magic-txd/
*
*****************************************************************************/

#pragma once

#include <algorithm>
#include <renderware.h>
#include <QtCore/QFile>
#include <QtCore/QTextStream>
#include "testmessage.h"

class RwVersionSets {
public:
    enum ePlatformType {
        RWVS_PL_NOT_DEFINED,
        RWVS_PL_PC,
        RWVS_PL_PS2,
        RWVS_PL_PSP,
        RWVS_PL_XBOX,
        RWVS_PL_MOBILE,
        RWVS_PL_GAMECUBE,

        RWVS_PL_NUM_OF_PLATFORMS = RWVS_PL_GAMECUBE
    };

    enum eDataType {
        RWVS_DT_NOT_DEFINED,
        RWVS_DT_D3D8,
        RWVS_DT_D3D9,
        RWVS_DT_PS2,
        RWVS_DT_XBOX,
        RWVS_DT_AMD_COMPRESS,
        RWVS_DT_S3TC_MOBILE,
        RWVS_DT_UNCOMPRESSED_MOBILE,
        RWVS_DT_POWERVR,
        RWVS_DT_PSP,
        RWVS_DT_GAMECUBE,

        RWVS_DT_NUM_OF_TYPES = RWVS_DT_GAMECUBE
    };

    // translate name from settings file to id
    static ePlatformType platformIdFromName(QString name) {
        if (name == "PC")
            return RWVS_PL_PC;
        if (name == "PS2")
            return RWVS_PL_PS2;
        if (name == "PSP")
            return RWVS_PL_PSP;
        if (name == "XBOX")
            return RWVS_PL_XBOX;
        if (name == "MOBILE")
            return RWVS_PL_MOBILE;
        if (name == "GAMECUBE")
            return RWVS_PL_GAMECUBE;
        return RWVS_PL_NOT_DEFINED;
    }

    static eDataType dataIdFromName(QString name) {
        if (name == "D3D8")
            return RWVS_DT_D3D8;
        if (name == "D3D9")
            return RWVS_DT_D3D9;
        if (name == "PS2")
            return RWVS_DT_PS2;
        if (name == "XBOX")
            return RWVS_DT_XBOX;
        if (name == "AMD")
            return RWVS_DT_AMD_COMPRESS;
        if (name == "S3TC")
            return RWVS_DT_S3TC_MOBILE;
        if (name == "UNCOMPRESSED")
            return RWVS_DT_UNCOMPRESSED_MOBILE;
        if (name == "POWERVR")
            return RWVS_DT_POWERVR;
        if (name == "PSP")
            return RWVS_DT_PSP;
        if (name == "GAMECUBE")
            return RWVS_DT_GAMECUBE;
        return RWVS_DT_NOT_DEFINED;
    }

    // translate id to name for engine
    static const char *platformNameFromId(ePlatformType platform) {
        switch (platform) {
        case RWVS_PL_NOT_DEFINED:
            break;
        case RWVS_PL_PC:
            return "PC";
        case RWVS_PL_PS2:
            return "PlayStation2";
        case RWVS_PL_PSP:
            return "PSP";
        case RWVS_PL_XBOX:
            return "XBOX";
        case RWVS_PL_MOBILE:
            return "Mobile";
        case RWVS_PL_GAMECUBE:
            return "Gamecube";
        }
        return "Unknown";
    }

    static const char *dataNameFromId(eDataType dataType) {
        switch (dataType) {
        case RWVS_DT_NOT_DEFINED:
            break;
        case RWVS_DT_D3D8:
            return "Direct3D8";
        case RWVS_DT_D3D9:
            return "Direct3D9";
        case RWVS_DT_PS2:
            return "PlayStation2";
        case RWVS_DT_XBOX:
            return "XBOX";
        case RWVS_DT_AMD_COMPRESS:
            return "AMDCompress";
        case RWVS_DT_S3TC_MOBILE:
            return "s3tc_mobile";
        case RWVS_DT_UNCOMPRESSED_MOBILE:
            return "uncompressed_mobile";
        case RWVS_DT_POWERVR:
            return "PowerVR";
        case RWVS_DT_PSP:
            return "PSP";
        case RWVS_DT_GAMECUBE:
            return "Gamecube";
        }
        return "Unknown";
    }

    static eDataType dataIdFromEnginePlatformName(QString name) {
        if (name == "Direct3D8")
            return RWVS_DT_D3D8;
        if (name == "Direct3D9")
            return RWVS_DT_D3D9;
        if (name == "PlayStation2")
            return RWVS_DT_PS2;
        if (name == "XBOX")
            return RWVS_DT_XBOX;
        if (name == "AMDCompress")
            return RWVS_DT_AMD_COMPRESS;
        if (name == "s3tc_mobile")
            return RWVS_DT_S3TC_MOBILE;
        if (name == "uncompressed_mobile")
            return RWVS_DT_UNCOMPRESSED_MOBILE;
        if (name == "PowerVR")
            return RWVS_DT_POWERVR;
        if (name == "PSP")
            return RWVS_DT_PSP;
        if (name == "Gamecube")
            return RWVS_DT_GAMECUBE;
        return RWVS_DT_NOT_DEFINED;
    }

    static QString streamGetOneLine(QTextStream &stream) {
        QString line = stream.readLine();
        return line.trimmed();
    }

    static QString extractValue(QString &line) {
        QString value;
        int start = line.indexOf('(');
        if(start != -1){
            start += 1;
            int end = line.indexOf(')', start);
            if (end != -1) {
                if (end - start > 0)
                    value = line.mid(start, end - start);
            }
        }
        return value;
    }

    static void lineToVersion(const QString &line, rw::LibraryVersion &rwVersion) {
        QStringList strList = line.split('.');
        rwVersion.set(strList[0].toInt(), strList[1].toInt(), strList[2].toInt(), strList[3].toInt());
    }

    class Set {
    public:
        class Platform {
        public:
            ePlatformType platformType;
            rw::LibraryVersion version;
            rw::LibraryVersion versionMin;
            rw::LibraryVersion versionMax;
            bool hasMinVersion, hasMaxVersion;
            QVector<eDataType> availableDataTypes;

            Platform() {
                version.set(0, 0, 0, 0);
                hasMinVersion = hasMaxVersion = false;
            }

            QString Read(ePlatformType Type, QTextStream &stream) {
                platformType = Type;
                QString line = RwVersionSets::streamGetOneLine(stream);
                while (!stream.atEnd()) {
                    if (line.size() > 0 && line[0] != '#') {
                        if (line.startsWith("SET") || line.startsWith("PLATFORM"))
                            return line;
                        if (line.startsWith("RWVER("))
                            lineToVersion(RwVersionSets::extractValue(line), version);
                        else if (line.startsWith("RWVERMIN")) {
                            lineToVersion(RwVersionSets::extractValue(line), versionMin);
                            hasMinVersion = true;
                        }
                        else if (line.startsWith("RWVERMAX")) {
                            lineToVersion(RwVersionSets::extractValue(line), versionMax);
                            hasMaxVersion = true;
                        }
                        else if (line.startsWith("RWBUILD"))
                            version.buildNumber = RwVersionSets::extractValue(line).toInt(NULL, 0);
                        else if (line.startsWith("DATATYPE")) {
                            QStringList dataTypes = RwVersionSets::extractValue(line).split(',');
                            for (QString &dataStr : dataTypes) {
                                RwVersionSets::eDataType dataType = RwVersionSets::dataIdFromName(dataStr);
                                if (dataType != RWVS_DT_NOT_DEFINED)
                                    availableDataTypes.push_back(dataType);
                            }
                        }
                    }
                    line = RwVersionSets::streamGetOneLine(stream);
                }
                return QString();
            }
        };

        QString Read(QString setName, QTextStream &stream) {
            name = setName;
            QString line = RwVersionSets::streamGetOneLine(stream);
            while (!stream.atEnd()) {
                if (line.size() > 0 && line[0] != '#') {
                    if (line.startsWith("SET"))
                        return line;
                    if (line.startsWith("DISPNAME"))
                        displayName = RwVersionSets::extractValue(line);
                    else if(line.startsWith("ICONNAME"))
                        iconName = RwVersionSets::extractValue(line);
                    else if (line.startsWith("PLATFORM")) {
                        ePlatformType platformType = platformIdFromName(RwVersionSets::extractValue(line));
                        if (platformType != RWVS_PL_NOT_DEFINED) {
                            availablePlatforms.resize(availablePlatforms.size() + 1);
                            Platform &platform = availablePlatforms[availablePlatforms.size() - 1];
                            line = platform.Read(platformType, stream);
                            if (!platform.hasMinVersion)
                                platform.versionMin = platform.version;
                            if (!platform.hasMaxVersion)
                                platform.versionMax = platform.version;
                            continue;
                        }
                    }
                }
                line = RwVersionSets::streamGetOneLine(stream);
            }
            return QString();
        }

        QString name;
        QString displayName;
        QString iconName;
        QVector<Platform> availablePlatforms;
    };

    QVector<Set> sets;

    void readSetsFile(QString filename) {
        QFile file(filename);
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
            return;
        QTextStream stream(&file);
        QString line = RwVersionSets::streamGetOneLine(stream);
        while (!stream.atEnd()) {
            if (line.size() > 0 && line[0] != '#') {
                if (line.startsWith("SET")) {
                    sets.resize(sets.size() + 1);
                    line = sets[sets.size() - 1].Read(RwVersionSets::extractValue(line), stream);
                    continue;
                }
            }
            line = RwVersionSets::streamGetOneLine(stream);
        }
    }

    bool matchSet(const rw::LibraryVersion &libVersion, eDataType dataTypeId, int &setIndex, int &platformIndex, int &dataTypeIndex) {
        const int numSets = sets.size();
        for (int set = 0; set < numSets; set++) {
            const RwVersionSets::Set& currentSet = sets[set];
            const int numAvailPlatforms = currentSet.availablePlatforms.size();

            for (int p = 0; p < numAvailPlatforms; p++) {
                const RwVersionSets::Set::Platform& platform = currentSet.availablePlatforms[p];
                if (platform.versionMin.version <= libVersion.version && platform.versionMax.version >= libVersion.version) {
                    const int numAvailDataTypes = platform.availableDataTypes.size();

                    for (int d = 0; d < numAvailDataTypes; d++) {
                        if (platform.availableDataTypes[d] == dataTypeId) {
                            setIndex = set;
                            platformIndex = p;
                            dataTypeIndex = d;
                            return true;
                        }
                    }
                }
            }
        }
        return false;
    }
};
